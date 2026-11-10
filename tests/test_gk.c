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


/* ---------- tilemap and collide-and-slide ---------- */
/* A 5x3 corridor of 16px tiles, solid all round:
 *
 *   # # # # #
 *   # . . . #
 *   # # # # #
 *
 * and a second map with one solid block inside the corridor, to check that a
 * wall in the middle of a sweep stops the mover there and not at the far side.
 */
static const unsigned char TM_IDS[15] = {
    1, 1, 1, 1, 1,
    1, 0, 0, 0, 1,
    1, 1, 1, 1, 1,
};
static const unsigned char TM_IDS_BLOCK[15] = {
    1, 1, 1, 1, 1,
    1, 0, 1, 0, 1,
    1, 1, 1, 1, 1,
};
static const unsigned char TM_SOLID[256] = { 0, 1 };

static gk_tilemap tm_make(void) {
    gk_tilemap m = { 5, 3, 16, TM_IDS, TM_SOLID, 1 };
    return m;
}

static void test_tilemap(void) {
    gk_tilemap m = tm_make();

    OK("a map knows its id", gk_tilemap_id(&m, 0, 0) == 1);
    OK("an empty tile reads zero", gk_tilemap_id(&m, 1, 1) == 0);
    OK("off the map reads -1", gk_tilemap_id(&m, -1, 0) == -1 && gk_tilemap_id(&m, 5, 0) == -1);

    OK("a solid id is solid", gk_tilemap_is_solid(&m, 0, 0) == 1);
    OK("an empty id is not", gk_tilemap_is_solid(&m, 1, 1) == 0);
    OK("outside is solid by default", gk_tilemap_is_solid(&m, -1, 1) == 1);
    m.outside_solid = 0;
    OK("outside can be open", gk_tilemap_is_solid(&m, -1, 1) == 0);
    m.outside_solid = 1;

    OK("world x to tile floors", gk_tilemap_tile_at(0, 16) == 0 && gk_tilemap_tile_at(15.9, 16) == 0);
    OK("a boundary belongs to the next tile", gk_tilemap_tile_at(16, 16) == 1);
    OK("negative coordinates floor", gk_tilemap_tile_at(-0.5, 16) == -1);
    OK("solid_at maps a world point", gk_tilemap_solid_at(&m, 4, 4) == 1 && gk_tilemap_solid_at(&m, 20, 20) == 0);

    /* free movement is untouched */
    gk_rect b = { 20, 20, 10, 10 };  /* inside the corridor at tile 1,1 */
    gk_rect r = gk_move(&m, b, 2, 0);
    OKD("free movement is exact", near(r.x, 22, 1e-9), "%.4f", r.x);
    r = gk_move(&m, b, 0, 0);
    OK("a zero move changes nothing", r.x == b.x && r.y == b.y);

    /* into the right-hand wall: the box stops flush against it */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&m, b, 100, 0);
    OKD("moving right stops flush at the wall", near(r.x + r.w, 64, 1e-9), "right edge %.4f", r.x + r.w);
    OK("and the y is untouched", r.y == b.y);

    /* into the left wall */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&m, b, -100, 0);
    OKD("moving left stops flush", near(r.x, 16, 1e-9), "x %.4f", r.x);

    /* down onto the floor */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&m, b, 0, 100);
    OKD("falling stops on the floor", near(r.y + r.h, 32, 1e-9), "bottom %.4f", r.y + r.h);

    /* up into the ceiling */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&m, b, 0, -100);
    OKD("rising stops at the ceiling", near(r.y, 16, 1e-9), "y %.4f", r.y);

    /* a corner: both axes resolve, so it slides rather than sticks */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&m, b, 100, 100);
    OKD("a corner resolves both axes", near(r.x + r.w, 64, 1e-9) && near(r.y + r.h, 32, 1e-9),
        "%.4f,%.4f", r.x + r.w, r.y + r.h);

    /* sliding along a wall keeps the free axis moving */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&m, b, 100, 1);
    OKD("a wall does not stop the free axis", near(r.x + r.w, 64, 1e-9) && near(r.y, 21, 1e-9),
        "%.4f,%.4f", r.x + r.w, r.y);

    /* a block in the middle of the sweep stops the mover there, not at the far wall */
    gk_tilemap mb = { 5, 3, 16, TM_IDS_BLOCK, TM_SOLID, 1 };
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&mb, b, 100, 0);
    OKD("a mid-sweep block stops the move", near(r.x + r.w, 32, 1e-9), "right edge %.4f", r.x + r.w);

    /* a single 1px step does not reach the block, so it moves in full... */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&mb, b, 1, 0);
    OKD("a step short of the block moves in full", near(r.x + r.w, 31, 1e-9), "right edge %.4f", r.x + r.w);
    /* ...and a hundred of them stop in exactly the same place as one big sweep */
    b = (gk_rect){ 20, 20, 10, 10 };
    for (int i = 0; i < 100; i++) b = gk_move(&mb, b, 1, 0);
    OKD("many small steps stop at the same edge", near(b.x + b.w, 32, 1e-9), "right edge %.4f", b.x + b.w);

    /* out of bounds is solid, so the world is closed */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(&m, b, -1000, 0);
    OKD("the world is closed on the left", near(r.x, 16, 1e-9), "x %.4f", r.x);

    /* a null map moves freely */
    b = (gk_rect){ 20, 20, 10, 10 };
    r = gk_move(NULL, b, 5, 5);
    OK("a null map is free movement", r.x == 25 && r.y == 25);

    /* a zero-size tile is treated as no map rather than dividing by zero */
    gk_tilemap z = tm_make();
    z.tile = 0;
    r = gk_move(&z, b, 5, 5);
    OK("a zero tile size moves freely", r.x == 25 && r.y == 25);
}


