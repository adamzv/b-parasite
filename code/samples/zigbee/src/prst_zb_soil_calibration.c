#include "prst_zb_soil_calibration.h"

#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(soil_calibration, CONFIG_LOG_DEFAULT_LEVEL);

#define CALIBRATION_FILE "/lfs/soil_cal"

typedef struct {
  int16_t dry;
  int16_t wet;
} stored_calibration_t;

static stored_calibration_t pending;

// The live attributes, so a write can be checked against the other point.
static const prst_soil_calibration_attrs_t* current;

// ZBOSS stores a written value before the device callback runs and ignores that callback's result; only this
// hook can turn an invalid dry/wet pair into an INVALID_VALUE response instead of accepting it.
static zb_ret_t check_value(zb_uint16_t attr_id, zb_uint8_t endpoint, zb_uint8_t* value) {
  if (attr_id != PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID &&
      attr_id != PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID) {
    return RET_OK;
  }
  zb_int16_t v;
  memcpy(&v, value, sizeof(v));
  if (current == NULL) {
    return RET_OK;
  }
  if (attr_id == PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_DRY_ID) {
    return prst_soil_calibration_is_valid(v, current->wet) ? RET_OK : RET_ERROR;
  }
  if (attr_id == PRST_ZB_ZCL_ATTR_SOIL_CALIBRATION_WET_ID) {
    return prst_soil_calibration_is_valid(current->dry, v) ? RET_OK : RET_ERROR;
  }
  return RET_OK;
}

void prst_zcl_soil_calibration_init_server(void) {
  zb_zcl_add_cluster_handlers(PRST_ZB_ZCL_CLUSTER_ID_SOIL_CALIBRATION,
                              ZB_ZCL_CLUSTER_SERVER_ROLE,
                              check_value,
                              /*cluster_write_attr_hook=*/NULL,
                              /*cluster_handler=*/NULL);
}

void prst_zcl_soil_calibration_init_client(void) {
}

// Both points live on the factory scale, where wetter soil reads higher; dry above wet would invert the output.
bool prst_soil_calibration_is_valid(zb_int16_t dry, zb_int16_t wet) {
  return dry < wet;
}

void prst_soil_calibration_load(prst_soil_calibration_attrs_t* attrs) {
  current = attrs;
  attrs->dry = PRST_ZB_SOIL_CALIBRATION_DEFAULT_DRY;
  attrs->wet = PRST_ZB_SOIL_CALIBRATION_DEFAULT_WET;

  struct fs_file_t file;
  fs_file_t_init(&file);
  if (fs_open(&file, CALIBRATION_FILE, FS_O_READ) != 0) {
    return;
  }
  stored_calibration_t stored;
  ssize_t n = fs_read(&file, &stored, sizeof(stored));
  fs_close(&file);
  if (n == sizeof(stored) && prst_soil_calibration_is_valid(stored.dry, stored.wet)) {
    attrs->dry = stored.dry;
    attrs->wet = stored.wet;
  }
}

static void save_work_handler(struct k_work* work) {
  struct fs_file_t file;
  fs_file_t_init(&file);
  if (fs_open(&file, CALIBRATION_FILE, FS_O_CREATE | FS_O_WRITE) != 0) {
    LOG_ERR("Unable to open %s", CALIBRATION_FILE);
    return;
  }
  if (fs_write(&file, &pending, sizeof(pending)) != sizeof(pending)) {
    LOG_ERR("Unable to write %s", CALIBRATION_FILE);
  }
  fs_close(&file);
}

K_WORK_DEFINE(save_work, save_work_handler);

void prst_soil_calibration_save_async(const prst_soil_calibration_attrs_t* attrs) {
  pending.dry = attrs->dry;
  pending.wet = attrs->wet;
  k_work_submit(&save_work);
}

float prst_soil_calibration_apply(const prst_soil_calibration_attrs_t* attrs, float factory_fraction) {
  if (!prst_soil_calibration_is_valid(attrs->dry, attrs->wet)) {
    return MAX(0.0f, MIN(1.0f, factory_fraction));
  }
  const float dry = attrs->dry / 10000.0f;
  const float wet = attrs->wet / 10000.0f;
  return MAX(0.0f, MIN(1.0f, (factory_fraction - dry) / (wet - dry)));
}
