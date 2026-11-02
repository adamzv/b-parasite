#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#define FS_O_CREATE 1
#define FS_O_RDWR 2
#define FS_SEEK_SET 0
struct fs_file_t {
  int fd;
};
void fs_file_t_init(struct fs_file_t*);
int fs_open(struct fs_file_t*, const char*, int);
ssize_t fs_read(struct fs_file_t*, void*, size_t);
ssize_t fs_write(struct fs_file_t*, const void*, size_t);
int fs_seek(struct fs_file_t*, off_t, int);
int fs_close(struct fs_file_t*);
int fs_unlink(const char*);
