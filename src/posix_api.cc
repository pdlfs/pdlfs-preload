/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include "posix_api.h"

#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

namespace {

struct PosixAPI {
  template <typename T>
  void LoadSym(const char* name, T* result) {
    *result = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
    if (*result == NULL) {
      fprintf(stderr, "!!! FATAL error: dlsym(%s) failed\n", name);
      abort();
    }
  }

  explicit PosixAPI() {
    LoadSym("mkdir", &mkdir);
    LoadSym("open", &open);
    LoadSym("creat", &creat);
    LoadSym("pread", &pread);
    LoadSym("read", &read);
    LoadSym("pwrite", &pwrite);
    LoadSym("write", &write);
    LoadSym("__fxstat", &fxstat);
    LoadSym("fcntl", &fcntl);
    LoadSym("close", &close);
    LoadSym("fopen", &fopen);
    LoadSym("fread", &fread);
    LoadSym("fwrite", &fwrite);
    LoadSym("fseek", &fseek);
    LoadSym("ftell", &ftell);
    LoadSym("fflush", &fflush);
    LoadSym("fclose", &fclose);
    LoadSym("clearerr", &clearerr);
    LoadSym("ferror", &ferror);
    LoadSym("feof", &feof);
  }

  int (*mkdir)(const char*, mode_t);
  int (*open)(const char*, int, ...);
  int (*creat)(const char*, mode_t);
  ssize_t (*pread)(int, void*, size_t, off_t);
  ssize_t (*read)(int, void*, size_t);
  ssize_t (*pwrite)(int, const void*, size_t, off_t);
  ssize_t (*write)(int, const void*, size_t);
  int (*fxstat)(int, int, struct stat*);
  int (*fcntl)(int, int, ...);
  int (*close)(int);
  FILE* (*fopen)(const char*, const char*);
  size_t (*fread)(void*, size_t, size_t, FILE*);
  size_t (*fwrite)(const void*, size_t, size_t, FILE*);
  int (*fseek)(FILE*, long int, int);
  long int (*ftell)(FILE*);
  int (*fflush)(FILE*);
  int (*fclose)(FILE*);
  void (*clearerr)(FILE*);
  int (*ferror)(FILE*);
  int (*feof)(FILE*);
};
}  // namespace

static pthread_once_t once = PTHREAD_ONCE_INIT;
static PosixAPI* posix_api = NULL;

static void __init_posix_api() {
  PosixAPI* api = new PosixAPI;
  posix_api = api;
}

extern "C" {

int posix_mkdir(const char* path, mode_t mode) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->mkdir(path, mode);
}

int posix_open(const char* path, int oflags, mode_t mode) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->open(path, oflags, mode);
}

int posix_creat(const char* path, mode_t mode) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->creat(path, mode);
}

ssize_t posix_pread(int fd, void* buf, size_t sz, off_t off) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->pread(fd, buf, sz, off);
}

ssize_t posix_read(int fd, void* buf, size_t sz) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->read(fd, buf, sz);
}

ssize_t posix_pwrite(int fd, const void* buf, size_t sz, off_t off) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->pwrite(fd, buf, sz, off);
}

ssize_t posix_write(int fd, const void* buf, size_t sz) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->write(fd, buf, sz);
}

int posix_fstat(int fd, struct stat* buf) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fxstat(_STAT_VER, fd, buf);
}

int posix_fcntl0(int fd, int cmd) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fcntl(fd, cmd);
}

int posix_fcntl1(int fd, int cmd, int arg) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fcntl(fd, cmd, arg);
}

int posix_close(int fd) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->close(fd);
}

FILE* posix_fopen(const char* fname, const char* modes) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fopen(fname, modes);
}

size_t posix_fread(void* ptr, size_t sz, size_t n, FILE* stream) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fread(ptr, sz, n, stream);
}

size_t posix_fwrite(const void* ptr, size_t sz, size_t n, FILE* stream) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fwrite(ptr, sz, n, stream);
}

int posix_fseek(FILE* stream, long int off, int whence) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fseek(stream, off, whence);
}

long int posix_ftell(FILE* stream) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->ftell(stream);
}

int posix_fflush(FILE* stream) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fflush(stream);
}

int posix_fclose(FILE* stream) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->fclose(stream);
}

void posix_clearerr(FILE* stream) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->clearerr(stream);
}

int posix_ferror(FILE* stream) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->ferror(stream);
}

int posix_feof(FILE* stream) {
  if (posix_api == NULL) {
    pthread_once(&once, &__init_posix_api);
  }

  return posix_api->feof(stream);
}

}  // extern C
