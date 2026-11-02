#include "restart_handler.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zigbee/zigbee_app_utils.h>

#include "debug_counters.h"

LOG_MODULE_REGISTER(restart_handler, CONFIG_LOG_DEFAULT_LEVEL);

static uint32_t restart_timeout_sec = CONFIG_PRST_ZB_RESTART_WATCHDOG_TIMEOUT_SEC;
static bool recovery_active;
static int64_t next_retry_ms;

static void callback_work_handler(struct k_work *work);
static void restart_network_steering_cb(struct k_timer *timer);

K_MUTEX_DEFINE(recovery_lock);
K_WORK_DEFINE(callback_work, callback_work_handler);
K_TIMER_DEFINE(restart_timer, restart_network_steering_cb, NULL);

static void schedule_retry(void) {
  next_retry_ms = k_uptime_get() + (int64_t)restart_timeout_sec * 1000;
  k_timer_start(&restart_timer, K_SECONDS(restart_timeout_sec), K_MSEC(0));
}

static void callback_work_handler(struct k_work *work) {
  ARG_UNUSED(work);
  k_mutex_lock(&recovery_lock, K_FOREVER);
  // A stopped cycle can leave a callback queued after a newer cycle starts.
  if (recovery_active && k_uptime_get() >= next_retry_ms) {
    LOG_INF("Retrying network recovery.");
    prst_debug_counters_increment("steering_watchdog_restart");
    user_input_indicate();
    restart_timeout_sec = MIN((uint64_t)restart_timeout_sec * 2,
                              CONFIG_PRST_ZB_RESTART_WATCHDOG_MAX_TIMEOUT_SEC);
    // The SDK may ignore user input until its current rejoin round expires.
    schedule_retry();
  }
  k_mutex_unlock(&recovery_lock);
}

static void restart_network_steering_cb(struct k_timer *timer) {
  ARG_UNUSED(timer);
  k_work_submit(&callback_work);
}

void prst_restart_watchdog_start(void) {
  k_mutex_lock(&recovery_lock, K_FOREVER);
  if (!recovery_active) {
    recovery_active = true;
    schedule_retry();
  }
  k_mutex_unlock(&recovery_lock);
}

void prst_restart_watchdog_stop(void) {
  k_mutex_lock(&recovery_lock, K_FOREVER);
  recovery_active = false;
  k_timer_stop(&restart_timer);
  k_work_cancel(&callback_work);
  restart_timeout_sec = CONFIG_PRST_ZB_RESTART_WATCHDOG_TIMEOUT_SEC;
  k_mutex_unlock(&recovery_lock);
}
