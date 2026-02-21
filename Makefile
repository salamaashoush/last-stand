BUILD_DIR := build
BUILD_TYPE ?= Debug
BINARY := $(BUILD_DIR)/LastStand
JOBS := $(shell nproc)

.PHONY: all configure build run clean rebuild release debug

all: build

configure:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

run: build
	cd $(BUILD_DIR) && ./LastStand

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build

release:
	$(MAKE) BUILD_TYPE=Release build

debug:
	$(MAKE) BUILD_TYPE=Debug build
