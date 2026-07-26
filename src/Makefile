PROGRAM := logoframe
OBJS := logoframe.o logo_generator.o logoframe_det.o logoframe_mul.o

CXX ?= g++
PKG_CONFIG ?= pkg-config

WITH_AVISYNTH ?= auto
WITH_DTVINDEX ?= auto
AVISYNTH_CFLAGS ?= $(shell \
	if test -f /usr/local/include/avisynth/avisynth.h; then \
		printf '%s' '-I/usr/local/include/avisynth'; \
	elif test -f /usr/include/avisynth/avisynth.h; then \
		printf '%s' '-I/usr/include/avisynth'; \
	fi)
DTVINDEX_DIR ?=

FFMPEG_PACKAGES := libavformat libavcodec libavutil libswscale
VALID_FEATURE_VALUES := auto yes no

ifeq ($(filter $(WITH_AVISYNTH),$(VALID_FEATURE_VALUES)),)
  $(error WITH_AVISYNTH must be auto, yes, or no)
endif
ifeq ($(filter $(WITH_DTVINDEX),$(VALID_FEATURE_VALUES)),)
  $(error WITH_DTVINDEX must be auto, yes, or no)
endif

CPPFLAGS := -I.
CXXFLAGS := -O3 -x c++ -ffast-math -Wall -Wshadow -fpermissive \
	-Wempty-body -std=gnu++11 -fomit-frame-pointer \
	-fno-tree-vectorize -fno-zero-initialized-in-bss
LDLIBS := -ldl -pthread
DTVINDEX_DEPS :=

AVISYNTH_FOUND := $(shell \
	printf '%s\n' '\#include <avisynth.h>' | \
	$(CXX) $(AVISYNTH_CFLAGS) -x c++ -E - >/dev/null 2>&1 \
	&& echo yes || echo no)
ifeq ($(WITH_AVISYNTH),auto)
  ENABLE_AVISYNTH := $(AVISYNTH_FOUND)
else
  ENABLE_AVISYNTH := $(WITH_AVISYNTH)
endif
ifeq ($(ENABLE_AVISYNTH),yes)
  ifneq ($(AVISYNTH_FOUND),yes)
    $(error WITH_AVISYNTH=yes, but avisynth.h was not found)
  endif
  CPPFLAGS += -DHAVE_AVISYNTH=1 $(AVISYNTH_CFLAGS)
else
  CPPFLAGS += -DHAVE_AVISYNTH=0
endif

PKG_CONFIG_FOUND := $(strip $(shell \
	command -v "$(PKG_CONFIG)" >/dev/null 2>&1 \
	&& echo yes || echo no))
DTVINDEX_PKG_FOUND := $(strip $(if $(filter yes,$(PKG_CONFIG_FOUND)),\
	$(shell $(PKG_CONFIG) --exists dtvindex \
		&& echo yes || echo no),no))
FFMPEG_FOUND := $(strip $(if $(filter yes,$(PKG_CONFIG_FOUND)),\
	$(shell $(PKG_CONFIG) --exists $(FFMPEG_PACKAGES) \
		&& echo yes || echo no),no))

LOCAL_DTVINDEX_LIBRARY := $(CURDIR)/libdtvindex.a
LOCAL_DTVINDEX_INCLUDE := $(CURDIR)/include
LOCAL_DTVINDEX_FOUND := $(strip $(if \
	$(and $(wildcard $(LOCAL_DTVINDEX_LIBRARY)),\
		$(wildcard $(LOCAL_DTVINDEX_INCLUDE)/dtvindex/dtvindex.hpp)),\
	yes,no))

SIBLING_DTVINDEX_DIR := $(abspath ../../dtvindex)
SIBLING_DTVINDEX_FOUND := $(strip $(if \
	$(and $(wildcard $(SIBLING_DTVINDEX_DIR)/Makefile),\
		$(wildcard $(SIBLING_DTVINDEX_DIR)/include/dtvindex/dtvindex.hpp)),\
	yes,no))

DTVINDEX_MODE :=
DTVINDEX_SOURCE_DIR :=

