/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
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

#define BLE_LNS_LOC_SPEED_MAX_LEN	21U
#define BLE_LNS_DEFINED_FEATURE_MASK	GENMASK(20, 0)
#define BLE_LNS_IMPLEMENTED_FEATURE_MASK \
	(BLE_LNS_FEAT_INST_SPEED | BLE_LNS_FEAT_LOCATION | \
	 BLE_LNS_FEAT_HEADING | BLE_LNS_FEAT_UTC_TIME)
#define BLE_LNS_LOC_SPEED_DEFINED_FLAGS_MASK \
	(BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT | BLE_LNS_LOC_SPEED_TOTAL_DISTANCE_PRESENT | \
	 BLE_LNS_LOC_SPEED_LOCATION_PRESENT | BLE_LNS_LOC_SPEED_ELEVATION_PRESENT | \
	 BLE_LNS_LOC_SPEED_HEADING_PRESENT | BLE_LNS_LOC_SPEED_ROLLING_TIME_PRESENT | \
	 BLE_LNS_LOC_SPEED_UTC_TIME_PRESENT | BLE_LNS_LOC_SPEED_SPEED_DISTANCE_3D | \
	 BLE_LNS_LOC_SPEED_POS_STATUS_MASK | BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_MASK | \
	 BLE_LNS_LOC_SPEED_HEADING_SOURCE)

static uint32_t lns_feature;
static const struct ble_lns_cb *lns_cb;
static const struct bt_gatt_attr *loc_speed_attr;

static ssize_t read_ln_feature(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	uint32_t value = sys_cpu_to_le32(lns_feature);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value, sizeof(value));
}

static void loc_speed_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	bool enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_INF("LNS Location and Speed notifications %s", enabled ? "enabled" : "disabled");

	if ((lns_cb != NULL) && (lns_cb->notify_changed != NULL)) {
		lns_cb->notify_changed(enabled);
	}
}

BT_GATT_SERVICE_DEFINE(lns_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_LNS),
	BT_GATT_CHARACTERISTIC(BT_UUID_GATT_LNF, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_ln_feature, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_GATT_LOC_SPD, BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(loc_speed_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static int validate_feature_bits(uint32_t feature_bits)
{
	if ((feature_bits & ~BLE_LNS_DEFINED_FEATURE_MASK) != 0U) {
		return -EINVAL;
	}

	if ((feature_bits & ~BLE_LNS_IMPLEMENTED_FEATURE_MASK) != 0U) {
		return -ENOTSUP;
	}

	return 0;
}

static int validate_loc_speed_flags(uint16_t flags)
{
	uint16_t pos_status;
	uint16_t elevation_source;

	if ((flags & ~BLE_LNS_LOC_SPEED_DEFINED_FLAGS_MASK) != 0U) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT) &&
	    ((lns_feature & BLE_LNS_FEAT_INST_SPEED) == 0U)) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_TOTAL_DISTANCE_PRESENT) &&
	    ((lns_feature & BLE_LNS_FEAT_TOTAL_DISTANCE) == 0U)) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_LOCATION_PRESENT) &&
	    ((lns_feature & BLE_LNS_FEAT_LOCATION) == 0U)) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_ELEVATION_PRESENT) &&
	    ((lns_feature & BLE_LNS_FEAT_ELEVATION) == 0U)) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_HEADING_PRESENT) &&
	    ((lns_feature & BLE_LNS_FEAT_HEADING) == 0U)) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_ROLLING_TIME_PRESENT) &&
	    ((lns_feature & BLE_LNS_FEAT_ROLLING_TIME) == 0U)) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_UTC_TIME_PRESENT) &&
	    ((lns_feature & BLE_LNS_FEAT_UTC_TIME) == 0U)) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_SPEED_DISTANCE_3D) &&
	    (((lns_feature & BLE_LNS_FEAT_INST_SPEED) == 0U) ||
	     ((lns_feature & BLE_LNS_FEAT_TOTAL_DISTANCE) == 0U))) {
		return -EINVAL;
	}

	if ((flags & BLE_LNS_LOC_SPEED_HEADING_SOURCE) &&
	    ((lns_feature & BLE_LNS_FEAT_HEADING) == 0U)) {
		return -EINVAL;
	}

	elevation_source = (flags & BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_MASK) >>
			   BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_SHIFT;
	if ((elevation_source != BLE_LNS_ELEVATION_POSITIONING_SYSTEM) &&
	    ((lns_feature & BLE_LNS_FEAT_ELEVATION) == 0U)) {
		return -EINVAL;
	}

	pos_status = (flags & BLE_LNS_LOC_SPEED_POS_STATUS_MASK) >> BLE_LNS_LOC_SPEED_POS_STATUS_SHIFT;
	if ((pos_status != BLE_LNS_POS_NO_POSITION) &&
	    ((lns_feature & BLE_LNS_FEAT_POSITION_STATUS) == 0U)) {
		return -EINVAL;
	}

	return 0;
}

