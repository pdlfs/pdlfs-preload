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
#include <sys/stat.h>
#include <string>

#include "buffered_io.h"
#include "pdlfs-preload/pdlfs_api.h"

namespace {

class BufferedFile {
 public:
  BufferedFile(int fd, off_t size)
      : err_(false),
        eof_(false),
        append_(false),
        buf_pos_(0),
        off_(0),
        size_(size),
        fd_(fd) {}

  ~BufferedFile() {}

  void Clearerr() { err_ = eof_ = false; }

  void Seek(off_t off) {
    off_ = off;
    eof_ = false;
  }

  void SetAppend() {
    buf_pos_ = size_;
    append_ = true;
  }

  // Return the number of bytes read which is less then nbytes
  // only if a read error or end-of-file is encountered.
  size_t Read(void* buf, size_t nbytes) {
    if (err_ || eof_) return 0;
    int r = Flush(true);
    if (r == 0) {
      ssize_t n = pdlfs_pread(fd_, buf, nbytes, off_);
      if (n == -1) {
        err_ = true;
      } else {
        off_ += n;
        if (n < nbytes) eof_ = true;
        if (off_ > size_) size_ = off_;
        return n;
      }
    }

    return 0;
  }

  // Return the number of bytes written.
  size_t Append(const void* buf, size_t nbytes) {
    buf_.append(reinterpret_cast<const char*>(buf), nbytes);
    off_t end = buf_pos_ + buf_.size();
    if (end > size_) {
      size_ = end;
    }
    return nbytes;
  }

  // Return the number of bytes written or 0 on errors.
  size_t Write(const void* buf, size_t nbytes) {
    if (err_) return 0;
    if (append_) return Append(buf, nbytes);
    if (buf_.empty()) {
      buf_pos_ = off_;
    }
    if (off_ == buf_pos_ + buf_.size()) {
      buf_.append(reinterpret_cast<const char*>(buf), nbytes);
      off_ += nbytes;
      if (off_ > size_) {
        size_ = off_;
      }
      return nbytes;
    } else {
      int r = Flush(true);
      if (r == 0) {
        ssize_t n = pdlfs_pwrite(fd_, buf, nbytes, off_);
        if (n != nbytes) {
          err_ = true;
        } else {
          off_ += nbytes;
          if (off_ > size_) {
            size_ = off_;
          }
          return nbytes;
        }
      }
    }

    return 0;
  }

  // Return 0 on success, or EOF on errors.
  int Flush(bool force = false) {
    if (err_) return EOF;
    if (buf_.size() == 0) return 0;
    if (force || buf_.size() >= kMaxBufSize) {
      ssize_t n = pdlfs_pwrite(fd_, buf_.data(), buf_.size(), buf_pos_);
      if (n != buf_.size()) {
        err_ = true;
        return EOF;
      } else {
        buf_pos_ += buf_.size();
        buf_.resize(0);
      }
    }

    return 0;
  }

  // Return 0 on success, or EOF on errors.
  int Close() {
    int r = Flush(true);
    if (r == 0) {
      r = pdlfs_close(fd_);
    }
    return r == 0 ? 0 : EOF;
  }

  enum { kMaxBufSize = 4096 };
  bool err_;
  bool eof_;
  bool append_;
  std::string buf_;
  off_t buf_pos_;
  off_t off_;
  off_t size_;
  int fd_;
};
}  // namespace

static inline BufferedFile* buffered_file(FILE* f) {
  return reinterpret_cast<BufferedFile*>(f);
}

static int __convert_to_flags(std::string modes) {
  if (modes == "r") {
    return O_RDONLY;
  } else if (modes == "r+") {
    return O_RDWR;
  } else if (modes == "w") {
    return O_CREAT | O_WRONLY | O_TRUNC;
  } else if (modes == "w+") {
    return O_CREAT | O_RDWR | O_TRUNC;
  } else if (modes == "a") {
    return O_CREAT | O_WRONLY;
  } else if (modes == "a+") {
    return O_CREAT | O_RDWR;
  } else {
    return -1;
  }
}

extern "C" {

FILE* pdlfs_fopen(const char* fname, const char* modes) {
  int flags = __convert_to_flags(modes);
  if (flags == -1) {
    errno = EINVAL;
    return NULL;
  }

  FILE* file = NULL;
  struct stat stat_buf;
  BufferedFile* bf;
  int fd = pdlfs_open(fname, flags, DEFFILEMODE, &stat_buf);
  if (fd != -1) {
    bf = new BufferedFile(fd, stat_buf.st_size);
    if (modes[0] == 'a') {
      bf->SetAppend();
    }
    file = reinterpret_cast<FILE*>(bf);
  }

  return file;
}

size_t pdlfs_fread(void* ptr, size_t sz, size_t n, FILE* stream) {
  if (stream == NULL) {
    errno = EINVAL;
    return 0;
  } else {
    BufferedFile* file = buffered_file(stream);
    size_t ret = file->Read(ptr, sz * n) / sz;
    return ret;
  }
}

size_t pdlfs_fwrite(const void* ptr, size_t sz, size_t n, FILE* stream) {
  if (stream == NULL) {
    errno = EINVAL;
    return 0;
  } else {
    BufferedFile* file = buffered_file(stream);
    size_t ret = file->Write(ptr, sz * n) / sz;
    file->Flush();
    return ret;
  }
}

int pdlfs_fseek(FILE* stream, long int off, int whence) {
  if (stream == NULL) {
    errno = EINVAL;
    return -1;
  } else {
    BufferedFile* file = buffered_file(stream);
    if (whence == SEEK_CUR) {
      file->Seek(file->off_ + off);
    } else if (whence == SEEK_END) {
      file->Seek(file->size_ + off);
    } else {
      file->Seek(off);
    }
    return 0;
  }
}

long int pdlfs_ftell(FILE* stream) {
  if (stream == NULL) {
    errno = EINVAL;
    return -1;
  } else {
    BufferedFile* file = buffered_file(stream);
    return file->off_;
  }
}

int pdlfs_fflush(FILE* stream) {
  if (stream == NULL) {
    errno = EINVAL;
    return -1;
  } else {
    BufferedFile* file = buffered_file(stream);
    int r = file->Flush(true);
    return r;
  }
}

int pdlfs_fclose(FILE* stream) {
  if (stream == NULL) {
    errno = EINVAL;
    return -1;
  } else {
    BufferedFile* file = buffered_file(stream);
    file->Close();
    delete file;
    return 0;
  }
}

void pdlfs_clearerr(FILE* stream) {
  if (stream != NULL) {
    BufferedFile* file = buffered_file(stream);
    file->Clearerr();
  }
}

int pdlfs_ferror(FILE* stream) {
  if (stream != NULL) {
    BufferedFile* file = buffered_file(stream);
    if (file->err_) {
      return 1;
    }
  }
  return 0;
}

int pdlfs_feof(FILE* stream) {
  if (stream != NULL) {
    BufferedFile* file = buffered_file(stream);
    if (file->eof_) {
      return 1;
    }
  }
  return 0;
}

}  // extern C
