/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_CORE_H
#define BLE_CORE_H

#include <stdbool.h>
#include <zephyr/bluetooth/conn.h>

/**
 * @brief Initialize BLE service.
 */
int ble_init(void);

/**
 * @brief Start BLE advertising.
 */
int ble_adv_start(void);

/**
 * @brief Stop BLE advertising.
 */
int ble_adv_stop(void);

/**
 * @brief Get the current default LE connection.
 *
 * @return Connection reference or NULL if not connected.
 */
struct bt_conn *ble_get_default_conn(void);

/**
 * @brief Check whether LNS Location and Speed notification is enabled.
 */
bool ble_lns_notify_is_enabled(void);

#endif /* BLE_CORE_H */
