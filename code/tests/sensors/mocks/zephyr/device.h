#pragma once
#include <stdbool.h>
struct device {
  int id;
};
extern const struct device sensor_test_device;
extern bool sensor_test_ready;
static inline bool device_is_ready(const struct device* dev) {
  return dev != 0 && sensor_test_ready;
}
