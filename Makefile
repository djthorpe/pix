CMAKE ?= $(shell which cmake 2>/dev/null)
BUILD_DIR ?= build

all: dep-cmake mkdir build

.PHONY: build
build:
	@$(CMAKE) -B ${BUILD_DIR} .
	@$(CMAKE) --build ${BUILD_DIR}

.PHONY: mkdir
mkdir:
	@install -d -m 755 $(BUILD_DIR)

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)

.PHONY: dep-cmake
dep-cmake:
	@test -f "${CMAKE}" && test -x "${CMAKE}" || (echo "Missing CMAKE: ${CMAKE}" && exit 1)
