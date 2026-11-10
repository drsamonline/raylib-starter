# Changelog
## 1.0.0 — 2026-11-10
### Added
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
- 266 assertions in the demo suite, 35 in snake, all passing under gcc and zig cc
- both games run 120 frames headless past the title scene (2551 and 712 draw calls), driven by a scripted autoplay so the play renderer executes
- real binary: 931,272 bytes stripped against raylib 6.2 - 11x under the ceiling
- warning-free under `-Wall -Wextra -Werror` in eight configurations
- the core cross-compiles for aarch64, armv7, riscv64, x86_64-windows, aarch64-macos and wasm32

### Not verified here
- that raylib draws on screen - needs a GPU and a display
- `make web` - needs emsdk
- `ndk-build` - needs the Android NDK

PolyForm Noncommercial 1.0.0. (c) 2026 Dr Sohil Momin.
