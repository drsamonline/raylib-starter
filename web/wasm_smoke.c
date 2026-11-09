/* web/wasm_smoke.c - the core, linked for wasm32 with no engine at all.
 *
 * `make wasm-core` builds this and reports the size: it is the proof that the
 * core is portable, independent of whether the full raylib web build works.
 */
#include "main.c"
__attribute__((export_name("gk_smoke")))
int gk_smoke(int ticks) {
    gk_loop l;
    gk_loop_init(&l, 1.0 / 60.0);
    int total = 0;
    for (int i = 0; i < ticks; i++) total += gk_loop_tick(&l, 1.0 / 60.0, NULL, NULL, NULL);
    return total;
}
