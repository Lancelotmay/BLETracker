/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "ble_core.h"
#include "ble_lns.h"
#include <zephyr/bluetooth/gatt.h>

LOG_MODULE_REGISTER(ble_core, LOG_LEVEL_INF);

#define BLE_LNS_DEFAULT_FEATURES \
    (BLE_LNS_FEAT_INST_SPEED | BLE_LNS_FEAT_LOCATION | BLE_LNS_FEAT_HEADING | BLE_LNS_FEAT_UTC_TIME)

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                    BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
                    BT_UUID_16_ENCODE(BT_UUID_LNS_VAL)),
};
static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, 0x41, 0x14),
};
enum {
    STATE_CONNECTED,
    STATE_DISCONNECTED,
    STATE_RECYCLED,

    STATE_BITS,
};

static ATOMIC_DEFINE(state, STATE_BITS);
static atomic_t lns_notify_enabled;
static struct bt_conn *default_conn;
static struct bt_gatt_exchange_params exchange_params;

static void update_data_length(struct bt_conn *conn)
{
    int err;
    struct bt_conn_le_data_len_param my_data_len = {
        .tx_max_len = BT_GAP_DATA_LEN_MAX,
        .tx_max_time = BT_GAP_DATA_TIME_MAX,
    };
    err = bt_conn_le_data_len_update(conn, &my_data_len);
    if (err) {
        LOG_ERR("data_len_update failed (err %d)", err);
    }
}

static void exchange_func(struct bt_conn *conn, uint8_t att_err,
			  struct bt_gatt_exchange_params *params)
{
	LOG_INF("MTU exchange %s", att_err == 0 ? "successful" : "failed");
    if (!att_err) {
        uint16_t payload_mtu = bt_gatt_get_mtu(conn) - 3;   // 3 bytes used for Attribute headers.
        LOG_INF("New MTU: %d bytes", payload_mtu);
    }
}

static void update_mtu(struct bt_conn *conn)
{
    int err;
    exchange_params.func = exchange_func;

    err = bt_gatt_exchange_mtu(conn, &exchange_params);
    if (err) {
        LOG_ERR("bt_gatt_exchange_mtu failed (err %d)", err);
    }
}

static void on_connected(struct bt_conn* conn, uint8_t err)
{
    struct bt_conn_info info;
    double connection_interval;
    uint16_t supervision_timeout;

    if (err != 0U) {
        LOG_INF("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
        return;
    }

    LOG_INF("Connected");
    (void)atomic_set_bit(state, STATE_CONNECTED);
    if (default_conn == NULL) {
        default_conn = bt_conn_ref(conn);
    }

    err = bt_conn_get_info(conn, &info);
    if (err != 0U) {
        LOG_ERR("bt_conn_get_info() returned %d", err);
        return;
    }

    LOG_DBG("Connection type: %s", (info.type == BT_CONN_TYPE_LE) ? "LE" : "BR/EDR");

    connection_interval = BT_GAP_US_TO_CONN_INTERVAL(info.le.interval_us) * 1.25;
    supervision_timeout = info.le.timeout * 10;

    LOG_DBG("Connection parameters: interval %.2f ms, latency %d intervals, timeout %d ms",
            connection_interval, info.le.latency, supervision_timeout);
    k_sleep(K_MSEC(1000));  // Delay added to avoid link layer collisions.
    update_data_length(conn);
    update_mtu(conn);
}

static void on_disconnected(struct bt_conn* conn, uint8_t reason)
{
    LOG_INF("Disconnected, reason 0x%02x %s", reason, bt_hci_err_to_str(reason));
    (void)atomic_set_bit(state, STATE_DISCONNECTED);
    if (conn == default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
}

static void on_recycled(void)
{
    int err;
    LOG_INF("Connection recycled");
    err = ble_adv_start();
    if (err != 0) {
        LOG_WRN("Failed to restart advertising (err %d)", err);
    }

    (void)atomic_set_bit(state, STATE_RECYCLED);
}

static void on_le_param_updated(struct bt_conn* conn,
                                uint16_t interval,
                                uint16_t latency,
                                uint16_t timeout)
{
    double connection_interval;
    uint16_t supervision_timeout;

    ARG_UNUSED(conn);

    connection_interval = interval * 1.25;
    supervision_timeout = timeout * 10;

    LOG_INF("Connection parameters updated: interval %.2f ms, latency %d intervals, timeout %d ms",
            connection_interval, latency, supervision_timeout);
}


void on_le_data_len_updated(struct bt_conn *conn, struct bt_conn_le_data_len_info *info)
{
    uint16_t tx_len     = info->tx_max_len; 
    uint16_t tx_time    = info->tx_max_time;
    uint16_t rx_len     = info->rx_max_len;
    uint16_t rx_time    = info->rx_max_time;
    LOG_INF("Data length updated. Length %d/%d bytes, time %d/%d us", tx_len, rx_len, tx_time, rx_time);
}



BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected              = on_connected,
    .disconnected           = on_disconnected,
    .recycled               = on_recycled,
    .le_param_updated       = on_le_param_updated,
    .le_data_len_updated    = on_le_data_len_updated,
};


static void lns_on_notify_changed(bool enabled)
{
    atomic_set(&lns_notify_enabled, enabled ? 1 : 0);
    LOG_INF("LNS Service: notifications are %s", enabled ? "ENABLED" : "DISABLED");
}

static const struct ble_lns_cb lns_callbacks = {
    .notify_changed = lns_on_notify_changed,
};

int ble_adv_start(void)
{
    return bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
}

int ble_adv_stop(void)
{
    return bt_le_adv_stop();
}

int ble_init(void)
{
    int err;

    atomic_clear(&lns_notify_enabled);

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }

    err = ble_lns_init(BLE_LNS_DEFAULT_FEATURES, &lns_callbacks);
    if (err != 0) {
        LOG_ERR("Failed to initialize LNS service (err %d)", err);
        return err;
    }

    err = ble_adv_start();
    if (err) {
        LOG_ERR("Failed to start advertising (err %d)", err);
        return err;
    }

    LOG_INF("Bluetooth initialized and advertising");
    return 0;
}

struct bt_conn *ble_get_default_conn(void)
{
    return default_conn;
}

bool ble_lns_notify_is_enabled(void)
{
    return (atomic_get(&lns_notify_enabled) != 0);
}
