/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef APP_M10_H
#define APP_M10_H

#include <stdbool.h>
#include <stdint.h>

struct app_m10_fix {
	int32_t latitude_1e7;
	int32_t longitude_1e7;
	int32_t heading_1e5_deg;
	int32_t ground_speed_mm_s;
	uint32_t horiz_acc_mm;
	uint32_t i_tow_ms;
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t fix_type;
	uint8_t fix_flags;
	uint8_t num_sv;
};

/**
 * @brief Initialize M10 GNSS.
 *
 * This project supports UBX over I2C only.
 */
int app_m10_init(void);


/**
 * @brief Start GNSS engine of M10.
 */
int app_m10_start(void);

/**
 * @brief Poll one NAV-PVT sample and send it over LNS notification.
 */
int m10_pvt_update(void);

/**
 * @brief Stop GNSS engine of M10.
 */
void app_m10_stop(void);

/**
 * @brief Check if a GNSS fix is usable.
 */
bool app_m10_fix_valid(const struct app_m10_fix *fix);

/**
 * @brief Read the latest decoded fix.
 *
 * @retval 0 on success.
 * @retval -ENODATA if no fix has been decoded yet.
 */
int app_m10_get_last_fix(struct app_m10_fix *fix);

#endif /* APP_M10_H */
