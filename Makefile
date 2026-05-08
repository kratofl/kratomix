.DEFAULT_GOAL := help

ROOT_DIR := $(abspath $(dir $(firstword $(MAKEFILE_LIST))))

CMAKE ?= cmake
PYTHON ?= python3
CMAKE_GENERATOR ?= Unix Makefiles
BUILD_DIR ?= build
CONFIG ?= Release
JOBS ?= 4

include $(ROOT_DIR)/mk/plugin.mk
include $(ROOT_DIR)/mk/build.mk
include $(ROOT_DIR)/mk/scaffold.mk
