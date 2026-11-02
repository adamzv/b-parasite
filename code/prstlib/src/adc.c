#include "prstlib/adc.h"

#include <math.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "prstlib/macros.h"

LOG_MODULE_REGISTER(adc, CONFIG_PRSTLIB_LOG_LEVEL);

// PWM spec for square wave. Input to the soil sensing circuit.
static const struct pwm_dt_spec soil_pwm_dt =
    PWM_DT_SPEC_GET(DT_NODELABEL(soil_pwm));
static const uint32_t pulse = DT_PROP(DT_NODELABEL(soil_pwm), pulse);

// Calibration coefficients for the soil sensing circuit.
static const int dry_coeffs[3] = DT_PROP(DT_NODELABEL(soil_calibration_coeffs), dry);
static const int wet_coeffs[3] = DT_PROP(DT_NODELABEL(soil_calibration_coeffs), wet);

struct gpio_dt_spec fast_disch_dt =
    GPIO_DT_SPEC_GET(DT_NODELABEL(fast_disch), gpios);

// Shared buffer and adc_sequennce.
static int16_t buf;
static struct adc_sequence sequence = {
    .buffer = &buf,
    .buffer_size = sizeof(buf),
};

static const struct adc_dt_spec adc_soil_spec =
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

static const struct adc_dt_spec adc_batt_spec =
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

#if DT_NODE_EXISTS(DT_NODELABEL(photo_transistor))

static const struct adc_dt_spec adc_photo_transistor_spec =
    ADC_DT_SPEC_GET(DT_NODELABEL(photo_transistor));
struct gpio_dt_spec photo_transistor_enable_dt =
    GPIO_DT_SPEC_GET(DT_NODELABEL(photo_transistor_enable), gpios);

#elif DT_NODE_EXISTS(DT_NODELABEL(ldr))

static const struct adc_dt_spec adc_ldr_spec =
    ADC_DT_SPEC_GET(DT_NODELABEL(ldr));
struct gpio_dt_spec ldr_enable_dt =
    GPIO_DT_SPEC_GET(DT_NODELABEL(ldr_enable), gpios);

#endif

typedef struct {
  // High (h) and low (p) voltage (v) and % (p) points.
  float vh, vl, ph, pl;
} batt_disch_linear_section_t;

static void set_battery_percent(const prst_adc_read_t* read, prst_batt_t* out) {
  // Must be sorted by .vh.
  static const batt_disch_linear_section_t sections[] = {
      {.vh = 3.00f, .vl = 2.90f, .ph = 1.00f, .pl = 0.42f},
      {.vh = 2.90f, .vl = 2.74f, .ph = 0.42f, .pl = 0.18f},
      {.vh = 2.74f, .vl = 2.44f, .ph = 0.18f, .pl = 0.06f},
      {.vh = 2.44f, .vl = 2.01f, .ph = 0.06f, .pl = 0.00f},
  };

  const float v = read->voltage;

  if (v > sections[0].vh) {
    out->percentage = 1.0f;
    return;
  }
  for (int i = 0; i < ARRAY_SIZE(sections); i++) {
    const batt_disch_linear_section_t* s = &sections[i];
    if (v > s->vl) {
      out->percentage = s->pl + (v - s->vl) * ((s->ph - s->pl) / (s->vh - s->vl));
      return;
    }
  }
  out->percentage = 0.0f;
  return;
}

static inline float eval_poly(const int coeffs[3], float x) {
  // The coefficients are specified times 1000, as a workaround the lack of support for floating
  // points in devicetree bindings.
  return (coeffs[0] + coeffs[1] * x + coeffs[2] * x * x) / 1000.0f;
}

static inline float get_soil_moisture_percent(float battery_voltage, float raw_adc_output) {
  const float x = battery_voltage;
  const float dry = eval_poly(dry_coeffs, x);
  const float wet = eval_poly(wet_coeffs, x);
  const float percent = (raw_adc_output - dry) / (wet - dry);
  LOG_DBG("Read soil moisture 2: %.2f | Raw %.1f | Batt: %.2f | Dry: %.2f | Wet: %.2f",
          DOUBLE_PROMO_OK(100 * percent),
          DOUBLE_PROMO_OK(raw_adc_output), DOUBLE_PROMO_OK(x), DOUBLE_PROMO_OK(dry), DOUBLE_PROMO_OK(wet));
  return percent;
}

static int read_adc_spec(const struct adc_dt_spec* spec, prst_adc_read_t* out) {
  RET_IF_ERR(adc_sequence_init_dt(spec, &sequence));

  RET_IF_ERR(adc_read(spec->dev, &sequence));

  int32_t val_mv = buf;
  RET_IF_ERR(adc_raw_to_millivolts_dt(spec, &val_mv));
  val_mv = MAX(0, val_mv);

  out->raw = buf;
  out->millivolts = val_mv;
  out->voltage = val_mv / 1000.0f;
  return 0;
}

