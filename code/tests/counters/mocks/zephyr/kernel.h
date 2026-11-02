#pragma once
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#define K_NO_WAIT 0
struct k_work { void (*handler)(struct k_work *); bool pending; };
#define K_WORK_DEFINE(name, fn) struct k_work name = { .handler = fn }
struct k_msgq {
  char *buffer;
  size_t message_size, capacity, count, head;
  pthread_mutex_t lock;
};
#define K_MSGQ_DEFINE(name, size, depth, align) \
  static char name##_storage[(size) * (depth)]; \
  struct k_msgq name = { .buffer = name##_storage, .message_size = size, \
    .capacity = depth, .lock = PTHREAD_MUTEX_INITIALIZER }
int k_work_submit(struct k_work *);
int k_msgq_put(struct k_msgq *, const void *, int);
int k_msgq_get(struct k_msgq *, void *, int);
