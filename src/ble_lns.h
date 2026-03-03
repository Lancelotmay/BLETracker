/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_LNS_H_
#define BLE_LNS_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Location and Navigation Feature characteristic (0x2A6A) supported bits. */
#define BLE_LNS_FEAT_INST_SPEED	BIT(0)
#define BLE_LNS_FEAT_TOTAL_DISTANCE	BIT(1)
#define BLE_LNS_FEAT_LOCATION	BIT(2)
#define BLE_LNS_FEAT_ELEVATION	BIT(3)
#define BLE_LNS_FEAT_HEADING	BIT(4)
#define BLE_LNS_FEAT_ROLLING_TIME	BIT(5)
#define BLE_LNS_FEAT_UTC_TIME	BIT(6)
#define BLE_LNS_FEAT_REMAINING_DISTANCE	BIT(7)
#define BLE_LNS_FEAT_REMAINING_VERT_DISTANCE	BIT(8)
#define BLE_LNS_FEAT_EST_TIME_OF_ARRIVAL	BIT(9)
#define BLE_LNS_FEAT_NUM_BEACONS_SOLUTION	BIT(10)
#define BLE_LNS_FEAT_NUM_BEACONS_VIEW	BIT(11)
#define BLE_LNS_FEAT_TIME_TO_FIRST_FIX	BIT(12)
#define BLE_LNS_FEAT_EST_HOR_POS_ERR	BIT(13)
#define BLE_LNS_FEAT_EST_VERT_POS_ERR	BIT(14)
#define BLE_LNS_FEAT_HOR_DILUTION_PRECISION	BIT(15)
#define BLE_LNS_FEAT_VERT_DILUTION_PRECISION	BIT(16)
#define BLE_LNS_FEAT_LOC_SPEED_CONTENT_MASKING	BIT(17)
#define BLE_LNS_FEAT_FIX_RATE_SETTING	BIT(18)
#define BLE_LNS_FEAT_ELEVATION_SETTING	BIT(19)
#define BLE_LNS_FEAT_POSITION_STATUS	BIT(20)

/** @brief Position Status values encoded in Location and Speed Flags bits 7..8. */
enum ble_lns_position_status {
	BLE_LNS_POS_NO_POSITION = 0,
	BLE_LNS_POS_POSITION_OK = 1,
	BLE_LNS_POS_ESTIMATED = 2,
	BLE_LNS_POS_LAST_KNOWN = 3,
};



/** @brief Location and Speed Flags field bit definitions. */
enum ble_lns_loc_speed_flags {
	BLE_LNS_LOC_SPEED_INST_SPEED_PRESENT = BIT(0),
	BLE_LNS_LOC_SPEED_TOTAL_DISTANCE_PRESENT = BIT(1),
	BLE_LNS_LOC_SPEED_LOCATION_PRESENT = BIT(2),
	BLE_LNS_LOC_SPEED_ELEVATION_PRESENT = BIT(3),
	BLE_LNS_LOC_SPEED_HEADING_PRESENT = BIT(4),
	BLE_LNS_LOC_SPEED_ROLLING_TIME_PRESENT = BIT(5),
	BLE_LNS_LOC_SPEED_UTC_TIME_PRESENT = BIT(6),
	BLE_LNS_LOC_SPEED_SPEED_DISTANCE_3D = BIT(9),
	BLE_LNS_LOC_SPEED_HEADING_SOURCE = BIT(12),
};

#define BLE_LNS_LOC_SPEED_POS_STATUS_SHIFT	7U
#define BLE_LNS_LOC_SPEED_POS_STATUS_MASK	GENMASK(8, 7)
#define BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_SHIFT	10U
#define BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_MASK	GENMASK(11, 10)
#define BLE_LNS_LOC_SPEED_POS_STATUS_ENCODE(v) \
	((((uint16_t)(v)) << BLE_LNS_LOC_SPEED_POS_STATUS_SHIFT) & BLE_LNS_LOC_SPEED_POS_STATUS_MASK)
#define BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_ENCODE(v) \
	((((uint16_t)(v)) << BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_SHIFT) & \
	 BLE_LNS_LOC_SPEED_ELEVATION_SOURCE_MASK)

/** @brief Elevation source values encoded in Location and Speed Flags bits 10..11. */
enum ble_lns_elevation_source {
	BLE_LNS_ELEVATION_POSITIONING_SYSTEM = 0,
	BLE_LNS_ELEVATION_BAROMETRIC_AIR_PRESSURE = 1,
	BLE_LNS_ELEVATION_DATABASE_SERVICE = 2,
	BLE_LNS_ELEVATION_OTHER_SOURCE = 3,
};

/** @brief UTC time structure used by Location and Speed characteristic. */
struct ble_lns_utc_time {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
};

/** @brief Heading source values for Location and Speed flag bit 12. */
enum ble_lns_heading_source {
	BLE_LNS_HEADING_MOVEMENT = 0,
	BLE_LNS_HEADING_MAGNETIC_COMPASS = 1,
};

/** @brief Location and Speed sample payload.
 *
 * Application provides @p flags and all field values. The stack encodes fields
 * in Bluetooth SIG-defined order according to @p flags.
 */
struct ble_lns_loc_speed {
	/** Bit field of @ref ble_lns_loc_speed_flags and encoded multi-bit fields. */
	uint16_t flags;

	/** Instantaneous speed field (when bit 0 is set). */
	uint16_t inst_speed;
	/** Total distance field, lower 24 bits are encoded (when bit 1 is set). */
	uint32_t total_distance;
	/** Latitude field (when bit 2 is set). */
	int32_t latitude;
	/** Longitude field (when bit 2 is set). */
	int32_t longitude;
	/** Elevation field, lower 24 bits are encoded (when bit 3 is set). */
	int32_t elevation;
	/** Heading field (when bit 4 is set). */
	uint16_t heading;
	/** Rolling time field (when bit 5 is set). */
	uint8_t rolling_time;
	/** UTC time field (when bit 6 is set). */
	struct ble_lns_utc_time utc_time;
};

/** @brief LNS event callbacks. */
struct ble_lns_cb {
	void (*notify_changed)(bool enabled);
};

/**
 * @brief Initialize local LNS service state.
 *
 * @param feature_bits Enabled feature bits for LN Feature characteristic.
 * @param cb           Callback structure or NULL.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p feature_bits contains unsupported bits.
 */
int ble_lns_init(uint32_t feature_bits, const struct ble_lns_cb *cb);


/**
 * @brief Try to notify a Location and Speed sample on a specific connection.
 *
 * This call attempts immediate notification.
 *
 * @param conn Connected LE connection.
 * @param sample Sample data to encode.
 *
 * @retval 0 on success.
 * @retval -EMSGSIZE if encoded payload does not fit current ATT MTU.
 */
int ble_lns_notify_location_speed(struct bt_conn *conn, const struct ble_lns_loc_speed *sample);


#ifdef __cplusplus
}
#endif

#endif /* BLE_LNS_H_ */
