/* web/wasm_smoke.c - the core with an exported entry, so a cross-compile keeps
 * the core in the object instead of dropping it as unreachable.
 *
 * `make wasm-core` links it for wasm32 and reports the size; `make cross`
 * compiles it for five other machines. Both are the portability proof, and
 * neither needs an engine.
 */
#include "main.c"

/* export_name is a wasm attribute; the other five targets would warn about it,
   and this file is their probe too. */
#if defined(__wasm__)
#define GK_EXPORT(name) __attribute__((export_name(name)))
#else
#define GK_EXPORT(name)
#endif

GK_EXPORT("gk_smoke")
int gk_smoke(int ticks) {
    gk_loop l;
    gk_loop_init(&l, 1.0 / 60.0);
    int total = 0;
    for (int i = 0; i < ticks; i++) total += gk_loop_tick(&l, 1.0 / 60.0, NULL, NULL, NULL);
    return total;
}