static void encode_utc_time(struct net_buf_simple *buf, const struct ble_lns_utc_time *utc)
{
	net_buf_simple_add_le16(buf, utc->year);
	net_buf_simple_add_u8(buf, utc->month);
	net_buf_simple_add_u8(buf, utc->day);
	net_buf_simple_add_u8(buf, utc->hours);
	net_buf_simple_add_u8(buf, utc->minutes);
	net_buf_simple_add_u8(buf, utc->seconds);
}

static void encode_u24(struct net_buf_simple *buf, uint32_t value)
{
	net_buf_simple_add_u8(buf, value & 0xFFU);
	net_buf_simple_add_u8(buf, (value >> 8) & 0xFFU);
	net_buf_simple_add_u8(buf, (value >> 16) & 0xFFU);
}

static void encode_s24(struct net_buf_simple *buf, int32_t value)
{
	encode_u24(buf, (uint32_t)value);
}

static void encode_loc_speed(struct net_buf_simple *buf, const struct ble_lns_loc_speed *sample)
{
	uint16_t flags = sample->flags;

	net_buf_simple_reset(buf);
	net_buf_simple_add_le16(buf, flags);

	if ((flags & BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT) != 0U) {
		net_buf_simple_add_le16(buf, sample->inst_speed);
	}

	if ((flags & BLE_LNS_LOC_SPEED_TOTAL_DISTANCE_PRESENT) != 0U) {
		encode_u24(buf, sample->total_distance);
	}

	if ((flags & BLE_LNS_LOC_SPEED_LOCATION_PRESENT) != 0U) {
		net_buf_simple_add_le32(buf, sample->latitude);
		net_buf_simple_add_le32(buf, sample->longitude);
	}

	if ((flags & BLE_LNS_LOC_SPEED_ELEVATION_PRESENT) != 0U) {
		encode_s24(buf, sample->elevation);
	}

	if ((flags & BLE_LNS_LOC_SPEED_HEADING_PRESENT) != 0U) {
		net_buf_simple_add_le16(buf, sample->heading);
	}

	if ((flags & BLE_LNS_LOC_SPEED_ROLLING_TIME_PRESENT) != 0U) {
		net_buf_simple_add_u8(buf, sample->rolling_time);
	}

	if ((flags & BLE_LNS_LOC_SPEED_UTC_TIME_PRESENT) != 0U) {
		encode_utc_time(buf, &sample->utc_time);
	}
}

struct ble_lns_conn_lookup {
	struct bt_conn *conn;
};

static void find_first_conn(struct bt_conn *conn, void *user_data)
{
	struct ble_lns_conn_lookup *ctx = user_data;

	if (ctx->conn == NULL) {
		ctx->conn = bt_conn_ref(conn);
	}
}

int ble_lns_init(uint32_t feature_bits)
{
	int err = validate_feature_bits(feature_bits);

	if (err != 0) {
		return err;
	}

	lns_feature = feature_bits;
	lns_cb = NULL;
	loc_speed_attr = bt_gatt_find_by_uuid(lns_svc.attrs, lns_svc.attr_count, BT_UUID_GATT_LOC_SPD);
	__ASSERT_NO_MSG(loc_speed_attr != NULL);

	LOG_INF("LNS initialized (feature bits: 0x%08x)", lns_feature);
	return 0;
}

int ble_lns_register_cb(const struct ble_lns_cb *cb)
{
	lns_cb = cb;
	return 0;
}

uint32_t ble_lns_get_feature(void)
{
	return lns_feature;
}

int ble_lns_notify_location_speed(struct bt_conn *conn, const struct ble_lns_loc_speed *sample)
{
	int err;
	uint16_t mtu;
	NET_BUF_SIMPLE_DEFINE(encoded, BLE_LNS_LOC_SPEED_MAX_LEN);
	const struct bt_gatt_attr *attr = loc_speed_attr;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	if (conn == NULL) {
		return -EINVAL;
	}

	if (attr == NULL) {
		return -EIO;
	}

	if (sample == NULL) {
		return -EINVAL;
	}

	err = validate_loc_speed_flags(sample->flags);
	if (err != 0) {
		return err;
	}

	if (!bt_gatt_is_subscribed(conn, attr, BT_GATT_CCC_NOTIFY)) {
		return -EAGAIN;
	}

	encode_loc_speed(&encoded, sample);
	mtu = bt_gatt_get_mtu(conn);

	if (encoded.len > (mtu - 3U)) {
		return -EMSGSIZE;
	}

	err = bt_gatt_notify(conn, attr, encoded.data, encoded.len);
	if (err == -ENOTCONN) {
		return -EAGAIN;
	}

	return err;
}

int ble_lns_update_location_speed(const struct ble_lns_loc_speed *sample)
{
	struct ble_lns_conn_lookup ctx = { 0 };
	int err;

	bt_conn_foreach(BT_CONN_TYPE_LE, find_first_conn, &ctx);
	if (ctx.conn == NULL) {
		return -EAGAIN;
	}

	err = ble_lns_notify_location_speed(ctx.conn, sample);
	bt_conn_unref(ctx.conn);

	return err;
}
