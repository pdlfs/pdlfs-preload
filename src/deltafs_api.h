#pragma once

/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int deltafs_open(const char* __path, int __oflags, mode_t __mode, struct stat*);
ssize_t deltafs_pread(int __fd, void* __buf, size_t __sz, off_t __off);
ssize_t deltafs_read(int __fd, void* __buf, size_t __sz);
ssize_t deltafs_pwrite(int __fd, const void* __buf, size_t __sz, off_t __off);
ssize_t deltafs_write(int __fd, const void* __buf, size_t __sz);
int deltafs_close(int __fd);

#ifdef __cplusplus
}
#endif
