#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>

#include "boot_loop_guard.h"

static const char* root;
static const char legacy_file[] = "/lfs/boots_since_join";
static const char crash_file[] = "/lfs/crash_boots";
static const char* fault;
static int handles, closes, writes, unlinks, submissions, unlink_failures, submit_failures;
static struct k_work* queued;
static bool failed;

#define CHECK(condition)                                           \
  do {                                                             \
    if (!(condition)) {                                            \
      fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
      failed = true;                                               \
    }                                                              \
  } while (0)

static void local_path(const char* path, char* out, size_t size) {
  int n = snprintf(out, size, "%s/%s", root, strrchr(path, '/') + 1);
  if (n < 0 || (size_t)n >= size) abort();
}
static void seed(const char* path, uint32_t count) {
  char local[1024];
  local_path(path, local, sizeof(local));
  FILE* file = fopen(local, "wb");
  if (!file || fwrite(&count, sizeof(count), 1, file) != 1 || fclose(file)) abort();
}
static bool stored(const char* path, uint32_t count) {
  char local[1024];
  local_path(path, local, sizeof(local));
  FILE* file = fopen(local, "rb");
  if (!file) return false;
  uint32_t value = 0;
  size_t n = fread(&value, 1, sizeof(value), file);
  fclose(file);
  return n == sizeof(value) && value == count;
}
static bool exists(const char* path) {
  char local[1024];
  local_path(path, local, sizeof(local));
  return access(local, F_OK) == 0;
}
static bool inject(const char* operation) { return fault && strcmp(fault, operation) == 0; }

