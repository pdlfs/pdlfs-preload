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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <map>
#include <string>

#include "buffered_io.h"
#include "pdlfs-preload/pdlfs_api.h"
#include "posix_api.h"
#include "preload.h"

#ifdef HAVE_MPI
#include <mpi/mpi.h>
#endif
#ifdef HAVE_ATOMIC
#include <atomic>
#endif
#ifdef GLOG
#include <glog/logging.h>
#endif

namespace {

struct Logger {
  Logger(int id, FILE* f = stderr) : id(id), file(f) {}
  void Logv(const char* fmt, va_list ap);
  FILE* file;
  int id;
};

void Logger::Logv(const char* fmt, va_list ap) {
  char tmp[500];
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  fprintf(file, "[%d] %s", id, tmp);
}

struct CallStats {
#ifdef HAVE_ATOMIC
  typedef std::atomic_int ctr_t;
#else
  typedef int ctr_t;
#endif
  ctr_t mkdir;
  ctr_t open;
  ctr_t creat;
  ctr_t fstat;
  ctr_t pread;
  ctr_t read;
  ctr_t pwrite;
  ctr_t write;
  ctr_t close;
  ctr_t feof;
  ctr_t ferror;
  ctr_t clearerr;
  ctr_t fopen;
  ctr_t fread;
  ctr_t fwrite;
  ctr_t fseek;
  ctr_t ftell;
  ctr_t fflush;
  ctr_t fclose;
};

enum FileType { kPDLFS, kPOSIX };

struct ParsedPath {
  FileType type;
  const char* path;
};

struct Context {
  CallStats posix_stats;
  CallStats pdlfs_stats;
  Logger* logger;
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
    int rank = -1;
#ifdef HAVE_MPI
    int mpi;
    MPI_Initialized(&mpi);
    if (mpi) {
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    }
#endif
    memset(&posix_stats, 0, sizeof(CallStats));
    memset(&pdlfs_stats, 0, sizeof(CallStats));
    logger = new Logger(rank);
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

static void Logv(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fs_ctx->logger->Logv(fmt, ap);
  va_end(ap);
}

static void Trace(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fs_ctx->logger->Logv(fmt, ap);
  va_end(ap);
}

static void LogStats(const char* prefix, const CallStats& stats) {
  Logv("num %s_mkdir\t%d\n", prefix, static_cast<int>(stats.mkdir));
  Logv("num %s_fopen\t%d\n", prefix, static_cast<int>(stats.fopen));
  Logv("num %s_fread\t%d\n", prefix, static_cast<int>(stats.fread));
  Logv("num %s_fwrite\t%d\n", prefix, static_cast<int>(stats.fwrite));
  Logv("num %s_fflush\t%d\n", prefix, static_cast<int>(stats.fflush));
  Logv("num %s_fclose\t%d\n", prefix, static_cast<int>(stats.fclose));
}

static void __print_stats() {
  LogStats("pdlfs", fs_ctx->pdlfs_stats);
  LogStats("posix", fs_ctx->posix_stats);
}

static void __init_ctx() {
#ifdef GLOG
  int verbose = 0;
  const char* p = getenv("PDLFS_Verbose");
  if (p != NULL) verbose = atoi(p);
  FLAGS_v = verbose;
  FLAGS_logtostderr = true;
  google::InitGoogleLogging("pdlfs");
  google::InstallFailureSignalHandler();
#endif
  Context* ctx = new Context;
  fs_ctx = ctx;
  atexit(&__print_stats);
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
  assert(fs_ctx != NULL);
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
  assert(fs_ctx != NULL);
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

int mkdir(const char* path, mode_t mode) __THROW {
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

  int r;
  ParsedPath parsed;
  bool ok = fs_ctx->ParsePath(path, &parsed);
  if (!ok || parsed.type == kPOSIX) {
    parsed.type = kPOSIX;
    const char* p = ok ? parsed.path : path;
    fs_ctx->posix_stats.mkdir++;
    Trace("posix_mkdir %s\n", p);
    r = posix_mkdir(p, mode);
  } else {
    fs_ctx->pdlfs_stats.mkdir++;
    Trace("pdlfs_mkdir %s\n", parsed.path);
    r = pdlfs_mkdir(parsed.path, mode);
  }

  return r;
}

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
    fs_ctx->posix_stats.open++;
    __fd = posix_open(ok ? parsed.path : path, oflags, mode);
  } else {
    fs_ctx->pdlfs_stats.open++;
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

int fstat(int fd, struct stat* buf) __THROW {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.fstat++;
    return pdlfs_fstat(__fd, buf);
  } else {
    fs_ctx->posix_stats.fstat++;
    return posix_fstat(fd, buf);
  }
}

ssize_t pread(int fd, void* buf, size_t sz, off_t off) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.pread++;
    return pdlfs_pread(__fd, buf, sz, off);
  } else {
    fs_ctx->posix_stats.pread++;
    return posix_pread(fd, buf, sz, off);
  }
}

