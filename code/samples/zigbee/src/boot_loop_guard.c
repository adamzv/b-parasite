#include "boot_loop_guard.h"

#include <errno.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

LOG_MODULE_REGISTER(boot_loop_guard, CONFIG_LOG_DEFAULT_LEVEL);

// The legacy boots_since_join file counted every boot, not just crashes.
static const char kBootsFile[] = "/lfs/crash_boots";
static atomic_t clear_requested;

uint32_t prst_boot_loop_guard_register_boot(bool crash_reset) {
  if (!crash_reset) {
    int err = fs_unlink(kBootsFile);
    if (err != 0 && err != -ENOENT) {
      LOG_ERR("Unable to clear %s: %d", kBootsFile, err);
    }
    return 0;
  }
  struct fs_file_t file;
  fs_file_t_init(&file);
  int err = fs_open(&file, kBootsFile, FS_O_CREATE | FS_O_RDWR);
  if (err != 0) {
    LOG_ERR("Unable to open %s: %d", kBootsFile, err);
    return 0;
  }

  uint32_t boots = 0;
  ssize_t bytes = fs_read(&file, &boots, sizeof(boots));
  if (bytes != 0 && bytes != (ssize_t)sizeof(boots)) {
    err = bytes < 0 ? (int)bytes : -EIO;
  }
  if (!err) {
    err = fs_seek(&file, 0, FS_SEEK_SET);
  }
  if (!err) {
    const uint32_t next = boots + 1;
    bytes = fs_write(&file, &next, sizeof(next));
    if (bytes != (ssize_t)sizeof(next)) {
      err = bytes < 0 ? (int)bytes : -EIO;
    }
  }
  // LittleFS releases its handle even when close reports a sync failure.
  int close_err = fs_close(&file);
  if (err || close_err) {
    LOG_ERR("Crash boot count unconfirmed: operation=%d, close=%d", err, close_err);
    return 0;
  }
  return boots;
}

// Flash writes stall the radio, so they stay off the Zigbee thread.
static void clear_work_handler(struct k_work* work) {
  int err = fs_unlink(kBootsFile);
  if (err != 0 && err != -ENOENT) {
    LOG_ERR("Unable to clear %s: %d", kBootsFile, err);
    atomic_clear(&clear_requested);
  }
}

K_WORK_DEFINE(clear_work, clear_work_handler);

void prst_boot_loop_guard_clear_async() {
  if (atomic_cas(&clear_requested, 0, 1)) {
    int err = k_work_submit(&clear_work);
    if (err < 0) {
      LOG_ERR("Unable to schedule crash count clear: %d", err);
      atomic_clear(&clear_requested);
    }
  }
}
