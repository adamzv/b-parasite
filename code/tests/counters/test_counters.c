#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include "debug_counters.h"

static const char *root, *fault;
static int handles, fs_calls, caller_fs_calls, submit_failures;
static struct k_work *queued;
static bool worker, fault_used;
static pthread_mutex_t work_lock = PTHREAD_MUTEX_INITIALIZER;

#define CHECK(condition) do { if (!(condition)) { \
  fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static bool inject(const char *operation) {
  if (!fault_used && fault && strcmp(fault, operation) == 0) {
    fault_used = true;
    return true;
  }
  return false;
}
static void fs_access(void) { fs_calls++; if (!worker) caller_fs_calls++; }
static void local_path(const char *name, char *out, size_t size) {
  const char *last = strrchr(name, '/');
  int n = snprintf(out, size, "%s/%s", root, last ? last + 1 : name);
  CHECK(n >= 0 && (size_t)n < size);
}
static bool stored(const char *name, uint32_t count) {
  char path[1024]; local_path(name, path, sizeof(path));
  FILE *file = fopen(path, "rb");
  if (!file) return false;
  uint32_t value = 0;
  size_t n = fread(&value, 1, sizeof(value), file);
  fclose(file);
  return n == sizeof(value) && value == count;
}
static bool exists(const char *name) {
  char path[1024]; local_path(name, path, sizeof(path));
  return access(path, F_OK) == 0;
}
void fs_file_t_init(struct fs_file_t *file) { file->fd = -1; }
int fs_open(struct fs_file_t *file, const char *path, int flags) {
  fs_access();
  if (inject("open_error") && strstr(path, "bad")) return -EIO;
  char local[1024]; local_path(path, local, sizeof(local));
  file->fd = open(local, O_RDWR | ((flags & FS_O_CREATE) ? O_CREAT : 0), 0600);
  if (file->fd < 0) return -errno;
  handles++;
  return 0;
}
ssize_t fs_read(struct fs_file_t *file, void *data, size_t size) {
  fs_access();
  return inject("read_error") ? -EIO : read(file->fd, data, size);
}
ssize_t fs_write(struct fs_file_t *file, const void *data, size_t size) {
  fs_access();
  if (inject("write_error")) return -EIO;
  return write(file->fd, data, size);
}
int fs_seek(struct fs_file_t *file, off_t offset, int whence) {
  fs_access();
  if (inject("seek_error")) return -EIO;
  return lseek(file->fd, offset, whence) < 0 ? -errno : 0;
}
int fs_sync(struct fs_file_t *file) {
  fs_access(); return inject("sync_error") ? -EIO : fsync(file->fd);
}
int fs_close(struct fs_file_t *file) {
  fs_access();
  CHECK(file->fd >= 0);
  int result = close(file->fd);
  file->fd = -1;
  handles--;
  return inject("close_error") ? -EIO : result;
}
int fs_stat(const char *path, struct fs_dirent *entry) { fs_access(); return 0; }
int fs_mkdir(const char *path) { fs_access(); return 0; }
void fs_dir_t_init(struct fs_dir_t *dir) { dir->unused = 0; }
int fs_opendir(struct fs_dir_t *dir, const char *path) { fs_access(); return 0; }
int fs_readdir(struct fs_dir_t *dir, struct fs_dirent *entry) { fs_access(); entry->name[0] = 0; return 0; }
int fs_closedir(struct fs_dir_t *dir) { fs_access(); return 0; }

