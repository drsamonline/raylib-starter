# raylib-starter

[![test](https://github.com/drsamonline/raylib-starter/actions/workflows/test.yml/badge.svg)](https://github.com/drsamonline/raylib-starter/actions/workflows/test.yml)
[![License: PolyForm Noncommercial 1.0.0](https://img.shields.io/badge/license-PolyForm%20Noncommercial%201.0.0-blue)](LICENSE)

**One C file, a fixed timestep, scenes, save and an ECS — on raylib.** The C answer to
`game-kit`: the same names, the same behaviour, and the same tests, on a 2.4 MB engine instead
of a browser.

Built for the 10 MB ceiling. A raylib game is one binary that runs on Windows, macOS, Linux,
the web (via Emscripten), Android and a Raspberry Pi.

## Use it

```bash
make              # the game        (needs raylib)
make test         # the suite       (needs only a C compiler)
make size         # fail if over 10 MB
make web          # the game in a browser (needs emsdk + raylib for wasm)
make wasm-core CC="zig cc"   # the core alone as wasm32, needs nothing installed
```

```c
gk_loop_init(&G.loop, 1.0 / 60.0);
gk_loop_tick(&G.loop, GetFrameTime(), loop_step, loop_draw, &G);   // fixed steps, then draw

gk_scenes_go(&G.scenes, "play", &G, NULL);                          // enter / exit / update / render
gk_save("progress.bin", &p, sizeof p);                              // magic + version + CRC32
gk_rng_seed(&G.rng, 12345);                                         // the same sequence as game-kit
```

## What is in it

| | |
| --- | --- |
| `gk_loop` | a fixed timestep: accumulator, a 0.25 s frame clamp, a 240-step cap, and an alpha for interpolation |
| `gk_scenes` | named scenes with `enter` / `exit` / `update` / `render`; an unknown name is refused rather than blanking the screen |
| `gk_input` | key down and pressed-this-frame, driven by the host — no engine types in the core |
| `gk_save` / `gk_load` | a struct on disk with a magic, a version and a CRC32, so a half-written save is caught, not loaded |
| `gk_ecs` | entities, component bags and systems over a bitmask; systems are handed the entity, so component access is O(1); fixed capacity, no allocation |
| `gk_rng` | mulberry32, **bit-identical to `game-kit`'s `rng()`** — verified in the tests |
| `gk_ease` / `gk_spring` / `gk_tween` | the eleven easing curves, the settle-time spring presets and a tween — **matching `motion-kit` to 1e-9** |
| `gk_camera` | world↔screen, follow with easing, zoom, and bounds that keep the view inside the level |
| `gk_tilemap` / `gk_move` | a grid of tiles with solid ids, and collide-and-slide: each axis swept and pushed back out, so a corner slides instead of sticking |
| `gk_clampf`, `gk_lerp`, `gk_dist`, `gk_aabb`, `gk_circle_hit` | the maths `game-kit` exposes, same names |

### Targets

| target | needs | gives |
| --- | --- | --- |
| `make` | raylib + a C compiler | `gk`, the desktop game |
| `make test` | a C compiler | 99 assertions, headless, no display |
| `make size` | raylib | builds, then fails above 10 MB |
| `make web` | emsdk + raylib built for wasm | `web/gk.html` + `.js` + `.wasm` |
| `make wasm-core` | only a wasm32-capable compiler (`CC="zig cc"`) | `web/gk-core.wasm`, and its size |
| `make snake` | raylib + a C compiler | `snake`, the second game |
| `make test-snake` | a C compiler | 35 assertions for the second game |
| `make link-check` | a C compiler | both games' real `main()`, 120 frames headless |
| `make bench` | a C compiler | the ECS benchmark, at a 4096 entity cap |
| `make cross CC="zig cc"` | a cross-capable compiler | the core compiled for five other machines |
| `make size-real` | an unpacked raylib in `RAYLIB_DIR` | the real binary, gated at 10 MB |
| `ndk-build` in `android/` | Android NDK + raylib for Android | the game on Android |

The core is engine-free: `main.c` compiles with `-DGK_NO_MAIN` and then contains no raylib at
all. That is what makes it testable, and it is why the stub exists.

## A second game, to prove it is a starter

`examples/snake.c` includes `main.c` for the core only and brings its own rules, scenes and
save. That is the real test of whether this is a starter or just one game: if a second game
cannot reuse the machinery, the machinery is wrong.

```
make snake          # needs raylib
make test-snake     # needs only a C compiler
```

Its suite covers turning (and refusing a reversal), movement on the ticker, eating, the three
ways to die (wall, self, and the legal move into the cell the tail is leaving), food never
landing on the snake, and the best score round-tripping through the save file.

## The demo game

`main.c` is also a small game, so nothing here is a sketch: three scenes (title, play, over), a
player and six drifting motes that are all entities in the ECS, collection by AABB, a 30-second
round, and a best score saved to disk. The world is 1280×800 inside a 640×400 window, so the
camera has something to do: it follows the player and stops at the level edges. A ring of solid
tiles closes the world, and the player moves through `gk_move`, so it collides with the border
instead of bouncing off it — the motes still bounce, through the ECS system. It exercises every part of the core, which is why the
tests can drive the real game headless.

## Verified

```
make test
184 passed, 0 failed    (the demo game)
35 passed, 0 failed     (snake)
```

What the suite covers, all headless — no engine, no window, no display:

- **the loop**: 60 steps from 60 frames, a 1 s frame clamped to 15 steps, a 0.25 s frame × 4
  giving 60 steps, the step cap holding and resetting the accumulator, alpha in `[0,1)`
- **the RNG**: five values from seed 12345 compared against `game-kit`'s JavaScript output to
  1e-12, seed 1 likewise, `rng_int` inside its bounds and reaching both ends, `rng_pick` in range
- **the motion**: all eleven easing curves checked at t = 0.25/0.5/0.75 against `motion-kit`'s
  JavaScript to 1e-9, the `smooth`/`snappy`/`bouncy` presets against its stiffness and damping,
  and each spring's curve at seven points — so a C game and a JS game move identically
- **scenes**: enter before exit ordering, routing of update and render, an unknown scene refused
- **save**: round-trip, a size mismatch, a foreign magic, a future version, one corrupted payload
  byte caught by the CRC, and an empty payload
- **ECS**: create/get/has/remove/destroy, systems matching only the entities that hold the
  components they name, systems in the order added, a full world refusing more entities
- **the tilemap**: world-to-tile flooring (including negatives), solid ids and the out-of-bounds
  rule, free movement exact, a wall stopping the box flush on each of four sides, a corner
  resolving both axes, sliding along a wall without losing the free axis, a block mid-sweep
  stopping the mover there rather than at the far wall, many 1px steps landing in exactly the
  same place as one big sweep, a closed world, and a null or zero-tile map moving freely
- **the camera**: world↔screen round-trips at zoom 1 and 2, the camera's own point lands at the
  viewport centre, follow closes the gap and arrives, speed 1 snaps and speed 0 does not move,
  bounds stop at half a viewport, zoomed in the view may travel further, a world smaller than the
  viewport centres instead of clamping, unbounded leaves the camera alone, a zero zoom is repaired
- **the game itself**: start goes to play, seven entities spawn, the clock advances, the player
  stays in the world, walking into a mote scores, the round ends on the clock, the best is kept

### What was and was not verified here

Being precise, because "it builds" is easy to overclaim:

- **The suite was compiled and run** with `zig cc` (Zig 0.17.1's bundled clang), the only C
  compiler available in the environment this was written in. 99 assertions, all passing.
- **The raylib-facing half compiles**: `main.c` with `main()`, the three render functions and the
  key mapping compiles against `stub/raylib.h`, which mirrors raylib 5.x signatures. That catches
  a wrong call, not a wrong engine.
- **The real raylib link was not performed here.** raylib needs X11 and GL headers that this
  environment does not have, so `make` and `make size` are the two things to run on your machine.
  `make` itself is not installed here either; the commands those targets wrap were run directly.
- **Size, measured against real raylib**: **902,424 bytes** stripped, linked against raylib 6.2's
  own `libraylib.a`. That is 881 KB, 11.6× under the 10 MB ceiling. `make size-real` and CI both
  check it.
- **The core compiles for wasm32**: `make wasm-core` produced a **1,351-byte** module at `-Os`
  with only the loop reachable, which is the portability proof that does not depend on
  Emscripten being installed. CI builds it on every push and fails above **64 KB**, so a
  retained libc or a creeping dependency is caught rather than noticed later.
- **Both suites run in CI**: 100 assertions for the demo game and 35 for snake, under gcc on
  Ubuntu, in the same workflow that builds the wasm core.
- **Both games actually run**: `make link-check` links the real `main()` of each game (not the
  `GK_NO_MAIN` core) against the stub and runs it for 120 frames. The log reads
  `gk frames=120 draws=480` and `snake frames=120 draws=480` — the loop, the scene machine, the
  render path and the key mapping all execute. This is the closest a machine without a GPU can
  get to "it runs".
- **Capacities are compile-time**: `-DGK_MAX_ENTITIES=8` was run through the whole suite to
  prove the core adapts, and the default is 256.
- **The Emscripten build was not run here** — emsdk is not installable in the environment this
  was written in. `make web` and `web/shell.html` are the pieces to run with emsdk present.
- **What still is not proven**: that raylib itself draws. Everything above runs against the stub,
  so a wrong `DrawText` argument or a bad window flag would only show up on a machine with a GPU
  and a display.

## The same motion as the JS kits

`motion-kit` and this starter now share a vocabulary, and the tests hold them together:

| From motion-kit | In C | Agreement |
| --- | --- | --- |
| 11 cubic-bezier curves | `gk_ease_named("out")` etc. | 1e-9 at three points each |
| `smooth(300)` stiffness / damping | `gk_smooth(300)` | 947.203211111 / 61.553333333 |
| `snappy(250)` | `gk_snappy(250)` | 1148.803236 / 54.2304 |
| `bouncy(280)` | `gk_bouncy(280)` | 662.878584183 / 36.045 |
| spring value at time t | `gk_spring_at(s, ms)` | 1e-9 across seven samples |
| `tween(from, to, dur, ease)` | `gk_tween(...)` | endpoints, midpoint, clamp |

The demo game uses it: collecting a mote eases a ring outward on the `out` curve, which is the
same curve the JS kits use for the same gesture.

## Warning-free

Every configuration compiles with `-Wall -Wextra -Werror` — the real build, the headless core,
the stub link, both games, both suites, the benchmark, the five cross targets and the wasm
module. Seven configurations, no warnings.

That took two things: a `GK_API` attribute on the core's entry points, because a single
translation unit that does not use all of them would warn about the rest; and a
`GK_EXPORT` guard around the wasm-only `export_name` attribute, which the other five targets
would otherwise reject.

## The real binary, measured

`make size-real` links the game against raylib 6.2's own `libraylib.a` — not the stub — and gates
the result. Measured on raylib 6.2, linux amd64:

| | bytes | |
| --- | --- | --- |
| `libraylib.a` | 2,847,000 | the engine, static |
| game + engine, unstripped | 1,091,856 | dead code already dropped by the linker |
| game + engine, **stripped** | **904,696** | 883 KB, with the camera and tilemap |
| 10 MB ceiling | 10,485,760 | **11.6× headroom** |

`tools/x11_stubs.c` carries six weak X11 symbols so this links on a machine with no X server;
on a machine with X11 the real symbols win and the binary runs. CI does this link and the gate
on every push, downloading raylib 6.2 itself.

Plain `make` needs the X11 development headers on Linux (raylib's static library references
`XConvertSelection` and five others). Where they are missing, `make size-real` is the target
that still links, because it carries the weak stubs.

## Portability, measured

The core has no engine in it, so it cross-compiles anywhere a C compiler runs. `make cross` does
this on every push:

| Target | Core object | Verified |
| --- | --- | --- |
| `aarch64-linux-musl` | 4,024 B | ✅ |
| `arm-linux-musleabihf` | 2,964 B | ✅ |
| `riscv64-linux-musl` | 4,736 B | ✅ |
| `x86_64-windows-gnu` | 9,986 B | ✅ |
| `aarch64-macos` | 3,048 B | ✅ |
| `wasm32-wasi-musl` | 1,351 B | ✅ |
| `*-linux-android` | — | ❌ needs bionic headers, i.e. the NDK |

Android is scaffolded in `android/` (`ndk-build`, two `.mk` files, a manifest note) and is
honestly marked as unbuilt here. ARM is what Android devices run, so the instruction selection
and float behaviour are already exercised by the two ARM rows; what is untested is bionic and
raylib's Android glue.

## Why not archetype storage

The textbook ECS groups entities by their component set and stores component data inline, so a
system walks only matching entities and the data is contiguous. It is genuinely faster — and
`make bench` measures whether that matters here:

| entities | scan ns/entity | archetype ns/entity | ratio |
| --- | --- | --- | --- |
| 256 | 8.1 | 0.46 | 17.7× |
| 1,024 | 17.2 | 0.47 | 36.6× |
| 4,096 | 8.9 | 2.18 | 4.1× |

The ratio is real and the absolute numbers are not: at the 256-entity default, one system run is
**2.1 µs** scanning against **0.12 µs** on archetypes — in a 16.6 ms frame at 60 fps that is
0.013% against 0.0007%. Archetype storage would cost component registration, inline storage and
archetype churn on every add and remove, to save a hundredth of a millisecond. It stays out, and
this table is here so the decision can be revisited if a game ever needs thousands of entities.

**The benchmark earned its keep anyway**: the first run showed 260 ns/entity, which turned out to
be a linear id lookup inside every component access — an O(n²) system. Systems now receive the
entity itself, `gk_ecs_comp(e, comp)` is O(1), and the same measurement dropped to 8.1 ns: a 30×
win on the shipped design.

## The stub

`stub/raylib.h` and `stub/raylib_stub.c` are the slice of raylib this starter uses: a virtual
clock, a scripted keyboard, and drawing that counts calls instead of drawing. Signatures match
raylib, so the same source compiles both ways, and adding a raylib call means adding it to the
stub too — the stub is the contract.

## Consumers

None yet — this is the first C kit in the fleet, written to be the starting point for the small
games. It is not wired into anything and changes no app.

## Licence

PolyForm Noncommercial 1.0.0. (c) 2026 Dr Sohil Momin. Part of the Forge system — kits open,
orchestrator licensed. raylib itself is zlib licensed and is not vendored here.
