# raylib-starter — build, test, and prove the size.
#
#   make            the game  (needs raylib installed)
#   make test       the suite (needs only a C compiler - no engine, no display)
#   make size       build, then fail if the binary is over 10 MB
#   make clean
#
# Point CC at whatever you have; `CC="zig cc" make test` works too.

CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
LDFLAGS ?=

# raylib: use pkg-config when it is there, otherwise the usual Linux flags.
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS ?= $(shell pkg-config --libs raylib 2>/dev/null || echo -lraylib -lm -lpthread -ldl -lrt -lX11)

BIN := gk
TEST_BIN := gk-tests
SNAKE_BIN := snake
SNAKE_TEST_BIN := snake-tests
MAX_BYTES := 10485760

# Web: the full game needs Emscripten and a raylib built for wasm; the core
# alone needs nothing installed, and `make wasm-core` proves it.
EMCC ?= emcc
RAYLIB_SRC ?= ../raylib

.PHONY: all test size size-real snake test-snake link-check web wasm-core bench cross clean

all: $(BIN)

$(BIN): main.c
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o $@ main.c $(RAYLIB_LIBS) $(LDFLAGS)

# The suite compiles main.c with GK_NO_MAIN, so the core and the game run with
# no window at all. because the headless build has no main.
test: $(TEST_BIN) $(SNAKE_TEST_BIN)
	./$(TEST_BIN)
	./$(SNAKE_TEST_BIN)

$(TEST_BIN): main.c stub/raylib.h stub/raylib_stub.c tests/test_gk.c
	$(CC) $(CFLAGS) -DGK_NO_MAIN -I. -Istub \
		-o $@ main.c stub/raylib_stub.c tests/test_gk.c -lm

# The second game: the same core, its own rules.
snake: examples/snake.c main.c
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -I. -o $(SNAKE_BIN) examples/snake.c $(RAYLIB_LIBS) $(LDFLAGS)

test-snake: $(SNAKE_TEST_BIN)
	./$(SNAKE_TEST_BIN)

$(SNAKE_TEST_BIN): examples/snake.c tests/test_snake.c stub/raylib.h stub/raylib_stub.c
	$(CC) $(CFLAGS) -DSNAKE_HEADLESS -I. -Istub \
		-o $@ examples/snake.c stub/raylib_stub.c tests/test_snake.c -lm

# Measure the real thing: the game linked against a real raylib, not the stub.
# RAYLIB_DIR points at an unpacked raylib release (include/ and lib/).
# The weak X11 stubs let this link on a machine with no X server; on one with X11
# the real symbols win and the binary can actually run.
RAYLIB_DIR ?= ../raylib

size-real:
	@test -f $(RAYLIB_DIR)/lib/libraylib.a || { \
		printf 'needs raylib: unpack a release into %s\n' "$(RAYLIB_DIR)"; exit 1; }
	$(CC) $(CFLAGS) -I$(RAYLIB_DIR)/include -o gk-real \
		main.c tools/x11_stubs.c $(RAYLIB_DIR)/lib/libraylib.a \
		-lm -lpthread -ldl -lrt $(LDFLAGS)
	@bytes=$$(wc -c < gk-real | tr -d ' '); \
	printf 'gk-real is %s bytes (%s KB)\n' "$$bytes" "$$((bytes / 1024))"; \
	if [ "$$bytes" -gt $(MAX_BYTES) ]; then \
		printf 'FAIL: over the 10 MB ceiling\n'; exit 1; \
	fi; \
	printf 'OK: under the 10 MB ceiling, %sx headroom\n' "$$(( $(MAX_BYTES) / bytes ))"

# Link the REAL main() (not the GK_NO_MAIN core) against the stub and run it for
# a fixed number of frames. GK_AUTOPLAY presses start and the arrows on a
# schedule, so the run reaches the play scene and the tile and sprite renderers
# execute, not just the title. The draw count is asserted for that reason: a run
# that never left the title draws 480, a run in play draws far more.
LINK_FRAMES ?= 120

link-check: main.c examples/snake.c stub/raylib.h stub/raylib_stub.c
	$(CC) $(CFLAGS) -I. -Istub -o gk-linkcheck main.c stub/raylib_stub.c -lm
	$(CC) $(CFLAGS) -I. -Istub -o snake-linkcheck examples/snake.c stub/raylib_stub.c -lm
	@for game in gk snake; do \
		out=$$(GK_AUTOPLAY=1 GK_FRAMES=$(LINK_FRAMES) ./$$game-linkcheck); \
		frames=$$(printf '%s' "$$out" | sed -n 's/.*frames=\([0-9]*\).*/\1/p'); \
		draws=$$(printf '%s' "$$out" | sed -n 's/.*draws=\([0-9]*\).*/\1/p'); \
		printf '%-6s frames=%s draws=%s\n' "$$game" "$$frames" "$$draws"; \
		[ "$$frames" = "$(LINK_FRAMES)" ] || { printf 'FAIL: %s ran %s frames\n' "$$game" "$$frames"; exit 1; }; \
		[ "$$draws" -gt 500 ] || { printf 'FAIL: %s drew only %s - the play scene never ran\n' "$$game" "$$draws"; exit 1; }; \
	done
	@printf 'OK: both games ran %s frames headless, past the title\n' "$(LINK_FRAMES)"

# Cross-compile the core for other machines. Needs a compiler that can target
# them (zig cc can, for all of these) - no NDK, no SDK, no sysroot.
CROSS_TARGETS := aarch64-linux-musl arm-linux-musleabihf riscv64-linux-musl x86_64-windows-gnu aarch64-macos

cross: web/wasm_smoke.c main.c
	@for t in $(CROSS_TARGETS); do \
		printf '%-24s ' "$$t"; \
		if $(CC) -target $$t -std=c11 -O2 -DGK_NO_MAIN -I. -Istub -c web/wasm_smoke.c -o /tmp/gk-$$t.o 2>/tmp/gk-cross.err; then \
			printf 'OK  %s bytes\n' "$$(wc -c < /tmp/gk-$$t.o)"; \
		else \
			printf 'FAIL\n'; head -3 /tmp/gk-cross.err; exit 1; \
		fi; \
	done

# The ECS benchmark: scan-and-mask against archetype storage. Built with a raised
# entity cap so the larger sizes can be measured too.
bench: bench/ecs_bench.c main.c
	$(CC) $(CFLAGS) -DGK_NO_MAIN -DGK_MAX_ENTITIES=4096 \
		-I. -Istub -o ecs-bench bench/ecs_bench.c -lm
	./ecs-bench

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
	rm -f $(BIN) $(TEST_BIN) $(SNAKE_BIN) $(SNAKE_TEST_BIN) ecs-bench gk-linkcheck snake-linkcheck gk-real \
		gk-progress.bin test-progress.bin snake-best.bin \
		web/gk.html web/gk.js web/gk.wasm web/gk-core.wasm
