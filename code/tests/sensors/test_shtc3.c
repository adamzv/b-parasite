#include <math.h>
#include <prstlib/shtc3.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/i2c.h>

const struct device sensor_test_device = {0};
bool sensor_test_ready = true;
static int wake_error, measure_error, read_error, sleep_error;
static int wakes, measures, reads, sleeps;
static bool corrupt_temperature, corrupt_humidity;

int i2c_write_dt(const struct i2c_dt_spec* spec, const uint8_t* data, size_t size) {
  (void)spec;
  if (size != 2) return -90;
  unsigned command = ((unsigned)data[0] << 8) | data[1];
  switch (command) {
    case PRST_SHTC3_CMD_WAKEUP:
      wakes++;
      return wake_error;
    case PRST_SHTC3_CMD_MEASURE_TFIRST_NORMAL:
      measures++;
      return measure_error;
    case PRST_SHTC3_CMD_SLEEP:
      sleeps++;
      return sleep_error;
    default:
      return -91;
  }
}

int i2c_read_dt(const struct i2c_dt_spec* spec, uint8_t* data, size_t size) {
  (void)spec;
  reads++;
  if (read_error) return read_error;
  const uint8_t response[] = {0xbe, 0xef, 0x92, 0xbe, 0xef, 0x92};
  if (size != sizeof(response)) return -92;
  memcpy(data, response, size);
  if (corrupt_temperature) data[2] ^= 1;
  if (corrupt_humidity) data[5] ^= 1;
  return 0;
}

int main(void) {
  const struct {
    const char* name;
    bool ready;
    int wake, measure, read, sleep;
    bool bad_temp, bad_humi;
    int result, want_wakes, want_measures, want_reads, want_sleeps;
  } cases[] = {
      {"success", true, 0, 0, 0, 0, false, false, 0, 1, 1, 1, 1},
      {"not ready", false, 0, 0, 0, 0, false, false, 1, 0, 0, 0, 0},
      {"wake NACK still sleeps", true, -5, 0, 0, 0, false, false, -5, 1, 0, 0, 1},
      {"measurement NACK still sleeps", true, 0, -6, 0, 0, false, false, -6, 1, 1, 0, 1},
      {"read NACK still sleeps", true, 0, 0, -7, 0, false, false, -7, 1, 1, 1, 1},
      {"sleep failure preserves output", true, 0, 0, 0, -8, false, false, -8, 1, 1, 1, 1},
      {"wake error wins over sleep error", true, -5, 0, 0, -8, false, false, -5, 1, 0, 0, 1},
      {"measurement error wins over sleep error", true, 0, -6, 0, -8, false, false, -6, 1, 1, 0, 1},
      {"read error wins over sleep error", true, 0, 0, -7, -8, false, false, -7, 1, 1, 1, 1},
      {"temperature CRC rejection", true, 0, 0, 0, 0, true, false, 1, 1, 1, 1, 1},
      {"humidity CRC rejection", true, 0, 0, 0, 0, false, true, 1, 1, 1, 1, 1},
      {"CRC error wins over sleep error", true, 0, 0, 0, -8, true, false, 1, 1, 1, 1, 1},
  };
  int failures = 0;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    sensor_test_ready = cases[i].ready;
    wake_error = cases[i].wake;
    measure_error = cases[i].measure;
    read_error = cases[i].read;
    sleep_error = cases[i].sleep;
    corrupt_temperature = cases[i].bad_temp;
    corrupt_humidity = cases[i].bad_humi;
    wakes = measures = reads = sleeps = 0;
    prst_shtc3_read_t out = {123, 456}, before = out;
    int result = prst_shtc3_read(&out);
    bool output_ok = result ? memcmp(&out, &before, sizeof(out)) == 0 : fabsf(out.temp_c - 85.521011f) < 0.0001f && fabsf(out.rel_humi - 0.74584573f) < 0.00001f;
    bool ok = result == cases[i].result && output_ok && wakes == cases[i].want_wakes &&
              measures == cases[i].want_measures && reads == cases[i].want_reads && sleeps == cases[i].want_sleeps;
    printf("%s SHTC3 %s (result=%d, sleeps=%d, output_ok=%d)\n", ok ? "PASS" : "FAIL", cases[i].name, result, sleeps, output_ok);
    failures += !ok;
  }
  return failures ? 1 : 0;
}
