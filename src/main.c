#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include "app_battery.h"
#include "ble_core.h"


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);



/* Get I2C spec from i2c0 node */
static const struct i2c_dt_spec gnss_i2c = 
    I2C_DT_SPEC_GET(DT_NODELABEL(gnss));

int main(void) {
    int err;

    LOG_INF("BLE Tracker starting via RTT...\n");
    /* Hardware Init */

    if (!i2c_is_ready_dt(&gnss_i2c)) {
        LOG_INF("I2C/GNSS not ready!\n");
    }

    /* Enable Bluetooth stack */
    err = bt_enable(NULL);
    if (err) {
        return err;
    }
    err = ble_adv_start();
    if (err) {
        LOG_ERR("Failed to start advertising, err %d\n", err);
        return err;
    }
    /* Init battery monitoring */
    err = app_battery_init();
    if (err){
        LOG_ERR("BATT Mon ERR %d\n", err);
    } else {
        app_battery_start();
    }
    

    while (1) {
        // Your logic here
        k_sleep(K_MSEC(1000));
    }
}