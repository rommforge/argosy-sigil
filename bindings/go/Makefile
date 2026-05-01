# SPDX-License-Identifier: MPL-2.0
# Convenience wrapper: build sigil's static libs, then run go test/build.
# Consumers can also invoke cmake themselves and use plain `go build`.

SIGIL_ROOT := $(abspath ../..)
BUILD_DIR  := $(SIGIL_ROOT)/build

.PHONY: all build test clean

all: build

$(BUILD_DIR)/libsigil.a:
	cmake -B $(BUILD_DIR) -S $(SIGIL_ROOT) \
		-DSIGIL_BUILD_CLI=OFF -DSIGIL_BUILD_TESTS=OFF
	cmake --build $(BUILD_DIR) --target sigil

build: $(BUILD_DIR)/libsigil.a
	go build ./...

test: $(BUILD_DIR)/libsigil.a
	go test -v ./...

clean:
	rm -rf $(BUILD_DIR)
	go clean ./...
