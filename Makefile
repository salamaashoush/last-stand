BUILD_DIR := build
BUILD_TYPE ?= Debug
BINARY := $(BUILD_DIR)/LastStand
JOBS := $(shell nproc)

WEB_BUILD_DIR := build-web
TEST_BUILD_DIR := build-test
EMSDK_ENV := source $(HOME)/emsdk/emsdk_env.sh > /dev/null 2>&1

.PHONY: all configure build run clean rebuild release debug web-configure web web-serve test format format-check tidy

all: build

configure:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

run: build
	./$(BUILD_DIR)/LastStand

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build

release:
	$(MAKE) BUILD_TYPE=Release build

debug:
	$(MAKE) BUILD_TYPE=Debug build

web-configure:
	$(EMSDK_ENV) && emcmake cmake -B $(WEB_BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

web: web-configure
	$(EMSDK_ENV) && cmake --build $(WEB_BUILD_DIR) -j$(JOBS)

web-serve: web
	miniserve $(WEB_BUILD_DIR) --index LastStand.html -p 8080

test:
	cmake -B $(TEST_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
	cmake --build $(TEST_BUILD_DIR) -j$(JOBS)
	cd $(TEST_BUILD_DIR) && ctest --output-on-failure

format:
	find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

format-check:
	find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror

tidy:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR) -j$(JOBS)
	find src -name '*.cpp' -o -name '*.hpp' | xargs clang-tidy -p $(BUILD_DIR)