/* ---------- sprites, animation and visible tiles ---------- */
static void test_sprites(void) {
    gk_atlas a = { 16, 24, 4, 10 };   /* 4 across, 10 frames */
    gk_rect r = gk_atlas_src(&a, 0);
    OK("frame 0 is the top-left", r.x == 0 && r.y == 0 && r.w == 16 && r.h == 24);
    r = gk_atlas_src(&a, 3);
    OKD("the fourth frame ends the first row", r.x == 48 && r.y == 0, "%.0f,%.0f", r.x, r.y);
    r = gk_atlas_src(&a, 4);
    OKD("the fifth wraps to the second row", r.x == 0 && r.y == 24, "%.0f,%.0f", r.x, r.y);
    r = gk_atlas_src(&a, 9);
    OKD("the last frame is one into the third row", r.x == 16 && r.y == 48, "%.0f,%.0f", r.x, r.y);
    r = gk_atlas_src(&a, 10);
    OK("a frame past the end wraps", r.x == 0 && r.y == 0);
    r = gk_atlas_src(&a, -1);
    OKD("a negative frame wraps backwards", r.x == 16 && r.y == 48, "%.0f,%.0f", r.x, r.y);

    gk_atlas bad = { 0, 0, 0, 0 };
    r = gk_atlas_src(&bad, 3);
    OK("a degenerate atlas gives an empty rect", r.w == 0 && r.h == 0);
    r = gk_atlas_src(NULL, 3);
    OK("a null atlas gives an empty rect", r.w == 0 && r.h == 0);

    /* animation */
    gk_anim an = { a, 10.0, 0, 3, 1 };   /* 4 frames at 10 fps, looping */
    OK("t=0 shows the first frame", gk_anim_frame(&an, 0) == 0);
    OKD("0.15s is frame 1", gk_anim_frame(&an, 0.15) == 1, "%d", gk_anim_frame(&an, 0.15));
    OKD("0.35s is frame 3", gk_anim_frame(&an, 0.35) == 3, "%d", gk_anim_frame(&an, 0.35));
    OKD("0.45s wraps to frame 0", gk_anim_frame(&an, 0.45) == 0, "%d", gk_anim_frame(&an, 0.45));

    gk_anim once = { a, 10.0, 0, 3, 0 };  /* no loop */
    OKD("a non-looping anim holds the last frame", gk_anim_frame(&once, 99.0) == 3, "%d", gk_anim_frame(&once, 99.0));

    gk_anim still = { a, 0, 2, 5, 1 };
    OK("fps 0 is a still", gk_anim_frame(&still, 12.3) == 2);

    gk_anim sub = { a, 10.0, 2, 5, 1 };   /* a sub-range, 4 frames */
    OK("a sub-range starts at from", gk_anim_frame(&sub, 0) == 2);
    OKD("a sub-range wraps inside itself", gk_anim_frame(&sub, 0.45) == 2, "%d", gk_anim_frame(&sub, 0.45));

    gk_anim rev = { a, 10.0, 5, 2, 1 };   /* from > to is tolerated */
    OK("a reversed range still yields a frame", gk_anim_frame(&rev, 0) == 2);

    /* destination rect through a camera */
    gk_camera c = gk_camera_make(640, 400);
    double dx, dy, dw, dh;
    gk_sprite_dst(&c, 0, 0, 16, 16, &dx, &dy, &dw, &dh);
    OKD("a sprite at the camera's point is centred", near(dx, 320, 1e-9) && near(dy, 200, 1e-9), "%.1f,%.1f", dx, dy);
    OK("an unzoomed sprite keeps its size", dw == 16 && dh == 16);
    c.zoom = 2.0;
    gk_sprite_dst(&c, 10, 10, 16, 16, &dx, &dy, &dw, &dh);
    OKD("zoom scales the sprite", near(dw, 32, 1e-9) && near(dx, 340, 1e-9), "w %.1f x %.1f", dw, dx);
    gk_sprite_dst(NULL, 5, 7, 8, 9, &dx, &dy, &dw, &dh);
    OK("a null camera is world coordinates", dx == 5 && dy == 7 && dw == 8 && dh == 9);

    /* visible tiles */
    gk_tilemap m = tm_make();   /* 5x3 of 16px */
    gk_visible_tile out[64];
    gk_camera all = gk_camera_make(200, 200);
    all.x = 40; all.y = 24;     /* centred on the map */
    int n = gk_tiles_visible(&m, &all, out, 64);
    OKD("a view covering the map sees every tile", n == 15, "%d", n);
    OK("the first is the top-left tile", out[0].tx == 0 && out[0].ty == 0 && out[0].id == 1);
    OK("tiles carry world rectangles", out[0].x == 0 && out[0].y == 0 && out[0].w == 16);

    /* a 32x16 viewport centred at x=24 covers world x 8..40, which is tiles 0..2 */
    gk_camera small = gk_camera_make(32, 16);
    small.x = 24; small.y = 8;
    n = gk_tiles_visible(&m, &small, out, 64);
    OKD("a small view sees only its own tiles", n == 3, "%d", n);
    OKD("and they are the right ones", out[0].tx == 0 && out[2].tx == 2 && out[0].ty == 0,
        "%d..%d", out[0].tx, out[2].tx);

    gk_camera far = gk_camera_make(32, 16);
    far.x = 10000; far.y = 10000;
    n = gk_tiles_visible(&m, &far, out, 64);
    OK("a view outside the map sees nothing", n == 0);

    n = gk_tiles_visible(&m, &all, out, 4);
    OKD("the capacity is respected", n == 4, "%d", n);

    n = gk_tiles_visible(&m, NULL, out, 64);
    OKD("a null camera means the whole map", n == 15, "%d", n);

    n = gk_tiles_visible(NULL, &all, out, 64);
    OK("a null map sees nothing", n == 0);

    /* zoomed out, more tiles fit in the same viewport */
    gk_camera zoomed = gk_camera_make(32, 16);
    zoomed.zoom = 0.5;
    zoomed.x = 24; zoomed.y = 8;
    n = gk_tiles_visible(&m, &zoomed, out, 64);
    OKD("zooming out shows more tiles", n > 2, "%d", n);
}


