#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>

#include "restart_handler.h"

static int64_t now_ms;
static struct k_timer* scheduled_timer;
static struct k_work* queued_work;
static unsigned retry_requests;
static unsigned sdk_rejoin_starts;
static bool sdk_waiting_for_input;
static bool in_timer_callback;
static pthread_mutex_t sdk_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sdk_condition = PTHREAD_COND_INITIALIZER;
static bool block_sdk;
static bool sdk_entered;
static bool release_sdk;
static bool stop_waiting_for_mutex;
static atomic_bool stop_returned;
static _Thread_local bool stopping_thread;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition); \
      exit(1);                                                             \
    }                                                                      \
  } while (0)

int64_t k_uptime_get(void) { return now_ms; }

int k_mutex_lock(struct k_mutex* mutex, k_timeout_t timeout) {
  CHECK(timeout == K_FOREVER);
  CHECK(!in_timer_callback);
  if (stopping_thread) {
    pthread_mutex_lock(&sdk_lock);
    stop_waiting_for_mutex = true;
    pthread_cond_broadcast(&sdk_condition);
    pthread_mutex_unlock(&sdk_lock);
  }
  return pthread_mutex_lock(&mutex->native);
}

int k_mutex_unlock(struct k_mutex* mutex) {
  return pthread_mutex_unlock(&mutex->native);
}

void k_timer_start(struct k_timer* timer, k_timeout_t duration, k_timeout_t period) {
  CHECK(duration > 0);
  CHECK(period == 0);
  scheduled_timer = timer;
  timer->deadline = now_ms + duration;
  timer->armed = true;
}

void k_timer_stop(struct k_timer* timer) { timer->armed = false; }

int k_work_submit(struct k_work* work) {
  queued_work = work;
  work->pending = true;
  return 1;
}

int k_work_cancel(struct k_work* work) {
  work->pending = false;
  return 0;
}

int prst_debug_counters_increment(const char* name) {
  CHECK(strcmp(name, "steering_watchdog_restart") == 0);
  CHECK(!in_timer_callback);
  return 0;
}

void user_input_indicate(void) {
  CHECK(!in_timer_callback);
  ++retry_requests;
  if (sdk_waiting_for_input) {
    ++sdk_rejoin_starts;
    sdk_waiting_for_input = false;
  }
  pthread_mutex_lock(&sdk_lock);
  sdk_entered = true;
  pthread_cond_broadcast(&sdk_condition);
  while (block_sdk && !release_sdk) {
    pthread_cond_wait(&sdk_condition, &sdk_lock);
  }
  pthread_mutex_unlock(&sdk_lock);
}

static void expire_timer(void) {
  CHECK(scheduled_timer && scheduled_timer->armed);
  now_ms = scheduled_timer->deadline;
  scheduled_timer->armed = false;
  in_timer_callback = true;
  scheduled_timer->expiry(scheduled_timer);
  in_timer_callback = false;
}

static void run_work(void) {
  CHECK(queued_work && queued_work->pending);
  queued_work->pending = false;
  queued_work->handler(queued_work);
}

static void retry_at(int64_t expected_deadline) {
  CHECK(scheduled_timer && scheduled_timer->armed);
  CHECK(scheduled_timer->deadline == expected_deadline);
  expire_timer();
  run_work();
}

static void test_duplicate_start_keeps_first_deadline(void) {
  prst_restart_watchdog_start();
  CHECK(scheduled_timer->deadline == 600000);
  now_ms = 590000;
  prst_restart_watchdog_start();
  CHECK(scheduled_timer->deadline == 600000);
  retry_at(600000);
  CHECK(retry_requests == 1);
  now_ms = 1799999;
  prst_restart_watchdog_start();
  CHECK(scheduled_timer->deadline == 1800000);
}

static void test_noop_then_failed_rejoins_keep_retrying(void) {
  prst_restart_watchdog_start();
  retry_at(600000);
  CHECK(retry_requests == 1);
  CHECK(sdk_rejoin_starts == 0);
  sdk_waiting_for_input = true;
  retry_at(1800000);
  CHECK(sdk_rejoin_starts == 1);
  sdk_waiting_for_input = true;
  retry_at(4200000);
  CHECK(sdk_rejoin_starts == 2);
  CHECK(scheduled_timer->armed);
}

