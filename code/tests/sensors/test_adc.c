#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <prstlib/adc.h>

const struct device sensor_test_device = {0};
bool sensor_test_ready = true;
static int enable_error, disable_error, start_error, stop_error;
static int init_failure_call, read_failure_call, conversion_failure_call, setup_failure_call;
static int init_calls, read_calls, conversion_calls, setup_calls;
static int enables, disables, starts, stops;
static bool supply_on, pwm_on, force_saturation;
static unsigned channel;
static enum adc_gain active_gain;

static void reset(void) {
  enable_error = disable_error = start_error = stop_error = 0;
  init_failure_call = read_failure_call = conversion_failure_call = setup_failure_call = 0;
  init_calls = read_calls = conversion_calls = setup_calls = 0;
  enables = disables = starts = stops = 0;
  supply_on = pwm_on = force_saturation = false;
  channel = 0;
  active_gain = ADC_GAIN_1;
}

int gpio_pin_set_dt(const struct gpio_dt_spec *spec, int value) {
  if (spec->pin != 1 && spec->pin != 2) return -90;
  if (value) {
    enables++;
    supply_on = true;
    return enable_error;
  }
  disables++;
  if (!disable_error) supply_on = false;
  return disable_error;
}
int gpio_pin_configure_dt(const struct gpio_dt_spec *spec, int flags) {
  (void)spec; (void)flags; return 0;
}
int pwm_set_dt(const struct pwm_dt_spec *spec, uint32_t period, uint32_t pulse) {
  if (period) {
    if (period != spec->period || pulse != 5) return -91;
    starts++;
    pwm_on = true;
    return start_error;
  }
  if (pulse) return -92;
  stops++;
  if (!stop_error) pwm_on = false;
  return stop_error;
}
int adc_sequence_init_dt(const struct adc_dt_spec *spec, struct adc_sequence *seq) {
  init_calls++;
  channel = spec->channel_cfg.channel_id;
  seq->channels = 1u << channel;
  return init_calls == init_failure_call ? -21 : 0;
}
int adc_read(const struct device *dev, const struct adc_sequence *seq) {
  (void)dev;
  read_calls++;
  if (read_calls == read_failure_call) return -22;
  if (seq->channels != (1u << 1) && !supply_on) return -93;
  if (seq->channels == 1 && !pwm_on) return -94;
  if (seq->channels == (1u << 2)) {
    *(int16_t *)seq->buffer = force_saturation && active_gain == ADC_GAIN_1 ? 4090 : 400;
  } else {
    *(int16_t *)seq->buffer = channel == 1 ? 2800 : 600;
  }
  return 0;
}
int adc_raw_to_millivolts_dt(const struct adc_dt_spec *spec, int32_t *value) {
  (void)spec; (void)value;
  conversion_calls++;
  return conversion_calls == conversion_failure_call ? -23 : 0;
}
int adc_channel_setup_dt(const struct adc_dt_spec *spec) { (void)spec; return 0; }
int adc_channel_setup(const struct device *dev, const struct adc_channel_cfg *cfg) {
  (void)dev;
  setup_calls++;
  active_gain = cfg->gain;
  return setup_calls == setup_failure_call ? -24 : 0;
}
uint16_t adc_ref_internal(const struct device *dev) { (void)dev; return 600; }
int adc_gain_invert(enum adc_gain gain, int32_t *value) {
  if (gain == ADC_GAIN_1_6) *value *= 6;
  conversion_calls++;
  return conversion_calls == conversion_failure_call ? -23 : 0;
}

static int report(const char *name, bool ok, int result, bool output_ok) {
  printf("%s ADC-%s %s (result=%d, stops=%d, disables=%d, output_ok=%d)\n",
      ok ? "PASS" : "FAIL", TEST_PHOTOTRANSISTOR ? "photo" : "LDR", name,
      result, stops, disables, output_ok);
  return !ok;
}

