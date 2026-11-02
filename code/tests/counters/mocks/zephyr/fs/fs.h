#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#define FS_O_CREATE 1
#define FS_O_RDWR 2
#define FS_O_READ 4
struct fs_file_t {
  int fd;
};
struct fs_dir_t {
  int unused;
};
struct fs_dirent {
  char name[256];
};
void fs_file_t_init(struct fs_file_t*);
int fs_open(struct fs_file_t*, const char*, int);
ssize_t fs_read(struct fs_file_t*, void*, size_t);
ssize_t fs_write(struct fs_file_t*, const void*, size_t);
int fs_seek(struct fs_file_t*, off_t, int);
int fs_sync(struct fs_file_t*);
int fs_close(struct fs_file_t*);
int fs_stat(const char*, struct fs_dirent*);
int fs_mkdir(const char*);
void fs_dir_t_init(struct fs_dir_t*);
int fs_opendir(struct fs_dir_t*, const char*);
int fs_readdir(struct fs_dir_t*, struct fs_dirent*);
int fs_closedir(struct fs_dir_t*);