void fs_file_t_init(struct fs_file_t* file) { file->fd = -1; }
int fs_open(struct fs_file_t* file, const char* path, int mode) {
  if (inject("open_error")) return -EIO;
  char local[1024];
  local_path(path, local, sizeof(local));
  file->fd = open(local, O_RDWR | ((mode & FS_O_CREATE) ? O_CREAT : 0), 0600);
  if (file->fd < 0) return -errno;
  handles++;
  return 0;
}
ssize_t fs_read(struct fs_file_t* file, void* out, size_t size) {
  if (inject("read_error")) return -EIO;
  if (inject("short_read")) size = 2;
  ssize_t n = read(file->fd, out, size);
  return n < 0 ? -errno : n;
}
ssize_t fs_write(struct fs_file_t* file, const void* value, size_t size) {
  writes++;
  if (inject("write_error")) return -EIO;
  if (inject("short_write")) size = 2;
  ssize_t n = write(file->fd, value, size);
  return n < 0 ? -errno : n;
}
int fs_seek(struct fs_file_t* file, off_t offset, int whence) {
  if (inject("seek_error")) return -EIO;
  return lseek(file->fd, offset, whence) < 0 ? -errno : 0;
}
int fs_close(struct fs_file_t* file) {
  closes++;
  if (file->fd < 0) abort();
  int result = close(file->fd);
  file->fd = -1;
  handles--;
  // Zephyr LittleFS releases file data even when its close reports an error.
  return inject("close_error") ? -EIO : (result < 0 ? -errno : 0);
}
int fs_unlink(const char* path) {
  unlinks++;
  if (unlink_failures > 0) {
    unlink_failures--;
    return -EIO;
  }
  char local[1024];
  local_path(path, local, sizeof(local));
  return unlink(local) < 0 ? -errno : 0;
}
int k_work_submit(struct k_work* work) {
  submissions++;
  if (submit_failures > 0) {
    submit_failures--;
    return -EIO;
  }
  if (work->pending) return 0;
  if (queued) abort();
  work->pending = true;
  queued = work;
  return 1;
}
static void run_work(void) {
  if (!queued) return;
  struct k_work* work = queued;
  queued = NULL;
  work->pending = false;
  work->handler(work);
}

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const char* name = argv[1];
  root = argv[2];
  if (strcmp(name, "legacy_count_ignored") == 0) {
    seed(legacy_file, 12);
    CHECK(prst_boot_loop_guard_register_boot(true) == 0);
    CHECK(stored(crash_file, 1));
    CHECK(stored(legacy_file, 12));
  } else if (strcmp(name, "first_crash") == 0) {
    CHECK(prst_boot_loop_guard_register_boot(true) == 0);
    CHECK(stored(crash_file, 1));
    CHECK(handles == 0 && closes == 1);
  } else if (strcmp(name, "crash_streak") == 0) {
    seed(legacy_file, 12);
    seed(crash_file, 4);
    CHECK(prst_boot_loop_guard_register_boot(true) == 4);
    CHECK(stored(crash_file, 5));
    CHECK(prst_boot_loop_guard_register_boot(true) == 5);
    CHECK(stored(crash_file, 6));
    CHECK(stored(legacy_file, 12));
  } else if (strcmp(name, "power_reset") == 0 || strcmp(name, "pin_reset") == 0) {
    seed(legacy_file, 12);
    seed(crash_file, 7);
    CHECK(prst_boot_loop_guard_register_boot(false) == 0);
    CHECK(!exists(crash_file));
    CHECK(stored(legacy_file, 12));
    CHECK(prst_boot_loop_guard_register_boot(true) == 0);
    CHECK(stored(crash_file, 1));
  } else if (strstr(name, "_error") || strcmp(name, "short_write") == 0 || strcmp(name, "short_read") == 0) {
    seed(legacy_file, 4);
    seed(crash_file, 4);
    fault = name;
    CHECK(prst_boot_loop_guard_register_boot(true) == 0);
    CHECK(handles == 0);
    CHECK(closes == (strcmp(name, "open_error") == 0 ? 0 : 1));
    if (strcmp(name, "read_error") == 0 || strcmp(name, "short_read") == 0 ||
        strcmp(name, "seek_error") == 0 || strcmp(name, "open_error") == 0) {
      CHECK(writes == 0);
      CHECK(stored(crash_file, 4));
    }
  } else if (strcmp(name, "clear_success") == 0 || strcmp(name, "clear_missing") == 0) {
    seed(legacy_file, 12);
    if (strcmp(name, "clear_success") == 0) seed(crash_file, 4);
    prst_boot_loop_guard_clear_async();
    CHECK(unlinks == 0);
    run_work();
    CHECK(!exists(crash_file));
    CHECK(stored(legacy_file, 12));
    prst_boot_loop_guard_clear_async();
    run_work();
    CHECK(unlinks == 1);
  } else if (strcmp(name, "clear_retry") == 0) {
    seed(legacy_file, 12);
    seed(crash_file, 4);
    unlink_failures = 1;
    prst_boot_loop_guard_clear_async();
    run_work();
    CHECK(stored(crash_file, 4));
    prst_boot_loop_guard_clear_async();
    run_work();
    CHECK(unlinks == 2);
    CHECK(!exists(crash_file));
    CHECK(stored(legacy_file, 12));
  } else if (strcmp(name, "submit_retry") == 0) {
    seed(legacy_file, 12);
    seed(crash_file, 4);
    submit_failures = 1;
    prst_boot_loop_guard_clear_async();
    CHECK(!queued && unlinks == 0);
    prst_boot_loop_guard_clear_async();
    run_work();
    CHECK(submissions == 2);
    CHECK(!exists(crash_file));
    CHECK(stored(legacy_file, 12));
  } else if (strcmp(name, "clear_coalesces") == 0) {
    seed(legacy_file, 12);
    seed(crash_file, 4);
    for (int i = 0; i < 16; i++) prst_boot_loop_guard_clear_async();
    CHECK(submissions == 1 && unlinks == 0);
    run_work();
    CHECK(unlinks == 1);
    CHECK(!exists(crash_file));
    CHECK(stored(legacy_file, 12));
  } else {
    fprintf(stderr, "unknown case: %s\n", name);
    return 2;
  }
  CHECK(handles == 0);
  if (!failed) puts("PASS");
  return failed ? 1 : 0;
}
