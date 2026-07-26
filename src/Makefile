PROGRAM = logoframe
OBJS = logoframe.o logoframe_det.o logoframe_mul.o

CXX ?= g++
PKG_CONFIG ?= pkg-config
FFMPEG_PACKAGES = libavformat libavcodec libavutil libswscale
FFMPEG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(FFMPEG_PACKAGES))
FFMPEG_LIBS := $(shell $(PKG_CONFIG) --libs $(FFMPEG_PACKAGES))
CXXFLAGS = -O3 -x c++ -I/usr/local/include/avisynth -ffast-math -Wall -Wshadow -fpermissive -Wempty-body -I. -std=gnu++11 -fomit-frame-pointer -fno-tree-vectorize -fno-zero-initialized-in-bss $(FFMPEG_CFLAGS)
LDLIBS = -ldl -pthread $(FFMPEG_LIBS)

.SUFFIXES: .c .o

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(CXX) -o $(PROGRAM) $^ $(LDLIBS)

logoframe.o: video_source.h

.c.o:
	$(CXX) $(CXXFLAGS) -c $<

.PHONY: all clean
clean:
	$(RM) $(PROGRAM) $(OBJS)
