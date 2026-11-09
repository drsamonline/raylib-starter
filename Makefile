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

# Web: the full game needs Emscripten and a raylib built for wasm; the core
# alone needs nothing installed, and `make wasm-core` proves it.
EMCC ?= emcc
RAYLIB_SRC ?= ../raylib

.PHONY: all test size web wasm-core clean

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

# The whole game in a browser. Needs emsdk on PATH and a raylib built for web:
#   cd $(RAYLIB_SRC)/src && emmake make PLATFORM=PLATFORM_WEB
web: main.c web/shell.html
	$(EMCC) $(CFLAGS) -I. -o web/gk.html main.c \
		$(RAYLIB_SRC)/src/libraylib.web.a \
		-s USE_GLFW=3 -s WASM=1 -s ASYNCIFY -s ALLOW_MEMORY_GROWTH=1 \
		-s TOTAL_MEMORY=67108864 --shell-file web/shell.html
	@printf 'web/gk.html written\n'

# The core alone, as wasm32, with no engine and no Emscripten. Works with
#   make wasm-core CC="zig cc"
wasm-core: web/wasm_smoke.c main.c
	$(CC) -target wasm32-wasi-musl -std=c11 -Os -DGK_NO_MAIN -I. -Istub \
		-Wl,--no-entry -Wl,--export=gk_smoke -o web/gk-core.wasm web/wasm_smoke.c -lm
	@printf 'web/gk-core.wasm is %s bytes\n' "$$(wc -c < web/gk-core.wasm)"

clean:
	rm -f $(BIN) $(TEST_BIN) gk-progress.bin test-progress.bin web/gk.html web/gk.js web/gk.wasm web/gk-core.wasm
