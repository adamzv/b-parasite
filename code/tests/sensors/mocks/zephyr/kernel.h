#pragma once
#include <stddef.h>
#include <stdint.h>
#define BIT(n) (1UL << (n))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
static inline void k_msleep(int ms) { (void)ms; }
static inline void k_busy_wait(unsigned us) { (void)us; }
