# TARGET #

TARGET := 3DS
LIBRARY := 0

ifeq ($(TARGET),$(filter $(TARGET),3DS WIIU))
    ifeq ($(strip $(DEVKITPRO)),)
        $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
    endif
endif

# COMMON CONFIGURATION #

NAME := Orchestrina

BUILD_DIR := build
OUTPUT_DIR := output
INCLUDE_DIRS := include
SOURCE_DIRS := source

EXTRA_OUTPUT_FILES :=

LIBRARY_DIRS += $(DEVKITPRO)/libctru $(DEVKITPRO)/portlibs/armv6k
LIBRARIES += sftd freetype sfil png jpeg z sf2d ctru m

BUILD_FLAGS :=
RUN_FLAGS :=

VERSION_MAJOR := 0
VERSION_MINOR := 3
VERSION_MICRO := 0

# 3DS CONFIGURATION #

TITLE := $(NAME)
DESCRIPTION := Zelda Instrument Player
AUTHOR := LeifEricson
PRODUCT_CODE := CTR-P-ORCH
UNIQUE_ID := 0xF1020

SYSTEM_MODE := 64MB
SYSTEM_MODE_EXT := Legacy

ICON_FLAGS :=

ROMFS_DIR := romfs
BANNER_AUDIO := meta/audio.wav
BANNER_IMAGE := meta/banner2.png
ICON := meta/icon.png

# INTERNAL #

include buildtools/make_base
