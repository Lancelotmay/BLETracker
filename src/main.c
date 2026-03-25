/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "M10_hw.h"
#include "app_battery.h"
#include "app_m10.h"
#include "ble_core.h"


LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

enum {
	EVT_1PPS_TICK = BIT(0),
	EVT_BLE_CONFIG_UPDATED = BIT(1),
	EVT_PVT_TIMER = BIT(2),
};


static struct k_event main_events;

static void m10_pps_cb(void *user_data)
{
	struct k_event *ev = user_data;

	if (ev != NULL) {
		k_event_post(ev, EVT_1PPS_TICK);
	}
}

static void pvt_timer_hander(struct k_timer *timer)
{
	k_event_post(&main_events, EVT_PVT_TIMER);
}
static K_TIMER_DEFINE(pvt_timer, pvt_timer_hander, NULL);

int main(void)
{
    int err;
	uint32_t events;

    LOG_INF("BLE Tracker starting via RTT...\n");
    /* Hardware Init */

    err = ble_init();
    if (err) {
        LOG_ERR("BLE init failed, err %d\n", err);
        return err;
    }
    /* Init battery monitoring */
    err = app_battery_init();
    if (err){
        LOG_ERR("BATT Mon ERR %d\n", err);
    } else {
        app_battery_start();
    }

    err = app_m10_init();
    if (err != 0) {
        LOG_ERR("M10 init failed (err %d)", err);
		return err;
    }

	err = app_m10_start();
	if (err != 0) {
		LOG_ERR("M10 start failed (err %d)", err);
		return err;
	}

	k_event_init(&main_events);

	err = M10_hw_pps_enable(m10_pps_cb, &main_events);
	if (err != 0) {
		LOG_ERR("M10 1PPS enable failed (err %d)", err);
		return err;
	}
	k_timer_start(&pvt_timer, K_SECONDS(3), K_SECONDS(1));
	while (1) {
		events = k_event_wait(&main_events,
            EVT_1PPS_TICK | EVT_BLE_CONFIG_UPDATED|EVT_PVT_TIMER,
                    true, K_FOREVER);
		if ((events & EVT_1PPS_TICK) != 0U) {
			M10_led_set(1);
			k_timer_start(&pvt_timer, K_MSEC(200U), K_NO_WAIT);
		}
		if ((events & EVT_PVT_TIMER) != 0U) {
			if (ble_lns_notify_is_enabled()) {
				err = m10_pvt_update();
				if (err != 0) {
					LOG_DBG("PVT update skipped/failed (err %d)", err);
				}	
			}
			M10_led_toggle();
		}

		if ((events & EVT_BLE_CONFIG_UPDATED) != 0U) {
			LOG_DBG("BLE config update event received (TODO)");
		}
	}
}