// Spreads the samples over a few milliseconds so noise that is slower than a single conversion
// (PWM ripple, supply droop) also averages out. Returns the mean raw value.
static int read_adc_spec_avg(const struct adc_dt_spec* spec, int samples, int spacing_us,
                             prst_adc_read_t* out, float* mean_raw) {
  int32_t sum = 0;
  prst_adc_read_t sample;
  for (int i = 0; i < samples; i++) {
    RET_IF_ERR(read_adc_spec(spec, &sample));
    sum += sample.raw;
    k_busy_wait(spacing_us);
  }
  const float mean = (float)sum / samples;
  int32_t raw = (int32_t)lroundf(mean);
  int32_t val_mv = raw;
  RET_IF_ERR(adc_raw_to_millivolts_dt(spec, &val_mv));
  val_mv = MAX(0, val_mv);
  out->raw = raw;
  out->millivolts = val_mv;
  out->voltage = val_mv / 1000.0f;
  *mean_raw = mean;
  return 0;
}

int prst_adc_init() {
  RET_IF_ERR(adc_channel_setup_dt(&adc_soil_spec));
  RET_IF_ERR(adc_channel_setup_dt(&adc_batt_spec));

  RET_IF_ERR(!device_is_ready(fast_disch_dt.port));
  RET_IF_ERR(gpio_pin_configure_dt(&fast_disch_dt, GPIO_OUTPUT));

#if DT_NODE_EXISTS(DT_NODELABEL(photo_transistor))
  RET_IF_ERR(adc_channel_setup_dt(&adc_photo_transistor_spec));
  RET_IF_ERR(!device_is_ready(photo_transistor_enable_dt.port));
  RET_IF_ERR(gpio_pin_configure_dt(&photo_transistor_enable_dt, GPIO_OUTPUT));
#elif DT_NODE_EXISTS(DT_NODELABEL(ldr))
  RET_IF_ERR(adc_channel_setup_dt(&adc_ldr_spec));
  RET_IF_ERR(!device_is_ready(ldr_enable_dt.port));
  RET_IF_ERR(gpio_pin_configure_dt(&ldr_enable_dt, GPIO_OUTPUT));
#endif

  for (size_t idx = 0; idx < ARRAY_SIZE(dry_coeffs); idx++) {
    LOG_DBG("Dry coeff %d: %d\n", idx, dry_coeffs[idx]);
  }
  for (size_t idx = 0; idx < ARRAY_SIZE(wet_coeffs); idx++) {
    LOG_DBG("Wet coeff %d: %d\n", idx, wet_coeffs[idx]);
  }
  return 0;
}

// The battery voltage feeds the soil polynomials, so its noise ends up in the soil reading too.
#define BATT_SAMPLES 16
#define BATT_SAMPLE_SPACING_US 100
#define SOIL_SAMPLES 32
#define SOIL_SAMPLE_SPACING_US 100

int prst_adc_batt_read(prst_batt_t* out) {
  float mean_raw;
  RET_IF_ERR(read_adc_spec_avg(&adc_batt_spec, BATT_SAMPLES, BATT_SAMPLE_SPACING_US,
                               &out->adc_read, &mean_raw));
  set_battery_percent(&out->adc_read, out);
  return 0;
}

int prst_adc_soil_read(float battery_voltage, prst_adc_soil_moisture_t* out) {
  prst_adc_read_t sample;
  float mean_raw;
  int stop_err, disable_err;
  int read_err = gpio_pin_set_dt(&fast_disch_dt, 1);
  if (read_err) {
    goto cleanup;
  }
  read_err = pwm_set_dt(&soil_pwm_dt, soil_pwm_dt.period, pulse);
  if (read_err) {
    goto cleanup;
  }
  k_msleep(30);
  read_err = read_adc_spec_avg(&adc_soil_spec, SOIL_SAMPLES, SOIL_SAMPLE_SPACING_US,
                               &sample, &mean_raw);
cleanup:
  // Driver errors do not guarantee the power control stayed unchanged.
  stop_err = pwm_set_dt(&soil_pwm_dt, 0, 0);
  disable_err = gpio_pin_set_dt(&fast_disch_dt, 0);
  RET_IF_ERR(read_err);
  RET_IF_ERR(stop_err);
  RET_IF_ERR(disable_err);
  out->adc_read = sample;
  out->unclamped_percentage = get_soil_moisture_percent(battery_voltage, mean_raw);
  out->percentage = MAX(0.0f, MIN(1.0f, out->unclamped_percentage));
  return 0;
}

#if DT_NODE_EXISTS(DT_NODELABEL(photo_transistor))
// 20 samples 0.5 ms apart span one 100 Hz mains-flicker period, so artificial light averages out.
#define PHOTO_SAMPLES 20
#define PHOTO_SAMPLE_SPACING_US 500
#define PHOTO_RESOLUTION 12
#define PHOTO_SATURATED_RAW (BIT(PHOTO_RESOLUTION) - 16)

