/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "app_m10.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "ubx_messages_header.h"
#include "ubx_messages_header_custom.h"

#include <zephyr/bluetooth/conn.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "M10_hw.h"
#include "ble_core.h"
#include "ble_lns.h"
#include "ubx.h"

LOG_MODULE_REGISTER(app_m10, LOG_LEVEL_INF);

#define APP_M10_RX_BUFFER_SIZE 4
#define APP_M10_TX_BUFFER_SIZE 4
#define UBX_CFG_LAYER_RAM 0x01u

/* M10 configuration database keys for message output rate on I2C (U1). */
#define CFG_MSGOUT_UBX_NAV_PVT_I2C_U1 0X20910006UL
#define CFG_MSGOUT_NMEA_ID_DTM_I2C_U1 0X209100A6UL
#define CFG_MSGOUT_NMEA_ID_GGA_I2C_U1 0X209100BAUL
#define CFG_MSGOUT_NMEA_ID_GLL_I2C_U1 0X209100C9UL
#define CFG_MSGOUT_NMEA_ID_GNS_I2C_U1 0X209100B5UL
#define CFG_MSGOUT_NMEA_ID_GRS_I2C_U1 0X209100CEUL
#define CFG_MSGOUT_NMEA_ID_GSA_I2C_U1 0X209100BFUL
#define CFG_MSGOUT_NMEA_ID_GST_I2C_U1 0X209100D3UL
#define CFG_MSGOUT_NMEA_ID_GSV_I2C_U1 0X209100C4UL
#define CFG_MSGOUT_NMEA_ID_RMC_I2C_U1 0X209100ABUL
#define CFG_MSGOUT_NMEA_ID_VLW_I2C_U1 0X209100E7UL
#define CFG_MSGOUT_NMEA_ID_VTG_I2C_U1 0X209100B0UL
#define CFG_MSGOUT_NMEA_ID_ZDA_I2C_U1 0X209100D8UL

/* buffer for tx and rx*/
static uint8_t rx_buffer[UBX_MAX_RX_MSG_LENGTH];
static uint8_t tx_buffer[UBX_MAX_TX_MSG_LENGTH];
static bool started;

static int app_m10_configure_valset(void);

static inline int32_t mod(int32_t a, int32_t b) {
    int32_t r = a % b;
    return r < 0 ? r + b : r;
}

int m10_pvt_update(void)
{
    int err;
    uint16_t rx_len = 0;
    uint16_t rx_msg_len = 0;
    struct ble_lns_loc_speed fix;
    struct bt_conn* conn;
    UBX_NAV_PVT_DATA1_t* pvt;
    bool fix_ok;
    bool time_ok;
    int32_t gspeed_mm_s;
    uint16_t expected_len = UBX_HEAD_SIZE + sizeof(UBX_NAV_PVT_DATA1_t) + UBX_CHKSUM_SIZE;

    err = ubx_poll_message(UBXID_NAV_PVT, tx_buffer, rx_buffer, &rx_len, 80U);
    if (err != 0) {
        LOG_ERR("PVT query failed (err %d)", err);
        return err;
    }
    if (rx_len < expected_len) {
        LOG_ERR("PVT msg too short (%u bytes)", rx_len);
        return -EMSGSIZE;
    } else {
        err = ubx_verify_msg(rx_buffer, rx_len, &rx_msg_len);
        if (err != 0) {
            LOG_ERR("PVT decode failed (err %d)", err);
            return err;
        }
    }

    pvt = (UBX_NAV_PVT_DATA1_t*)&(rx_buffer[UBX_HEAD_SIZE]);

    fix_ok = (UBX_NAV_PVT_DATA1_FLAGS_GNSSFIXOK_GET(pvt->flags) != 0U);
    time_ok = (UBX_NAV_PVT_DATA1_VALID_VALIDDATE_GET(pvt->valid) != 0U) &&
                (UBX_NAV_PVT_DATA1_VALID_VALIDTIME_GET(pvt->valid) != 0U);

    memset(&fix, 0, sizeof(fix));
    fix.flags = 0U;

    if (fix_ok) {
        fix.flags = (BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT | BLE_LNS_LOC_SPEED_LOCATION_PRESENT |
                        BLE_LNS_LOC_SPEED_HEADING_PRESENT |
                        BLE_LNS_LOC_SPEED_POS_STATUS_ENCODE(BLE_LNS_POS_POSITION_OK));

		/* covert from int32 to uint16, ignore negitive speed*/
		gspeed_mm_s = pvt->gSpeed;
		if (gspeed_mm_s < 0) {
			gspeed_mm_s = 0;
		}
		fix.inst_speed = (uint16_t)MIN((uint32_t)(gspeed_mm_s / 10), (uint32_t)UINT16_MAX);

		fix.latitude = pvt->lat;
		fix.longitude = pvt->lon;
		fix.heading = (uint16_t) (mod((int32_t)(pvt->headMot / 1000), 36000));

		if (time_ok) {
			fix.flags |= BLE_LNS_LOC_SPEED_UTC_TIME_PRESENT;
			fix.utc_time.year = pvt->year;
			fix.utc_time.month = pvt->month;
			fix.utc_time.day = pvt->day;
			fix.utc_time.hours = pvt->hour;
			fix.utc_time.minutes = pvt->min;
			fix.utc_time.seconds = pvt->sec;
		}

		conn = ble_get_default_conn();
		if (conn == NULL) {
			LOG_DBG("No BLE connection; skipping LNS notify");
			return -ENOTCONN;
		}

		err = ble_lns_notify_location_speed(conn, &fix);
		if (err != 0) {
			LOG_WRN("LNS notify failed (err %d)", err);
		}
		return 0;
	} else {
		LOG_INF("No fix");
		return -ENODATA;
	}
}

