#include <limits.h>
#include <math.h>
#include <prstlib/adc.h>
#include <prstlib/button.h>
#include <prstlib/led.h>
#include <prstlib/macros.h>
#include <prstlib/sensors.h>
#include <prstlib/shtc3.h>
#include <ram_pwrdn.h>
#include <zb_nrf_platform.h>
#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zcl/zb_zcl_power_config.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>

#include "boot_loop_guard.h"
#include "debug_counters.h"
#include "factory_reset.h"
#include "flash_fs.h"
#include "prst_zb_attrs.h"
#include "prst_zb_endpoint_defs.h"
#include "prst_zb_soil_moisture_defs.h"
#include "restart_handler.h"
#include "watchdog.h"

LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

static struct zb_device_ctx dev_ctx;

static prst_sensors_t sensors;

// A board that has measured and reported for this long is past any boot-time fault.
#define PRST_ZB_STABLE_UPTIME_MS (10 * 60 * 1000)

static void led_blink_end_cb(struct k_timer *timer) {
  prst_led_off();
}

K_TIMER_DEFINE(led_blink_end_timer, led_blink_end_cb, /*stop_fn=*/NULL);

static int led_flashing_left;

// A short blink per period instead of a 50% toggle: the LED draws more than the sleeping radio.
static void led_flashing_cb(struct k_timer *timer) {
  if (led_flashing_left-- <= 0) {
    k_timer_stop(timer);
    return;
  }
  prst_led_on();
  k_timer_start(&led_blink_end_timer, K_MSEC(40), K_NO_WAIT);
}

K_TIMER_DEFINE(led_flashing_timer, led_flashing_cb, /*stop_fn=*/NULL);

static void led_flashing_start(k_timeout_t period, int blinks) {
  led_flashing_left = blinks;
  k_timer_start(&led_flashing_timer, K_NO_WAIT, period);
}

