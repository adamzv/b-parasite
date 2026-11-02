#pragma once
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
enum adc_gain { ADC_GAIN_1, ADC_GAIN_1_6 };
struct adc_channel_cfg { enum adc_gain gain; uint8_t channel_id; };
struct adc_dt_spec {
  const struct device *dev;
  struct adc_channel_cfg channel_cfg;
};
struct adc_sequence {
  void *buffer;
  size_t buffer_size;
  uint32_t channels;
  uint8_t resolution;
};
#define ADC_DT_SPEC_GET_BY_IDX(node, index) { .dev = &sensor_test_device, .channel_cfg = { .channel_id = index } }
#define ADC_DT_SPEC_GET(node) ADC_DT_SPEC_GET_BY_IDX(node, 2)
int adc_sequence_init_dt(const struct adc_dt_spec *, struct adc_sequence *);
int adc_read(const struct device *, const struct adc_sequence *);
int adc_raw_to_millivolts_dt(const struct adc_dt_spec *, int32_t *);
int adc_channel_setup_dt(const struct adc_dt_spec *);
int adc_channel_setup(const struct device *, const struct adc_channel_cfg *);
uint16_t adc_ref_internal(const struct device *);
int adc_gain_invert(enum adc_gain, int32_t *);
