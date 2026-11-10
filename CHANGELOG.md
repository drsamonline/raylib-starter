# Changelog
## 1.0.0 — 2026-11-10
### Added
- `examples/harvest.c`: a second game on the whole stack, with its own 43-assertion suite. It is
  the integration test the per-feature suites cannot be
- `gk_scenes_set_render`, so a headless build registers scenes with no renderer and the real build
  attaches one

### Fixed
- `gk_scenes_add` kept the caller's pointer, so a compound-literal scene dangled as soon as the
  registering function returned. The registry now copies the scene and tracks it by index. Found by
  the second game as a segfault, not by any suite
- `gk_tiles_visible` takes a frame map, so each visible tile carries the atlas frame for its id
  and a renderer gets a source rect and a destination rect from one walk
- the demo's round is driven by a `gk_machine`: warmup → live → finished, with the scene drawing
  "ready" during warmup. The scene owns the screen, the machine owns the phase
- `gk_str_eq`, a null-safe compare, because `gk_machine_current` can return NULL
- `gk_machine`: the same shape as `game-kit`'s `Machine` — named states, a transition table,
  enter/exit handlers, `can`/`send` refusing what the table does not allow
- `gk_tween_chain`: tweens stepped in order, with the overshoot carried across a boundary
- `gk_audio`: a thin wrapper over raylib's audio device, with its own headless suite that checks
  call routing and volume clamping - what it cannot check is that anything is audible
- 14 shake assertions, 15 chain assertions, 28 audio assertions
- `gk_shake`, a camera effect in the elapsed-time shape: `make`/`start`/`step`/`running`, decaying offset, wired to each collect in the demo
- `gk_ecs_queue_destroy` + `gk_ecs_flush` (deferred removal)
- `gk_tween_t`, a tween with state (`make`/`step`/`value`/`retarget`), and `gk_camera_deadzone`, which holds the camera still for small steps
- `gk_atlas`/`gk_anim`/`gk_sprite_dst` and `gk_tiles_visible`: frame rectangles, animation frames, and the tiles a camera can see
- `gk_ecs_queue_destroy` + `gk_ecs_flush`, so a loop can destroy what it is iterating
- a CHANGELOG, and the demo now draws its border from `gk_tiles_visible` and animates its motes
- the core: `gk_loop` fixed timestep, `gk_scenes`, `gk_input`, `gk_save`/`gk_load` with magic, version and CRC32, `gk_ecs`, `gk_rng`, `gk_camera`, `gk_tilemap`/`gk_move`
- `gk_atlas`/`gk_anim`/`gk_sprite_dst` for frame rectangles and animation, `gk_tiles_visible` for the tiles a camera can see
- `gk_ease`/`gk_spring`/`gk_tween` plus a stateful `gk_tween_t`, matching motion-kit to 1e-9
- two games: the demo (camera, tilemap border, animated motes) and snake
- a headless stub, so the whole game runs in CI with no engine, window or display
- `make size-real`, which links raylib 6.2's own library and gates the binary at 10 MB
- `make cross` for five architectures, `make wasm-core` for a wasm32 module
- Android `ndk-build` scaffolding, marked unbuilt

### Verified
- 326 assertions in the demo suite, 28 in the audio suite, 35 in snake, all passing under gcc and zig cc
- both games run 120 frames headless past the title scene (2551 and 712 draw calls), driven by a scripted autoplay so the play renderer executes
- real binary: 931,272 bytes stripped against raylib 6.2 - 11x under the ceiling
- warning-free under `-Wall -Wextra -Werror` in eight configurations
- the core cross-compiles for aarch64, armv7, riscv64, x86_64-windows, aarch64-macos and wasm32

### Not verified here
- that raylib draws on screen - needs a GPU and a display
- `make web` - needs emsdk
- `ndk-build` - needs the Android NDK

PolyForm Noncommercial 1.0.0. (c) 2026 Dr Sohil Momin.
