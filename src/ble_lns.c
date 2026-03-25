/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include "ble_lns.h"

LOG_MODULE_REGISTER(ble_lns, LOG_LEVEL_INF);

#define BLE_LNS_LOC_SPEED_MAX_LEN 21U
#define BLE_LNS_DEFINED_FEATURE_MASK GENMASK(20, 0)
#define BLE_LNS_IMPLEMENTED_FEATURE_MASK \
    (BLE_LNS_FEAT_INST_SPEED | BLE_LNS_FEAT_LOCATION | BLE_LNS_FEAT_HEADING | BLE_LNS_FEAT_UTC_TIME)
#define BLE_LNS_LOC_SPEED_DEFINED_FLAGS_MASK                                           \
    (BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT | BLE_LNS_LOC_SPEED_TOTAL_DISTANCE_PRESENT | \
        BLE_LNS_LOC_SPEED_LOCATION_PRESENT | BLE_LNS_LOC_SPEED_ELEVATION_PRESENT |        \
        BLE_LNS_LOC_SPEED_HEADING_PRESENT | BLE_LNS_LOC_SPEED_ROLLING_TIME_PRESENT |      \
        BLE_LNS_LOC_SPEED_UTC_TIME_PRESENT | BLE_LNS_LOC_SPEED_SPEED_DISTANCE_3D |        \
        BLE_LNS_LOC_SPEED_POS_STATUS_MASK | BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_MASK |     \
        BLE_LNS_LOC_SPEED_HEADING_SOURCE)

static uint32_t lns_feature;
static struct ble_lns_cb lns_cb;
static const struct bt_gatt_attr* loc_speed_attr;

static ssize_t read_ln_feature(struct bt_conn* conn,
                                const struct bt_gatt_attr* attr,
                                void* buf,
                                uint16_t len,
                                uint16_t offset)
{
    uint32_t value = sys_cpu_to_le32(lns_feature);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, &value, sizeof(value));
}

static void loc_speed_ccc_cfg_changed(const struct bt_gatt_attr* attr, uint16_t value)
{
    bool enabled = (value == BT_GATT_CCC_NOTIFY);

    LOG_INF("LNS Location and Speed notifications %s", enabled ? "enabled" : "disabled");

    if (lns_cb.notify_changed != NULL) {
        lns_cb.notify_changed(enabled);
    }
}

