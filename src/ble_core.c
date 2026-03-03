#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "ble_core.h"
#include "ble_lns.h"

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
}

static void on_disconnected(struct bt_conn* conn, uint8_t reason)
{
    LOG_INF("Disconnected, reason 0x%02x %s", reason, bt_hci_err_to_str(reason));
    (void)atomic_set_bit(state, STATE_DISCONNECTED);
}

static void on_recycled(struct bt_conn* conn)
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

static void lns_on_notify_changed(bool enabled)
{
    LOG_INF("LNS Service: notifications are %s", enabled ? "ENABLED" : "DISABLED");
}

static const struct ble_lns_cb lns_callbacks = {
    .notify_changed = lns_on_notify_changed,
};

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
    .recycled = on_recycled,
    .le_param_updated = on_le_param_updated,
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
