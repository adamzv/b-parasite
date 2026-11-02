#pragma once
#include <stdatomic.h>
typedef atomic_int atomic_t;
static inline bool atomic_cas(atomic_t *target, int old_value, int new_value) {
  return atomic_compare_exchange_strong(target, &old_value, new_value);
}
static inline int atomic_clear(atomic_t *target) { return atomic_exchange(target, 0); }
