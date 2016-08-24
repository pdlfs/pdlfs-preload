#pragma once

/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include <sys/types.h>

#define DEFAULT_PDLFS_ROOT "/tmp/pdlfs"
struct _IO_FILE;
typedef struct _IO_FILE FILE;
#ifndef __THROW
#define __THROW
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int mkdir(const char* __path, mode_t __mode) __THROW;
extern int open(const char* __path, int __oflags, ...);
extern int creat(const char* __path, mode_t __mode);
extern int fstat(int __fd, struct stat* __statbuf) __THROW;
extern ssize_t pread(int __fd, void* __buf, size_t __sz, off_t __off);
extern ssize_t read(int __fd, void* __buf, size_t __sz);
extern ssize_t pwrite(int __fd, const void* __buf, size_t __sz, off_t __off);
extern ssize_t write(int __fd, const void* __buf, size_t __sz);
extern int close(int __fd);

extern int feof(FILE* __file) __THROW;
extern int ferror(FILE* __file) __THROW;
extern void clearerr(FILE* __file);
extern FILE* fopen(const char* __fname, const char* __modes);
extern size_t fread(void* __ptr, size_t __sz, size_t __n, FILE* __file);
extern size_t fwrite(const void* __ptr, size_t __sz, size_t __n, FILE* __file);
extern int fseek(FILE* __file, long int __off, int __whence);
extern long int ftell(FILE* __file);
extern int fflush(FILE* __file);
extern int fclose(FILE* __file);

#ifdef __cplusplus
}
#endif
