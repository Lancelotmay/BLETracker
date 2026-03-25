/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ubx.h"
#include "M10_hw.h"
#include "ubx_messages_header.h"
#include "ubx_messages_header_custom.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
LOG_MODULE_REGISTER(ubx, LOG_LEVEL_INF);

int ubx_verify_msg(uint8_t* ubx_msg, uint16_t buff_length, uint16_t* msg_length)
{
    uint16_t i;
    uint8_t ck_a = 0;
    uint8_t ck_b = 0;
    int err_code = GNSS_ERR_DECODE;

    if ((UBX_SYNC_CHAR_1 == ubx_msg[0]) && (UBX_SYNC_CHAR_2 == ubx_msg[1])) {
        *msg_length = ((uint16_t) ubx_msg[4]) + (((uint16_t)ubx_msg[5]) << 8) + UBX_HEAD_SIZE + UBX_CHKSUM_SIZE;
    } else {
        return -201;  // msg header wrong
    }
    if ((*msg_length >= UBX_MAX_RX_MSG_LENGTH) ||(*msg_length != buff_length)) {
        return -202;  // msg length wrong
    }
    for (i = 2; i < (*msg_length - 2); i++) {
        ck_a = ck_a + ubx_msg[i];
        ck_b = ck_b + ck_a;
    }
    if ((ck_a == ubx_msg[*msg_length - 2]) && (ck_b == ubx_msg[*msg_length - 1])) {
            err_code = GNSS_STATUS_OK;

    } else {
        err_code = GNSS_ERR_DECODE;
    }
    return err_code;
}

/**
 * Build a UBX message
 * @param class_msg_id The class and message ID
 * @param ubx_msg The UBX message buffer, payload should be already written to the buffer
 * @param payload_length The length of the payload
 * @return 0 on success, -1 on error
 */
int ubx_build_msg(uint16_t class_msg_id, uint8_t* ubx_msg, uint16_t payload_length)
{
    uint16_t i;
    uint8_t ck_a = 0;
    uint8_t ck_b = 0;
    ubx_msg[0] = UBX_SYNC_CHAR_1;
    ubx_msg[1] = UBX_SYNC_CHAR_2;
    ubx_msg[2] = (class_msg_id >> 8) & 0xFF;  // class id
    ubx_msg[3] = (class_msg_id) & 0xFF;       // msg id
    ubx_msg[4] = (payload_length) & 0xFF;
    ubx_msg[5] = (payload_length >> 8) & 0xFF;
    /* The checksum is calculated over the message, starting and including the
    class field up until, but excluding, the checksum fields*/
    for (i = 2; i < UBX_HEAD_SIZE + payload_length; i++) {
        ck_a = ck_a + ubx_msg[i];
        ck_b = ck_b + ck_a;
    }
    ubx_msg[UBX_HEAD_SIZE + payload_length] = ck_a;
    ubx_msg[UBX_HEAD_SIZE + payload_length + 1] = ck_b;
    return 0;
}

/**
 * Build a CFG-VALSET message payload
 * @param payload The payload buffer, should start from tx_buffer[i][0] + UBX_HEAD_SIZE
 * @param payload_length The length of the payload, return value to the caller
 * @return 0 on success, -1 on error
 */
int ubx_build_cfg_valset_payload(const struct ubx_cfg_pair_u1* cfg,
                                 size_t cfg_count,
                                 uint8_t layers,
                                 uint8_t* payload,
                                 uint16_t* payload_length)
{
    size_t i;
    size_t required;

    if ((cfg == NULL) || (payload == NULL) || (payload_length == NULL)) {
        return -EINVAL;
    }

    required = UBX_CFG_VALSET_HEADER_SIZE + (cfg_count * UBX_CFG_VALSET_ITEM_SIZE_U1);
    if (required > UINT16_MAX) {
        return -E2BIG;
    }

    payload[0] = UBX_CFG_VALSET_VERSION;
    payload[1] = layers;
    payload[2] = 0U;
    payload[3] = 0U;

    for (i = 0; i < cfg_count; i++) {
        size_t offset = UBX_CFG_VALSET_HEADER_SIZE + (i * UBX_CFG_VALSET_ITEM_SIZE_U1);

        sys_put_le32(cfg[i].key_id, &payload[offset]);
        payload[offset + sizeof(uint32_t)] = cfg[i].value;
    }

    *payload_length = (uint16_t)required;
    return 0;
}

/* to check whether the message is a correct message, return
- 0 if the message is aligned with ubx message format
- 1 if the message is correct, but the total msg_buffer length is bigger than
- 2 if the msg could not pass chechsum test
*/

