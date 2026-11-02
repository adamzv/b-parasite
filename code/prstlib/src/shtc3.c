#include "prstlib/shtc3.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "prstlib/macros.h"

LOG_MODULE_REGISTER(shtc3, CONFIG_PRSTLIB_LOG_LEVEL);

static const struct i2c_dt_spec shtc3 = I2C_DT_SPEC_GET(DT_NODELABEL(shtc3));

static uint8_t buff[6];

static int write_cmd(uint16_t command) {
  static uint8_t cmd[2];
  cmd[0] = command >> 8;
  cmd[1] = command & 0xff;
  RET_IF_ERR(i2c_write_dt(&shtc3, cmd, sizeof(cmd)));
  return 0;
}

// CRC-8 from the datasheet: polynomial 0x31, init 0xFF, over each 2-byte word.
static uint8_t crc8(const uint8_t* data) {
  uint8_t crc = 0xff;
  for (int i = 0; i < 2; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = crc & 0x80 ? (crc << 1) ^ 0x31 : crc << 1;
    }
  }
  return crc;
}

int prst_shtc3_read(prst_shtc3_read_t* out) {
  RET_IF_ERR_MSG(!device_is_ready(shtc3.bus), "SHTC3 is not ready");

  int sleep_err;
  int read_err = write_cmd(PRST_SHTC3_CMD_WAKEUP);
  k_msleep(1);
  if (read_err) {
    goto sleep;
  }

  read_err = write_cmd(PRST_SHTC3_CMD_MEASURE_TFIRST_NORMAL);
  if (read_err) {
    goto sleep;
  }

  // Normal mode conversion can take up to 12.1 ms, according to the datasheet.
  k_msleep(20);
  read_err = i2c_read_dt(&shtc3, buff, sizeof(buff));
  if (!read_err && (crc8(&buff[0]) != buff[2] || crc8(&buff[3]) != buff[5])) {
    LOG_ERR("SHTC3 CRC mismatch");
    read_err = 1;
  }

sleep:
  // A failed wake command can still have reached the sensor.
  sleep_err = write_cmd(PRST_SHTC3_CMD_SLEEP);
  RET_IF_ERR(read_err);
  RET_IF_ERR(sleep_err);

  out->temp_c = -45 + 175 * ((float)((buff[0] << 8) | buff[1])) / (1 << 16);
  out->rel_humi = ((float)((buff[3] << 8) | buff[4])) / UINT16_MAX;

  LOG_DBG("Read temp: %f oC (%d)", DOUBLE_PROMO_OK(out->temp_c), (int)out->temp_c);
  LOG_DBG("Read humi: %.0f %%", DOUBLE_PROMO_OK(100.0f * out->rel_humi));
  return 0;
}