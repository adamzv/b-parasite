#ifndef _PRST_DATA_H_
#define _PRST_DATA_H_

#include "prstlib/adc.h"
#include "prstlib/shtc3.h"

#include <zephyr/sys/util.h>

typedef struct {
  prst_adc_soil_moisture_t soil;
  prst_adc_photo_sensor_t photo;
  prst_batt_t batt;
  prst_shtc3_read_t shtc3;
} prst_sensors_t;

// Bits returned by prst_sensors_read_all for the sensors whose read failed.
#define PRST_SENSOR_BATT BIT(0)
#define PRST_SENSOR_SOIL BIT(1)
#define PRST_SENSOR_PHOTO BIT(2)
#define PRST_SENSOR_SHTC3 BIT(3)

// Reads every sensor even when one fails, so a dead sensor does not take the others down. Returns the
// PRST_SENSOR_* bits of the failed reads (0 when all succeeded); failed fields keep their previous values.
int prst_sensors_read_all(prst_sensors_t *out);

#endif  // _PRST_DATA_H_