ifneq ($(strip $(DTVINDEX_DIR)),)
  DTVINDEX_SOURCE_DIR := $(abspath $(DTVINDEX_DIR))
  ifneq ($(and $(wildcard $(DTVINDEX_SOURCE_DIR)/Makefile),\
		$(wildcard $(DTVINDEX_SOURCE_DIR)/include/dtvindex/dtvindex.hpp)),)
    ifeq ($(FFMPEG_FOUND),yes)
      DTVINDEX_MODE := source
    endif
  else ifneq ($(WITH_DTVINDEX),no)
    $(error DTVINDEX_DIR does not contain a dtvindex source tree)
  endif
else ifeq ($(LOCAL_DTVINDEX_FOUND)-$(FFMPEG_FOUND),yes-yes)
  DTVINDEX_MODE := local
else ifeq ($(DTVINDEX_PKG_FOUND),yes)
  DTVINDEX_MODE := pkg-config
else ifeq ($(SIBLING_DTVINDEX_FOUND)-$(FFMPEG_FOUND),yes-yes)
  DTVINDEX_MODE := source
  DTVINDEX_SOURCE_DIR := $(SIBLING_DTVINDEX_DIR)
endif

ifeq ($(WITH_DTVINDEX),auto)
  ENABLE_DTVINDEX := $(if $(DTVINDEX_MODE),yes,no)
else
  ENABLE_DTVINDEX := $(WITH_DTVINDEX)
endif

ifeq ($(ENABLE_DTVINDEX),yes)
  ifeq ($(DTVINDEX_MODE),)
    $(error WITH_DTVINDEX=yes, but dtvindex and its FFmpeg dependencies were not found)
  endif
  CPPFLAGS += -DHAVE_DTVINDEX=1

  ifeq ($(DTVINDEX_MODE),pkg-config)
    CPPFLAGS += $(shell $(PKG_CONFIG) --cflags dtvindex)
    LDLIBS += $(shell $(PKG_CONFIG) --libs dtvindex)
  else ifeq ($(DTVINDEX_MODE),local)
    CPPFLAGS += -I$(LOCAL_DTVINDEX_INCLUDE) \
		$(shell $(PKG_CONFIG) --cflags $(FFMPEG_PACKAGES))
    LDLIBS += $(LOCAL_DTVINDEX_LIBRARY) \
		$(shell $(PKG_CONFIG) --libs $(FFMPEG_PACKAGES))
    DTVINDEX_DEPS += $(LOCAL_DTVINDEX_LIBRARY)
  else ifeq ($(DTVINDEX_MODE),source)
    DTVINDEX_LIBRARY := $(DTVINDEX_SOURCE_DIR)/build/libdtvindex.a
    CPPFLAGS += -I$(DTVINDEX_SOURCE_DIR)/include \
		$(shell $(PKG_CONFIG) --cflags $(FFMPEG_PACKAGES))
    LDLIBS += $(DTVINDEX_LIBRARY) \
		$(shell $(PKG_CONFIG) --libs $(FFMPEG_PACKAGES))
    DTVINDEX_DEPS += $(DTVINDEX_LIBRARY)
  endif
else
  CPPFLAGS += -DHAVE_DTVINDEX=0
endif

ifneq ($(MAKECMDGOALS),clean)
  ifeq ($(ENABLE_AVISYNTH)-$(ENABLE_DTVINDEX),no-no)
    $(error Neither AviSynth nor dtvindex input support is available)
  endif
endif

ifeq ($(OS),Windows_NT)
  PROGRAM := $(PROGRAM).exe
endif

$(info Logoframe input support: AviSynth=$(ENABLE_AVISYNTH) dtvindex=$(ENABLE_DTVINDEX)$(if $(filter yes,$(ENABLE_DTVINDEX)),$(if $(DTVINDEX_MODE), ($(DTVINDEX_MODE)))))

.SUFFIXES: .c .o

.PHONY: all clean

all: $(PROGRAM)

$(PROGRAM): $(OBJS) $(DTVINDEX_DEPS)
	$(CXX) -o $@ $(OBJS) $(LDLIBS)

ifeq ($(DTVINDEX_MODE),source)
$(DTVINDEX_LIBRARY):
	$(MAKE) -C "$(DTVINDEX_SOURCE_DIR)" build/libdtvindex.a
endif

logoframe.o: video_source.h logo_generator.h
ifeq ($(ENABLE_DTVINDEX),yes)
logoframe.o: dtvindex_video_source.h
endif

logo_generator.o: logo_generator.cpp logo_generator.h video_source.h logo.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

.c.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

clean:
	$(RM) $(PROGRAM) $(OBJS)
