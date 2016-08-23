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
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <map>
#include <string>

#include "buffered_io.h"
#include "pdlfs-preload/pdlfs_api.h"
#include "posix_api.h"
#include "preload.h"

namespace {

enum FileType { kPDLFS, kPOSIX };

struct ParsedPath {
  FileType type;
  const char* path;
};

struct Context {
  std::string pdlfs_root;
  std::map<int, int> fd_map;
  std::map<FILE*, FileType> files;
  int fd;

  bool ParsePath(const char* path, ParsedPath* result) {
    assert(path != NULL);
    assert(strlen(path) != 0);
    if (path[0] != '/') {
      return false;
    } else {
      if (strncmp(path, pdlfs_root.data(), pdlfs_root.length()) == 0 &&
          (path[pdlfs_root.length()] == '/' ||
           path[pdlfs_root.length()] == 0)) {
        result->type = kPDLFS;
        result->path = path + pdlfs_root.length();
        if (result->path[0] == 0) {
          result->path = "/";
        }
      } else {
        result->type = kPOSIX;
        result->path = path;
      }
      return true;
    }
  }

  // File descriptor 0, 1, 2 are reserved for stdin, stdout, and stderr
  Context() : fd(2) {
    const char* env = getenv("PDLFS_ROOT");
    if (env == NULL) {
      env = DEFAULT_PDLFS_ROOT;
    }
    std::string root = env;
    if (root.empty()) {
      root = DEFAULT_PDLFS_ROOT;
    }
    assert(root[0] == '/');
    // Removing tailing slashes
    while (root.length() != 1 && root[root.size() - 1] == '/') {
      root.resize(root.size() - 1);
    }
    pdlfs_root = root;
  }
};
}  // namespace

static const bool kRedirectCurDir = true;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static Context* fs_ctx = NULL;

static void __init_ctx() {
  Context* ctx = new Context;
  fs_ctx = ctx;
}

static void MutexLock() {
  int r = pthread_mutex_lock(&mutex);
  if (r != 0) {
    abort();
  }
}

static void MutexUnlock() {
  int r = pthread_mutex_unlock(&mutex);
  if (r != 0) {
    abort();
  }
}

static bool __check_file_by_fd(int fd, FileType* type, int* __fd,
                               bool remove = false) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }

  bool ok = true;
  int tmp;
  MutexLock();
  std::map<int, int>::iterator it;
  it = fs_ctx->fd_map.find(fd);
  if (it == fs_ctx->fd_map.end()) {
    ok = false;
  } else if (it->second >= 0) {
    *type = kPOSIX;
    *__fd = it->second;
  } else {
    *type = kPDLFS;
    tmp = -1 * it->second;
    tmp--;
    *__fd = tmp;
  }
  if (ok && remove) {
    fs_ctx->fd_map.erase(it);
  }
  MutexUnlock();

  return ok;
}

static bool __check_file(FILE* f, FileType* type, bool remove = false) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }

  bool ok = true;
  MutexLock();
  std::map<FILE*, FileType>::iterator it;
  it = fs_ctx->files.find(f);
  if (it == fs_ctx->files.end()) {
    ok = false;
  } else {
    *type = it->second;
    if (remove) {
      fs_ctx->files.erase(f);
    }
  }
  MutexUnlock();

  return ok;
}

extern "C" {

int open(const char* path, int oflags, ...) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  if (path == NULL) {
    errno = EINVAL;
    return -1;
  }
  std::string tmp;
  if (kRedirectCurDir && path[0] != '/') {
    tmp = fs_ctx->pdlfs_root + "/";
    tmp += path;
    path = tmp.c_str();
  }
  va_list args;
  va_start(args, oflags);
  mode_t mode;
  if (O_CREAT == (oflags & O_CREAT)) {
    mode = va_arg(args, mode_t);
  }
  va_end(args);

  int __fd;
  struct stat buf;
  ParsedPath parsed;
  bool ok = fs_ctx->ParsePath(path, &parsed);
  if (!ok || parsed.type == kPOSIX) {
    parsed.type = kPOSIX;
    __fd = posix_open(ok ? parsed.path : path, oflags, mode);
  } else {
    __fd = pdlfs_open(parsed.path, oflags, mode, &buf);
  }
  if (__fd == -1) {
    return __fd;
  }

  int err = 0;
  int fd;
  MutexLock();
  if (parsed.type == kPOSIX) {
    if (fs_ctx->fd_map.count(__fd) == 0) {
      fd = __fd;
    } else {
      fd = posix_fcntl1(__fd, F_DUPFD, ++fs_ctx->fd);
      err = errno;
      posix_close(__fd);
    }
    if (fd != -1) {
      fs_ctx->fd_map[fd] = fd;
    }
  } else {
    fd = ++fs_ctx->fd;
    fs_ctx->fd_map[fd] = -1 * (__fd + 1);
  }
  if (fd > fs_ctx->fd) {
    fs_ctx->fd = fd;
  }
  MutexUnlock();
  if (err != 0) {
    errno = err;
  }

  return fd;
}