static void test_backoff_is_capped(void) {
  prst_restart_watchdog_start();
  retry_at(600000);
  retry_at(1800000);
  retry_at(4200000);
  retry_at(9000000);
  retry_at(16200000);
  retry_at(23400000);
  CHECK(scheduled_timer->deadline == 30600000);
}

static void test_success_discards_pending_work(void) {
  prst_restart_watchdog_start();
  expire_timer();
  struct k_work* stale_work = queued_work;
  prst_restart_watchdog_stop();
  stale_work->handler(stale_work);
  CHECK(retry_requests == 0);
  CHECK(!scheduled_timer->armed);
}

static void test_late_timer_after_success_cannot_retry(void) {
  prst_restart_watchdog_start();
  prst_restart_watchdog_stop();
  scheduled_timer->expiry(scheduled_timer);
  run_work();
  CHECK(retry_requests == 0);
  CHECK(!scheduled_timer->armed);
}

static void test_restart_resets_backoff_and_rejects_stale_work(void) {
  prst_restart_watchdog_start();
  retry_at(600000);
  expire_timer();
  struct k_work* stale_work = queued_work;
  prst_restart_watchdog_stop();
  now_ms = 2000000;
  prst_restart_watchdog_start();
  stale_work->handler(stale_work);
  CHECK(retry_requests == 1);
  CHECK(scheduled_timer->deadline == 2600000);
  retry_at(2600000);
  CHECK(retry_requests == 2);
  CHECK(scheduled_timer->deadline == 3800000);
}

static void* work_thread(void* unused) {
  (void)unused;
  run_work();
  return NULL;
}

static void* stop_thread(void* unused) {
  (void)unused;
  stopping_thread = true;
  prst_restart_watchdog_stop();
  atomic_store(&stop_returned, true);
  pthread_mutex_lock(&sdk_lock);
  pthread_cond_broadcast(&sdk_condition);
  pthread_mutex_unlock(&sdk_lock);
  return NULL;
}

static void test_stop_serializes_with_running_retry(void) {
  pthread_t worker;
  pthread_t stopper;
  block_sdk = true;
  prst_restart_watchdog_start();
  expire_timer();
  CHECK(pthread_create(&worker, NULL, work_thread, NULL) == 0);
  pthread_mutex_lock(&sdk_lock);
  while (!sdk_entered) pthread_cond_wait(&sdk_condition, &sdk_lock);
  pthread_mutex_unlock(&sdk_lock);
  CHECK(pthread_create(&stopper, NULL, stop_thread, NULL) == 0);
  pthread_mutex_lock(&sdk_lock);
  while (!stop_waiting_for_mutex && !atomic_load(&stop_returned)) {
    pthread_cond_wait(&sdk_condition, &sdk_lock);
  }
  CHECK(!atomic_load(&stop_returned));
  release_sdk = true;
  pthread_cond_broadcast(&sdk_condition);
  pthread_mutex_unlock(&sdk_lock);
  CHECK(pthread_join(worker, NULL) == 0);
  CHECK(pthread_join(stopper, NULL) == 0);
  CHECK(atomic_load(&stop_returned));
  CHECK(!scheduled_timer->armed);
  scheduled_timer->expiry(scheduled_timer);
  run_work();
  CHECK(retry_requests == 1);
}

int main(int argc, char** argv) {
  CHECK(argc == 2);
  struct {
    const char* name;
    void (*run)(void);
  } tests[] = {
      {"duplicate_start", test_duplicate_start_keeps_first_deadline},
      {"noop_and_failure", test_noop_then_failed_rejoins_keep_retrying},
      {"capped_backoff", test_backoff_is_capped},
      {"pending_success", test_success_discards_pending_work},
      {"late_timer", test_late_timer_after_success_cannot_retry},
      {"restart_after_stop", test_restart_resets_backoff_and_rejects_stale_work},
      {"running_stop_race", test_stop_serializes_with_running_retry},
  };
  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    if (strcmp(argv[1], tests[i].name) != 0) continue;
    tests[i].run();
    puts("PASS");
    return 0;
  }
  return 2;
}