/* ---------- stateful tween and camera deadzone ---------- */
static void test_tween_state(void) {
    const gk_ease_t *lin = gk_ease_named("linear");
    gk_tween_t tw = gk_tween_make(10, 20, 200, lin);
    OK("a new tween starts at from", gk_tween_value(&tw) == 10);
    OK("a new tween is not done", tw.done == 0);

    gk_tween_step(&tw, 100);
    OKD("halfway is halfway", near(gk_tween_value(&tw), 15, 1e-6), "%.6f", gk_tween_value(&tw));
    gk_tween_step(&tw, 100);
    OK("the end is the target", gk_tween_value(&tw) == 20);
    OK("it reports done", tw.done == 1);

    gk_tween_step(&tw, 500);
    OK("stepping past the end does not overshoot", gk_tween_value(&tw) == 20);

    /* a negative delta is ignored rather than running time backwards */
    gk_tween_t back = gk_tween_make(0, 100, 100, lin);
    gk_tween_step(&back, 50);
    gk_tween_step(&back, -1000);
    OKD("a negative step is ignored", near(gk_tween_value(&back), 50, 1e-6), "%.3f", gk_tween_value(&back));

    /* zero duration is already done */
    gk_tween_t instant = gk_tween_make(5, 9, 0, lin);
    OK("a zero-duration tween is done at once", instant.done == 1 && gk_tween_value(&instant) == 9);

    /* the curve is applied */
    gk_tween_t out = gk_tween_make(0, 100, 100, gk_ease_named("out"));
    gk_tween_step(&out, 25);
    OKD("the easing curve is applied", gk_tween_value(&out) > 25, "%.2f", gk_tween_value(&out));

    /* retarget continues from where it is, not from the old start */
    gk_tween_t rt = gk_tween_make(0, 100, 100, lin);
    gk_tween_step(&rt, 50);              /* at 50 */
    gk_tween_retarget(&rt, 0);           /* now head back to 0 */
    OKD("retarget starts from the current value", near(gk_tween_value(&rt), 50, 1e-6), "%.3f", gk_tween_value(&rt));
    gk_tween_step(&rt, 100);
    OKD("and lands on the new target", near(gk_tween_value(&rt), 0, 1e-6), "%.3f", gk_tween_value(&rt));

    gk_tween_step(NULL, 10);
    OK("stepping a null tween is safe", gk_tween_value(NULL) == 0);
    gk_tween_retarget(NULL, 5);
    OK("retargeting a null tween is safe", 1);
}