// Shows "looking for the network" only while the stack is actively rejoining; a board that stays off the network
// would otherwise blink for days.
static void led_searching_start(void) {
  led_flashing_start(K_SECONDS(2), CONFIG_ZIGBEE_DEV_REJOIN_TIMEOUT_MS / 2000);
}

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(
    identify_attr_list,
    &dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(
    basic_attr_list,
    &dev_ctx.basic_attr.zcl_version,
    &dev_ctx.basic_attr.app_version,
    &dev_ctx.basic_attr.stack_version,
    &dev_ctx.basic_attr.hw_version,
    &dev_ctx.basic_attr.mf_name,
    &dev_ctx.basic_attr.model_id,
    &dev_ctx.basic_attr.date_code,
    &dev_ctx.basic_attr.power_source,
    &dev_ctx.basic_attr.location_id,
    &dev_ctx.basic_attr.ph_env,
    &dev_ctx.basic_attr.sw_ver);

ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(temp_measurement_attr_list,
                                            &dev_ctx.temp_measure_attrs.measure_value,
                                            &dev_ctx.temp_measure_attrs.min_measure_value,
                                            &dev_ctx.temp_measure_attrs.max_measure_value,
                                            &dev_ctx.temp_measure_attrs.tolerance);

ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(
    rel_humi_attr_list,
    &dev_ctx.rel_humidity_attrs.rel_humidity,
    &dev_ctx.rel_humidity_attrs.min_val,
    &dev_ctx.rel_humidity_attrs.max_val);

// https://devzone.nordicsemi.com/f/nordic-q-a/85315/zboss-declare-power-config-attribute-list-for-battery-bat_num
#define bat_num
ZB_ZCL_DECLARE_POWER_CONFIG_BATTERY_ATTRIB_LIST_EXT(
    batt_attr_list,
    &dev_ctx.batt_attrs.voltage,
    /*battery_size=*/&dev_ctx.batt_attrs.size,
    /*battery_quantity=*/&dev_ctx.batt_attrs.quantity,
    /*battery_rated_voltage=*/NULL,
    /*battery_alarm_mask=*/&dev_ctx.batt_attrs.alarm_mask,
    /*battery_voltage_min_threshold=*/NULL,
    /*battery_percentage_remaining=*/&dev_ctx.batt_attrs.percentage,
    /*battery_voltage_threshold1=*/NULL,
    /*battery_voltage_threshold2=*/NULL,
    /*battery_voltage_threshold3=*/NULL,
    /*battery_percentage_min_threshold=*/NULL,
    /*battery_percentage_threshold1=*/NULL,
    /*battery_percentage_threshold2=*/NULL,
    /*battery_percentage_threshold3=*/NULL,
    /*battery_alarm_state=*/&dev_ctx.batt_attrs.alarm_state);

PRST_ZB_ZCL_DECLARE_SOIL_MOISTURE_ATTRIB_LIST(
    soil_moisture_attr_list,
    &dev_ctx.soil_moisture_attrs.percentage);

ZB_ZCL_DECLARE_ILLUMINANCE_MEASUREMENT_ATTRIB_LIST(
    illuminance_attr_list,
    /*value=*/&dev_ctx.illuminance_attrs.log_lux,
    /*min_value=*/NULL,
    /*max_value*/ NULL);

PRST_ZB_ZCL_DECLARE_SOIL_CALIBRATION_ATTRIB_LIST(
    soil_calibration_attr_list,
    &dev_ctx.soil_calibration_attrs);

PRST_ZB_DECLARE_CLUSTER_LIST(
    app_template_clusters,
    basic_attr_list,
    identify_attr_list,
    temp_measurement_attr_list,
    rel_humi_attr_list,
    batt_attr_list,
    soil_moisture_attr_list,
    illuminance_attr_list,
    soil_calibration_attr_list);

PRST_ZB_DECLARE_ENDPOINT(
    app_template_ep,
    PRST_ZIGBEE_ENDPOINT,
    app_template_clusters);

ZBOSS_DECLARE_DEVICE_CTX_1_EP(
    app_template_ctx,
    app_template_ep);

static void led_flash_async(int times);

void identify_cb(zb_bufid_t bufid) {
  LOG_DBG("Remote identify command called");
  led_flash_async(15);
}

static int64_t calibration_mode_end_ms;
static int64_t receive_window_end_ms;

typedef enum {
  CAPTURE_NONE = 0,
  CAPTURE_DRY,
  CAPTURE_WET,
} capture_target_t;

static capture_target_t capture_target;
static zb_int16_t capture_samples[CONFIG_PRST_ZB_CALIBRATION_CAPTURE_SAMPLES];
static int capture_count;

static void led_flash_async(int times) {
  led_flashing_start(K_MSEC(400), times);
}

static bool calibration_mode_active(void) {
  return k_uptime_get() < calibration_mode_end_ms;
}

static void led_blink_once(void) {
  prst_led_on();
  k_timer_start(&led_blink_end_timer, K_MSEC(40), K_NO_WAIT);
}

static void end_calibration_mode(void);
static void calibration_timeout_cb(zb_uint8_t arg);

static zb_int16_t median(zb_int16_t *values, int n) {
  for (int i = 1; i < n; i++) {
    const zb_int16_t v = values[i];
    int j = i - 1;
    for (; j >= 0 && values[j] > v; j--) {
      values[j + 1] = values[j];
    }
    values[j + 1] = v;
  }
  return values[n / 2];
}

// The median rather than the mean, so a single disturbed reading (probe bumped, someone watering)
// does not drag the calibration point.
static void capture_soil_sample(zb_int16_t soil_raw) {
  k_timer_stop(&led_flashing_timer);
  capture_samples[capture_count++] = soil_raw;
  if (capture_count < CONFIG_PRST_ZB_CALIBRATION_CAPTURE_SAMPLES) {
    led_blink_once();
    return;
  }
  zb_int16_t point = median(capture_samples, capture_count);
  prst_soil_calibration_attrs_t cal = dev_ctx.soil_calibration_attrs;
  zb_uint16_t attr_id;
  if (capture_target == CAPTURE_DRY) {
    cal.dry = point;
    attr_id = PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID;
  } else {
    cal.wet = point;
    attr_id = PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID;
  }
  capture_target = CAPTURE_NONE;
  end_calibration_mode();
  // An invalid pair means the probe was likely in the wrong soil for this point.
  if (!prst_soil_calibration_is_valid(cal.dry, cal.wet)) {
    led_flash_async(10);
    return;
  }
  prst_zb_set_attr_value(PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION, attr_id, &point);
  prst_soil_calibration_save_async(&cal);
  led_flash_async(3);
}

// Counted per failure episode, not per read: a dead sensor would otherwise write to flash on every wake-up.
static void count_new_sensor_failures(int failed) {
  static int previously_failed;
  const int newly_failed = failed & ~previously_failed;
  previously_failed = failed;
  if (newly_failed & PRST_SENSOR_BATT) {
    prst_debug_counters_increment_async("read_error_batt");
  }
  if (newly_failed & PRST_SENSOR_SOIL) {
    prst_debug_counters_increment_async("read_error_soil");
  }
  if (newly_failed & PRST_SENSOR_PHOTO) {
    prst_debug_counters_increment_async("read_error_photo");
  }
  if (newly_failed & PRST_SENSOR_SHTC3) {
    prst_debug_counters_increment_async("read_error_shtc3");
  }
}

void update_sensors_cb(zb_uint8_t arg) {
  LOG_DBG("Updating sensors");

  if (calibration_mode_end_ms && !calibration_mode_active()) {
    end_calibration_mode();
  }

  const uint32_t sleep_sec = calibration_mode_active() ? CONFIG_PRST_ZB_CALIBRATION_SLEEP_DURATION_SEC
                                                       : CONFIG_PRST_ZB_SLEEP_DURATION_SEC;
  // Reschedule the same callback.
  zb_ret_t ret = ZB_SCHEDULE_APP_ALARM(
      update_sensors_cb,
      /*param=*/0,
      ZB_MILLISECONDS_TO_BEACON_INTERVAL(1000 * sleep_sec));
  if (ret != RET_OK) {
    prst_debug_counters_increment("sens_cb_schedule_err");
    __ASSERT(false, "Unable to schedule sensor update callback");
  }

  __ASSERT(!prst_watchdog_feed(), "Failed to feed watchdog");

  if (ZB_JOINED() && k_uptime_get() > PRST_ZB_STABLE_UPTIME_MS) {
    prst_boot_loop_guard_clear_async();
  }

  const int failed = prst_sensors_read_all(&sensors);
  count_new_sensor_failures(failed);
  zb_bool_t soil_valid = !(failed & PRST_SENSOR_SOIL);
  zb_uint8_t sensor_errors = failed;
  zb_uint32_t uptime_sec = k_uptime_get() / 1000;
  prst_zb_set_attr_value(PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION,
                         PRST_ZB_ZCL_ATTR_SOIL_VALID_ID, &soil_valid);
  prst_zb_set_attr_value(PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION,
                         PRST_ZB_ZCL_ATTR_SENSOR_ERRORS_ID, &sensor_errors);
  prst_zb_set_attr_value(PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION,
                         PRST_ZB_ZCL_ATTR_UPTIME_SEC_ID, &uptime_sec);

  if (!(failed & PRST_SENSOR_BATT)) {
    zb_uint16_t battery_mv = sensors.batt.adc_read.millivolts;
    prst_zb_set_attr_value(PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION,
                           PRST_ZB_ZCL_ATTR_BATTERY_MV_ID, &battery_mv);
    // Battery voltage in units of 100 mV.
    uint8_t batt_voltage = sensors.batt.adc_read.millivolts / 100;
    prst_zb_set_attr_value(ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
                           ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID,
                           &batt_voltage);

    // Battery percentage in units of 0.5%.
    zb_uint8_t batt_percentage = 2 * 100 * sensors.batt.percentage;
    prst_zb_set_attr_value(ZB_ZCL_CLUSTER_ID_POWER_CONFIG,
                           ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
                           &batt_percentage);
  }

  // A dead SHTC3 reports the ZCL "unknown" values rather than its last reading, which would look current forever.
  // Temperature in units of 0.01 degrees Celcius.
  zb_int16_t temperature_value = (failed & PRST_SENSOR_SHTC3) ? ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_UNKNOWN
                                                              : 100 * sensors.shtc3.temp_c;
  prst_zb_set_attr_value(ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                         ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
                         &temperature_value);

  // Relative humidity in units of 0.01%.
  zb_uint16_t rel_humi = (failed & PRST_SENSOR_SHTC3) ? ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_UNKNOWN
                                                      : 100 * 100 * sensors.shtc3.rel_humi;
  prst_zb_set_attr_value(ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
                         ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
                         &rel_humi);

  {
    // Illuminance in 10000 * log_10(lux) + 1; ZCL reserves 0 for "too low to measure".
    zb_uint16_t log_lux =
        (failed & PRST_SENSOR_PHOTO) ? UINT16_MAX :
        sensors.photo.brightness ? 10000 * log10((float)sensors.photo.brightness) + 1 : 0;
    prst_zb_set_attr_value(ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT,
                           ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID,
                           &log_lux);
  }

  if (failed & PRST_SENSOR_SOIL) {
    zb_uint16_t unknown = UINT16_MAX;
    zb_int16_t raw_unknown = INT16_MIN;
    prst_zb_set_attr_value(PRST_ZB_ZCL_ATTR_SOIL_MOISTURE_CLUSTER_ID,
                           PRST_ZB_ZCL_ATTR_SOIL_MOISTURE_VALUE_ID, &unknown);
    prst_zb_set_attr_value(PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION,
                           PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_RAW_ID, &raw_unknown);
    return;
  }

  zb_int16_t soil_raw = CLAMP(10000 * sensors.soil.unclamped_percentage, INT16_MIN, INT16_MAX);
  prst_zb_set_attr_value(PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION,
                         PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_RAW_ID,
                         &soil_raw);

  // Soil moisture in units of 0.01%.
  zb_int16_t soil_moisture =
      10000 * prst_soil_calibration_apply(&dev_ctx.soil_calibration_attrs,
                                          sensors.soil.unclamped_percentage);
  prst_zb_set_attr_value(PRST_ZB_ZCL_ATTR_SOIL_MOISTURE_CLUSTER_ID,
                         PRST_ZB_ZCL_ATTR_SOIL_MOISTURE_VALUE_ID,
                         &soil_moisture);

  if (capture_target != CAPTURE_NONE) {
    capture_soil_sample(soil_raw);
  } else if (calibration_mode_active()) {
    // Tells whoever is poking the probe around that this position has been measured.
    led_blink_once();
  }
}

static void reschedule_sensor_update_now(void) {
  ZB_SCHEDULE_APP_ALARM_CANCEL(update_sensors_cb, ZB_ALARM_ANY_PARAM);
  ZB_ERROR_CHECK(ZB_SCHEDULE_APP_ALARM(
      update_sensors_cb, /*param=*/0, ZB_MILLISECONDS_TO_BEACON_INTERVAL(1000)));
}

static void enter_long_poll(zb_uint8_t arg) {
  ZB_SCHEDULE_APP_ALARM_CANCEL(enter_long_poll, ZB_ALARM_ANY_PARAM);
  const int64_t until_ms = MAX(calibration_mode_end_ms, receive_window_end_ms);
  const int64_t remaining_ms = until_ms - k_uptime_get();
  if (remaining_ms > 0) {
    zb_zdo_pim_set_long_poll_interval(CONFIG_PRST_ZB_INTERVIEW_POLL_INTERVAL_MS);
    ZB_ERROR_CHECK(ZB_SCHEDULE_APP_ALARM(
        enter_long_poll, /*param=*/0, ZB_MILLISECONDS_TO_BEACON_INTERVAL(remaining_ms)));
  } else {
    zb_zdo_pim_set_long_poll_interval(1000 * CONFIG_PRST_ZB_PARENT_POLL_INTERVAL_SEC);
  }
}

// Interview, service and button receive windows may overlap; the longest one wins.
static void enter_interview_poll(void) {
  receive_window_end_ms = MAX(receive_window_end_ms,
      k_uptime_get() + 1000LL * CONFIG_PRST_ZB_INTERVIEW_DURATION_SEC);
  enter_long_poll(/*arg=*/0);
}

static void zcl_device_cb(zb_bufid_t bufid) {
  zb_zcl_device_callback_param_t *param =
      ZB_BUF_GET_PARAM(bufid, zb_zcl_device_callback_param_t);
  param->status = RET_OK;
  if (param->device_cb_id != ZB_ZCL_SET_ATTR_VALUE_CB_ID) {
    return;
  }
  const zb_zcl_set_attr_value_param_t *set = &param->cb_param.set_attr_value_param;
  if (set->cluster_id != PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION) {
    return;
  }
  prst_soil_calibration_attrs_t cal = dev_ctx.soil_calibration_attrs;
  const zb_int16_t value = (zb_int16_t)set->values.data16;
  if (set->attr_id == PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID) {
    cal.dry = value;
  } else if (set->attr_id == PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID) {
    cal.wet = value;
  } else {
    return;
  }
  // check_value in prst_zb_soil_calibration.c has already rejected an invalid pair.
  prst_soil_calibration_save_async(&cal);
  // Re-measure right away so a new calibration point shows up without waiting a full sleep cycle.
  reschedule_sensor_update_now();
}

static void end_calibration_mode(void) {
  calibration_mode_end_ms = 0;
  capture_target = CAPTURE_NONE;
  capture_count = 0;
  k_timer_stop(&led_flashing_timer);
  k_timer_stop(&led_blink_end_timer);
  prst_led_off();
  ZB_SCHEDULE_APP_ALARM_CANCEL(calibration_timeout_cb, ZB_ALARM_ANY_PARAM);
  ZB_SCHEDULE_APP_ALARM_CANCEL(enter_long_poll, ZB_ALARM_ANY_PARAM);
  enter_long_poll(/*arg=*/0);
}

static void calibration_timeout_cb(zb_uint8_t arg) {
  end_calibration_mode();
  reschedule_sensor_update_now();
}

static void start_calibration_mode(zb_uint8_t presses) {
  end_calibration_mode();
  user_input_indicate();
  if (presses == 1) {
    receive_window_end_ms = MAX(receive_window_end_ms,
        k_uptime_get() + 1000LL * CONFIG_PRST_ZB_BUTTON_POLL_DURATION_SEC);
    enter_long_poll(/*arg=*/0);
    reschedule_sensor_update_now();
    return;
  }

  calibration_mode_end_ms = k_uptime_get() + 1000LL * CONFIG_PRST_ZB_CALIBRATION_DURATION_SEC;
  capture_target = presses == 2 ? CAPTURE_DRY : presses == 3 ? CAPTURE_WET : CAPTURE_NONE;
  enter_long_poll(/*arg=*/0);
  ZB_ERROR_CHECK(ZB_SCHEDULE_APP_ALARM(
      calibration_timeout_cb, /*param=*/0,
      ZB_MILLISECONDS_TO_BEACON_INTERVAL(1000 * CONFIG_PRST_ZB_CALIBRATION_DURATION_SEC)));
  if (capture_target == CAPTURE_NONE) {
    reschedule_sensor_update_now();
    return;
  }

  led_flashing_start(K_SECONDS(1), INT_MAX);
  ZB_SCHEDULE_APP_ALARM_CANCEL(update_sensors_cb, ZB_ALARM_ANY_PARAM);
  ZB_ERROR_CHECK(ZB_SCHEDULE_APP_ALARM(
      update_sensors_cb, /*param=*/0,
      ZB_MILLISECONDS_TO_BEACON_INTERVAL(1000 * CONFIG_PRST_ZB_CALIBRATION_SETTLE_SEC)));
}

static int button_presses;

static void button_sequence_end_handler(struct k_work *work) {
  const int presses = MIN(button_presses, 4);
  button_presses = 0;
  // Button callbacks run on the system workqueue; ZBOSS state may only be touched from its thread.
  ZB_ERROR_CHECK(zigbee_schedule_callback(start_calibration_mode, presses));
}

K_WORK_DELAYABLE_DEFINE(button_sequence_end_work, button_sequence_end_handler);

static void button_cb(prst_button_t button, bool is_active) {
  if (button != PRST_BUTTON_SW1 || !is_active) {
    return;
  }
  led_blink_once();
  button_presses++;
  k_work_reschedule(&button_sequence_end_work, K_MSEC(CONFIG_PRST_ZB_BUTTON_MULTI_PRESS_GAP_MS));
}

void zboss_signal_handler(zb_bufid_t bufid) {
  // See zigbee_default_signal_handler() for all available signals.
  zb_zdo_app_signal_hdr_t *sig_hndler = NULL;
  zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, /*sg_p=*/&sig_hndler);
  zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);
  switch (sig) {
    case ZB_BDB_SIGNAL_STEERING:         // New network.
    case ZB_BDB_SIGNAL_DEVICE_REBOOT:
    case ZB_BDB_SIGNAL_TC_REJOIN_DONE: {  // Previously joined network.
      LOG_DBG("Steering complete. Status: %d", status);
      if (status == RET_OK) {
        LOG_DBG("Steering successful. Status: %d", status);
        k_timer_stop(&led_flashing_timer);
        prst_led_off();
        prst_restart_watchdog_stop();
        // Poll intervals only take effect after joining.
        enter_interview_poll();
      } else {
        LOG_DBG("Steering failed. Status: %d", status);
        prst_restart_watchdog_start();
        // Power saving.
        k_timer_stop(&led_flashing_timer);
        prst_led_off();
      }
      break;
    }
    case ZB_ZDO_SIGNAL_LEAVE:
      if (status == RET_OK) {
        led_searching_start();
        LOG_DBG("Network left (leave type: %d)",
                ((zb_zdo_signal_leave_params_t *)ZB_ZDO_SIGNAL_GET_PARAMS(
                    sig_hndler, zb_zdo_signal_leave_params_t))->leave_type);
        // The default handler rejoins for CONFIG_ZIGBEE_DEV_REJOIN_TIMEOUT_MS and then waits for user input
        // forever; the restart watchdog is what brings an unattended board back.
        prst_restart_watchdog_start();
      }
      break;
    case ZB_ZDO_SIGNAL_SKIP_STARTUP: {
      LOG_DBG("Started zigbee stack and waiting for connection to network.");
      led_searching_start();
      // Covers a stack that never reports the rejoin outcome; a successful join stops it.
      prst_restart_watchdog_start();

      // Kick off main sensor update task.
      ZB_ERROR_CHECK(ZB_SCHEDULE_APP_ALARM(update_sensors_cb,
                                           /*param=*/0,
                                           ZB_MILLISECONDS_TO_BEACON_INTERVAL(1000)));
      __ASSERT_NO_MSG(!prst_watchdog_start());
      break;
    }
    case ZB_NLME_STATUS_INDICATION: {
      zb_zdo_signal_nlme_status_indication_params_t *nlme_status_ind =
          ZB_ZDO_SIGNAL_GET_PARAMS(sig_hndler, zb_zdo_signal_nlme_status_indication_params_t);
      if (nlme_status_ind->nlme_status.status == ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE) {
        // Not zb_reset(), Nordic's workaround for KRKNWK-12017 (NCS 1.8-2.3): it reboots on every parent link
        // failure before a join, which never ends while the parent stays unreachable. The restart watchdog
        // retries the rejoin with backoff instead.
        prst_restart_watchdog_start();
      }
      break;
    }
  }

  ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
  if (bufid) {
    zb_buf_free(bufid);
  }
}

