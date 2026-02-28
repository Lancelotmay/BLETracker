#ifndef BLE_CORE_H
#define BLE_CORE_H

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

#endif /* BLE_CORE_H */
