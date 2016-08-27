/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "pdlfs-preload/pdlfs_api.h"
#include "posix_api.h"
#include "preload.h"

namespace {

struct Context {
  static const int kDirMode = S_IRWXU | S_IRWXG | S_IRWXO;
  std::string pdlfs_root;

  void Init() {
    std::string path = pdlfs_root;
    posix_mkdir(path.c_str(), kDirMode);
  }

  explicit Context() {
    const char* env = getenv("PDLFS_Root");
    if (env == NULL) env = DEFAULT_PDLFS_ROOT;
    std::string root = env;
    if (root.empty()) root = DEFAULT_PDLFS_ROOT;
    assert(root[0] == '/');
    assert(root.size() > 1);
    while (root.length() != 1 && root[root.size() - 1] == '/') {
      root.resize(root.size() - 1);
    }
    pdlfs_root = root;
    Init();
  }
};
}  // namespace

static pthread_once_t once = PTHREAD_ONCE_INIT;
static Context* api_ctx = NULL;

static void InitContext() {
  Context* ctx = new Context;
  api_ctx = ctx;
}

extern "C" {

int pdlfs_mkdir(const char* path, mode_t mode) {
  if (api_ctx == NULL) {
    pthread_once(&once, &InitContext);
  }

  assert(path != NULL);
  assert(path[0] == '/');

  std::string p = api_ctx->pdlfs_root;
  p += path;

  return posix_mkdir(p.c_str(), mode);
}

int pdlfs_open(const char* path, int oflags, mode_t mode, struct stat* buf) {
  if (api_ctx == NULL) {
    pthread_once(&once, &InitContext);
  }

  assert(path != NULL);
  assert(path[0] == '/');

  std::string p = api_ctx->pdlfs_root;
  p += path;

  int fd = posix_open(p.c_str(), oflags, mode);
  if (fd != -1) {
    int r = posix_fstat(fd, buf);
    if (r == -1) {
      int err = errno;
      posix_close(fd);
      errno = err;
      return -1;
    }
  }

  return fd;
}

int pdlfs_creat(const char* path, mode_t mode) {
  if (api_ctx == NULL) {
    pthread_once(&once, &InitContext);
  }

  assert(path != NULL);
  assert(path[0] == '/');

  std::string p = api_ctx->pdlfs_root;
  p += path;

  return posix_creat(p.c_str(), mode);
}

ssize_t pdlfs_pread(int fd, void* buf, size_t sz, off_t off) {
  return posix_pread(fd, buf, sz, off);
}

ssize_t pdlfs_read(int fd, void* buf, size_t sz) {
  return posix_read(fd, buf, sz);
}

ssize_t pdlfs_pwrite(int fd, const void* buf, size_t sz, off_t off) {
  return posix_pwrite(fd, buf, sz, off);
}

ssize_t pdlfs_write(int fd, const void* buf, size_t sz) {
  return posix_write(fd, buf, sz);
}

int pdlfs_fstat(int fd, struct stat* buf) { return posix_fstat(fd, buf); }

int pdlfs_close(int fd) {
  posix_close(fd);
  return 0;
}

}  // extern C
