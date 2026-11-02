#include <prstlib/button.h>
#include <prstlib/sensors.h>

#include "../../samples/zigbee/src/debug_counters.h"
#include "platform.h"
int prst_sensors_read_all(prst_sensors_t* s) {
  if (!failed_sensors) {
    s->soil.unclamped_percentage = 0.5;
    s->batt.adc_read.millivolts = 3000;
    s->batt.percentage = 1;
  }
  return failed_sensors;
}
int prst_adc_init(void) { return 0; }
int prst_button_init(void) { return 0; }
int prst_button_poll(prst_button_t b) {
  (void)b;
  return 0;
}
int prst_button_register_callback(prst_button_callback_t cb) {
  (void)cb;
  return 0;
}
int prst_debug_counters_increment(const char* s) {
  (void)s;
  synchronous_counters++;
  return 0;
}
int prst_debug_counters_increment_async(const char* s) {
  (void)s;
  asynchronous_counters++;
  return 0;
}
int prst_debug_counters_get(const char* s, prst_debug_counter_t* v) {
  (void)s;
  *v = 2;
  return 0;
}
int prst_debug_counters_init(void) { return 0; }
int prst_debug_counters_get_all(prst_debug_counters_callback_t cb) {
  (void)cb;
  return 0;
}
uint32_t prst_boot_loop_guard_register_boot(bool c) {
  (void)c;
  return 0;
}
void prst_boot_loop_guard_clear_async(void) {}
int prst_flash_fs_init(void) { return 0; }
int prst_zb_factory_reset_check(void) { return 0; }
int prst_watchdog_start(void) { return 0; }
int prst_watchdog_feed(void) { return 0; }
void prst_restart_watchdog_start(void) { retry_starts++; }
void prst_restart_watchdog_stop(void) { retry_stops++; }
#define main firmware_main
#include "../../samples/zigbee/src/main.c"
#undef main
struct alarm {
  zb_callback_t cb;
  int param;
  int64_t at;
  bool active;
};
static struct alarm alarms[24];
static int alarm_schedule(zb_callback_t cb, int p, int64_t delay) {
  for (int i = 0; i < 24; i++)
    if (!alarms[i].active) {
      alarms[i] = (struct alarm){cb, p, now_ms + delay, true};
      return 0;
    }
  return -2;
}
static int alarm_cancel(zb_callback_t cb, int p) {
  (void)p;
  for (int i = 0; i < 24; i++)
    if (alarms[i].active && alarms[i].cb == cb) alarms[i].active = false;
  return 0;
}
static void advance(int64_t ms) {
  now_ms += ms;
  for (int n = 0; n < 50; n++) {
    int i;
    for (i = 0; i < 24; i++)
      if (alarms[i].active && alarms[i].at <= now_ms) break;
    if (i == 24) break;
    struct alarm a = alarms[i];
    alarms[i].active = false;
    a.cb(a.param);
  }
}
static void reset(void) {
  memset(&dev_ctx, 0, sizeof(dev_ctx));
  dev_ctx.soil_calibration_attrs.wet = 10000;
  memset(alarms, 0, sizeof(alarms));
  calibration_mode_end_ms = 0;
  receive_window_end_ms = 0;
  capture_target = CAPTURE_NONE;
  capture_count = 0;
  now_ms = 0;
  failed_sensors = 0;
  blocked_ms = 0;
  poll_ms = 30000;
  led_on = false;
  saves = 0;
  valid_value = -1;
  soil_value = -1;
  raw_value = 0;
  error_value = 0;
  retry_stops = 0;
  submitted_work = NULL;
  led_flashing_timer.active = false;
  led_blink_end_timer.active = false;
}
#define CHECK(cond, msg)                      \
  do {                                        \
    if (!(cond)) {                            \
      printf("FAIL %s: %s\n", __func__, msg); \
      return 1;                               \
    }                                         \
  } while (0)