void dump_counter(const char *counter_name, prst_debug_counter_t value) {
  LOG_INF("- %s: %d", counter_name, value);
}

int log_reset_reason_counter(uint32_t *cause_out) {
  uint32_t cause;
  const char *reset_counter_str = "reset_cause_unknown";
  RET_IF_ERR(hwinfo_get_reset_cause(&cause));
  RET_IF_ERR(hwinfo_clear_reset_cause());
  *cause_out = cause;
  if (cause & RESET_PIN) {
    reset_counter_str = "reset_cause_pin";
  } else if (cause & RESET_SOFTWARE) {
    // Includes fatal errors from __ASSERT, ZB_ERROR_CHECK and friends.
    reset_counter_str = "reset_cause_software";
  } else if (cause & RESET_BROWNOUT) {
    reset_counter_str = "reset_cause_brownout";
  } else if (cause & RESET_POR) {
    reset_counter_str = "reset_cause_power_on";
  } else if (cause & RESET_WATCHDOG) {
    reset_counter_str = "reset_cause_watchdog";
  } else if (cause & RESET_DEBUG) {
    reset_counter_str = "reset_cause_debug";
  } else if (cause & RESET_LOW_POWER_WAKE) {
    reset_counter_str = "reset_cause_low_power";
  } else if (cause & RESET_CPU_LOCKUP) {
    reset_counter_str = "reset_cause_cpu_lockup";
  } else if (cause & RESET_HARDWARE) {
    reset_counter_str = "reset_cause_hardware";
  } else if (cause & RESET_USER) {
    reset_counter_str = "reset_cause_user";
  }
  return prst_debug_counters_increment(reset_counter_str);
}

