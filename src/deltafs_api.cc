/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include "deltafs_api.h"

#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef GLOG
#include <glog/logging.h>
#endif

namespace {

struct DeltafsAPI {
  template <typename T>
  void LoadSym(const char* name, T* result) {
    *result = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
    if (*result == NULL) {
      fprintf(stderr, "!!! FATAL error: dlsym(%s) failed\n", name);
      abort();
    }
  }

  explicit DeltafsAPI() {
    LoadSym("deltafs_mkdir", &deltafs_mkdir);
    LoadSym("deltafs_open", &deltafs_open);
    LoadSym("deltafs_fstat", &deltafs_fstat);
    LoadSym("deltafs_ftruncate", &deltafs_ftruncate);
    LoadSym("deltafs_pread", &deltafs_pread);
    LoadSym("deltafs_read", &deltafs_read);
    LoadSym("deltafs_pwrite", &deltafs_pwrite);
    LoadSym("deltafs_write", &deltafs_write);
    LoadSym("deltafs_close", &deltafs_close);
  }

  int (*deltafs_mkdir)(const char*, mode_t);
  int (*deltafs_open)(const char*, int, mode_t, struct stat*);
  int (*deltafs_fstat)(int, struct stat*);
  int (*deltafs_ftruncate)(int, off_t);
  ssize_t (*deltafs_pread)(int, void*, size_t, off_t);
  ssize_t (*deltafs_read)(int, void*, size_t);
  ssize_t (*deltafs_pwrite)(int, const void*, size_t, off_t);
  ssize_t (*deltafs_write)(int, const void*, size_t);
  int (*deltafs_close)(int);
};
}  // namespace

static pthread_once_t once = PTHREAD_ONCE_INIT;
static DeltafsAPI* deltafs_api = NULL;

static void __init_deltafs_api() {
#ifdef GLOG
  int verbose = 0;
  const char* p = getenv("DELTAFS_Verbose");
  if (p != NULL) {
    verbose = atoi(p);
  }
  FLAGS_v = verbose;
  FLAGS_logtostderr = true;
  google::InitGoogleLogging("deltafs");
  google::InstallFailureSignalHandler();
#endif
  DeltafsAPI* api = new DeltafsAPI;
  deltafs_api = api;
}

extern "C" {

int deltafs_mkdir(const char* p, mode_t m) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_mkdir(p, m);
}

int deltsfs_open(const char* p, int f, mode_t m, struct stat* statbuf) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_open(p, f, m, statbuf);
}

int deltafs_fstat(int fd, struct stat* statbuf) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_fstat(fd, statbuf);
}

int deltafs_ftruncate(int fd, off_t len) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_ftruncate(fd, len);
}

ssize_t deltafs_pread(int fd, void* buf, size_t sz, off_t off) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_pread(fd, buf, sz, off);
}

ssize_t deltafs_read(int fd, void* buf, size_t sz) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_read(fd, buf, sz);
}

ssize_t deltafs_pwrite(int fd, const void* buf, size_t sz, off_t off) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_pwrite(fd, buf, sz, off);
}

ssize_t deltafs_write(int fd, const void* buf, size_t sz) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_write(fd, buf, sz);
}

int deltafs_close(int fd) {
  if (deltafs_api == NULL) {
    pthread_once(&once, &__init_deltafs_api);
  }

  return deltafs_api->deltafs_close(fd);
}

}  // extern C
