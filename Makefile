#
# Copyright (c) 2016 Carnegie Mellon University.
# ---------------------------------------
# All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file. See the AUTHORS file for names of contributors.

#-----------------------------------------------
# Uncomment exactly one of the lines labeled (A), (B), and (C) below
# to switch between compilation modes.

# (A) Production use (optimized mode)
# OPT ?= -O2
# (B) Debug mode, w/ full line-level debugging symbols
OPT ?= -g2
# (C) Profiling mode: opt, but w/debugging symbols
# OPT ?= -O2 -g2
#-----------------------------------------------

OUTDIR=build

CFLAGS = -DGLOG -DHAVE_MPI -DNOTRACE -DNDEBUG -I./include $(OPT)
CXXFLAGS = -DGLOG -DHAVE_MPI -DNOTRACE -DNDEBUG -std=c++0x -I./include $(OPT)
CXX=mpicxx
CC=mpicc

default: all

all: $(OUTDIR)/libpdlfs-preload.so $(OUTDIR)/libpdlfs-preload-posix.so $(OUTDIR)/libpdlfs-preload-deltafs.so

TEST_LD_PRELOAD=$(OUTDIR)/libpdlfs-preload.so $(OUTDIR)/libpdlfs-preload-posix.so libglog.so

check: all $(OUTDIR)/preload_test
	env LD_PRELOAD="$(TEST_LD_PRELOAD)" $(OUTDIR)/preload_test

clean:
	-rm -rf $(OUTDIR)

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/src: | $(OUTDIR)
	mkdir -p $@

.PHONY: DIRS
DIRS: $(OUTDIR)/src

$(OUTDIR)/libpdlfs-preload-deltafs.so: DIRS $(OUTDIR)/src/pdlfs_api_deltafs.o $(OUTDIR)/src/deltafs_api.o
	$(CXX) -pthread -shared $(OUTDIR)/src/pdlfs_api_deltafs.o $(OUTDIR)/src/deltafs_api.o -o $@ -lglog

$(OUTDIR)/libpdlfs-preload-posix.so: DIRS $(OUTDIR)/src/pdlfs_api_posix.o
	$(CXX) -pthread -shared $(OUTDIR)/src/pdlfs_api_posix.o -o $@ -lglog

$(OUTDIR)/libpdlfs-preload.so: DIRS $(OUTDIR)/src/preload.o $(OUTDIR)/src/posix_api.o $(OUTDIR)/src/buffered_io.o
	$(CXX) -pthread -shared $(OUTDIR)/src/preload.o $(OUTDIR)/src/posix_api.o $(OUTDIR)/src/buffered_io.o -o $@ -ldl

$(OUTDIR)/preload_test: DIRS
	$(CXX) -pthread $(CXXFLAGS) src/preload_test.cc -o $@

$(OUTDIR)/%.o: %.cc
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

$(OUTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@
