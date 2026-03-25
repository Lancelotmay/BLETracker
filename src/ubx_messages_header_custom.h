/*
 * Copyright (C) 2026 Lancelot MEI
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef __UBX_MESSAGES_HEADER_CUSTOM__
#define __UBX_MESSAGES_HEADER_CUSTOM__

#include "ubx_messages_header.h"

#define UBXID_ESF_RAW 0x1003
#define UBX_ESF_RAW_DATATYPE_Z_GYRO                 ( 5 << 24)
#define UBX_ESF_RAW_DATATYPE_REAR_LEFT_WHEELTICK    ( 8 << 24)
#define UBX_ESF_RAW_DATATYPE_REAR_RIGHT_WHEELTICK   ( 9 << 24)
#define UBX_ESF_RAW_DATATYPE_SINGLE_TICK            (10 << 24)
#define UBX_ESF_RAW_DATATYPE_SPEED                  (11 << 24)
#define UBX_ESF_RAW_DATATYPE_GYRO_TEMP              (12 << 24)
#define UBX_ESF_RAW_DATATYPE_Y_GYRO                 (13 << 24)
#define UBX_ESF_RAW_DATATYPE_X_GYRO                 (14 << 24)
#define UBX_ESF_RAW_DATATYPE_X_ACCL                 (16 << 24)
#define UBX_ESF_RAW_DATATYPE_Y_ACCL                 (17 << 24)
#define UBX_ESF_RAW_DATATYPE_Z_ACCL                 (18 << 24)

typedef struct UBX_ESF_RAW_s
{
    UBX_HEAD_t ubx_header;
    U4 timeTag;
    X2 flags;
    U2 id;
    X4 data[7];
//    U4 calibTtag;
    U1 chk_a;
    U1 chk_b;
} UBX_ESF_RAW_t ;

#define UBX_ESF_RAW_SIZE sizeof(UBX_ESF_RAW_t)

/*----------------------------------------------------------------
 from ubxlib
*/
/* changed for M10 protocol */ 



#define UBXID_MON_VER 0x0A04
typedef struct UBX_MON_VER_s{
    char sw[30];   //!< Sofware Version/ROM version for M10
    char hw[10];    //!< Hardware / Chip Version
    char fw[30];   //!< Underlying ROM Version
    char protocol[30];    //!< Protocol Version
    char mod[30];  //!< module name
    char gnss[30];   //!< GNSS supported
    char other[30]; //!< other features
} UBX_MON_VER_t; //!< return structs with different information
#define UBX_MON_VER_SIZE sizeof(UBX_MON_VER_t)



#define UBXID_CFG_VALSET 0x068Au
#define UBX_CFG_VALSET_VERSION 0x00u
#define UBX_CFG_LAYER_RAM 0x01u
#define UBX_CFG_VALSET_HEADER_SIZE 4u
#define UBX_CFG_VALSET_ITEM_SIZE_U1 5u

/** Structure to hold a value to get or set using uGnssCfgValGetListAlloc() /
 * uGnssCfgValSetList() / uGnssCfgValDelListX().
 */
typedef struct {
    U4 keyId;   /**< the ID of the key to get/set/del; may be found in
                           the u-blox GNSS reference manual or you may use
                           the macros defined in u_gnss_cfg_val_key.h;
                           for instance, key ID CFG-ANA-USE_ANA would be
                           #U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L (prefix
                           with U_GNSS_CFG_VAL_KEY_ID_, drop the CFG, replace
                           any dashes with underscores and add the type on
                           the end (just so it sticks in your mind)). */
    U8 value;   /**< the value, of size defined by the keyId. */
} uGnssCfgVal_t;


#endif // __UBX_MESSAGES_HEADER_CUSTOM__
