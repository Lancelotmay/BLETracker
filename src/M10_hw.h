/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef APP_M10_HW_H
#define APP_M10_HW_H

#include <stddef.h>
#include <stdint.h>

typedef void (*m10_hw_pps_cb_t)(void *user_data);

/**
 * @brief Initialize M10 hardware binding (I2C + reset GPIO).
 */
int M10_hw_init(void);

/**
 * @brief Configure and enable 1PPS GPIO interrupt.
 *
 * Callback is invoked from interrupt context.
 */
int M10_hw_pps_enable(m10_hw_pps_cb_t cb, void *user_data);

/**
 * @brief Read M10 stream bytes via I2C DDC stream register.
 * 
 * @param buf Buffer to read into
 * @param len Number of bytes read
 * @param remains Number of bytes remaining in buffer
 * @return int 0 on success, negative on error
 */
int M10_hw_random_read(uint8_t *buf, uint16_t *len, uint16_t *remains);

/**
 * @brief Read M10 current address via I2C DDC stream register.
 * 
 * @param buf Buffer to read into
 * @param len Number of bytes to read
 * @return int 0 on success, negative on error
 */
int M10_hw_currentaddr_read(uint8_t *buf, uint16_t len);

/**
 * @brief Write M10 stream bytes via I2C DDC stream register.
 */
int M10_hw_write_stream(const uint8_t *buf, uint16_t len);

/**
 * @brief Read number of pending bytes in M10 DDC output buffer.
 */
int M10_hw_bytes_available(uint16_t *available);

/**
 * @brief Assert/deassert reset pin for a short pulse.
 */
int M10_hw_reset_pulse(uint32_t hold_ms);

int M10_led_toggle(void);
int M10_led_set(int8_t value);

#endif /* APP_M10_HW_H */
