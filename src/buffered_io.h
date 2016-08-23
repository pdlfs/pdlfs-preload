#pragma once

/*
 * Copyright (c) 2014-2016 Carnegie Mellon University.
 *
 * All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. See the AUTHORS file for names of contributors.
 */

#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int pdlfs_feof(FILE* __stream);
extern int pdlfs_ferror(FILE* __stream);
extern void pdlfs_clearerr(FILE* __stream);
FILE* pdlfs_fopen(const char* __fname, const char* __modes);
size_t pdlfs_fread(void* __ptr, size_t __sz, size_t __n, FILE* __stream);
size_t pdlfs_fwrite(const void* __ptr, size_t __sz, size_t __n, FILE* __stream);
int pdlfs_fseek(FILE* __stream, long int __off, int __whence);
long int pdlfs_ftell(FILE* __stream);
int pdlfs_fflush(FILE* __stream);
int pdlfs_fclose(FILE* __stream);

#ifdef __cplusplus
}
#endif
