#pragma once

/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int posix_mkdir(const char* __path, mode_t __mode);
int posix_open(const char* __path, int __oflags, mode_t __mode);
int posix_creat(const char* __path, mode_t __mode);
ssize_t posix_pread(int __fd, void* __buf, size_t __sz, off_t __off);
ssize_t posix_read(int __fd, void* __buf, size_t __sz);
ssize_t posix_pwrite(int __fd, const void* __buf, size_t __sz, off_t __off);
ssize_t posix_write(int __fd, const void* __buf, size_t __sz);
int posix_fstat(int __fd, struct stat* __buf);
int posix_fcntl0(int __fd, int __cmd);
int posix_fcntl1(int __fd, int __cmd, int arg);
int posix_close(int __fd);

FILE* posix_fopen(const char* __fname, const char* __modes);
size_t posix_fread(void* __ptr, size_t __sz, size_t __n, FILE* __stream);
size_t posix_fwrite(const void* __ptr, size_t __sz, size_t __n, FILE* __stream);
int posix_fseek(FILE* __stream, long int __off, int __whence);
long int posix_ftell(FILE* __stream);
int posix_fflush(FILE* __stream);
int posix_fclose(FILE* __stream);

#ifdef __cplusplus
}
#endif
