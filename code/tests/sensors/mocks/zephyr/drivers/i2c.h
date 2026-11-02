#pragma once
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>
struct i2c_dt_spec {
  const struct device* bus;
};
#define DT_NODELABEL(name) name
#define I2C_DT_SPEC_GET(node) {.bus = &sensor_test_device}
int i2c_write_dt(const struct i2c_dt_spec*, const uint8_t*, size_t);
int i2c_read_dt(const struct i2c_dt_spec*, uint8_t*, size_t);