int k_msgq_put(struct k_msgq *queue, const void *data, int timeout) {
  CHECK(timeout == K_NO_WAIT);
  pthread_mutex_lock(&queue->lock);
  int result = -ENOMSG;
  if (queue->count < queue->capacity) {
    size_t index = (queue->head + queue->count) % queue->capacity;
    memcpy(queue->buffer + index * queue->message_size, data, queue->message_size);
    queue->count++;
    result = 0;
  }
  pthread_mutex_unlock(&queue->lock);
  return result;
}
int k_msgq_get(struct k_msgq *queue, void *data, int timeout) {
  CHECK(timeout == K_NO_WAIT);
  pthread_mutex_lock(&queue->lock);
  int result = -ENOMSG;
  if (queue->count) {
    memcpy(data, queue->buffer + queue->head * queue->message_size, queue->message_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    result = 0;
  }
  pthread_mutex_unlock(&queue->lock);
  return result;
}
int k_work_submit(struct k_work *work) {
  pthread_mutex_lock(&work_lock);
  int result;
  if (submit_failures) {
    submit_failures--;
    result = -EIO;
  } else if (work->pending) {
    result = 0;
  } else {
    CHECK(!queued);
    work->pending = true;
    queued = work;
    result = 1;
  }
  pthread_mutex_unlock(&work_lock);
  return result;
}
static void run_work(void) {
  while (queued) {
    struct k_work *work = queued;
    queued = NULL;
    work->pending = false;
    worker = true;
    work->handler(work);
    worker = false;
  }
}
static void *produce(void *unused) {
  CHECK(prst_debug_counters_increment_async("shared") == 0);
  return NULL;
}

int main(int argc, char **argv) {
  if (argc != 3) return 2;
  root = argv[2];
  const char *name = argv[1];
  if (strcmp(name, "deferred") == 0) {
    CHECK(prst_debug_counters_increment_async("sensor") == 0);
    CHECK(fs_calls == 0 && !exists("sensor"));
    run_work();
    CHECK(stored("sensor", 1));
  } else if (strcmp(name, "copied_name") == 0) {
    char source[] = "original";
    CHECK(prst_debug_counters_increment_async(source) == 0);
    strcpy(source, "changed");
    CHECK(fs_calls == 0);
    run_work();
    CHECK(stored("original", 1) && !exists("changed"));
  } else if (strcmp(name, "multiple") == 0) {
    CHECK(prst_debug_counters_increment_async("soil") == 0);
    CHECK(prst_debug_counters_increment_async("soil") == 0);
    CHECK(prst_debug_counters_increment_async("light") == 0);
    CHECK(fs_calls == 0);
    run_work();
    CHECK(stored("soil", 2) && stored("light", 1));
  } else if (strcmp(name, "concurrent_producers") == 0) {
    pthread_t threads[8];
    for (size_t i = 0; i < 8; i++) CHECK(pthread_create(&threads[i], NULL, produce, NULL) == 0);
    for (size_t i = 0; i < 8; i++) CHECK(pthread_join(threads[i], NULL) == 0);
    CHECK(fs_calls == 0);
    run_work();
    CHECK(stored("shared", 8));
  } else if (strcmp(name, "queue_full") == 0 || strcmp(name, "full_queue_retry") == 0) {
    bool retry = strcmp(name, "full_queue_retry") == 0;
    submit_failures = retry ? 8 : 0;
    for (int i = 0; i < 8; i++) CHECK(prst_debug_counters_increment_async("soil") == (retry ? -EIO : 0));
    CHECK(prst_debug_counters_increment_async("overflow") == -ENOMSG);
    CHECK(fs_calls == 0);
    run_work();
    CHECK(stored("soil", 8) && !exists("overflow"));
  } else if (strcmp(name, "name_limit") == 0) {
    char limit[65]; memset(limit, 'x', 64); limit[64] = 0;
    CHECK(prst_debug_counters_increment_async(limit) == 0);
    CHECK(fs_calls == 0);
    run_work();
    CHECK(stored(limit, 1));
  } else if (strcmp(name, "bad_name") == 0) {
    char excessive[66]; memset(excessive, 'x', 65); excessive[65] = 0;
    CHECK(prst_debug_counters_increment_async(NULL) == -EINVAL);
    CHECK(prst_debug_counters_increment_async("") == -EINVAL);
    CHECK(prst_debug_counters_increment_async(excessive) == -ENAMETOOLONG);
    CHECK(fs_calls == 0);
    run_work();
    CHECK(fs_calls == 0);
  } else if (strcmp(name, "submit_retry") == 0) {
    submit_failures = 1;
    CHECK(prst_debug_counters_increment_async("retained") == -EIO);
    CHECK(!queued && fs_calls == 0);
    CHECK(prst_debug_counters_increment_async("next") == 0);
    run_work();
    CHECK(stored("retained", 1) && stored("next", 1));
  } else if (strstr(name, "_error")) {
    fault = name;
    CHECK(prst_debug_counters_increment_async("bad") == 0);
    CHECK(prst_debug_counters_increment_async("after_error") == 0);
    CHECK(fs_calls == 0);
    run_work();
    CHECK(handles == 0 && fault_used);
    CHECK(stored("after_error", 1));
  } else if (strcmp(name, "sync_api") == 0) {
    CHECK(prst_debug_counters_increment("startup") == 0);
    CHECK(stored("startup", 1) && fs_calls > 0);
    CHECK(handles == 0);
    puts("PASS");
    return 0;
  } else {
    return 2;
  }
  CHECK(caller_fs_calls == 0 && handles == 0);
  puts("PASS");
  return 0;
}
