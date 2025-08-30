CMAKE ?= $(shell which cmake 2>/dev/null)
DOCKER ?= $(shell which docker 2>/dev/null)
GO ?= $(shell which go 2>/dev/null)
GIT ?= $(shell which git 2>/dev/null)
BUILD_DIR ?= build
JOBS ?= 8

# Icon generation settings
TABLER_FILLED_DIR ?= third_party/tabler-icons/icons/filled
TABLER_OUTLINE_DIR ?= third_party/tabler-icons/icons/outline
ICON_OUT_DIR ?= src/icons/generated
ICON_UPSCALE ?= 1  # geometric upscale factor applied before rounding (improves zoom fidelity)
SVG2PIX := $(BUILD_DIR)/svg2pix

all: dep-cmake dep-go mkdir $(SVG2PIX) icons build

.PHONY: build
build:
	@$(CMAKE) -B ${BUILD_DIR} .
	@$(CMAKE) --build ${BUILD_DIR} -- -j$(JOBS)

# Generate the documentation
.PHONY: docs
docs: dep-docker 
	@echo
	@echo make docs
	@${DOCKER} run -v .:/data greenbone/doxygen doxygen /data/doxygen/Doxyfile

# Build svg2pix code generator
$(SVG2PIX): dep-go cmd/svg2pix/main.go go.mod
	@echo Building svg2pix tool
	${GO} build -o $@ ./cmd/svg2pix

# Generate icon C sources from Tabler icons (filled + outline)
.PHONY: icons
icons: $(SVG2PIX) submodule
	@echo Generating icon sources from Tabler \(filled + outline\)
	@install -d -m 755 $(ICON_OUT_DIR)/filled
	@install -d -m 755 $(ICON_OUT_DIR)/outline
	@$(SVG2PIX) -in $(TABLER_FILLED_DIR) -out $(ICON_OUT_DIR)/filled -prefix vg_icon_f_ -upscale $(ICON_UPSCALE)
	@$(SVG2PIX) -in $(TABLER_OUTLINE_DIR) -out $(ICON_OUT_DIR)/outline -prefix vg_icon_o_ -upscale $(ICON_UPSCALE)
	@echo Icon generation complete: $(ICON_OUT_DIR)

# Update submodules
.PHONY: submodule
submodule: dep-git
	@echo
	@echo "checking out submodules"
	@${GIT} submodule update --init --recursive

.PHONY: mkdir
mkdir:
	@install -d -m 755 $(BUILD_DIR)

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(ICON_OUT_DIR)

.PHONY: dep-cmake
dep-cmake:
	@test -f "${CMAKE}" && test -x "${CMAKE}" || (echo "Missing CMAKE: ${CMAKE}" && exit 1)

.PHONY: dep-docker
dep-docker:
	@test -f "${DOCKER}" && test -x "${DOCKER}" || (echo "Missing DOCKER: ${DOCKER}" && exit 1)

.PHONY: dep-go
dep-go:
	@test -f "${GO}" && test -x "${GO}" || (echo "Missing Go toolchain: ${GO}" && exit 1)

.PHONY: dep-git
dep-git:
	@test -f "${GIT}" && test -x "${GIT}" || (echo "Missing GIT: ${GIT}" && exit 1)
