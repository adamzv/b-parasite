#pragma once
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _PRST_ZB_ATTRS_H_
#define _PRST_ZB_H_
#define _PRST_ZB_SOIL_MOISTURE_DEFS_
#define _PRST_ZB_SOIL_CALIBRATION_H_
typedef uint8_t zb_uint8_t;
typedef int16_t zb_int16_t;
typedef uint16_t zb_uint16_t;
typedef uint32_t zb_uint32_t;
typedef uint8_t zb_bool_t;
typedef int zb_ret_t;
typedef int zb_zcl_status_t;
typedef int zb_bufid_t;
typedef void (*zb_callback_t)(zb_uint8_t);
typedef struct {
  int16_t raw, dry, wet;
  zb_bool_t soil_valid;
  uint8_t sensor_errors;
  uint16_t battery_mv;
  uint32_t boot_count, reset_cause, uptime_sec;
} prst_soil_calibration_attrs_t;
struct zb_device_ctx {
  prst_soil_calibration_attrs_t soil_calibration_attrs;
};
#define PRST_ZIGBEE_ENDPOINT 10
#define PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION 0xfc00
#define PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_RAW_ID 0
#define PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID 1
#define PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID 2
#define PRST_ZB_ZCL_ATTR_SOIL_VALID_ID 0x10
#define PRST_ZB_ZCL_ATTR_SENSOR_ERRORS_ID 0x11
#define PRST_ZB_ZCL_ATTR_BATTERY_MV_ID 0x12
#define PRST_ZB_ZCL_ATTR_BOOT_COUNT_ID 0x13
#define PRST_ZB_ZCL_ATTR_RESET_CAUSE_ID 0x14
#define PRST_ZB_ZCL_ATTR_UPTIME_SEC_ID 0x15
#define PRST_ZB_ZCL_ATTR_SOIL_MOISTURE_CLUSTER_ID 1032
#define PRST_ZB_ZCL_ATTR_SOIL_MOISTURE_VALUE_ID 0
#define PRST_ZB_SOIL_CALIBRATION_DEFAULT_DRY 0
#define PRST_ZB_SOIL_CALIBRATION_DEFAULT_WET 10000
#define ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(...)
#define ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(...)
#define ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(...)
#define ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(...)
#define ZB_ZCL_DECLARE_POWER_CONFIG_BATTERY_ATTRIB_LIST_EXT(...)
#define PRST_ZB_ZCL_DECLARE_SOIL_MOISTURE_ATTRIB_LIST(...)
#define ZB_ZCL_DECLARE_ILLUMINANCE_MEASUREMENT_ATTRIB_LIST(...)
#define PRST_ZB_ZCL_DECLARE_SOIL_CALIBRATION_ATTRIB_LIST(...)
#define PRST_ZB_DECLARE_CLUSTER_LIST(...)
#define PRST_ZB_DECLARE_ENDPOINT(...)
#define ZBOSS_DECLARE_DEVICE_CTX_1_EP(...)
#define ZB_AF_REGISTER_DEVICE_CTX(...) ((void)0)
#define ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(...) ((void)0)
#define ZB_ZCL_REGISTER_DEVICE_CB(...) ((void)0)
#define ZB_ZCL_CLUSTER_ID_POWER_CONFIG 1
#define ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID 32
#define ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID 33
#define ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT 1026
#define ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID 0
#define ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_UNKNOWN INT16_MIN
#define ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT 1029
#define ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID 0
#define ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_UNKNOWN UINT16_MAX
#define ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT 1024
#define ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID 0
#define ZB_ZCL_SET_ATTR_VALUE_CB_ID 1
#define ZB_ALARM_ANY_PARAM 255
#define RET_OK 0
#define RET_NOT_FOUND -1
#define ZB_TRUE 1
#define ZB_FALSE 0
#define ZB_MILLISECONDS_TO_BEACON_INTERVAL(ms) (ms)
#define ZB_ERROR_CHECK(x)  \
  do {                     \
    if ((x) != 0) abort(); \
  } while (0)
