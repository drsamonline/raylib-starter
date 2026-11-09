# raylib-starter — build, test, and prove the size.
#
#   make            the game  (needs raylib installed)
#   make test       the suite (needs only a C compiler - no engine, no display)
#   make size       build, then fail if the binary is over 10 MB
#   make clean
#
# Point CC at whatever you have; `CC="zig cc" make test` works too.

CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra
LDFLAGS ?=

# raylib: use pkg-config when it is there, otherwise the usual Linux flags.
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS ?= $(shell pkg-config --libs raylib 2>/dev/null || echo -lraylib -lm -lpthread -ldl -lrt -lX11)

BIN := gk
TEST_BIN := gk-tests
MAX_BYTES := 10485760

.PHONY: all test size clean

all: $(BIN)

$(BIN): main.c
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o $@ main.c $(RAYLIB_LIBS) $(LDFLAGS)

# The suite compiles main.c with GK_NO_MAIN, so the core and the game run with
# no window at all. -Wno-unused-function because the headless build has no main.
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): main.c stub/raylib.h stub/raylib_stub.c tests/test_gk.c
	$(CC) $(CFLAGS) -Wno-unused-function -DGK_NO_MAIN -I. -Istub \
		-o $@ main.c stub/raylib_stub.c tests/test_gk.c -lm

# The gate: a build that grows past 10 MB fails here rather than at release.
size: $(BIN)
	@bytes=$$(wc -c < $(BIN) | tr -d ' '); \
	printf '%s is %s bytes (%s KB)\n' "$(BIN)" "$$bytes" "$$((bytes / 1024))"; \
	if [ "$$bytes" -gt $(MAX_BYTES) ]; then \
		printf 'FAIL: over the 10 MB ceiling (%s > %s)\n' "$$bytes" "$(MAX_BYTES)"; \
		exit 1; \
	fi; \
	printf 'OK: under the 10 MB ceiling\n'

clean:
	rm -f $(BIN) $(TEST_BIN) gk-progress.bin test-progress.bin
