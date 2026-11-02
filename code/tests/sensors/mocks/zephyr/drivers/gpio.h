#pragma once
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#define GPIO_OUTPUT 1
struct gpio_dt_spec {
  const struct device* port;
  int pin;
};
#define PIN_fast_disch 1
#define PIN_photo_transistor_enable 2
#define PIN_ldr_enable 2
#define GPIO_DT_SPEC_GET(node, property) {.port = &sensor_test_device, .pin = DT_CAT(PIN_, node)}
int gpio_pin_set_dt(const struct gpio_dt_spec*, int);
int gpio_pin_configure_dt(const struct gpio_dt_spec*, int);
