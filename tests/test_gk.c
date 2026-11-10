/* tests/test_gk.c — the suite. No engine, no window, no framework.
 *
 *   cc -std=c11 -DGK_NO_MAIN -I stub main.c stub/raylib_stub.c tests/test_gk.c
 *
 * main.c defines the core and the game; GK_NO_MAIN drops the raylib half, so
 * everything below tests the code the game actually runs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "main.c"

static int pass = 0, fail = 0;

static void ok(const char *name, int cond, const char *detail) {
    if (cond) pass++; else fail++;
    printf("%s  %s", cond ? "PASS" : "FAIL", name);
    if (detail) printf("  (%s)", detail);
    printf("\n");
}

#define OK(n, c) ok(n, (c), NULL)
static char detail[128];
#define OKD(n, c, fmt, ...) do { snprintf(detail, sizeof detail, fmt, __VA_ARGS__); ok(n, (c), detail); } while (0)

static int near(double a, double b, double tol) { return fabs(a - b) <= tol; }

/* ---------- maths ---------- */
static void test_maths(void) {
    OK("clamp keeps a value inside", gk_clampf(5, 0, 10) == 5);
    OK("clamp lifts a value below", gk_clampf(-3, 0, 10) == 0);
    OK("clamp lowers a value above", gk_clampf(99, 0, 10) == 10);
    OK("lerp at 0 is the start", gk_lerp(10, 20, 0) == 10);
    OK("lerp at 1 is the end", gk_lerp(10, 20, 1) == 20);
    OK("lerp at half", near(gk_lerp(10, 20, 0.5), 15, 1e-6));
    OK("dist of 3-4-5", near(gk_dist(0, 0, 3, 4), 5, 1e-6));
    OK("dist to itself is zero", gk_dist(7, 7, 7, 7) == 0);

    gk_rect a = { 0, 0, 10, 10 }, b = { 5, 5, 10, 10 }, c = { 20, 20, 5, 5 };
    OK("aabb overlap", gk_aabb(a, b) == 1);
    OK("aabb apart", gk_aabb(a, c) == 0);
    OK("aabb touching edges do not overlap", gk_aabb(a, (gk_rect){ 10, 0, 5, 5 }) == 0);
    OK("aabb contains", gk_aabb(a, (gk_rect){ 2, 2, 2, 2 }) == 1);

    gk_circle c1 = { 0, 0, 5 }, c2 = { 9, 0, 5 }, c3 = { 11, 0, 5 };
    OK("circles touch at the sum of radii", gk_circle_hit(c1, c2) == 1);
    OK("circles apart", gk_circle_hit(c1, c3) == 0);
}

