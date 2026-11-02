#pragma once
#include <stdint.h>
#include <zephyr/device.h>
struct pwm_dt_spec {
  const struct device* dev;
  uint32_t period;
};
#define PWM_DT_SPEC_GET(node) {.dev = &sensor_test_device, .period = 10}
int pwm_set_dt(const struct pwm_dt_spec*, uint32_t, uint32_t);
