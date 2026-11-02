#ifndef RECOVERY_TEST_KERNEL_H
#define RECOVERY_TEST_KERNEL_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef int64_t k_timeout_t;
#define K_SECONDS(value) ((int64_t)(value) * 1000)
#define K_MSEC(value) ((int64_t)(value))
#define K_FOREVER (-1)

struct k_work {
  void (*handler)(struct k_work*);
  bool pending;
};
struct k_timer {
  void (*expiry)(struct k_timer*);
  int64_t deadline;
  bool armed;
};
struct k_mutex {
  pthread_mutex_t native;
};

#define K_WORK_DEFINE(name, handler_fn) struct k_work name = {.handler = handler_fn}
#define K_TIMER_DEFINE(name, expiry_fn, stop_fn) \
  struct k_timer name = {.expiry = expiry_fn}
#define K_MUTEX_DEFINE(name) struct k_mutex name = {.native = PTHREAD_MUTEX_INITIALIZER}

int k_work_submit(struct k_work* work);
int k_work_cancel(struct k_work* work);
void k_timer_start(struct k_timer* timer, k_timeout_t duration, k_timeout_t period);
void k_timer_stop(struct k_timer* timer);
int k_mutex_lock(struct k_mutex* mutex, k_timeout_t timeout);
int k_mutex_unlock(struct k_mutex* mutex);
int64_t k_uptime_get(void);

#endif