int ubx_poll_message(uint16_t class_msg_id,
                     uint8_t* ubx_tx_msg,
                     uint8_t* ubx_rx_msg,
                     uint16_t* rx_msg_length, uint16_t read_delay_ms)
{
    int err = 0;
    uint16_t i = 0;
    uint16_t remains = 0;
    err = ubx_build_msg(class_msg_id, ubx_tx_msg, 0);
    if (err) {
        return err;
    }

    for (i = 0U; i < 100U; i++) {
        M10_hw_random_read(ubx_rx_msg, rx_msg_length, &remains);
        LOG_INF("Read message: %u read, %u remaining", *rx_msg_length, remains);
        if (0 == remains) {
            break;
        }
    }
    err = M10_hw_write_stream(ubx_tx_msg, UBX_FRAME_SIZE);
    if (err) {
        LOG_ERR("Failed to write UBX message: %d", err);
        return err;
    }

    k_sleep(K_MSEC(read_delay_ms));

    for (i = 0U; i < 40U; i++) {
        err = M10_hw_random_read(ubx_rx_msg, rx_msg_length, &remains);
        if ((0 == err)){
            LOG_INF("%u Pull msg: %u read, %d remaining", i, *rx_msg_length, remains);
            if (*rx_msg_length > 0U) {
                if (remains > 0) {
                    LOG_ERR("UBX message too long: %u", remains);
                    return -EMSGSIZE;
                }
                return err;
            }
        }
        k_sleep(K_MSEC(20U));
    }
    return -1;
}

int ubx_send_message(uint16_t class_msg_id,
                     const uint8_t* ubx_tx_msg,
                     uint16_t ubx_tx_length,
                     uint8_t* ubx_rx_msg,
                     uint16_t* rx_length)
{
    int err;
    uint16_t remains = 0;
    uint16_t rx_msg_length = 0;
    uint16_t acked_msg;

    if ((ubx_tx_msg == NULL) || (ubx_rx_msg == NULL) || (rx_length == NULL)) {
        return -EINVAL;
    }

    for (uint16_t i = 0U; i < 100U; i++) {
        err = M10_hw_random_read(ubx_rx_msg, rx_length, &remains);
        if (err != 0) {
            return err;
        }
        if (remains == 0U) {
            break;
        }
    }

    err = M10_hw_write_stream(ubx_tx_msg, ubx_tx_length);
    if (err != 0) {
        LOG_ERR("Failed to write UBX message: %d", err);
        return err;
    }

    k_sleep(K_MSEC(10));

    err = M10_hw_random_read(ubx_rx_msg, rx_length, &remains);
    if (err != 0) {
        LOG_ERR("Failed to read UBX response: %d", err);
        return err;
    }
    if (remains > 0U) {
        LOG_ERR("UBX response too long: %u", remains);
        return -EMSGSIZE;
    }

    err = ubx_verify_msg(ubx_rx_msg, *rx_length, &rx_msg_length);
    if (err != GNSS_STATUS_OK) {
        LOG_ERR("UBX response decode failed: %d", err);
        return err;
    }

    if ((ubx_rx_msg[2] != (UBXID_ACK_ACK >> 8)) || (ubx_rx_msg[3] != (UBXID_ACK_ACK & 0xFF))) {
        LOG_ERR("Unexpected UBX response class=0x%02x id=0x%02x", ubx_rx_msg[2], ubx_rx_msg[3]);
        return -EBADMSG;
    }

    if (rx_msg_length < (UBX_HEAD_SIZE + sizeof(UBX_ACK_ACK_DATA0_t) + UBX_CHKSUM_SIZE)) {
        return -EMSGSIZE;
    }

    acked_msg = ((uint16_t)ubx_rx_msg[UBX_HEAD_SIZE] << 8) | ubx_rx_msg[UBX_HEAD_SIZE + 1];
    if (acked_msg != class_msg_id) {
        LOG_ERR("ACK mismatch: expected 0x%04x got 0x%04x", class_msg_id, acked_msg);
        return -EBADMSG;
    }

    return 0;
}

int ubx_cfg_valset_u1(const struct ubx_cfg_pair_u1* cfg,
                        size_t cfg_count,
                        uint8_t layers,
                        uint8_t* ubx_tx_msg,
                        uint8_t* ubx_rx_msg,
                        uint16_t* rx_msg_length)
{
    int err;
    uint16_t payload_length = 0;

    if ((cfg == NULL) || (cfg_count == 0U)) {
        return -EINVAL;
    }

    err = ubx_build_cfg_valset_payload(cfg, cfg_count, layers, &ubx_tx_msg[UBX_HEAD_SIZE],
                                        &payload_length);
    if (err != 0) {
        return err;
    }
    err = ubx_build_msg(UBXID_CFG_VALSET, ubx_tx_msg, payload_length);
    if (err != 0) {
        return err;
    }
    err = ubx_send_message(UBXID_CFG_VALSET, ubx_tx_msg, (UBX_HEAD_SIZE + payload_length + UBX_CHKSUM_SIZE),
                                ubx_rx_msg, rx_msg_length);
    return err;
}