// Reads the averaged phototransistor voltage at the given gain. Indoor light only produces tens of
// millivolts across the load resistor, so the devicetree's 3.6 V range would resolve ~50 lx per step.
static int read_photo_mv(enum adc_gain gain, float* out_mv, bool* saturated) {
  struct adc_channel_cfg cfg = adc_photo_transistor_spec.channel_cfg;
  cfg.gain = gain;
  RET_IF_ERR(adc_channel_setup(adc_photo_transistor_spec.dev, &cfg));
  const struct adc_sequence seq = {
      .channels = BIT(cfg.channel_id),
      .buffer = &buf,
      .buffer_size = sizeof(buf),
      .resolution = PHOTO_RESOLUTION,
  };
  int32_t sum = 0;
  int16_t max_raw = 0;
  for (int i = 0; i < PHOTO_SAMPLES; i++) {
    RET_IF_ERR(adc_read(adc_photo_transistor_spec.dev, &seq));
    sum += MAX(buf, 0);
    max_raw = MAX(max_raw, buf);
    k_busy_wait(PHOTO_SAMPLE_SPACING_US);
  }
  int32_t full_scale_mv = adc_ref_internal(adc_photo_transistor_spec.dev);
  RET_IF_ERR(adc_gain_invert(gain, &full_scale_mv));
  *out_mv = (float)sum / PHOTO_SAMPLES * full_scale_mv / BIT(PHOTO_RESOLUTION);
  *saturated = max_raw >= PHOTO_SATURATED_RAW;
  return 0;
}
#endif

int prst_adc_photo_read(float battery_voltage, prst_adc_photo_sensor_t* out) {
#if DT_NODE_EXISTS(DT_NODELABEL(photo_transistor))
  float mv;
  bool saturated;
  int read_err = gpio_pin_set_dt(&photo_transistor_enable_dt, 1);
  if (!read_err) {
    k_msleep(10);
    // The 0.6 V range covers up to several thousand lux; direct sun needs the 3.6 V range.
    read_err = read_photo_mv(ADC_GAIN_1, &mv, &saturated);
    if (!read_err && saturated) {
      read_err = read_photo_mv(ADC_GAIN_1_6, &mv, &saturated);
    }
  }
  int disable_err = gpio_pin_set_dt(&photo_transistor_enable_dt, 0);
  RET_IF_ERR(read_err);
  RET_IF_ERR(disable_err);
  out->adc_read.raw = buf;
  out->adc_read.millivolts = (int32_t)mv;
  out->adc_read.voltage = mv / 1000.0f;
  const float phototransistor_resistor = DT_PROP(DT_NODELABEL(photo_transistor), output_ohms);
  const float photocurrent_na = 1e6f * mv / phototransistor_resistor;
  out->brightness =
      MAX(0, MIN(photocurrent_na / CONFIG_PRSTLIB_PHOTO_NA_PER_LUX, UINT16_MAX));
  LOG_DBG("Read phototransistor: %u lx | %.2f V", out->brightness, DOUBLE_PROMO_OK(out->adc_read.voltage));

#elif DT_NODE_EXISTS(DT_NODELABEL(ldr))
  prst_adc_read_t sample;
  int read_err = gpio_pin_set_dt(&ldr_enable_dt, 1);
  if (!read_err) {
    k_msleep(10);
    read_err = read_adc_spec(&adc_ldr_spec, &sample);
  }
  int disable_err = gpio_pin_set_dt(&ldr_enable_dt, 0);
  RET_IF_ERR(read_err);
  RET_IF_ERR(disable_err);
  out->adc_read = sample;
  // The photo resistor forms a voltage divider with R.
  // The voltage here is measured in the middle of the voltage divider.
  // Vcc ---- (R_photo) ---|--- (R) ---- GND
  //                      Vout
  // So we can estimate R_photo = R * (Vcc - Vout) / Vout
  const float r = DT_PROP(DT_NODELABEL(ldr), output_ohms);
  const float photo_resistance =
      r * (battery_voltage - out->adc_read.voltage) / out->adc_read.voltage;
  // The relationship between the LDR resistance and the lux level is
  // logarithmic. We need to solve a logarithmic equation to find the lux
  // level, given the LDR resistance we just measured.
  // These values work for the GL5528 LDR and were borrowed from
  // https://github.com/QuentinCG/Arduino-Light-Dependent-Resistor-Library.
  const float mult_value = 32017200.0f;
  const float pow_value = 1.5832f;
  out->brightness =
      MAX(0, MIN(mult_value / powf(photo_resistance, pow_value), UINT16_MAX));
  LOG_DBG("Read LDR: %u lx | %.2f V", out->brightness, DOUBLE_PROMO_OK(out->adc_read.voltage));

#endif

  return 0;
}