static int single_press_is_one_measurement(void) {
  reset();
  start_calibration_mode(1);
  CHECK(!calibration_mode_active(), "single press entered long calibration");
  advance(1000);
  CHECK(soil_value == 5000, "measurement missing");
  advance(60000);
  CHECK(poll_ms == 30000, "brief receive window did not end");
  return 0;
}
static int identify_never_blocks_buttons(void) {
  reset();
  led_flash_async(15);
  if (submitted_work) submitted_work->handler(submitted_work);
  CHECK(blocked_ms == 0, "identify slept on system workqueue");
  CHECK(led_flashing_timer.active, "identify not scheduled");
  return 0;
}
static int failed_capture_expires_without_saving(void) {
  reset();
  failed_sensors = 2;
  start_calibration_mode(2);
  advance(180000);
  CHECK(capture_target != CAPTURE_NONE, "capture ended before deadline");
  advance(721000);
  CHECK(capture_target == CAPTURE_NONE, "capture remained pending after expiry");
  CHECK(!led_flashing_timer.active && !led_on, "settling LED remained on");
  failed_sensors = 0;
  advance(600000);
  CHECK(saves == 0, "expired capture saved later soil");
  return 0;
}
static int invalid_soil_clears_the_reported_value(void) {
  reset();
  failed_sensors = 2;
  update_sensors_cb(0);
  CHECK(soil_value == 65535, "old soil remains valid");
  CHECK(valid_value == 0, "validity not false");
  CHECK(error_value == 2, "error not reported");
  failed_sensors = 0;
  update_sensors_cb(0);
  CHECK(soil_value == 5000 && valid_value == 1, "soil did not recover");
  return 0;
}
static int trust_center_success_stops_retries(void) {
  reset();
  signal_type = ZB_BDB_SIGNAL_TC_REJOIN_DONE;
  signal_status = 0;
  zboss_signal_handler(1);
  CHECK(retry_stops == 1, "TC rejoin success left retries active");
  return 0;
}
static int explicit_service_mode_expires(void) {
  reset();
  start_calibration_mode(4);
  CHECK(calibration_mode_active() && poll_ms == 1000, "service window not active");
  advance(901000);
  CHECK(!calibration_mode_active() && poll_ms == 30000, "service window did not close");
  return 0;
}
static int quick_measure_cancels_capture(void) {
  reset();
  start_calibration_mode(3);
  start_calibration_mode(1);
  CHECK(capture_target == CAPTURE_NONE && !calibration_mode_active(), "quick measure kept pending capture");
  advance(901000);
  CHECK(saves == 0, "cancelled capture saved");
  return 0;
}
static int successful_capture_saves_once(void) {
  reset();
  start_calibration_mode(2);
  advance(180000);
  for (int i = 1; i < 9; i++) advance(10000);
  CHECK(saves == 1, "capture did not save once");
  CHECK(capture_target == CAPTURE_NONE && !calibration_mode_active(), "capture remained active");
  CHECK(poll_ms == 30000, "capture left fast polling enabled");
  advance(900000);
  CHECK(saves == 1, "capture saved twice");
  return 0;
}
static int failed_air_and_light_report_unknown(void) {
  reset();
  failed_sensors = 12;
  update_sensors_cb(0);
  CHECK(temp_value == -32768 && humidity_value == 65535, "failed air sensor kept old reading");
  CHECK(light_value == 65535, "failed light sensor kept old reading");
  return 0;
}
static int battery_diagnostic_uses_millivolts(void) {
  reset();
  update_sensors_cb(0);
  CHECK(battery_value == 3000, "battery diagnostic lost precision or was absent");
  return 0;
}
static int capture_deadline_also_checked_by_measurement(void) {
  reset();
  start_calibration_mode(2);
  now_ms = 901000;
  update_sensors_cb(0);
  CHECK(capture_target == CAPTURE_NONE && saves == 0, "delayed deadline callback allowed late capture");
  return 0;
}
static int identify_delivers_finite_pulses(void) {
  reset();
  led_flash_async(3);
  int pulses = 0;
  for (int i = 0; i < 4; i++) {
    led_flashing_cb(&led_flashing_timer);
    if (led_on) pulses++;
    led_blink_end_cb(&led_blink_end_timer);
  }
  CHECK(pulses == 3 && !led_flashing_timer.active && !led_on, "identify pulse count or final LED state wrong");
  return 0;
}
static int quick_measure_preserves_interview_window(void) {
  reset();
  enter_interview_poll();
  advance(30000);
  start_calibration_mode(1);
  advance(60000);
  CHECK(poll_ms == 1000, "quick measure shortened interview window");
  advance(90000);
  CHECK(poll_ms == 30000, "interview never ended");
  return 0;
}
static int rejoin_preserves_service_window(void) {
  reset();
  start_calibration_mode(4);
  advance(60000);
  enter_interview_poll();
  advance(180000);
  CHECK(calibration_mode_active() && poll_ms == 1000, "rejoin shortened service window");
  advance(660000);
  CHECK(!calibration_mode_active() && poll_ms == 30000, "service window never ended");
  return 0;
}
static int capture_completion_preserves_interview_window(void) {
  reset();
  start_calibration_mode(2);
  advance(180000);
  for (int i = 1; i < 8; i++) advance(10000);
  enter_interview_poll();
  advance(10000);
  CHECK(saves == 1 && poll_ms == 1000, "capture completion shortened interview window");
  advance(170000);
  CHECK(poll_ms == 30000, "remaining interview never ended");
  return 0;
}
static int sensor_errors_do_not_write_flash_in_zigbee(void) {
  reset();
  update_sensors_cb(0);
  synchronous_counters = 0;
  asynchronous_counters = 0;
  failed_sensors = 8;
  update_sensors_cb(0);
  CHECK(synchronous_counters == 0 && asynchronous_counters == 1, "sensor error persisted on Zigbee thread");
  update_sensors_cb(0);
  CHECK(asynchronous_counters == 1, "same failure repeatedly persisted");
  return 0;
}
int main(void) {
  int failed = 0;
  failed += single_press_is_one_measurement();
  failed += identify_never_blocks_buttons();
  failed += failed_capture_expires_without_saving();
  failed += invalid_soil_clears_the_reported_value();
  failed += trust_center_success_stops_retries();
  failed += explicit_service_mode_expires();
  failed += quick_measure_cancels_capture();
  failed += successful_capture_saves_once();
  failed += failed_air_and_light_report_unknown();
  failed += battery_diagnostic_uses_millivolts();
  failed += capture_deadline_also_checked_by_measurement();
  failed += identify_delivers_finite_pulses();
  failed += quick_measure_preserves_interview_window();
  failed += rejoin_preserves_service_window();
  failed += capture_completion_preserves_interview_window();
  failed += sensor_errors_do_not_write_flash_in_zigbee();
  printf("application: %d/16 passed\n", 16 - failed);
  return failed ? 1 : 0;
}