/* ---------- rng: the same sequence as game-kit ---------- */
static void test_rng(void) {
    gk_rng r;
    gk_rng_seed(&r, 12345);
    /* these five are what game-kit's rng(12345) returns in JavaScript */
    double want[] = { 0.979728267760947, 0.306752264499664, 0.484205421525985,
                      0.817934412509203, 0.509428369347006 };
    int same = 1;
    for (int i = 0; i < 5; i++) {
        double got = gk_rng_next(&r);
        if (fabs(got - want[i]) > 1e-12) same = 0;
    }
    OKD("rng matches game-kit bit for bit (seed 12345)", same, "%.15f vs %.15f", gk_rng_next(&(gk_rng){ 12345 }), want[0]);

    gk_rng_seed(&r, 1);
    double s1 = gk_rng_next(&r);
    OKD("rng matches game-kit (seed 1)", fabs(s1 - 0.627073940588161) < 1e-12, "%.15f", s1);

    gk_rng_seed(&r, 0);
    double z = gk_rng_next(&r);
    OK("a zero seed falls back rather than repeating", z > 0.0 && z < 1.0);

    gk_rng_seed(&r, 99);
    int in_range = 1, lo = 99, hi = -99;
    for (int i = 0; i < 2000; i++) {
        int v = gk_rng_int(&r, 5, 10);
        if (v < 5 || v > 10) in_range = 0;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    OK("rng_int stays inside its bounds", in_range);
    OK("rng_int reaches both ends", lo == 5 && hi == 10);

    gk_rng_seed(&r, 3);
    int pick_ok = 1;
    for (int i = 0; i < 500; i++) {
        int p = gk_rng_pick(&r, 4);
        if (p < 0 || p > 3) pick_ok = 0;
    }
    OK("rng_pick stays in the array", pick_ok);
    gk_rng_seed(&r, 3);
    OK("rng_pick of one element is 0", gk_rng_pick(&r, 1) == 0);
}


/* ---------- motion: the same curves and springs as motion-kit ---------- */
static int ease_is_monotonic(const gk_ease_t *e) {
    double last = -1;
    for (double p = 0; p <= 1.0001; p += 0.01) {
        double v = gk_ease(e, p);
        if (v < last - 1e-9) return 0;
        last = v;
    }
    return 1;
}

static void test_motion(void) {
    /* the reference values are motion-kit's own output, in JavaScript */
    struct { const char *name; double at25, at50, at75; } ref[] = {
        { "soft",     0.779131353905, 0.954809411668, 0.992348713951 },
        { "exit",     0.098626562350, 0.324814610578, 0.630084361093 },
        { "out",      0.600300054782, 0.875094244823, 0.976024025500 },
        { "inOut",    0.087230029862, 0.516875000000, 0.931267701740 },
        { "outBack",  0.768725537161, 1.066202719226, 1.107019768530 },
        { "outQuint", 0.775381656265, 0.965982780444, 0.997362255473 },
        { "outCirc",  0.838077510531, 0.957911779367, 0.992719998615 },
        { "css",      0.408510593016, 0.802403387695, 0.960459072953 },
        { "cssIn",    0.093464650994, 0.315356812506, 0.621861869174 },
        { "toast",    0.764864719749, 0.961382593944, 0.996894217613 },
        { "linear",   0.250000001431, 0.500000000000, 0.749999998569 },
    };
    for (unsigned i = 0; i < sizeof ref / sizeof ref[0]; i++) {
        const gk_ease_t *e = gk_ease_named(ref[i].name);
        if (!e) { OK("every named curve exists", 0); continue; }
        int same = near(gk_ease(e, 0.25), ref[i].at25, 1e-9) &&
                   near(gk_ease(e, 0.5), ref[i].at50, 1e-9) &&
                   near(gk_ease(e, 0.75), ref[i].at75, 1e-9);
        OKD(ref[i].name, same, "%.12f %.12f %.12f",
            gk_ease(e, 0.25), gk_ease(e, 0.5), gk_ease(e, 0.75));
    }
    OK("eleven curves are named", GK_EASE_COUNT == 11);
    OK("an unknown curve is null", gk_ease_named("nope") == NULL);

    /* endpoints and shape */
    for (int i = 0; i < GK_EASE_COUNT; i++) {
        const gk_ease_t *e = &GK_EASES[i].curve;
        if (gk_ease(e, 0) != 0 || gk_ease(e, 1) != 1) { OK("every curve starts at 0 and ends at 1", 0); break; }
        if (i == GK_EASE_COUNT - 1) OK("every curve starts at 0 and ends at 1", 1);
    }
    OK("out is front-loaded", gk_ease(gk_ease_named("out"), 0.25) > 0.25);
    OK("exit is back-loaded", gk_ease(gk_ease_named("exit"), 0.25) < 0.25);
    OK("outBack overshoots past 1", gk_ease(gk_ease_named("outBack"), 0.75) > 1.0);
    OK("inOut is symmetric about the middle", near(gk_ease(gk_ease_named("inOut"), 0.5), 0.5, 0.02));
    OK("a curve is monotonic where it should be", ease_is_monotonic(gk_ease_named("out")));

    /* springs: the presets must land on the same stiffness and damping */
    gk_spring_t sm = gk_smooth(300), sn = gk_snappy(250), bo = gk_bouncy(280);
    OKD("smooth(300) stiffness matches motion-kit", near(sm.stiffness, 947.203211111, 1e-6), "%.9f", sm.stiffness);
    OKD("smooth(300) damping matches motion-kit", near(sm.damping, 61.553333333, 1e-6), "%.9f", sm.damping);
    OKD("snappy(250) stiffness matches", near(sn.stiffness, 1148.803236000, 1e-6), "%.9f", sn.stiffness);
    OKD("snappy(250) damping matches", near(sn.damping, 54.230400000, 1e-6), "%.9f", sn.damping);
    OKD("bouncy(280) stiffness matches", near(bo.stiffness, 662.878584183, 1e-6), "%.9f", bo.stiffness);
    OKD("bouncy(280) damping matches", near(bo.damping, 36.045000000, 1e-6), "%.9f", bo.damping);
    OK("a preset has unit mass", sm.mass == 1 && sn.mass == 1 && bo.mass == 1);

    /* the curve a spring draws, at seven points, against the JavaScript */
    double want_sm[7] = { 0, 0.455086752238, 0.812155678677, 0.944467747778, 0.984815427504, 0.996040012317, 0.998999626859 };
    double want_sn[7] = { 0, 0.572134798995, 0.950402114386, 1.014974993124, 1.007356171422, 1.001000060211, 0.999782495613 };
    double want_bo[7] = { 0, 0.437209126619, 0.887756107654, 1.037540153397, 1.037000401679, 1.012031086252, 0.999822861502 };
    int ms[7] = { 0, 50, 100, 150, 200, 250, 300 };
    int ok_sm = 1, ok_sn = 1, ok_bo = 1;
    for (int i = 0; i < 7; i++) {
        if (!near(gk_spring_at(sm, ms[i]), want_sm[i], 1e-9)) ok_sm = 0;
        if (!near(gk_spring_at(sn, ms[i]), want_sn[i], 1e-9)) ok_sn = 0;
        if (!near(gk_spring_at(bo, ms[i]), want_bo[i], 1e-9)) ok_bo = 0;
    }
    OKD("smooth(300) follows motion-kit's curve", ok_sm, "at 150ms %.12f", gk_spring_at(sm, 150));
    OKD("snappy(250) follows motion-kit's curve", ok_sn, "at 150ms %.12f", gk_spring_at(sn, 150));
    OKD("bouncy(280) follows motion-kit's curve", ok_bo, "at 150ms %.12f", gk_spring_at(bo, 150));
    OK("bouncy overshoots", gk_spring_at(bo, 150) > 1.0);
    OK("smooth does not overshoot", gk_spring_at(sm, 150) < 1.0);
    OK("a spring lands on 1", near(gk_spring_at(sm, 300), 1, 0.002));

    /* tween */
    const gk_ease_t *lin = gk_ease_named("linear");
    OK("tween starts at from", gk_tween(10, 20, 200, lin, 0) == 10);
    OK("tween ends at to", gk_tween(10, 20, 200, lin, 200) == 20);
    OK("tween midpoint", near(gk_tween(10, 20, 200, lin, 100), 15, 1e-6));
    OK("tween clamps past the end", gk_tween(10, 20, 200, lin, 999) == 20);
    OK("a zero duration jumps", gk_tween(10, 20, 0, lin, 0) == 20);
    OK("tween uses the curve", gk_tween(0, 100, 100, gk_ease_named("out"), 25) > 25);
}


/* ---------- camera ---------- */
static void test_camera(void) {
    gk_camera c = gk_camera_make(640, 400);
    OK("a camera starts at unit zoom", c.zoom == 1.0);
    OK("a camera starts at the origin", c.x == 0 && c.y == 0);

    double sx, sy, wx, wy;
    gk_camera_world_to_screen(&c, 0, 0, &sx, &sy);
    OKD("the camera's own point is the viewport centre", sx == 320 && sy == 200, "%.1f,%.1f", sx, sy);

    gk_camera_world_to_screen(&c, 100, 50, &sx, &sy);
    OKD("a world point offsets by its distance", sx == 420 && sy == 250, "%.1f,%.1f", sx, sy);

    /* round trip */
    gk_camera_screen_to_world(&c, sx, sy, &wx, &wy);
    OKD("screen to world inverts world to screen", near(wx, 100, 1e-9) && near(wy, 50, 1e-9), "%.6f,%.6f", wx, wy);

    gk_camera_screen_to_world(&c, 320, 200, &wx, &wy);
    OK("the centre maps to the camera", near(wx, 0, 1e-9) && near(wy, 0, 1e-9));

    /* zoom */
    c.zoom = 2.0;
    gk_camera_world_to_screen(&c, 100, 50, &sx, &sy);
    OKD("zoom scales the offset", sx == 520 && sy == 300, "%.1f,%.1f", sx, sy);
    gk_camera_screen_to_world(&c, sx, sy, &wx, &wy);
    OK("zoom round-trips", near(wx, 100, 1e-9) && near(wy, 50, 1e-9));
    c.zoom = 1.0;

    /* follow */
    c.x = 0; c.y = 0;
    gk_camera_follow(&c, 1000, 1000, 1.0 / 60.0, 0.12);
    OKD("follow moves towards the target", c.x > 0 && c.x < 1000, "%.2f", c.x);
    double first = c.x;
    gk_camera_follow(&c, 1000, 1000, 1.0 / 60.0, 0.12);
    OKD("follow keeps closing the gap", c.x > first && c.x < 1000, "%.2f", c.x);
    for (int i = 0; i < 400; i++) gk_camera_follow(&c, 1000, 1000, 1.0 / 60.0, 0.12);
    OKD("follow arrives", near(c.x, 1000, 1.0), "%.3f", c.x);
    c.x = 0; c.y = 0;
    gk_camera_follow(&c, 500, 500, 1.0 / 60.0, 1.0);
    OK("speed 1 snaps", near(c.x, 500, 1e-9));
    c.x = 10; c.y = 10;
    gk_camera_follow(&c, 500, 500, 1.0 / 60.0, 0);
    OK("speed 0 does not move", c.x == 10 && c.y == 10);

    /* bounds: world bigger than the viewport */
    gk_camera b = gk_camera_make(640, 400);
    gk_camera_bounds(&b, 0, 0, 1280, 800);
    b.x = 0; b.y = 0;
    gk_camera_clamp(&b);
    OKD("the left edge stops at half a viewport", b.x == 320 && b.y == 200, "%.1f,%.1f", b.x, b.y);
    b.x = 5000; b.y = 5000;
    gk_camera_clamp(&b);
    OKD("the right edge stops inside the world", b.x == 1280 - 320 && b.y == 800 - 200, "%.1f,%.1f", b.x, b.y);
    b.x = 700; b.y = 400;
    gk_camera_clamp(&b);
    OK("a camera already inside is untouched", b.x == 700 && b.y == 400);

    /* zoomed in, the clamp allows a wider travel */
    b.zoom = 2.0;
    b.x = 0; b.y = 0;
    gk_camera_clamp(&b);
    OKD("zoom changes how far the view may travel", b.x == 160 && b.y == 100, "%.1f,%.1f", b.x, b.y);

    /* a world smaller than the viewport centres instead of clamping */
    gk_camera sm = gk_camera_make(640, 400);
    gk_camera_bounds(&sm, 0, 0, 320, 200);
    sm.x = 999; sm.y = 999;
    gk_camera_clamp(&sm);
    OKD("a small world is centred", sm.x == 160 && sm.y == 100, "%.1f,%.1f", sm.x, sm.y);

    /* unbounded */
    gk_camera u = gk_camera_make(640, 400);
    u.x = 99999; u.y = -99999;
    gk_camera_clamp(&u);
    OK("unbounded keeps the camera where it is", u.x == 99999 && u.y == -99999);

    /* a zero zoom is repaired rather than dividing by zero */
    gk_camera z = gk_camera_make(640, 400);
    z.zoom = 0;
    gk_camera_clamp(&z);
    OK("a zero zoom is repaired", z.zoom == 1.0);
}

/* ---------- loop ---------- */
static int g_steps = 0;
static double g_alpha = -1;
static void count_step(void *ud, double step) { (void)ud; (void)step; g_steps++; }
static void take_alpha(void *ud, double alpha) { (void)ud; g_alpha = alpha; }

static void test_loop(void) {
    gk_loop l;
    /* one frame of a whole second is a stall: max_frame (0.25s) clamps it */
    gk_loop_init(&l, 1.0 / 60.0);
    g_steps = 0;
    int steps = gk_loop_tick(&l, 1.0, count_step, take_alpha, NULL);
    OKD("a one-second frame is clamped to max_frame", steps == 15, "%d steps", steps);
    OK("ticks accumulate", l.ticks == 15);

    /* four quarter-second frames make the second up */
    gk_loop_init(&l, 1.0 / 60.0);
    int total4 = 0;
    for (int i = 0; i < 4; i++) total4 += gk_loop_tick(&l, 0.25, count_step, take_alpha, NULL);
    OKD("four 0.25s frames are 60 steps", total4 == 60, "%d steps", total4);

    gk_loop_init(&l, 1.0 / 60.0);
    g_steps = 0;
    int total = 0;
    for (int i = 0; i < 60; i++) total += gk_loop_tick(&l, 1.0 / 60.0, count_step, take_alpha, NULL);
    OKD("sixty 1/60 frames is 60 steps", total == 60, "%d steps", total);

    gk_loop_init(&l, 1.0 / 60.0);
    g_steps = 0;
    steps = gk_loop_tick(&l, 0.0, count_step, take_alpha, NULL);
    OK("a zero-length frame runs nothing", steps == 0);
    steps = gk_loop_tick(&l, -5.0, count_step, take_alpha, NULL);
    OK("a negative frame is treated as zero", steps == 0);

    gk_loop_init(&l, 1.0 / 60.0);
    g_steps = 0;
    steps = gk_loop_tick(&l, 10.0, count_step, take_alpha, NULL);
    OKD("a huge frame is clamped to max_frame", steps == 15, "%d steps", steps);

    gk_loop_init(&l, 1.0 / 60.0);
    l.max_steps = 10;
    g_steps = 0;
    steps = gk_loop_tick(&l, 1.0, count_step, take_alpha, NULL);
    OKD("the step cap holds", steps == 10, "%d steps", steps);
    OK("the accumulator resets after a cap", l.acc == 0);

    gk_loop_init(&l, 1.0 / 60.0);
    g_alpha = -1;
    gk_loop_tick(&l, 1.0 / 120.0, count_step, take_alpha, NULL);
    OKD("alpha is the leftover fraction", g_alpha >= 0 && g_alpha < 1, "%.3f", g_alpha);

    gk_loop_init(&l, 1.0 / 60.0);
    gk_loop_tick(&l, 1.0 / 60.0, count_step, take_alpha, NULL);
    OKD("a whole step leaves no remainder", near(l.acc, 0, 1e-9), "%.9f", l.acc);
}

/* ---------- input ---------- */
static void test_input(void) {
    gk_input in;
    memset(&in, 0, sizeof in);
    OK("a key starts up", gk_input_is_down(&in, GK_KEY_LEFT) == 0);
    gk_input_down(&in, GK_KEY_LEFT);
    OK("a pressed key is down", gk_input_is_down(&in, GK_KEY_LEFT) == 1);
    OK("a pressed key reports pressed", gk_input_was_pressed(&in, GK_KEY_LEFT) == 1);
    gk_input_end_frame(&in);
    OK("pressed lasts one frame", gk_input_was_pressed(&in, GK_KEY_LEFT) == 0);
    OK("but down persists", gk_input_is_down(&in, GK_KEY_LEFT) == 1);
    gk_input_up(&in, GK_KEY_LEFT);
    OK("released is up", gk_input_is_down(&in, GK_KEY_LEFT) == 0);
    gk_input_down(&in, (gk_key)99);
    OK("an out-of-range key is ignored", gk_input_is_down(&in, (gk_key)99) == 0);
}

/* ---------- scenes ---------- */
static char g_trace[64];
static void trace_add(const char *what) { strncat(g_trace, what, sizeof g_trace - strlen(g_trace) - 1); }
static void s1_enter(void *ud, const void *d) { (void)ud; (void)d; trace_add("A+"); }
static void s1_exit(void *ud) { (void)ud; trace_add("A-"); }
static void s2_enter(void *ud, const void *d) { (void)ud; (void)d; trace_add("B+"); }
static void s2_update(void *ud, double step) { (void)ud; (void)step; trace_add("u"); }

static void test_scenes(void) {
    gk_scenes sc;
    gk_scenes_init(&sc);
    const gk_scene A = { "a", s1_enter, s1_exit, NULL, NULL };
    const gk_scene B = { "b", s2_enter, NULL, s2_update, NULL };
    OK("add a scene", gk_scenes_add(&sc, &A) == 0);
    OK("add a second", gk_scenes_add(&sc, &B) == 0);

    g_trace[0] = 0;
    OK("go enters the first scene", gk_scenes_go(&sc, "a", NULL, NULL) == 0);
    OK("enter ran", strcmp(g_trace, "A+") == 0);
    OK("current is a", strcmp(gk_scenes_current(&sc), "a") == 0);

    g_trace[0] = 0;
    OK("go leaves then enters", gk_scenes_go(&sc, "b", NULL, NULL) == 0);
    OK("exit before enter", strcmp(g_trace, "A-B+") == 0);
    OK("current is b", strcmp(gk_scenes_current(&sc), "b") == 0);

    g_trace[0] = 0;
    gk_scenes_update(&sc, NULL, 0.016);
    OK("update routes to the current scene", strcmp(g_trace, "u") == 0);

    g_trace[0] = 0;
    OK("an unknown scene is refused", gk_scenes_go(&sc, "nope", NULL, NULL) == -1);
    OK("and the current scene is untouched", strcmp(gk_scenes_current(&sc), "b") == 0);
    OK("and nothing entered", strcmp(g_trace, "") == 0);

    OK("null name is refused", gk_scenes_go(&sc, NULL, NULL, NULL) == -1);
}

/* ---------- save / load ---------- */
static void test_save(void) {
    const char *path = "test-progress.bin";
    remove(path);

    gk_progress out = { 0 }, in = { 42 };
    OK("load of a missing file fails", gk_load(path, &out, sizeof out) == GK_SAVE_ERR_OPEN);

    OK("save writes", gk_save(path, &in, sizeof in) == GK_SAVE_OK);
    OK("load reads it back", gk_load(path, &out, sizeof out) == GK_SAVE_OK);
    OKD("the value survives", out.best == 42, "best=%d", out.best);

    OK("a size mismatch is refused", gk_load(path, &out, sizeof out - 1) == GK_SAVE_ERR_SIZE);

    /* corrupt the magic */
    {
        FILE *f = fopen(path, "r+b");
        unsigned char junk[4] = { 'X', 'X', 'X', 'X' };
        fwrite(junk, 4, 1, f);
        fclose(f);
    }
    OK("a foreign file is refused", gk_load(path, &out, sizeof out) == GK_SAVE_ERR_MAGIC);

    /* restore, then corrupt one payload byte */
    gk_save(path, &in, sizeof in);
    {
        FILE *f = fopen(path, "r+b");
        fseek(f, (long)sizeof(gk_save_header), SEEK_SET);
        unsigned char b = 0xFF;
        fwrite(&b, 1, 1, f);
        fclose(f);
    }
    OK("a changed payload is caught by the CRC", gk_load(path, &out, sizeof out) == GK_SAVE_ERR_CRC);

    /* a future version */
    gk_save(path, &in, sizeof in);
    {
        FILE *f = fopen(path, "r+b");
        uint32_t v = GK_SAVE_VERSION + 1;
        fseek(f, 4, SEEK_SET);
        fwrite(&v, sizeof v, 1, f);
        fclose(f);
    }
    OK("a newer version is refused", gk_load(path, &out, sizeof out) == GK_SAVE_ERR_VERSION);

    OK("save tolerates a null path", gk_save(NULL, &in, sizeof in) == GK_SAVE_ERR_OPEN);
    OK("load tolerates a null path", gk_load(NULL, &out, sizeof out) == GK_SAVE_ERR_OPEN);

    /* an empty payload round-trips */
    OK("an empty save writes", gk_save(path, NULL, 0) == GK_SAVE_OK);
    OK("an empty save loads", gk_load(path, NULL, 0) == GK_SAVE_OK);

    remove(path);
}

/* ---------- ecs ---------- */
static int g_sys_runs = 0;
static int g_sys_order[8];
static int g_sys_n = 0;
static void sys_a(gk_world *w, gk_entity *e) { (void)w; (void)e; g_sys_runs++; g_sys_order[g_sys_n++] = 1; }
static void sys_b(gk_world *w, gk_entity *e) { (void)w; (void)e; g_sys_runs++; g_sys_order[g_sys_n++] = 2; }

/* a system that reads a component through the entity it was handed */
static int g_sys_read_ok = 0;
static void sys_read(gk_world *w, gk_entity *e) {
    (void)w;
    int *v = (int *)gk_ecs_comp(e, COMP_BODY);
    if (v && *v == 7) g_sys_read_ok++;
}

static void test_ecs(void) {
    gk_world w;
    gk_world_init(&w);
    OK("a new world is empty", gk_ecs_count(&w) == 0);

    uint32_t e1 = gk_ecs_create(&w);
    uint32_t e2 = gk_ecs_create(&w);
    OK("create returns an id", e1 != 0 && e2 != 0);
    OK("ids are distinct", e1 != e2);
    OK("count is two", gk_ecs_count(&w) == 2);

    int body = 1, vel = 2;
    OK("add a component", gk_ecs_add(&w, e1, COMP_BODY, &body) == 0);
    OK("has it", gk_ecs_has(&w, e1, COMP_BODY) == 1);
    OK("does not have what it was not given", gk_ecs_has(&w, e1, COMP_VEL) == 0);
    OK("get returns the pointer", gk_ecs_get(&w, e1, COMP_BODY) == &body);
    OK("get of an absent component is null", gk_ecs_get(&w, e1, COMP_VEL) == NULL);

    OK("add a second", gk_ecs_add(&w, e1, COMP_VEL, &vel) == 0);
    OK("remove one", gk_ecs_remove(&w, e1, COMP_VEL) == 0);
    OK("removed is gone", gk_ecs_has(&w, e1, COMP_VEL) == 0);
    OK("the other survives", gk_ecs_has(&w, e1, COMP_BODY) == 1);

    {
        gk_world wr;
        gk_world_init(&wr);
        uint32_t e = gk_ecs_create(&wr);
        int seven = 7;
        gk_ecs_add(&wr, e, COMP_BODY, &seven);
        g_sys_read_ok = 0;
        gk_ecs_system(&wr, (1u << COMP_BODY), sys_read);
        gk_ecs_run(&wr);
        OK("a system sees its own component without a lookup", g_sys_read_ok == 1);
    }

    OK("add to an unknown entity fails", gk_ecs_add(&w, 999, COMP_BODY, &body) == -1);
    OK("an out-of-range component fails", gk_ecs_add(&w, e1, 99, &body) == -1);

    g_sys_runs = 0;
    g_sys_n = 0;
    OK("register a system", gk_ecs_system(&w, (1u << COMP_BODY), sys_a) == 0);
    int ran = gk_ecs_run(&w);
    OKD("the system runs only for matching entities", ran == 1, "%d ran", ran);

    gk_ecs_add(&w, e2, COMP_BODY, &body);
    ran = gk_ecs_run(&w);
    OKD("and now for both", ran == 2, "%d ran", ran);

    g_sys_n = 0;
    gk_ecs_system(&w, (1u << COMP_BODY), sys_b);
    gk_ecs_run(&w);
    OKD("systems run in the order added", g_sys_order[0] == 1 && g_sys_order[2] == 2, "%d,%d", g_sys_order[0], g_sys_order[2]);

    OK("destroy removes an entity", gk_ecs_destroy(&w, e2) == 0);
    OKD("count drops", gk_ecs_count(&w) == 1, "%d", gk_ecs_count(&w));
    OK("destroying twice fails", gk_ecs_destroy(&w, e2) == -1);
    OK("a destroyed entity has nothing", gk_ecs_has(&w, e2, COMP_BODY) == 0);

    /* a system that requires two components skips an entity with one */
    gk_world w2;
    gk_world_init(&w2);
    uint32_t only_body = gk_ecs_create(&w2);
    gk_ecs_add(&w2, only_body, COMP_BODY, &body);
    g_sys_runs = 0;
    gk_ecs_system(&w2, (1u << COMP_BODY) | (1u << COMP_VEL), sys_a);
    ran = gk_ecs_run(&w2);
    OK("a two-component system needs both", ran == 0);

    /* filling the world fails gracefully rather than overrunning */
    gk_world w3;
    gk_world_init(&w3);
    for (int i = 0; i < GK_MAX_ENTITIES; i++) gk_ecs_create(&w3);
    OKD("the world is full at the cap", gk_ecs_count(&w3) == GK_MAX_ENTITIES, "%d", gk_ecs_count(&w3));
    OK("and refuses one more", gk_ecs_create(&w3) == 0);
}

/* ---------- the game itself, headless ---------- */
static void test_game(void) {
    memset(&G, 0, sizeof G);
    gk_loop_init(&G.loop, 1.0 / 60.0);
    gk_scenes_init(&G.scenes);
    gk_scenes_add(&G.scenes, &(gk_scene){ "title", NULL, NULL, title_update, NULL });
    gk_scenes_add(&G.scenes, &(gk_scene){ "play", play_enter, NULL, play_update, NULL });
    gk_scenes_add(&G.scenes, &(gk_scene){ "over", NULL, NULL, over_update, NULL });
    gk_scenes_go(&G.scenes, "title", &G, NULL);
    OK("the game starts on the title", strcmp(gk_scenes_current(&G.scenes), "title") == 0);

    gk_input_down(&G.input, GK_KEY_START);
    gk_scenes_update(&G.scenes, &G, 1.0 / 60.0);
    gk_input_end_frame(&G.input);
    OK("start goes to play", strcmp(gk_scenes_current(&G.scenes), "play") == 0);
    OKD("play spawns the player and the motes", gk_ecs_count(&G.world) == 7, "%d entities", gk_ecs_count(&G.world));
    OK("the player is an entity", G.player_id != 0);

    /* run a few seconds of simulated play */
    for (int i = 0; i < 120; i++) { gk_scenes_update(&G.scenes, &G, 1.0 / 60.0); gk_input_end_frame(&G.input); }
    OK("the clock advanced", G.clock > 1.9 && G.clock < 2.1);
    OK("the player stays on the field",
       G.player_body.box.x >= 0 && G.player_body.box.x <= 640 - G.player_body.box.w);

    /* walk the player onto a mote: teleport the player, then one update */
    if (gk_ecs_count(&G.world) > 1) {
        int before = G.score;
        uint32_t mote = 0;
        for (int i = 0; i < G.world.count; i++) {
            uint32_t id = G.world.ents[i].id;
            if (id != G.player_id && gk_ecs_has(&G.world, id, COMP_WORTH)) { mote = id; break; }
        }
        gk_body *mb = (gk_body *)gk_ecs_get(&G.world, mote, COMP_BODY);
        if (mb) {
            G.player_body.box.x = mb->box.x;
            G.player_body.box.y = mb->box.y;
            gk_scenes_update(&G.scenes, &G, 1.0 / 60.0);
            OKD("walking into a mote scores", G.score > before, "score %d", G.score);
        } else {
            OK("a mote exists to collect", 0);
        }
    }

    /* let the timer end the round */
    for (int i = 0; i < 60 * 31; i++) gk_scenes_update(&G.scenes, &G, 1.0 / 60.0);
    OK("the round ends on the clock", strcmp(gk_scenes_current(&G.scenes), "over") == 0);
    OK("the best score was kept", G.best >= G.score);

    remove(gk_save_path());
}

int main(void) {
    printf("raylib-starter tests\n\n");
    test_maths();
    test_motion();
    test_camera();
    test_rng();
    test_loop();
    test_input();
    test_scenes();
    test_save();
    test_ecs();
    test_game();
    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
