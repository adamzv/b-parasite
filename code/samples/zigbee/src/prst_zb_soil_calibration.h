#ifndef _PRST_ZB_SOIL_CALIBRATION_H_
#define _PRST_ZB_SOIL_CALIBRATION_H_

#include <stdbool.h>
#include <zboss_api.h>
#include <zcl/zb_zcl_common.h>

// Private cluster that rescales the factory air (0%) to water (100%) reading onto the user's own
// "time to water" and "freshly watered" points. All three attributes are in units of 0.01% of
// that factory scale, so soil_raw = 3000 means the factory formula reads 30.00%.
#define PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION 0xFC00

#define PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_RAW_ID 0x0000
#define PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID 0x0001
#define PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID 0x0002

#define PRST_ZB_ZCL_ATTR_SOIL_VALID_ID 0x0010
#define PRST_ZB_ZCL_ATTR_SENSOR_ERRORS_ID 0x0011
#define PRST_ZB_ZCL_ATTR_BATTERY_MV_ID 0x0012
#define PRST_ZB_ZCL_ATTR_BOOT_COUNT_ID 0x0013
#define PRST_ZB_ZCL_ATTR_RESET_CAUSE_ID 0x0014
#define PRST_ZB_ZCL_ATTR_UPTIME_SEC_ID 0x0015

#define PRST_ZB_SOIL_CALIBRATION_DEFAULT_DRY 0
#define PRST_ZB_SOIL_CALIBRATION_DEFAULT_WET 10000

void prst_zcl_soil_calibration_init_server(void);
void prst_zcl_soil_calibration_init_client(void);

#define PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION_SERVER_ROLE_INIT prst_zcl_soil_calibration_init_server
#define PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION_CLIENT_ROLE_INIT prst_zcl_soil_calibration_init_client

#define PRST_ZB_ZCL_SOIL_CALIBRATION_CLUSTER_REVISION_DEFAULT ((zb_uint16_t)1)

#define ZB_SET_ATTR_DESCR_WITH_PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_RAW_ID(data_ptr) \
  {PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_RAW_ID, ZB_ZCL_ATTR_TYPE_S16,                \
   ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_REPORTING,                  \
   ZB_ZCL_NON_MANUFACTURER_SPECIFIC, (void*)data_ptr}

#define ZB_SET_ATTR_DESCR_WITH_PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID(data_ptr) \
  {PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID, ZB_ZCL_ATTR_TYPE_S16,                \
   ZB_ZCL_ATTR_ACCESS_READ_WRITE | ZB_ZCL_ATTR_ACCESS_REPORTING,                  \
   ZB_ZCL_NON_MANUFACTURER_SPECIFIC, (void*)data_ptr}

#define ZB_SET_ATTR_DESCR_WITH_PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID(data_ptr) \
  {PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID, ZB_ZCL_ATTR_TYPE_S16,                \
   ZB_ZCL_ATTR_ACCESS_READ_WRITE | ZB_ZCL_ATTR_ACCESS_REPORTING,                  \
   ZB_ZCL_NON_MANUFACTURER_SPECIFIC, (void*)data_ptr}

#define PRST_ZB_DIAGNOSTIC_ATTR(id, ptr, type) \
  ZB_ZCL_SET_ATTR_DESC_M(id, ptr, type, ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_REPORTING)
#define PRST_ZB_ZCL_DECLARE_SOIL_CALIBRATION_ATTRIB_LIST(attr_list, attrs) \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, PRST_ZB_ZCL_SOIL_CALIBRATION) \
  ZB_ZCL_SET_ATTR_DESC(PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_RAW_ID, &((attrs)->raw)) \
  ZB_ZCL_SET_ATTR_DESC(PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID, &((attrs)->dry)) \
  ZB_ZCL_SET_ATTR_DESC(PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID, &((attrs)->wet)) \
  PRST_ZB_DIAGNOSTIC_ATTR(PRST_ZB_ZCL_ATTR_SOIL_VALID_ID, &((attrs)->soil_valid), ZB_ZCL_ATTR_TYPE_BOOL) \
  PRST_ZB_DIAGNOSTIC_ATTR(PRST_ZB_ZCL_ATTR_SENSOR_ERRORS_ID, &((attrs)->sensor_errors), ZB_ZCL_ATTR_TYPE_U8) \
  PRST_ZB_DIAGNOSTIC_ATTR(PRST_ZB_ZCL_ATTR_BATTERY_MV_ID, &((attrs)->battery_mv), ZB_ZCL_ATTR_TYPE_U16) \
  PRST_ZB_DIAGNOSTIC_ATTR(PRST_ZB_ZCL_ATTR_BOOT_COUNT_ID, &((attrs)->boot_count), ZB_ZCL_ATTR_TYPE_U32) \
  PRST_ZB_DIAGNOSTIC_ATTR(PRST_ZB_ZCL_ATTR_RESET_CAUSE_ID, &((attrs)->reset_cause), ZB_ZCL_ATTR_TYPE_U32) \
  PRST_ZB_DIAGNOSTIC_ATTR(PRST_ZB_ZCL_ATTR_UPTIME_SEC_ID, &((attrs)->uptime_sec), ZB_ZCL_ATTR_TYPE_U32) \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

typedef struct {
  zb_int16_t raw;
  zb_int16_t dry;
  zb_int16_t wet;
  zb_bool_t soil_valid;
  zb_uint8_t sensor_errors;
  zb_uint16_t battery_mv;
  zb_uint32_t boot_count;
  zb_uint32_t reset_cause;
  zb_uint32_t uptime_sec;
} prst_soil_calibration_attrs_t;

// Fills dry/wet from flash, falling back to the factory scale.
void prst_soil_calibration_load(prst_soil_calibration_attrs_t* attrs);

// Persists dry/wet off the Zigbee thread; flash writes are too slow for a ZCL callback.
void prst_soil_calibration_save_async(const prst_soil_calibration_attrs_t* attrs);

bool prst_soil_calibration_is_valid(zb_int16_t dry, zb_int16_t wet);

// Maps the factory-scale reading (0..1 = air..water, unclamped) onto [0, 1] between dry and wet.
float prst_soil_calibration_apply(const prst_soil_calibration_attrs_t* attrs, float factory_fraction);

#endif  // _PRST_ZB_SOIL_CALIBRATION_H_
