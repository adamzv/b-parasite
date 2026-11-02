#ifndef _PRST_DEBUG_COUNTERS_H_
#define _PRST_DEBUG_COUNTERS_H_

#include <stdint.h>

typedef uint32_t prst_debug_counter_t;

typedef void (*prst_debug_counters_callback_t)(const char* counter_name, prst_debug_counter_t value);

int prst_debug_counters_init();

int prst_debug_counters_increment(const char* counter_name);

// Copies a name (up to 64 bytes) without FS access or waiting; safe from ISR/Zigbee context.
// A submit failure retains the request; the next valid call retries scheduling it.
int prst_debug_counters_increment_async(const char* counter_name);

int prst_debug_counters_get(const char* counter_name, prst_debug_counter_t* value);

int prst_debug_counters_get_all(prst_debug_counters_callback_t callback);

#endif  // _PRST_DEBUG_COUNTERS_H_