BT_GATT_SERVICE_DEFINE(lns_svc,
                        BT_GATT_PRIMARY_SERVICE(BT_UUID_LNS),
                        BT_GATT_CHARACTERISTIC(BT_UUID_GATT_LNF,
                                            BT_GATT_CHRC_READ,
                                            BT_GATT_PERM_READ,
                                            read_ln_feature,
                                            NULL,
                                            NULL),
                        BT_GATT_CHARACTERISTIC(BT_UUID_GATT_LOC_SPD,
                                            BT_GATT_CHRC_NOTIFY,
                                            BT_GATT_PERM_NONE,
                                            NULL,
                                            NULL,
                                            NULL),
                        BT_GATT_CCC(loc_speed_ccc_cfg_changed,
                                    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

static int validate_feature_bits(uint32_t feature_bits)
{
    if ((feature_bits & ~BLE_LNS_DEFINED_FEATURE_MASK) != 0U) {
        return -EINVAL;
    }

    return 0;
}

static void encode_utc_time(struct net_buf_simple* buf, const struct ble_lns_utc_time* utc)
{
    net_buf_simple_add_le16(buf, utc->year);
    net_buf_simple_add_u8(buf, utc->month);
    net_buf_simple_add_u8(buf, utc->day);
    net_buf_simple_add_u8(buf, utc->hours);
    net_buf_simple_add_u8(buf, utc->minutes);
    net_buf_simple_add_u8(buf, utc->seconds);
}

static void encode_u24(struct net_buf_simple* buf, uint32_t value)
{
    net_buf_simple_add_u8(buf, value & 0xFFU);
    net_buf_simple_add_u8(buf, (value >> 8) & 0xFFU);
    net_buf_simple_add_u8(buf, (value >> 16) & 0xFFU);
}

static void encode_s24(struct net_buf_simple* buf, int32_t value)
{
    encode_u24(buf, (uint32_t)value);
}

static void encode_loc_speed(struct net_buf_simple* buf, const struct ble_lns_loc_speed* sample)
{
    uint16_t in_flags = sample->flags & BLE_LNS_LOC_SPEED_DEFINED_FLAGS_MASK;
    uint16_t flags = 0U;
    bool inst_speed_encoded = false;
    bool total_distance_encoded = false;
    uint8_t* flags_ptr;

    net_buf_simple_reset(buf);
    flags_ptr = net_buf_simple_add(buf, sizeof(uint16_t));
    flags_ptr[0] = 0U;
    flags_ptr[1] = 0U;

    if (((lns_feature & BLE_LNS_FEAT_INST_SPEED) != 0U) &&
        ((in_flags & BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT) != 0U)) {
        net_buf_simple_add_le16(buf, sample->inst_speed);
        flags |= BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT;
        inst_speed_encoded = true;
    }

    if (((lns_feature & BLE_LNS_FEAT_TOTAL_DISTANCE) != 0U) &&
        ((in_flags & BLE_LNS_LOC_SPEED_TOTAL_DISTANCE_PRESENT) != 0U)) {
        encode_u24(buf, sample->total_distance);
        flags |= BLE_LNS_LOC_SPEED_TOTAL_DISTANCE_PRESENT;
        total_distance_encoded = true;
    }

    if (((lns_feature & BLE_LNS_FEAT_LOCATION) != 0U) &&
        ((in_flags & BLE_LNS_LOC_SPEED_LOCATION_PRESENT) != 0U)) {
        net_buf_simple_add_le32(buf, sample->latitude);
        net_buf_simple_add_le32(buf, sample->longitude);
        flags |= BLE_LNS_LOC_SPEED_LOCATION_PRESENT;
        flags |= in_flags & BLE_LNS_LOC_SPEED_POS_STATUS_MASK;

    }

    if (((lns_feature & BLE_LNS_FEAT_ELEVATION) != 0U) &&
        ((in_flags & BLE_LNS_LOC_SPEED_ELEVATION_PRESENT) != 0U)) {
        encode_s24(buf, sample->elevation);
        flags |= BLE_LNS_LOC_SPEED_ELEVATION_PRESENT;
        flags |= in_flags & BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_MASK;

    }

    if (((lns_feature & BLE_LNS_FEAT_HEADING) != 0U) &&
        ((in_flags & BLE_LNS_LOC_SPEED_HEADING_PRESENT) != 0U)) {
        net_buf_simple_add_le16(buf, sample->heading);
        flags |= BLE_LNS_LOC_SPEED_HEADING_PRESENT;
        flags |= in_flags & BLE_LNS_LOC_SPEED_HEADING_SOURCE;
    }

    if (((lns_feature & BLE_LNS_FEAT_ROLLING_TIME) != 0U) &&
        ((in_flags & BLE_LNS_LOC_SPEED_ROLLING_TIME_PRESENT) != 0U)) {
        net_buf_simple_add_u8(buf, sample->rolling_time);
        flags |= BLE_LNS_LOC_SPEED_ROLLING_TIME_PRESENT;
    }

    if (((lns_feature & BLE_LNS_FEAT_UTC_TIME) != 0U) &&
        ((in_flags & BLE_LNS_LOC_SPEED_UTC_TIME_PRESENT) != 0U)) {
        encode_utc_time(buf, &sample->utc_time);
        flags |= BLE_LNS_LOC_SPEED_UTC_TIME_PRESENT;
    }

    if ((((lns_feature & BLE_LNS_FEAT_TOTAL_DISTANCE) != 0U) && ((in_flags & BLE_LNS_LOC_SPEED_TOTAL_DISTANCE_PRESENT) != 0U)) ||
        (((lns_feature & BLE_LNS_FEAT_INST_SPEED) != 0U) && ((in_flags & BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT) != 0U))) {
        flags |= (in_flags & BLE_LNS_LOC_SPEED_SPEED_DISTANCE_3D);
    }

    sys_put_le16(flags, flags_ptr);
}

int ble_lns_init(uint32_t feature_bits, const struct ble_lns_cb* cb)
{
    int err = validate_feature_bits(feature_bits);
    uint32_t effective_features;
    if (err != 0) {
        return err;
    }
    effective_features =
        feature_bits & BLE_LNS_DEFINED_FEATURE_MASK & BLE_LNS_IMPLEMENTED_FEATURE_MASK;
    if (effective_features != feature_bits) {
        LOG_WRN("Unsupported LNS feature bits were ignored (requested: 0x%08x, effective: 0x%08x)",
                feature_bits, effective_features);
    }

    lns_feature = effective_features;
    
    __ASSERT(cb != NULL, "LNS callback cannot be NULL");
    lns_cb = *cb;
    
    loc_speed_attr = bt_gatt_find_by_uuid(lns_svc.attrs, lns_svc.attr_count, BT_UUID_GATT_LOC_SPD);
    __ASSERT_NO_MSG(loc_speed_attr != NULL);

    LOG_INF("LNS initialized (feature bits: 0x%08x)", lns_feature);
    return 0;
}

uint32_t ble_lns_get_feature(void)
{
    return lns_feature;
}

int ble_lns_notify_location_speed(struct bt_conn* conn, const struct ble_lns_loc_speed* sample)
{
    int err;
    uint16_t mtu;
    NET_BUF_SIMPLE_DEFINE(encoded, BLE_LNS_LOC_SPEED_MAX_LEN);
    const struct bt_gatt_attr* attr = loc_speed_attr;

    encode_loc_speed(&encoded, sample);
    mtu = bt_gatt_get_mtu(conn);

    if (encoded.len > (mtu - 3U)) {
        return -EMSGSIZE;
    }

    err = bt_gatt_notify(conn, attr, encoded.data, encoded.len);
    return err;
}
