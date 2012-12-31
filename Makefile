########################################################################
#
# Makefile: Makefile for axffmpeg
#
#     Copyright (C) 2012 Yak! / Yasutaka ATARASHI
#
#     This software is distributed under the terms of a zlib/libpng
#     License.
#
#     $Id: d0f2b92c2e64c3ecc337917efe561e377d1fb69b $
#
########################################################################

VER=0_01

CXX = i686-pc-mingw32-g++
CXXFLAGS = -Wall -O2 -I /usr/local/include

all: axffmpeg.spi

axffmpeg.o: axffmpeg.cpp

odstream.o: odstream.cpp odstream.hpp

axffmpeg.spi: axffmpeg.o axffmpeg.ro odstream.o axffmpeg.def

%.spi: %.o
	$(LINK.cc) -mwindows -shared -static $^ -o $@

%.ro: %.rc
	windres -v -O coff $^ -o $@

dist:
	-rm -rf source source.zip axffmpeg-$(VER).zip disttemp
	mkdir source
	git archive --worktree-attributes master | tar xf - -C source
	(cd source; zip ../source.zip *)
	-rm -rf source
	mkdir disttemp
	cp axffmpeg.spi axffmepg.txt source.zip disttemp
	(cd disttemp; zip ../axffmpeg-$(VER).zip *)
	-rm -rf disttemp

tag:
	git tag axffmpeg-$(VER)

clean:
	-rm -rf *.o *.spi *.ro
