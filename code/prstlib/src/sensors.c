#include "prstlib/sensors.h"

#include <zephyr/logging/log.h>

#include "prstlib/adc.h"
#include "prstlib/led.h"
#include "prstlib/macros.h"

LOG_MODULE_REGISTER(sensors, CONFIG_PRSTLIB_LOG_LEVEL);

int prst_sensors_read_all(prst_sensors_t* sensors) {
  int failed = 0;
  if (prst_adc_batt_read(&sensors->batt)) {
    failed |= PRST_SENSOR_BATT;
  }
  // The soil reading is compensated with the battery voltage, so it cannot be trusted without one.
  if ((failed & PRST_SENSOR_BATT) ||
      prst_adc_soil_read(sensors->batt.adc_read.voltage, &sensors->soil)) {
    failed |= PRST_SENSOR_SOIL;
  }
  if (prst_adc_photo_read(sensors->batt.adc_read.voltage, &sensors->photo)) {
    failed |= PRST_SENSOR_PHOTO;
  }
  if (prst_shtc3_read(&sensors->shtc3)) {
    failed |= PRST_SENSOR_SHTC3;
  }
  if (failed) {
    LOG_ERR("Failed sensor reads: 0x%x", failed);
  }

  LOG_DBG("Batt: %d mV (%.2f%%)", sensors->batt.adc_read.millivolts,
          DOUBLE_PROMO_OK(100 * sensors->batt.percentage));
  LOG_DBG("Soil: %.0f %%", DOUBLE_PROMO_OK(100 * sensors->soil.percentage));
  LOG_DBG("Photo: %u lx (%d mV)", sensors->photo.brightness,
          sensors->photo.adc_read.millivolts);
  LOG_DBG("Temp: %f oC", DOUBLE_PROMO_OK(sensors->shtc3.temp_c));
  LOG_DBG("Humi: %.0f %%", DOUBLE_PROMO_OK(100 * sensors->shtc3.rel_humi));
  LOG_DBG("--------------------------------------------------");

  return failed;
}