int main(void) {
  RET_IF_ERR(prst_adc_init());
  RET_IF_ERR(prst_led_init());
  RET_IF_ERR(prst_button_init());
  RET_IF_ERR(prst_flash_fs_init());
  RET_IF_ERR(prst_debug_counters_init());

  prst_debug_counters_increment("boot");

  // Puts the sensors into low power mode. A failure must not reboot: a dead SHTC3 would otherwise hold the board in a
  // boot loop although soil readings still work.
  count_new_sensor_failures(prst_sensors_read_all(&sensors));

  uint32_t reset_cause = 0;
  log_reset_reason_counter(&reset_cause);

  // A fault that recurs right after boot would otherwise restart the board every few seconds until the battery is
  // flat. Only crash resets count: a battery swap or reflash starts over. One long blink marks the wait; SW1 ends it.
  const bool crash_reset = reset_cause & (RESET_SOFTWARE | RESET_WATCHDOG | RESET_CPU_LOCKUP);
  if (prst_boot_loop_guard_register_boot(crash_reset) >= CONFIG_PRST_ZB_BOOT_LOOP_LIMIT) {
    prst_debug_counters_increment("boot_loop_backoff");
    prst_led_on();
    k_msleep(1000);
    prst_led_off();
    for (int sec = 0; sec < CONFIG_PRST_ZB_BOOT_LOOP_BACKOFF_SEC && !prst_button_poll(PRST_BUTTON_SW1); sec++) {
      k_sleep(K_SECONDS(1));
    }
  }

  LOG_INF("Dumping debug counters:");
  prst_debug_counters_get_all(dump_counter);

  __ASSERT_NO_MSG(!prst_zb_factory_reset_check());

  prst_zb_attrs_init(&dev_ctx);
  prst_soil_calibration_load(&dev_ctx.soil_calibration_attrs);
  prst_debug_counters_get("boot", &dev_ctx.soil_calibration_attrs.boot_count);
  dev_ctx.soil_calibration_attrs.reset_cause = reset_cause;

  ZB_AF_REGISTER_DEVICE_CTX(&app_template_ctx);

  prst_led_flash(2);
  k_msleep(100);

  ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(PRST_ZIGBEE_ENDPOINT, identify_cb);
  ZB_ZCL_REGISTER_DEVICE_CB(zcl_device_cb);
  RET_IF_ERR(prst_button_register_callback(button_cb));

  zigbee_configure_sleepy_behavior(/*enable=*/true);
  power_down_unused_ram();
  zigbee_enable();

  prst_debug_counters_increment("main_finish");

  return 0;
}