static void test_deadzone(void) {
    gk_camera c = gk_camera_make(640, 400);
    c.x = 0; c.y = 0;

    /* a 200x100 dead zone: the target inside it does not move the camera */
    gk_camera_deadzone(&c, 50, 20, 200, 100);
    OK("a target inside the dead zone does not move the camera", c.x == 0 && c.y == 0);

    gk_camera_deadzone(&c, 99, 49, 200, 100);
    OK("just inside the edge still does not move it", c.x == 0 && c.y == 0);

    /* outside to the right: the camera follows just enough */
    c.x = 0; c.y = 0;
    gk_camera_deadzone(&c, 200, 0, 200, 100);
    OKD("a target outside pushes the camera", near(c.x, 100, 1e-9), "%.2f", c.x);

    c.x = 0; c.y = 0;
    gk_camera_deadzone(&c, -200, 0, 200, 100);
    OKD("and to the left", near(c.x, -100, 1e-9), "%.2f", c.x);

    c.x = 0; c.y = 0;
    gk_camera_deadzone(&c, 0, 200, 200, 100);
    OKD("and down", near(c.y, 150, 1e-9), "%.2f", c.y);

    c.x = 0; c.y = 0;
    gk_camera_deadzone(&c, 0, -200, 200, 100);
    OKD("and up", near(c.y, -150, 1e-9), "%.2f", c.y);

    /* zoomed in, the dead zone is smaller in world units */
    gk_camera z = gk_camera_make(640, 400);
    z.zoom = 2.0;
    z.x = 0; z.y = 0;
    gk_camera_deadzone(&z, 60, 0, 200, 100);
    OKD("zoom shrinks the dead zone in world units", near(z.x, 10, 1e-9), "%.2f", z.x);

    /* a zero-size dead zone is a hard follow */
    gk_camera h = gk_camera_make(640, 400);
    h.x = 0; h.y = 0;
    gk_camera_deadzone(&h, 123, 45, 0, 0);
    OKD("a zero dead zone follows exactly", near(h.x, 123, 1e-9) && near(h.y, 45, 1e-9), "%.1f,%.1f", h.x, h.y);
}


/* ---------- deferred destroy ---------- */
static int g_destroyed_in_pass = 0;
static void sys_kill(gk_world *w, gk_entity *e) {
    /* queue, never destroy inline: the pass is still walking the array */
    if (gk_ecs_queue_destroy(w, e->id) == 0) g_destroyed_in_pass++;
}

