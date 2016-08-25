#pragma once

/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#ifdef HAVE_ATOMIC
#include <atomic>
typedef std::atomic_int ctr_t;
#else
typedef int ctr_t;
#endif

struct __MonStats {
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
