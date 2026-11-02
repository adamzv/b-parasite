#pragma once
#include <errno.h>
#include <stdbool.h>
struct k_work {
  void (*handler)(struct k_work*);
  bool pending;
};
#define K_WORK_DEFINE(name, fn) struct k_work name = {.handler = fn}
int k_work_submit(struct k_work*);
