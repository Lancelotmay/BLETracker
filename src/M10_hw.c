/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "M10_hw.h"
#include "ubx.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(M10_hw, LOG_LEVEL_INF);

#define M10_I2C_STREAM_REG 0xFF
#define M10_I2C_BYTES_AVAIL_REG 0xFD
#define M10_I2C_MAX_WRITE_LEN 191

static const struct i2c_dt_spec m10_i2c = I2C_DT_SPEC_GET(DT_NODELABEL(gnss));
static const struct gpio_dt_spec m10_reset = GPIO_DT_SPEC_GET(DT_NODELABEL(m10_reset), gpios);
static const struct gpio_dt_spec m10_pps = GPIO_DT_SPEC_GET(DT_NODELABEL(m10_pps), gpios);
static const struct gpio_dt_spec m10_led = GPIO_DT_SPEC_GET(DT_NODELABEL(m10_led), gpios);
static struct gpio_callback m10_pps_cb_data;
static m10_hw_pps_cb_t m10_pps_cb;
static void *m10_pps_cb_user_data;
static bool m10_pps_enabled;

static void m10_pps_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	if (m10_pps_cb != NULL) {
		m10_pps_cb(m10_pps_cb_user_data);
	}
}

int M10_hw_init(void)
{
	int err;

	if (!i2c_is_ready_dt(&m10_i2c)) {
		LOG_ERR("M10 I2C bus is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&m10_reset)) {
		LOG_ERR("M10 reset GPIO is not ready");
		return -ENODEV;
	}
	if (!gpio_is_ready_dt(&m10_pps)) {
		LOG_ERR("M10 1PPS GPIO is not ready");
		return -ENODEV;
	}
	if (!gpio_is_ready_dt(&m10_led)) {
		LOG_ERR("M10 LED GPIO is not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&m10_reset, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("Failed to configure M10 reset pin (err %d)", err);
		return err;
	}
	err = gpio_pin_configure_dt(&m10_pps, GPIO_INPUT);
	if (err != 0) {
		LOG_ERR("Failed to configure M10 1PPS pin (err %d)", err);
		return err;
	}
	err = gpio_pin_configure_dt(&m10_led, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("Failed to configure M10 LED pin (err %d)", err);
		return err;
	}

	return 0;
}

int M10_hw_random_read(uint8_t *buf, uint16_t *len, uint16_t *remains)
{
	uint8_t data_avail_reg = M10_I2C_BYTES_AVAIL_REG;
	uint8_t data[2];
	int err;

	*remains = 0U;
	*len = 0U;
	if ((buf == NULL)) {
		return -EINVAL;
	}
	err = i2c_write_read_dt(&m10_i2c, &data_avail_reg, sizeof(data_avail_reg), data, sizeof(data));
	if (err != 0) {
		LOG_ERR("Failed to read M10 available byte count (err %d)", err);
		return err;
	}
	
	*len = ((uint16_t)data[0] << 8) | data[1];
	if (*len == 0U) {
		return 0U;
	}else if (*len > UBX_MAX_RX_MSG_LENGTH) {
		*remains = *len - UBX_MAX_RX_MSG_LENGTH;
		*len = UBX_MAX_RX_MSG_LENGTH;
	}
	return i2c_read_dt(&m10_i2c, buf, *len);
}

int M10_hw_write_stream(const uint8_t *buf, uint16_t len)
{
	uint8_t tx_buf[M10_I2C_MAX_WRITE_LEN + 1U];

	if ((buf == NULL) || (len == 0U)) {
		return -EINVAL;
	}

	if (len > M10_I2C_MAX_WRITE_LEN) {
		return -EMSGSIZE;
	}

	tx_buf[0] = M10_I2C_STREAM_REG;
	memcpy(&tx_buf[1], buf, len);
	return i2c_write_dt(&m10_i2c, tx_buf, len + 1U);
}

int M10_hw_reset_pulse(uint32_t hold_ms)
{
	int err;

	err = gpio_pin_set_dt(&m10_reset, 1);
	if (err != 0) {
		return err;
	}

	k_sleep(K_MSEC(hold_ms));
	return gpio_pin_set_dt(&m10_reset, 0);
}

int M10_hw_pps_enable(m10_hw_pps_cb_t cb, void *user_data)
{
	int err;

	if (cb == NULL) {
		return -EINVAL;
	}

	if (!gpio_is_ready_dt(&m10_pps)) {
		LOG_ERR("M10 1PPS GPIO is not ready");
		return -ENODEV;
	}

	m10_pps_cb = cb;
	m10_pps_cb_user_data = user_data;

	if (m10_pps_enabled) {
		return 0;
	}

	err = gpio_pin_configure_dt(&m10_pps, GPIO_INPUT);
	if (err != 0) {
		LOG_ERR("Failed to configure M10 1PPS pin (err %d)", err);
		return err;
	}

	gpio_init_callback(&m10_pps_cb_data, m10_pps_isr, BIT(m10_pps.pin));
	err = gpio_add_callback(m10_pps.port, &m10_pps_cb_data);
	if (err != 0) {
		LOG_ERR("Failed to add M10 1PPS callback (err %d)", err);
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&m10_pps, GPIO_INT_EDGE_TO_ACTIVE);
	if (err != 0) {
		(void)gpio_remove_callback(m10_pps.port, &m10_pps_cb_data);
		LOG_ERR("Failed to enable M10 1PPS interrupt (err %d)", err);
		return err;
	}

	m10_pps_enabled = true;
	return 0;
}

int M10_led_toggle(void)
{
	return gpio_pin_toggle_dt(&m10_led);
}

int M10_led_set(int8_t value)
{
	return gpio_pin_set_dt(&m10_led, value);
}
