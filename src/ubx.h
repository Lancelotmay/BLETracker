/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UBX_H
#define UBX_H
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#define GNSS_STATUS_MORE_DATA    1
#define GNSS_STATUS_OK           0

#define GNSS_ERR_BUFF_EMPTY     -ENODATA 
#define GNSS_ERR_I2C            -EIO     
#define GNSS_ERR_BUFF_FULL      -ENOBUFS 
#define GNSS_ERR_DECODE         -EBADMSG 
#define GNSS_ERR_UNKNOWN_MSG    -ENOTSUP 
#define UBX_MAX_RX_MSG_LENGTH     1024
#define UBX_MAX_TX_MSG_LENGTH     256

struct ubx_cfg_pair_u1 {
	uint32_t key_id;
	uint8_t value;
};

int ubx_build_msg(uint16_t class_msg_id, uint8_t *ubx_msg, uint16_t payload_length);

int ubx_build_cfg_valset_payload(const struct ubx_cfg_pair_u1 *cfg, size_t cfg_count,
				 uint8_t layers, uint8_t *payload, uint16_t *payload_length);

int ubx_verify_msg(uint8_t *ubx_msg, uint16_t buff_length, uint16_t *msg_length);

int ubx_poll_message(uint16_t class_msg_id, uint8_t *ubx_tx_msg, uint8_t *ubx_rx_msg, uint16_t *rx_msg_length, uint16_t read_delay_ms);

int ubx_send_message(uint16_t class_msg_id, const uint8_t *tx_msg, uint16_t tx_msg_length,
		     uint8_t *ubx_rx_msg, uint16_t *rx_msg_length);

int ubx_cfg_valset_u1(const struct ubx_cfg_pair_u1 *cfg, size_t cfg_count, uint8_t layers,
		      uint8_t *ubx_tx_msg, uint8_t *ubx_rx_msg, uint16_t *rx_msg_length);


#endif /* UBX_H */
