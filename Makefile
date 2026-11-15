CMAKE ?= cmake
BUILD_DIR ?= build
CONFIG ?= Release
GENERATOR ?=
CMAKE_GENERATOR = $(if $(GENERATOR),-G "$(GENERATOR)",)

.PHONY: all configure build test clean rebuild install help

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) $(CMAKE_GENERATOR) -DCMAKE_BUILD_TYPE=$(CONFIG)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG)

test: build
	ctest --test-dir $(BUILD_DIR) -C $(CONFIG) --output-on-failure

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)

rebuild: clean build

install: build
	$(CMAKE) --install $(BUILD_DIR) --config $(CONFIG)

help:
	@echo "YComp Makefile targets:"
	@echo "  all      Configure and build (default)"
	@echo "  configure Configure the CMake build directory"
	@echo "  build    Build YComp"
	@echo "  test     Build and run integration tests"
	@echo "  clean    Remove the build directory"
	@echo "  rebuild  Clean and build from scratch"
	@echo "  install  Install YComp using CMake"