#define ZB_BDB_SIGNAL_STEERING 1
#define ZB_BDB_SIGNAL_DEVICE_REBOOT 2
#define ZB_ZDO_SIGNAL_LEAVE 3
#define ZB_ZDO_SIGNAL_SKIP_STARTUP 4
#define ZB_NLME_STATUS_INDICATION 5
#define ZB_BDB_SIGNAL_TC_REJOIN_DONE 6
#define ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE 9
typedef int zb_zdo_app_signal_type_t;
typedef struct {
  int dummy;
} zb_zdo_app_signal_hdr_t;
typedef struct {
  int leave_type;
} zb_zdo_signal_leave_params_t;
typedef struct {
  struct {
    int status;
  } nlme_status;
} zb_zdo_signal_nlme_status_indication_params_t;
typedef struct {
  int cluster_id, attr_id;
  struct {
    uint16_t data16;
  } values;
} zb_zcl_set_attr_value_param_t;
typedef struct {
  int status, device_cb_id;
  struct {
    zb_zcl_set_attr_value_param_t set_attr_value_param;
  } cb_param;
} zb_zcl_device_callback_param_t;
static zb_zcl_device_callback_param_t device_param;
static zb_zdo_signal_leave_params_t leave_param;
#define ZB_BUF_GET_PARAM(buf, type) (&device_param)
#define ZB_ZDO_SIGNAL_GET_PARAMS(buf, type) ((type*)&leave_param)
#define ZB_GET_APP_SIGNAL_STATUS(buf) signal_status
#define RESET_PIN 1
#define RESET_SOFTWARE 2
#define RESET_BROWNOUT 4
#define RESET_POR 8
#define RESET_WATCHDOG 16
#define RESET_DEBUG 32
#define RESET_LOW_POWER_WAKE 64
#define RESET_CPU_LOCKUP 128
#define RESET_HARDWARE 256
#define RESET_USER 512
#define CONFIG_LOG_DEFAULT_LEVEL 0
#define CONFIG_PRST_ZB_SLEEP_DURATION_SEC 600
#define CONFIG_PRST_ZB_CALIBRATION_SLEEP_DURATION_SEC 10
#define CONFIG_PRST_ZB_CALIBRATION_DURATION_SEC 900
#define CONFIG_PRST_ZB_CALIBRATION_SETTLE_SEC 180
#define CONFIG_PRST_ZB_CALIBRATION_CAPTURE_SAMPLES 9
#define CONFIG_PRST_ZB_BUTTON_MULTI_PRESS_GAP_MS 800
#define CONFIG_PRST_ZB_INTERVIEW_POLL_INTERVAL_MS 1000
#define CONFIG_PRST_ZB_INTERVIEW_DURATION_SEC 180
#define CONFIG_PRST_ZB_PARENT_POLL_INTERVAL_SEC 30
#define CONFIG_PRST_ZB_BUTTON_POLL_DURATION_SEC 60
#define CONFIG_PRST_ZB_BOOT_LOOP_LIMIT 5
#define CONFIG_PRST_ZB_BOOT_LOOP_BACKOFF_SEC 3600
#define CONFIG_ZIGBEE_DEV_REJOIN_TIMEOUT_MS 360000
#define LOG_MODULE_REGISTER(...)
#define LOG_ERR(...)
#define LOG_WRN(...)
#define LOG_INF(...)
#define LOG_DBG(...)
#define __ASSERT(x, ...) \
  do {                   \
    if (!(x)) abort();   \
  } while (0)
