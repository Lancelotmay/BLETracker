#ifndef APP_BATTERY_H
#define APP_BATTERY_H

/**
 * @brief Initialize the battery reading module (e.g., configure ADC hardware).
 */
int app_battery_init(void);

/**
 * @brief Start periodic battery level monitoring and Bluetooth reporting.
 */
void app_battery_start(void);

/**
 * @brief Stop battery monitoring.
 */
void app_battery_stop(void);

#endif /* APP_BATTERY_H */