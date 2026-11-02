#ifndef IDENTITY_TEST_ZBOSS_API_ADDONS_H
#define IDENTITY_TEST_ZBOSS_API_ADDONS_H
#include <stdint.h>
#include <string.h>

#define _PRST_ZB_ATTRS_H_
#define _PRST_ZB_H_
typedef uint8_t zb_uint8_t;
typedef uint16_t zb_uint16_t;
typedef uint8_t zb_zcl_status_t;
struct zb_device_ctx {
  struct {
    uint8_t zcl_version, power_source, hw_version;
    uint8_t mf_name[33], model_id[33], date_code[17], sw_ver[17];
  } basic_attr;
  struct {
    uint8_t quantity, size;
  } batt_attrs;
  struct {
    uint16_t identify_time;
  } identify_attr;
};
#define ZB_ZCL_VERSION 3
#define ZB_ZCL_BASIC_POWER_SOURCE_BATTERY 3
#define ZB_ZCL_POWER_CONFIG_BATTERY_SIZE_OTHER 0
#define ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE 0
#define ZB_ZCL_CLUSTER_SERVER_ROLE 1
#define ZB_FALSE 0
#define PRST_ZIGBEE_ENDPOINT 10
#define PRST_BASIC_MANUF_NAME "b-parasite"
#define CONFIG_PRST_ZB_MODEL_ID "b-parasite"
#define CONFIG_PRST_ZB_BUILD_DATE ""
#define CONFIG_PRST_ZB_HARDWARE_VERSION 4
#define ZB_ZCL_STRING_CONST_SIZE(value) (sizeof(value) - 1)
#define ZB_ZCL_SET_STRING_VAL(destination, value, size) \
  (memcpy((destination) + 1, (value), (size)), (destination)[0] = (size))

void prst_zb_attrs_init(struct zb_device_ctx* dev_ctx);
zb_zcl_status_t zb_zcl_set_attr_val(uint8_t endpoint, uint16_t cluster,
                                    uint8_t role, uint16_t attribute,
                                    uint8_t* data, uint8_t check_access);
#endif
