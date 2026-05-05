.PHONY: build build-fast clean rebuild debug install help

BUILD_DIR ?= build
BUILD_TYPE ?= Release

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CMAKE_GENERATOR ?= Unix Makefiles
endif
ifeq ($(UNAME_S),Darwin)
    CMAKE_GENERATOR ?= Unix Makefiles
endif
ifeq ($(OS),Windows_NT)
    CMAKE_GENERATOR ?= Visual Studio 17 2022
endif

help:
	@echo "LECmd Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make build      - Configure and build (Release)"
	@echo "  make build-fast - Configure and build with parallel compilation"
	@echo "  make debug      - Build with debug symbols"
	@echo "  make clean      - Remove build directory"
	@echo "  make rebuild    - Clean and rebuild"
	@echo "  make install    - Install to system"
	@echo ""
	@echo "Options:"
	@echo "  BUILD_DIR=path  - Set build directory (default: build)"
	@echo "  BUILD_TYPE=type - Set build type (Release/Debug, default: Release)"

build:
	@cmake -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-G "$(CMAKE_GENERATOR)" \
		-S .
	@cmake --build $(BUILD_DIR) --config $(BUILD_TYPE)

build-fast:
	@cmake -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-G "$(CMAKE_GENERATOR)" \
		-S .
	@cmake --build $(BUILD_DIR) --config $(BUILD_TYPE) --parallel $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

debug:
	@$(MAKE) build BUILD_TYPE=Debug

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean build

install: build
	@cmake --install $(BUILD_DIR) --config $(BUILD_TYPE)
