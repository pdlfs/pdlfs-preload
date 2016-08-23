/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include "pdlfs-preload/pdlfs_api.h"
#include "deltafs_api.h"

extern "C" {

int pdlfs_open(const char* p, int f, mode_t m, struct stat* statbuf) {
  return deltafs_open(p, f, m, statbuf);
}

ssize_t pdlfs_pread(int fd, void* buf, size_t sz, off_t off) {
  return deltafs_pread(fd, buf, sz, off);
}

ssize_t pdlfs_read(int fd, void* buf, size_t sz) {
  return deltafs_read(fd, buf, sz);
}

ssize_t pdlfs_pwrite(int fd, const void* buf, size_t sz, off_t off) {
  return deltafs_pwrite(fd, buf, sz, off);
}

ssize_t pdlfs_write(int fd, const void* buf, size_t sz) {
  return deltafs_write(fd, buf, sz);
}

int pdlfs_close(int fd) { return deltafs_close(fd); }

}  // extern C