#define __ASSERT_NO_MSG(x) __ASSERT(x)
#define BIT(x) (1U << (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, a, b) MIN(MAX(x, a), b)
typedef int64_t k_timeout_t;
#define K_SECONDS(x) ((int64_t)(x) * 1000)
#define K_MSEC(x) (x)
#define K_NO_WAIT 0
struct k_timer {
  void (*expiry)(struct k_timer*);
  bool active;
  int64_t duration, period;
};
struct k_work {
  void (*handler)(struct k_work*);
  bool queued;
};
struct k_work_delayable {
  struct k_work work;
};
#define K_TIMER_DEFINE(name, cb, stop) struct k_timer name = {.expiry = cb}
#define K_WORK_DEFINE(name, cb) struct k_work name = {.handler = cb}
#define K_WORK_DELAYABLE_DEFINE(name, cb) struct k_work_delayable name = {.work = {.handler = cb}}
static int64_t now_ms, blocked_ms;
static bool joined = true, led_on;
static int signal_type, signal_status, failed_sensors, poll_ms, retry_starts, retry_stops, saves;
static int synchronous_counters, asynchronous_counters;
static int soil_value, raw_value, valid_value, error_value, battery_value, temp_value, humidity_value, light_value;
static void k_timer_start(struct k_timer* t, int64_t duration, int64_t period) {
  t->active = true;
  t->duration = duration;
  t->period = period;
}
static void k_timer_stop(struct k_timer* t) { t->active = false; }
static struct k_work* submitted_work;
static int k_work_submit(struct k_work* w) {
  w->queued = true;
  submitted_work = w;
  return 0;
}
static int k_work_reschedule(struct k_work_delayable* w, int64_t t) {
  (void)t;
  w->work.queued = true;
  return 0;
}
static int64_t k_uptime_get(void) { return now_ms; }
static void k_msleep(int ms) { blocked_ms += ms; }
static void k_sleep(int64_t ms) { blocked_ms += ms; }
static int prst_led_on(void) {
  led_on = true;
  return 0;
}
static int prst_led_off(void) {
  led_on = false;
  return 0;
}
static int prst_led_init(void) { return 0; }
static int prst_led_flash(int n) {
  k_msleep(n * 400);
  return 0;
}
static void power_down_unused_ram(void) {}
static int hwinfo_get_reset_cause(uint32_t* c) {
  *c = RESET_POR;
  return 0;
}
static int hwinfo_clear_reset_cause(void) { return 0; }
static bool ZB_JOINED(void) { return joined; }
static int alarm_schedule(zb_callback_t cb, int p, int64_t delay);
static int alarm_cancel(zb_callback_t cb, int p);
#define ZB_SCHEDULE_APP_ALARM(cb, p, delay) alarm_schedule(cb, p, delay)
#define ZB_SCHEDULE_APP_ALARM_CANCEL(cb, p) alarm_cancel(cb, p)
static int zigbee_schedule_callback(zb_callback_t cb, int p) {
  cb(p);
  return 0;
}
static void zb_zdo_pim_set_long_poll_interval(int ms) { poll_ms = ms; }
static void user_input_indicate(void) {}
static void zigbee_configure_sleepy_behavior(bool b) { (void)b; }
static void zigbee_enable(void) {}
static int zb_get_app_signal(int id, zb_zdo_app_signal_hdr_t** s) {
  (void)id;
  (void)s;
  return signal_type;
}
static int zigbee_default_signal_handler(int b) {
  (void)b;
  return 0;
}
static void zb_buf_free(int b) { (void)b; }
static int prst_zb_set_attr_value(uint16_t c, uint16_t a, void* v) {
  if (c == 1032) soil_value = *(uint16_t*)v;
  if (c == 1026) temp_value = *(int16_t*)v;
  if (c == 1029) humidity_value = *(uint16_t*)v;
  if (c == 1024) light_value = *(uint16_t*)v;
  if (c == 0xfc00) {
    if (a == 0) raw_value = *(int16_t*)v;
    if (a == 0x10) valid_value = *(uint8_t*)v;
    if (a == 0x11) error_value = *(uint8_t*)v;
    if (a == 0x12) battery_value = *(uint16_t*)v;
  }
  return 0;
}
static void prst_zb_attrs_init(struct zb_device_ctx* d) { memset(d, 0, sizeof(*d)); }
static void prst_soil_calibration_load(prst_soil_calibration_attrs_t* c) {
  c->dry = 0;
  c->wet = 10000;
}
static bool prst_soil_calibration_is_valid(int16_t d, int16_t w) { return d < w; }
static void prst_soil_calibration_save_async(const prst_soil_calibration_attrs_t* c) {
  (void)c;
  saves++;
}
static float prst_soil_calibration_apply(const prst_soil_calibration_attrs_t* c, float v) {
  (void)c;
  return v;
}