static int app_m10_version_query(void)
{
    int err = -1;
    uint16_t rx_len = 0;
    uint16_t rx_msg_len = 0;
    UBX_MON_VER_t* ver = NULL;

    err = ubx_poll_message(UBXID_MON_VER, tx_buffer, rx_buffer, &rx_len, 10U);
    if ((err != 0U) || (rx_len == 0U)) {
        LOG_ERR("VER query failed (err %d)", err);
        return err;
    }
    err = ubx_verify_msg(rx_buffer, rx_len, &rx_msg_len);
    if (err != 0) {
        LOG_ERR("VER decode failed (err %d)", err);
        return err;
    }
    if (rx_len < (UBX_HEAD_SIZE + UBX_MON_VER_SIZE + UBX_CHKSUM_SIZE)) {
        LOG_ERR("VER msg too short (%u bytes)", rx_len);
        LOG_ERR("Version: %.*s", rx_len - UBX_HEAD_SIZE - UBX_CHKSUM_SIZE,
                (char*)&rx_buffer[UBX_HEAD_SIZE]);
        return -EMSGSIZE;
    }
    ver = (UBX_MON_VER_t*)&(rx_buffer[UBX_HEAD_SIZE]);
    LOG_INF("Version \n SW: %.*s, HW: %.*s", sizeof(ver->sw), ver->sw, sizeof(ver->hw), ver->hw);
    return 0;
}

static int app_m10_configure_valset(void)
{
	static const struct ubx_cfg_pair_u1 msg_cfg[] = {
		{ CFG_MSGOUT_UBX_NAV_PVT_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_DTM_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_GGA_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_GLL_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_GNS_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_GRS_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_GST_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_GSA_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_GSV_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_RMC_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_VLW_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_VTG_I2C_U1, 0U },
		{ CFG_MSGOUT_NMEA_ID_ZDA_I2C_U1, 0U },
	};
	uint16_t rx_len = 0;
	int err;

	err = ubx_cfg_valset_u1(msg_cfg, ARRAY_SIZE(msg_cfg), UBX_CFG_LAYER_RAM,
				tx_buffer, rx_buffer, &rx_len);
	if (err != 0) {
		LOG_ERR("CFG-VALSET disable default I2C outputs failed (err %d)", err);
		return err;
	}

	LOG_INF("Disabled default I2C UBX/NMEA outputs; polling only");
	return 0;
}

int app_m10_init(void)
{
    int err;

    err = M10_hw_init();
    if (err != 0) {
        return err;
    }

    LOG_INF("M10 app initialized (I2C + UBX only)");
    return 0;
}

int app_m10_start(void)
{
    int err;

    if (started) {
        return 0;
    }

    err = M10_hw_reset_pulse(5U);
    if (err != 0) {
        LOG_WRN("M10 reset pulse failed (err %d)", err);
    }
    k_sleep(K_MSEC(1000U));

    err = app_m10_version_query();
    if (err != 0) {
        LOG_ERR("M10 version query failed (err %d)", err);
        return err;
    }
    k_sleep(K_MSEC(100U));

    err = app_m10_configure_valset();
    if (err != 0) {
        LOG_WRN("M10 UBX config failed (err %d)", err);
    }

    started = true;
    LOG_INF("M10 started");
    return 0;
}