static void test_deferred(void) {
    gk_world w;
    gk_world_init(&w);
    int body = 1;
    uint32_t a = gk_ecs_create(&w), b = gk_ecs_create(&w), c = gk_ecs_create(&w);
    gk_ecs_add(&w, a, COMP_BODY, &body);
    gk_ecs_add(&w, b, COMP_BODY, &body);
    gk_ecs_add(&w, c, COMP_BODY, &body);

    g_destroyed_in_pass = 0;
    gk_ecs_system(&w, (1u << COMP_BODY), sys_kill);
    int ran = gk_ecs_run(&w);
    OKD("every matching entity is visited in the pass", ran == 3, "%d", ran);
    OKD("each queued its own destroy", g_destroyed_in_pass == 3, "%d", g_destroyed_in_pass);
    OK("nothing is removed until the flush", gk_ecs_count(&w) == 3);

    int applied = gk_ecs_flush(&w);
    OKD("the flush applies all three", applied == 3, "%d", applied);
    OK("and the world is empty", gk_ecs_count(&w) == 0);

    /* queueing the same id twice is harmless */
    gk_world w2;
    gk_world_init(&w2);
    uint32_t d = gk_ecs_create(&w2);
    OK("queue one", gk_ecs_queue_destroy(&w2, d) == 0);
    OK("queue it again", gk_ecs_queue_destroy(&w2, d) == 0);
    OKD("the second is skipped on flush", gk_ecs_flush(&w2) == 1, "%d applied", 1);
    OK("the world is empty", gk_ecs_count(&w2) == 0);

    /* an unknown id in the queue is skipped, not fatal */
    gk_world w3;
    gk_world_init(&w3);
    gk_ecs_queue_destroy(&w3, 999);
    OK("an unknown id flushes to nothing", gk_ecs_flush(&w3) == 0);

    /* a full queue refuses rather than overrunning */
    gk_world w4;
    gk_world_init(&w4);
    uint32_t keep = gk_ecs_create(&w4);
    int accepted = 0, refused = 0;
    for (int i = 0; i < GK_MAX_QUEUED + 5; i++) {
        if (gk_ecs_queue_destroy(&w4, keep) == 0) accepted++; else refused++;
    }
    OKD("the queue caps at GK_MAX_QUEUED", accepted == GK_MAX_QUEUED, "%d accepted", accepted);
    OKD("and refuses the rest", refused == 5, "%d refused", refused);

    /* zero and null are refused */
    OK("id 0 is refused", gk_ecs_queue_destroy(&w4, 0) == -1);
    OK("a null world is refused", gk_ecs_queue_destroy(NULL, 1) == -1);
    OK("flushing a null world is safe", gk_ecs_flush(NULL) == 0);

    /* flush on an empty queue is a no-op */
    gk_world w5;
    gk_world_init(&w5);
    gk_ecs_create(&w5);
    OK("an empty flush changes nothing", gk_ecs_flush(&w5) == 0 && gk_ecs_count(&w5) == 1);
}


/* ---------- camera shake ---------- */
static void test_shake(void) {
    gk_shake s = gk_shake_make(300, 6, 8);
    OK("a new shake is not running", gk_shake_running(&s) == 0);
    OK("a new shake has no offset", s.dx == 0 && s.dy == 0);

    gk_shake_start(&s);
    OK("starting it makes it run", gk_shake_running(&s) == 1);

    gk_shake_step(&s, 16);
    OKD("it produces an offset", s.dx != 0 || s.dy != 0, "%.3f,%.3f", s.dx, s.dy);
    OKD("the offset stays within the amplitude", fabs(s.dx) <= 6.001 && fabs(s.dy) <= 6.001,
        "%.3f,%.3f", s.dx, s.dy);

    /* the offset decays: the peak early is larger than the peak late */
    gk_shake a = gk_shake_make(300, 6, 8);
    gk_shake_start(&a);
    double early = 0;
    for (int i = 0; i < 5; i++) { gk_shake_step(&a, 16); if (fabs(a.dx) > early) early = fabs(a.dx); }
    double late = 0;
    for (int i = 0; i < 15; i++) { gk_shake_step(&a, 16); if (fabs(a.dx) > late) late = fabs(a.dx); }
    OKD("it decays towards the end", late < early, "early %.3f late %.3f", early, late);

    /* it ends exactly on time and clears the offset */
    gk_shake e = gk_shake_make(100, 5, 8);
    gk_shake_start(&e);
    gk_shake_step(&e, 99);
    OK("it is still running just before the end", gk_shake_running(&e) == 1);
    gk_shake_step(&e, 1);
    OK("it stops at the duration", gk_shake_running(&e) == 0);
    OK("and the offset is cleared", e.dx == 0 && e.dy == 0);

    /* stepping a finished shake does nothing */
    gk_shake_step(&e, 1000);
    OK("stepping a finished shake is a no-op", e.dx == 0 && e.dy == 0);

    /* a negative step is ignored */
    gk_shake n = gk_shake_make(200, 4, 8);
    gk_shake_start(&n);
    gk_shake_step(&n, 50);
    double before = n.t;
    gk_shake_step(&n, -100);
    OKD("a negative step does not rewind", n.t == before, "%.1f", n.t);

    /* zero duration never runs */
    gk_shake z = gk_shake_make(0, 5, 8);
    gk_shake_start(&z);
    OK("a zero-duration shake never runs", gk_shake_running(&z) == 0);

    /* restarting resets the clock */
    gk_shake r = gk_shake_make(200, 4, 8);
    gk_shake_start(&r);
    gk_shake_step(&r, 150);
    gk_shake_start(&r);
    OKD("restarting resets the clock", r.t == 0 && gk_shake_running(&r) == 1, "%.1f", r.t);

    /* null safety */
    gk_shake_step(NULL, 10);
    gk_shake_start(NULL);
    OK("null shake calls are safe", gk_shake_running(NULL) == 0);
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
    test_tween_state();
    test_deadzone();
    test_tilemap();
    test_deferred();
    test_shake();
    test_sprites();
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
