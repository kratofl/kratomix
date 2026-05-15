.DEFAULT_GOAL := help

ROOT_DIR := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))

CMAKE ?= cmake
PYTHON ?= python3
MAKE_BIN ?= $(MAKE)
CMAKE_GENERATOR ?= Unix Makefiles
BUILD_DIR ?= build
CONFIG ?= Release
JOBS ?= 4

include $(ROOT_DIR)/mk/plugin.mk
include $(ROOT_DIR)/mk/build.mk
include $(ROOT_DIR)/mk/package.mk
include $(ROOT_DIR)/mk/scaffold.mk