static int soil_tests(void) {
  const struct {
    const char *name;
    int enable, start, init_call, read_call, convert_call, stop, disable, result;
  } cases[] = {
    {"soil success", 0, 0, 0, 0, 0, 0, 0, 0},
    {"soil enable failure cleans up", -11, 0, 0, 0, 0, 0, 0, -11},
    {"soil PWM start failure cleans up", 0, -12, 0, 0, 0, 0, 0, -12},
    {"soil ADC init failure", 0, 0, 1, 0, 0, 0, 0, -21},
    {"soil ADC failure preserves output", 0, 0, 0, 4, 0, 0, 0, -22},
    {"soil conversion failure preserves output", 0, 0, 0, 0, 4, 0, 0, -23},
    {"soil final conversion failure preserves output", 0, 0, 0, 0, 33, 0, 0, -23},
    {"soil PWM stop failure still disables discharge", 0, 0, 0, 0, 0, -13, 0, -13},
    {"soil discharge disable failure preserves output", 0, 0, 0, 0, 0, 0, -14, -14},
    {"soil ADC error wins over cleanup errors", 0, 0, 0, 4, 0, -13, -14, -22},
    {"soil start error wins over cleanup errors", 0, -12, 0, 0, 0, -13, -14, -12},
    {"soil first cleanup error wins", 0, 0, 0, 0, 0, -13, -14, -13},
  };
  int failures = 0;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    reset();
    enable_error = cases[i].enable; start_error = cases[i].start;
    init_failure_call = cases[i].init_call; read_failure_call = cases[i].read_call;
    conversion_failure_call = cases[i].convert_call;
    stop_error = cases[i].stop; disable_error = cases[i].disable;
    prst_adc_soil_moisture_t out;
    memset(&out, 0x5a, sizeof(out));
    prst_adc_soil_moisture_t before = out;
    int result = prst_adc_soil_read(3.0f, &out);
    bool output_ok = result ? memcmp(&out, &before, sizeof(out)) == 0 :
        out.adc_read.raw == 600 && out.adc_read.millivolts == 600 &&
        fabsf(out.adc_read.voltage - 0.6f) < 0.0001f &&
        fabsf(out.percentage - 0.5f) < 0.0001f && fabsf(out.unclamped_percentage - 0.5f) < 0.0001f;
    bool ok = result == cases[i].result && output_ok && stops == 1 && disables == 1 &&
        (!pwm_on || stop_error) && (!supply_on || disable_error);
    failures += report(cases[i].name, ok, result, output_ok);
  }
  return failures;
}

static int battery_tests(void) {
  const struct { const char *name; int read_call, conversion_call, result; } cases[] = {
    {"battery success", 0, 0, 0},
    {"battery ADC failure preserves output", 4, 0, -22},
    {"battery conversion failure preserves output", 0, 4, -23},
    {"battery final conversion failure preserves output", 0, 17, -23},
  };
  int failures = 0;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    reset();
    read_failure_call = cases[i].read_call; conversion_failure_call = cases[i].conversion_call;
    prst_batt_t out;
    memset(&out, 0x5a, sizeof(out));
    prst_batt_t before = out;
    int result = prst_adc_batt_read(&out);
    bool output_ok = result ? memcmp(&out, &before, sizeof(out)) == 0 :
        out.adc_read.raw == 2800 && out.adc_read.millivolts == 2800 &&
        fabsf(out.adc_read.voltage - 2.8f) < 0.0001f && fabsf(out.percentage - 0.27f) < 0.0001f;
    failures += report(cases[i].name, result == cases[i].result && output_ok, result, output_ok);
  }
  return failures;
}

static int photo_tests(void) {
  const struct {
    const char *name;
    int enable, read_call, convert_call, setup_call, disable, result;
    bool saturate;
  } cases[] = {
    {"light success", 0, 0, 0, 0, 0, 0, false},
    {"light enable failure powers off", -11, 0, 0, 0, 0, -11, false},
    {"light read failure powers off", 0, 1, 0, 0, 0, -22, false},
    {"light conversion failure powers off", 0, 0, 1, 0, 0, -23, false},
    {"light disable failure preserves output", 0, 0, 0, 0, -14, -14, false},
    {"light read error wins over disable error", 0, 1, 0, 0, -14, -22, false},
#if TEST_PHOTOTRANSISTOR
    {"light setup failure powers off", 0, 0, 0, 1, 0, -24, false},
    {"light saturation retries lower gain", 0, 0, 0, 0, 0, 0, true},
    {"light lower gain failure powers off", 0, 0, 0, 2, 0, -24, true},
    {"light lower gain read failure preserves output", 0, 24, 0, 0, 0, -22, true},
#endif
  };
  int failures = 0;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    reset();
    enable_error = cases[i].enable; read_failure_call = cases[i].read_call;
    conversion_failure_call = cases[i].convert_call; setup_failure_call = cases[i].setup_call;
    disable_error = cases[i].disable; force_saturation = cases[i].saturate;
    prst_adc_photo_sensor_t out;
    memset(&out, 0x5a, sizeof(out));
    prst_adc_photo_sensor_t before = out;
    int result = prst_adc_photo_read(3.0f, &out);
    bool output_ok = result ? memcmp(&out, &before, sizeof(out)) == 0 :
        out.adc_read.raw == 400 &&
#if TEST_PHOTOTRANSISTOR
        out.adc_read.millivolts == (cases[i].saturate ? 351 : 58) &&
        out.brightness == (cases[i].saturate ? 3515 : 585) &&
        setup_calls == (cases[i].saturate ? 2 : 1);
#else
        out.adc_read.millivolts == 400 && fabsf(out.adc_read.voltage - 0.4f) < 0.0001f && out.brightness == 0;
#endif
    bool ok = result == cases[i].result && output_ok && disables == 1 && (!supply_on || disable_error);
    failures += report(cases[i].name, ok, result, output_ok);
  }
  return failures;
}

int main(void) {
  int failures = soil_tests() + battery_tests() + photo_tests();
  return failures ? 1 : 0;
}