ssize_t read(int fd, void* buf, size_t sz) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.read++;
    return pdlfs_read(__fd, buf, sz);
  } else {
    fs_ctx->posix_stats.read++;
    return posix_read(fd, buf, sz);
  }
}

ssize_t pwrite(int fd, const void* buf, size_t sz, off_t off) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.pwrite++;
    return pdlfs_pwrite(__fd, buf, sz, off);
  } else {
    fs_ctx->posix_stats.pwrite++;
    return posix_pwrite(fd, buf, sz, off);
  }
}

ssize_t write(int fd, const void* buf, size_t sz) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.write++;
    return pdlfs_write(__fd, buf, sz);
  } else {
    fs_ctx->posix_stats.write++;
    return posix_write(fd, buf, sz);
  }
}

int close(int fd) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  const bool remove_fd = true;
  FileType type;
  int __fd;
  if (__check_file_by_fd(fd, &type, &__fd, remove_fd) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.close++;
    return pdlfs_close(__fd);
  } else {
    fs_ctx->posix_stats.close++;
    return posix_close(fd);
  }
}

FILE* fopen(const char* fname, const char* modes) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
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
    const char* p = ok ? parsed.path : fname;
    fs_ctx->posix_stats.fopen++;
    Trace("posix_fopen %s\n", p);
    f = posix_fopen(p, modes);
    if (f == NULL) {
      return NULL;
    }
  } else {
    fs_ctx->pdlfs_stats.fopen++;
    Trace("pdlfs_fopen %s\n", parsed.path);
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
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  if (__check_file(file, &type) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.fread++;
    return pdlfs_fread(ptr, sz, n, file);
  } else {
    fs_ctx->posix_stats.fread++;
    return posix_fread(ptr, sz, n, file);
  }
}

size_t fwrite(const void* ptr, size_t sz, size_t n, FILE* file) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  if (__check_file(file, &type) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.fwrite++;
    return pdlfs_fwrite(ptr, sz, n, file);
  } else {
    fs_ctx->posix_stats.fwrite++;
    return posix_fwrite(ptr, sz, n, file);
  }
}

int fseek(FILE* file, long int off, int whence) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  if (__check_file(file, &type) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.fseek++;
    return pdlfs_fseek(file, off, whence);
  } else {
    fs_ctx->posix_stats.fseek++;
    return posix_fseek(file, off, whence);
  }
}

long int ftell(FILE* file) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  if (__check_file(file, &type) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.ftell++;
    return pdlfs_ftell(file);
  } else {
    fs_ctx->posix_stats.ftell++;
    return posix_ftell(file);
  }
}

int fflush(FILE* file) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  if (__check_file(file, &type) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.fflush++;
    return pdlfs_fflush(file);
  } else {
    fs_ctx->posix_stats.fflush++;
    return posix_fflush(file);
  }
}

int fclose(FILE* file) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  const bool remove_file = true;
  if (__check_file(file, &type, remove_file) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.fclose++;
    return pdlfs_fclose(file);
  } else {
    fs_ctx->posix_stats.fclose++;
    return posix_fclose(file);
  }
}

void clearerr(FILE* file) {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  if (__check_file(file, &type) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.clearerr++;
    pdlfs_clearerr(file);
  } else {
    fs_ctx->posix_stats.clearerr++;
    posix_clearerr(file);
  }
}

int ferror(FILE* file) __THROW {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  if (__check_file(file, &type) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.ferror++;
    return pdlfs_ferror(file);
  } else {
    fs_ctx->posix_stats.ferror++;
    return posix_ferror(file);
  }
}

int feof(FILE* file) __THROW {
  if (fs_ctx == NULL) {
    pthread_once(&once, &__init_ctx);
  }
  FileType type;
  if (__check_file(file, &type) && type == kPDLFS) {
    fs_ctx->pdlfs_stats.feof++;
    return pdlfs_feof(file);
  } else {
    fs_ctx->posix_stats.feof++;
    return posix_feof(file);
  }
}

}  // extern C
