/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "app_battery.h"
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define APP_BATTERY_READ_INTERVAL_SEC 300
/* Battery voltage thresholds in microvolts (compensated for 1/2 divider)
 * 4.2V actual -> 2.1V at pin (2100000 uV)
 * 3.3V actual -> 1.65V at pin (1650000 uV)
 */
#define BATT_MAX_PIN_UV 2100000
#define BATT_MIN_PIN_UV 1650000

/* Register log module for debugging */
LOG_MODULE_REGISTER(app_bat, LOG_LEVEL_INF);

/* Get ADC spec from zephyr,user node */
static const struct adc_dt_spec batt_adc = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

/* Forward declarations */
static void battery_work_handler(struct k_work* work);
static void battery_timer_handler(struct k_timer* timer);

/* Define work item and timer statically */
static K_WORK_DEFINE(battery_work, battery_work_handler);
static K_TIMER_DEFINE(battery_timer, battery_timer_handler, NULL);

static void battery_work_handler(struct k_work* work)
{
    int err;
    uint16_t buf;
    int32_t val_uv;

    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
        .channels = BIT(batt_adc.channel_id), 
        .resolution = batt_adc.resolution,
        .oversampling = batt_adc.oversampling,
    };

    err = adc_read_dt(&batt_adc, &sequence);
    if (err < 0) {
        LOG_ERR("ADC read failed (err %d)", err);
        return;
    }

    val_uv = (int32_t)buf;
    err = adc_raw_to_microvolts_dt(&batt_adc, &val_uv);
    if (err < 0) {
        LOG_ERR("Microvolt conversion failed (err %d)", err);
        return;
    }

    uint8_t level = 0;

    if (val_uv >= BATT_MAX_PIN_UV) {
        level = 100;
    } else if (val_uv > BATT_MIN_PIN_UV) {
        level = (uint8_t)(((val_uv - BATT_MIN_PIN_UV) * 100) / (BATT_MAX_PIN_UV - BATT_MIN_PIN_UV));
    }

    LOG_INF("Pin voltage: %d uV, Level: %d%%", val_uv, level);

    err = bt_bas_set_battery_level(level);
    if (err) {
        LOG_ERR("Failed to update BAS (err %d)", err);
    }
}

static void battery_timer_handler(struct k_timer* timer)
{
    k_work_submit(&battery_work);
}

int app_battery_init(void)
{
    int err;
    if (!adc_is_ready_dt(&batt_adc)) {
        LOG_ERR("ADC controller not ready");
        return -ENODEV;
        ;
    }
    err = adc_channel_setup_dt(&batt_adc);
    if (err < 0) {
        LOG_ERR("Could not setup ADC channel (err %d)", err);
        return err;
    }
    LOG_INF("ADC channel configured successfully");
    return 0;
}

void app_battery_start(void)
{
    /* First trigger after 1 second, then periodically every 60 seconds */
    k_timer_start(&battery_timer, K_SECONDS(1), K_SECONDS(APP_BATTERY_READ_INTERVAL_SEC));
    LOG_INF("Battery monitoring started");
}

void app_battery_stop(void)
{
    k_timer_stop(&battery_timer);
    LOG_INF("Battery monitoring stopped");
}