int creat(const char* path, mode_t mode) {
  return open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

ssize_t pread(int fd, void* buf, size_t sz, off_t off) {
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd)) {
    if (type == kPDLFS) {
      return pdlfs_pread(__fd, buf, sz, off);
    }
  }

  return posix_pread(fd, buf, sz, off);
}

ssize_t read(int fd, void* buf, size_t sz) {
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd)) {
    if (type == kPDLFS) {
      return pdlfs_read(__fd, buf, sz);
    }
  }

  return posix_read(fd, buf, sz);
}

ssize_t pwrite(int fd, const void* buf, size_t sz, off_t off) {
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd)) {
    if (type == kPDLFS) {
      return pdlfs_pwrite(__fd, buf, sz, off);
    }
  }

  return posix_pwrite(fd, buf, sz, off);
}

ssize_t write(int fd, const void* buf, size_t sz) {
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd)) {
    if (type == kPDLFS) {
      return pdlfs_write(__fd, buf, sz);
    }
  }

  return posix_write(fd, buf, sz);
}

int close(int fd) {
  bool remove_fd = true;
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd, remove_fd)) {
    if (type == kPDLFS) {
      return pdlfs_close(__fd);
    }
  }

  return posix_close(fd);
}

FILE* fopen(const char* fname, const char* modes) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  std::string tmp;
  if (kRedirectCurDir && fname[0] != '/') {
    tmp = fs_ctx->pdlfs_root + "/";
    tmp += fname;
    fname = tmp.c_str();
  }

  FILE* f;
  ParsedPath parsed;
  bool ok = fs_ctx->ParsePath(fname, &parsed);
  if (!ok || parsed.type == kPOSIX) {
    parsed.type = kPOSIX;
    f = posix_fopen(ok ? parsed.path : fname, modes);
    if (f == NULL) {
      return NULL;
    }
  } else {
    f = pdlfs_fopen(parsed.path, modes);
    if (f == NULL) {
      return NULL;
    }
  }

  MutexLock();
  std::pair<FILE*, FileType> pair(f, parsed.type);
  fs_ctx->files.insert(pair);
  MutexUnlock();

  return f;
}

size_t fread(void* ptr, size_t sz, size_t n, FILE* file) {
  FileType type;
  if (__check_file(file, &type)) {
    if (type == kPDLFS) {
      return pdlfs_fread(ptr, sz, n, file);
    }
  }

  return posix_fread(ptr, sz, n, file);
}

size_t fwrite(const void* ptr, size_t sz, size_t n, FILE* file) {
  FileType type;
  if (__check_file(file, &type)) {
    if (type == kPDLFS) {
      return pdlfs_fwrite(ptr, sz, n, file);
    }
  }

  return posix_fwrite(ptr, sz, n, file);
}

int fseek(FILE* file, long int off, int whence) {
  FileType type;
  if (__check_file(file, &type)) {
    if (type == kPDLFS) {
      return pdlfs_fseek(file, off, whence);
    }
  }

  return posix_fseek(file, off, whence);
}

long int ftell(FILE* file) {
  FileType type;
  if (__check_file(file, &type)) {
    if (type == kPDLFS) {
      return pdlfs_ftell(file);
    }
  }

  return posix_ftell(file);
}

int fflush(FILE* file) {
  FileType type;
  if (__check_file(file, &type)) {
    if (type == kPDLFS) {
      return pdlfs_fflush(file);
    }
  }

  return posix_fflush(file);
}

int fclose(FILE* file) {
  bool remove_file = true;
  FileType type;
  if (__check_file(file, &type, remove_file)) {
    if (type == kPDLFS) {
      return pdlfs_fclose(file);
    }
  }

  return posix_fclose(file);
}

void clearerr(FILE* file) {
  FileType type;
  if (__check_file(file, &type)) {
    if (type == kPDLFS) {
      pdlfs_clearerr(file);
    }
  }

  posix_clearerr(file);
}

int ferror(FILE* file) {
  FileType type;
  if (__check_file(file, &type)) {
    if (type == kPDLFS) {
      return pdlfs_ferror(file);
    }
  }

  return posix_ferror(file);
}

int feof(FILE* file) {
  FileType type;
  if (__check_file(file, &type)) {
    if (type == kPDLFS) {
      return pdlfs_feof(file);
    }
  }

  return posix_feof(file);
}

}  // extern C
