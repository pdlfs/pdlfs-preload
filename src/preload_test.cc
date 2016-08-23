/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "preload.h"

static std::vector<int> open_files;

static void ASSERT(bool b) {
  if (!b) {
    fprintf(stderr, "!!! ERROR (errno=%d): %s\n", errno, strerror(errno));
    abort();
  }
}

static void TEST_LowLevelIO(const char* path, bool closef = true) {
  fprintf(stderr, "Creating file %s ...\n", path);
  int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, DEFFILEMODE);
  fprintf(stderr, ">> fd=%d\n", fd);
  ASSERT(fd != -1);
  fprintf(stderr, ">> writing ...\n");
  ssize_t written = pwrite(fd, "xxx", 3, 0);
  ASSERT(written == 3);
  char buf[3];
  fprintf(stderr, ">> reading ...\n");
  ssize_t read = pread(fd, buf, 3, 0);
  ASSERT(read == 3);
  ASSERT(strncmp(buf, "xxx", 3) == 0);
#if 0
  fprintf (stderr, ">> truncating ...\n");
  int r1 = ftruncate (fd, 0);
  ASSERT (r1 == 0);
  struct stat info;
  fprintf (stderr, ">> stating ...\n");
  int r2 = fstat (fd, &info);
  ASSERT (r2 == 0);
#endif
  if (closef) {
    fprintf(stderr, ">> closing file ...\n");
    int r3 = close(fd);
    ASSERT(r3 == 0);
  } else {
    open_files.push_back(fd);
  }
}

static void TEST_BufferedIO(const char* path) {
  fprintf(stderr, "Creating file %s ...\n", path);
  FILE* f = fopen(path, "w+");
  fprintf(stderr, ">> FILE=%p\n", f);
  ASSERT(f != NULL);
  fprintf(stderr, ">> writing ...\n");
  size_t written = fwrite("xxx", 1, 3, f);
  ASSERT(written == 3);
  fprintf(stderr, ">> rewinding ...\n");
  int r = fseek(f, 0, SEEK_SET);
  ASSERT(r == 0);
  long int off = ftell(f);
  ASSERT(off == 0);
  char buf[3];
  fprintf(stderr, ">> reading ...\n");
  size_t read = fread(buf, 1, 3, f);
  ASSERT(read == 3);
  ASSERT(strncmp(buf, "xxx", 3) == 0);
  fprintf(stderr, ">> flushing ...\n");
  r = fflush(f);
  ASSERT(r == 0);
  fprintf(stderr, ">> closing file ...\n");
  r = fclose(f);
  ASSERT(r == 0);
}

static void CloseAll() {
  for (size_t i = 0; i < open_files.size(); i++) {
    close(open_files[i]);
  }
}

int main(int argc, char* argv[]) {
  TEST_LowLevelIO("/tmp/pdlfs/1", false);
  TEST_LowLevelIO("/tmp/pdlfs/2", false);
  TEST_LowLevelIO("/tmp/1", false);
  TEST_LowLevelIO("/tmp/2", false);
  TEST_LowLevelIO("/tmp/lalala");
  TEST_LowLevelIO("/tmp/pdlfs/lalala");
  CloseAll();

  TEST_LowLevelIO("/tmp/pdlfs/1", false);
  TEST_LowLevelIO("/tmp/pdlfs/2", false);
  TEST_LowLevelIO("/tmp/1", false);
  TEST_LowLevelIO("/tmp/2", false);
  TEST_LowLevelIO("/tmp/3", false);
  TEST_LowLevelIO("/tmp/4", false);
  TEST_LowLevelIO("/tmp/5", false);
  TEST_LowLevelIO("/tmp/6", false);
  TEST_LowLevelIO("/tmp/7", false);
  TEST_LowLevelIO("/tmp/8", false);
  TEST_LowLevelIO("/tmp/9", false);
  TEST_LowLevelIO("/tmp/10", false);
  TEST_LowLevelIO("/tmp/11", false);
  TEST_LowLevelIO("/tmp/12", false);
  TEST_LowLevelIO("/tmp/lalala");
  TEST_LowLevelIO("/tmp/pdlfs/lalala");
  CloseAll();

  TEST_BufferedIO("/tmp/lalala");
  TEST_BufferedIO("/tmp/pdlfs/lalala");
  return 0;
}
