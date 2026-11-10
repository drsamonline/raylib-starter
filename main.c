/* raylib-starter — one file, a fixed timestep, scenes, save and an ECS.
 *
 * The C answer to game-kit: the same names, the same behaviour, on raylib
 * instead of a canvas. Build it with `make` (needs raylib), test it with
 * `make test` (needs nothing), and check the size with `make size`.
 *
 *   gk_loop    fixed timestep with an accumulator, a frame clamp and a step cap
 *   gk_scenes  named scenes with enter / exit / update / render
 *   gk_input   key down and pressed-this-frame, driven by the host
 *   gk_save    save and load a struct with a magic, a version and a CRC32
 *   gk_ecs     entities, component bags and systems over a mask
 *   gk_rng     mulberry32 - the same sequence as game-kit, for the same seed
 *   gk_*       clamp, lerp, dist, aabb, circle_hit
 *
 * PolyForm Noncommercial 1.0.0. (c) 2026 Dr Sohil Momin.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ==================================================================== */
/* gk — the core. No raylib in here, so it tests headless.              */
/* ==================================================================== */

/* The core is one translation unit, so a build that does not use every entry
   point would warn about the rest. They are API, not dead code. */
#if defined(__GNUC__) || defined(__clang__)
#define GK_API __attribute__((unused))
#else
#define GK_API
#endif

/* ---------- maths ---------- */
GK_API static inline float gk_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
GK_API static inline float gk_lerp(float a, float b, float t) { return a + (b - a) * t; }
GK_API static inline float gk_dist(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return sqrtf(dx * dx + dy * dy);
}

typedef struct { float x, y, w, h; } gk_rect;
typedef struct { float x, y, r; } gk_circle;

GK_API static inline int gk_aabb(gk_rect a, gk_rect b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}
GK_API static inline int gk_circle_hit(gk_circle a, gk_circle b) {
    return gk_dist(a.x, a.y, b.x, b.y) <= (a.r + b.r);
}

/* ---------- rng: mulberry32, bit-identical to game-kit ---------- */
typedef struct { uint32_t s; } gk_rng;

GK_API static void gk_rng_seed(gk_rng *r, uint32_t seed) {
    r->s = seed ? seed : 0x9e3779b9u;
}

GK_API static double gk_rng_next(gk_rng *r) {
    uint32_t a = r->s + 0x6D2B79F5u;
    r->s = a;
    uint32_t t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}

GK_API static int gk_rng_int(gk_rng *r, int a, int b) {
    return a + (int)(gk_rng_next(r) * (double)(b - a + 1));
}

GK_API static int gk_rng_pick(gk_rng *r, int n) {
    int i = (int)(gk_rng_next(r) * (double)n);
    return i < 0 ? 0 : (i >= n ? n - 1 : i);
}


/* ---------- motion: easing and springs ---------- */
/* The same curves and springs as motion-kit, so a C game and a JS game move
   alike. The numbers are the tests' business: see test_motion() in the suite. */

typedef struct { double x1, y1, x2, y2; } gk_ease_t;

/* UnitBezier: solve x for the parameter, then read y. Newton first, bisection
   when the derivative is too flat - the same approach as motion-kit and as a
   browser, so the same curve lands in the same place. */
GK_API static double gk_bezier_y(const gk_ease_t *e, double x) {
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    double cx = 3 * e->x1, bx = 3 * (e->x2 - e->x1) - cx, ax = 1 - cx - bx;
    double cy = 3 * e->y1, by = 3 * (e->y2 - e->y1) - cy, ay = 1 - cy - by;

    double t = x, i;
    for (i = 0; i < 8; i++) {
        double x2 = ((ax * t + bx) * t + cx) * t - x;
        if (fabs(x2) < 1e-6) return ((ay * t + by) * t + cy) * t;
        double d = (3 * ax * t + 2 * bx) * t + cx;
        if (fabs(d) < 1e-6) break;
        t -= x2 / d;
    }
    double lo = 0, hi = 1;
    t = x;
    for (i = 0; i < 20; i++) {
        double xv = ((ax * t + bx) * t + cx) * t;
        if (fabs(xv - x) < 1e-6) break;
        if (xv < x) lo = t; else hi = t;
        t = (lo + hi) / 2;
    }
    return ((ay * t + by) * t + cy) * t;
}

typedef struct { const char *name; gk_ease_t curve; } gk_ease_entry;

/* The eleven curves motion-kit ships, in its order. */
static const gk_ease_entry GK_EASES[] = {
    { "soft",     { 0.32, 0.72, 0, 1 } },
    { "exit",     { 0.4, 0, 1, 1 } },
    { "out",      { 0.215, 0.61, 0.355, 1 } },
    { "inOut",    { 0.645, 0.045, 0.355, 1 } },
    { "outBack",  { 0.12, 0.4, 0.29, 1.46 } },
    { "outQuint", { 0.23, 1, 0.32, 1 } },
    { "outCirc",  { 0.08, 0.82, 0.17, 1 } },
    { "css",      { 0.25, 0.1, 0.25, 1 } },
    { "cssIn",    { 0.42, 0, 1, 1 } },
    { "toast",    { 0.22, 1, 0.36, 1 } },
    { "linear",   { 0, 0, 1, 1 } },
};
#define GK_EASE_COUNT ((int)(sizeof GK_EASES / sizeof GK_EASES[0]))

GK_API static const gk_ease_t *gk_ease_named(const char *name) {
    for (int i = 0; i < GK_EASE_COUNT; i++) {
        if (strcmp(GK_EASES[i].name, name) == 0) return &GK_EASES[i].curve;
    }
    return NULL;
}

GK_API static double gk_ease(const gk_ease_t *e, double t) { return gk_bezier_y(e, t); }

/* ---------- springs ---------- */
typedef struct { double stiffness, damping, mass; } gk_spring_t;

/* The unit spring's step response and settle time, exactly as motion-kit
   computes them, so "smooth over 300 ms" means the same thing in both. */
GK_API static double gk_step_response(double zeta, double t) {
    if (zeta >= 1) return 1 - exp(-t) * (1 + t);
    double wd = sqrt(1 - zeta * zeta);
    return 1 - exp(-zeta * t) * (cos(wd * t) + (zeta / wd) * sin(wd * t));
}

GK_API static double gk_unit_settle(double zeta, double threshold) {
    const double STEP = 0.0005, HORIZON = 60.0;
    double last = 0;
    for (double t = 0; t < HORIZON; t += STEP) {
        if (fabs(gk_step_response(zeta, t) - 1) > threshold) last = t;
    }
    return last;
}

/* zeta + threshold + settle in ms -> the stiffness and damping that land there. */
GK_API static gk_spring_t gk_spring_preset(double zeta, double threshold, double settle_ms) {
    double omega = gk_unit_settle(zeta, threshold) / (settle_ms / 1000.0);
    gk_spring_t s = { omega * omega, 2 * zeta * omega, 1 };
    return s;
}

GK_API static gk_spring_t gk_smooth(double settle_ms) { return gk_spring_preset(1.0, 0.001, settle_ms); }
GK_API static gk_spring_t gk_snappy(double settle_ms) { return gk_spring_preset(0.8, 0.001, settle_ms); }
GK_API static gk_spring_t gk_bouncy(double settle_ms) { return gk_spring_preset(0.7, 0.003, settle_ms); }

/* The analytic solution of the damped oscillator, pulled from 0 to 1. */
GK_API static double gk_spring_at(gk_spring_t s, double ms) {
    double t = (ms > 0 ? ms : 0) / 1000.0;
    if (t == 0) return 0;
    double w0 = sqrt(s.stiffness / s.mass);
    double zeta = s.damping / (2 * sqrt(s.stiffness * s.mass));
    if (zeta < 1) {
        double wd = w0 * sqrt(1 - zeta * zeta);
        return 1 - exp(-zeta * w0 * t) * (cos(wd * t) + (zeta * w0 / wd) * sin(wd * t));
    }
    if (zeta == 1) return 1 - exp(-w0 * t) * (1 + w0 * t);
    double wp = w0 * sqrt(zeta * zeta - 1);
    return 1 - exp(-zeta * w0 * t) * (cosh(wp * t) + (zeta * w0 / wp) * sinh(wp * t));
}

/* ---------- tween ---------- */
GK_API static double gk_tween(double from, double to, double dur_ms, const gk_ease_t *e, double ms) {
    double p = dur_ms <= 0 ? 1 : ms / dur_ms;
    if (p < 0) p = 0;
    if (p > 1) p = 1;
    double k = e ? gk_ease(e, p) : p;
    return from + (to - from) * k;
}


/* ---------- camera ---------- */
/* A camera is a point in the world that sits at the middle of the viewport, plus
   a zoom. World to screen is a translate and a scale; screen to world is its
   inverse, which is what a click needs. Bounds keep the view inside the level. */
typedef struct {
    double x, y;          /* the world point at the centre of the viewport */
    double zoom;
    double vw, vh;        /* the viewport, in pixels */
    double bx, by, bw, bh; /* world bounds; bw or bh of 0 means unbounded */
} gk_camera;

GK_API static gk_camera gk_camera_make(double vw, double vh) {
    gk_camera c;
    memset(&c, 0, sizeof c);
    c.vw = vw;
    c.vh = vh;
    c.zoom = 1.0;
    return c;
}

GK_API static void gk_camera_bounds(gk_camera *c, double x, double y, double w, double h) {
    c->bx = x; c->by = y; c->bw = w; c->bh = h;
}

GK_API static void gk_camera_world_to_screen(const gk_camera *c, double wx, double wy,
                                             double *sx, double *sy) {
    if (sx) *sx = (wx - c->x) * c->zoom + c->vw * 0.5;
    if (sy) *sy = (wy - c->y) * c->zoom + c->vh * 0.5;
}

GK_API static void gk_camera_screen_to_world(const gk_camera *c, double sx, double sy,
                                             double *wx, double *wy) {
    if (wx) *wx = (sx - c->vw * 0.5) / c->zoom + c->x;
    if (wy) *wy = (sy - c->vh * 0.5) / c->zoom + c->y;
}

/* Move the view towards a target. speed is 0..1 per call: 1 snaps, small values
   ease. The step is applied through the same easing the kits use. */
GK_API static void gk_camera_follow(gk_camera *c, double tx, double ty, double step,
                                    double speed) {
    double k = speed <= 0 ? 0 : (speed >= 1 ? 1 : 1 - pow(1 - speed, step * 60.0));
    c->x += (tx - c->x) * k;
    c->y += (ty - c->y) * k;
}

/* Keep the whole viewport inside the world. When the world is smaller than the
   viewport there is nothing to keep, so the camera centres on it instead. */
GK_API static void gk_camera_clamp(gk_camera *c) {
    if (c->zoom <= 0) c->zoom = 1;
    double half_w = c->vw / (2 * c->zoom);
    double half_h = c->vh / (2 * c->zoom);
    if (c->bw > 0) {
        if (c->bw <= half_w * 2) c->x = c->bx + c->bw * 0.5;
        else {
            if (c->x - half_w < c->bx) c->x = c->bx + half_w;
            if (c->x + half_w > c->bx + c->bw) c->x = c->bx + c->bw - half_w;
        }
    }
    if (c->bh > 0) {
        if (c->bh <= half_h * 2) c->y = c->by + c->bh * 0.5;
        else {
            if (c->y - half_h < c->by) c->y = c->by + half_h;
            if (c->y + half_h > c->by + c->bh) c->y = c->by + c->bh - half_h;
        }
    }
}


/* ---------- tilemap ---------- */
/* A grid of tile ids with a solid flag per id, and the move that every
   platformer needs: step an axis, then push back out of whatever is solid.
   Resolving one axis at a time is what makes a corner slide instead of stick. */
typedef struct {
    int w, h;          /* in tiles */
    int tile;          /* pixels per tile */
    const unsigned char *ids;   /* w*h, row-major */
    const unsigned char *solid; /* 256 flags, indexed by id */
    int outside_solid;          /* is beyond the edge a wall? 1 by default */
} gk_tilemap;

GK_API static int gk_tilemap_id(const gk_tilemap *m, int tx, int ty) {
    if (!m || tx < 0 || ty < 0 || tx >= m->w || ty >= m->h) return -1;
    return m->ids[ty * m->w + tx];
}

GK_API static int gk_tilemap_is_solid(const gk_tilemap *m, int tx, int ty) {
    if (!m) return 0;
    if (tx < 0 || ty < 0 || tx >= m->w || ty >= m->h) return m->outside_solid;
    int id = m->ids[ty * m->w + tx];
    return id >= 0 && id < 256 ? (m->solid[id] ? 1 : 0) : 0;
}

/* World pixels to a tile index. Floor, not truncate: -0.5 is tile -1. */
GK_API static int gk_tilemap_tile_at(double v, int tile) {
    return (int)floor(v / (double)tile);
}

GK_API static int gk_tilemap_solid_at(const gk_tilemap *m, double x, double y) {
    if (!m || m->tile <= 0) return 0;
    return gk_tilemap_is_solid(m, gk_tilemap_tile_at(x, m->tile), gk_tilemap_tile_at(y, m->tile));
}

/* Step one axis and push back out. The epsilon keeps a box that ends exactly on a
   tile boundary from counting as inside it. */
GK_API static double gk_move_axis(const gk_tilemap *m, const gk_rect *b, double delta, int horizontal) {
    if (!m || m->tile <= 0) return delta;   /* no map: nothing to collide with */
    if (delta == 0) return 0;
    const double EPS = 1e-9;

    /* Sweep the leading edge across every tile it crosses, nearest first, and
       stop at the first solid one. Checking only the destination tile would let a
       fast mover pass through a wall it should have hit. */
    if (horizontal) {
        int y0 = gk_tilemap_tile_at(b->y, m->tile);
        int y1 = gk_tilemap_tile_at(b->y + b->h - EPS, m->tile);
        int from, to, step;
        if (delta > 0) {
            from = gk_tilemap_tile_at(b->x + b->w - EPS, m->tile);
            to   = gk_tilemap_tile_at(b->x + b->w + delta - EPS, m->tile);
            step = 1;
        } else {
            from = gk_tilemap_tile_at(b->x, m->tile);
            to   = gk_tilemap_tile_at(b->x + delta, m->tile);
            step = -1;
        }
        for (int tx = from; step > 0 ? tx <= to : tx >= to; tx += step) {
            for (int ty = y0; ty <= y1; ty++) {
                if (gk_tilemap_is_solid(m, tx, ty)) {
                    return delta > 0 ? (tx * m->tile - (b->x + b->w))
                                     : ((tx + 1) * m->tile - b->x);
                }
            }
        }
    } else {
        int x0 = gk_tilemap_tile_at(b->x, m->tile);
        int x1 = gk_tilemap_tile_at(b->x + b->w - EPS, m->tile);
        int from, to, step;
        if (delta > 0) {
            from = gk_tilemap_tile_at(b->y + b->h - EPS, m->tile);
            to   = gk_tilemap_tile_at(b->y + b->h + delta - EPS, m->tile);
            step = 1;
        } else {
            from = gk_tilemap_tile_at(b->y, m->tile);
            to   = gk_tilemap_tile_at(b->y + delta, m->tile);
            step = -1;
        }
        for (int ty = from; step > 0 ? ty <= to : ty >= to; ty += step) {
            for (int tx = x0; tx <= x1; tx++) {
                if (gk_tilemap_is_solid(m, tx, ty)) {
                    return delta > 0 ? (ty * m->tile - (b->y + b->h))
                                     : ((ty + 1) * m->tile - b->y);
                }
            }
        }
    }
    return delta;
}

/* Collide and slide: x first, then y, each against the box as it now stands. */
GK_API static gk_rect gk_move(const gk_tilemap *m, gk_rect box, double dx, double dy) {
    box.x += gk_move_axis(m, &box, dx, 1);
    box.y += gk_move_axis(m, &box, dy, 0);
    return box;
}


/* ---------- sprites and animation ---------- */
/* An atlas is a grid of frames in one image: frame n sits at (n % cols, n / cols)
   of frame_w x frame_h. Nothing here touches a texture - it produces rectangles,
   which is what a renderer needs and what a test can check. */
typedef struct {
    int frame_w, frame_h;
    int cols, count;
} gk_atlas;

typedef struct {
    gk_atlas atlas;
    double fps;
    int from, to;   /* inclusive frame range to play */
    int loop;       /* 1 wraps, 0 holds the last frame */
} gk_anim;

GK_API static gk_rect gk_atlas_src(const gk_atlas *a, int frame) {
    gk_rect r = { 0, 0, 0, 0 };
    if (!a || a->count <= 0 || a->cols <= 0 || a->frame_w <= 0 || a->frame_h <= 0) return r;
    int f = frame % a->count;
    if (f < 0) f += a->count;
    r.x = (float)((f % a->cols) * a->frame_w);
    r.y = (float)((f / a->cols) * a->frame_h);
    r.w = (float)a->frame_w;
    r.h = (float)a->frame_h;
    return r;
}

/* Which frame is showing at time t (seconds). fps of 0 is a still image. */
GK_API static int gk_anim_frame(const gk_anim *an, double t) {
    if (!an) return 0;
    int from = an->from, to = an->to;
    if (to < from) { int tmp = from; from = to; to = tmp; }
    int span = to - from + 1;
    if (span <= 0) return 0;
    if (!(an->fps > 0) || !(t > 0)) return from;
    int step = (int)(t * an->fps);
    if (step < 0) step = 0;
    if (an->loop) return from + (step % span);
    int f = from + step;
    return f > to ? to : f;
}

/* Where a world-space sprite of w x h lands on screen, through a camera. */
GK_API static void gk_sprite_dst(const gk_camera *c, double wx, double wy, double w, double h,
                                 double *dx, double *dy, double *dw, double *dh) {
    double sx = wx, sy = wy;
    if (c) gk_camera_world_to_screen(c, wx, wy, &sx, &sy);
    double z = (c && c->zoom > 0) ? c->zoom : 1.0;
    if (dx) *dx = sx;
    if (dy) *dy = sy;
    if (dw) *dw = w * z;
    if (dh) *dh = h * z;
}

/* ---------- drawing tiles ---------- */
/* The tiles a camera can see, as world rectangles. A renderer walks this list and
   draws each; a test walks it and counts. Empty tiles are reported too, with id 0,
   because a renderer may want to draw the background from the same list. */
typedef struct {
    int tx, ty, id;
    double x, y, w, h;   /* in world pixels */
} gk_visible_tile;

GK_API static int gk_tiles_visible(const gk_tilemap *m, const gk_camera *c,
                                   gk_visible_tile *out, int max) {
    if (!m || m->tile <= 0 || !out || max <= 0) return 0;
    /* the world rectangle the camera covers */
    double wx0 = 0, wy0 = 0, wx1 = 0, wy1 = 0;
    if (c) {
        double z = c->zoom > 0 ? c->zoom : 1.0;
        wx0 = c->x - c->vw / (2 * z);
        wy0 = c->y - c->vh / (2 * z);
        wx1 = c->x + c->vw / (2 * z);
        wy1 = c->y + c->vh / (2 * z);
    } else {
        wx1 = (double)m->w * m->tile;
        wy1 = (double)m->h * m->tile;
    }
    int tx0 = gk_tilemap_tile_at(wx0, m->tile);
    int ty0 = gk_tilemap_tile_at(wy0, m->tile);
    int tx1 = gk_tilemap_tile_at(wx1 - 1e-9, m->tile);
    int ty1 = gk_tilemap_tile_at(wy1 - 1e-9, m->tile);
    if (tx0 < 0) tx0 = 0;
    if (ty0 < 0) ty0 = 0;
    if (tx1 >= m->w) tx1 = m->w - 1;
    if (ty1 >= m->h) ty1 = m->h - 1;

    int n = 0;
    for (int ty = ty0; ty <= ty1 && n < max; ty++) {
        for (int tx = tx0; tx <= tx1 && n < max; tx++) {
            out[n].tx = tx;
            out[n].ty = ty;
            out[n].id = gk_tilemap_id(m, tx, ty);
            out[n].x = (double)tx * m->tile;
            out[n].y = (double)ty * m->tile;
            out[n].w = m->tile;
            out[n].h = m->tile;
            n++;
        }
    }
    return n;
}

/* ---------- loop ---------- */
typedef void (*gk_step_fn)(void *ud, double step);
typedef void (*gk_draw_fn)(void *ud, double alpha);

typedef struct {
    double step;      /* seconds per fixed step */
    double max_frame; /* a frame longer than this is treated as this */
    int max_steps;    /* hard cap per tick, so a stall never spirals */
    double acc;
    uint64_t ticks;
} gk_loop;

GK_API static void gk_loop_init(gk_loop *l, double step) {
    l->step = step > 0 ? step : 1.0 / 60.0;
    l->max_frame = 0.25;
    l->max_steps = 240;
    l->acc = 0;
    l->ticks = 0;
}

/* Run as many fixed steps as dt earned, then draw once with the leftover as an
   alpha in [0,1). Returns the number of steps run. */
GK_API static int gk_loop_tick(gk_loop *l, double dt, gk_step_fn step_fn, gk_draw_fn draw_fn, void *ud) {
    if (!(dt > 0)) dt = 0;
    if (dt > l->max_frame) dt = l->max_frame;
    l->acc += dt;

    int steps = 0;
    while (l->acc + 1e-9 >= l->step && steps < l->max_steps) {
        if (step_fn) step_fn(ud, l->step);
        l->acc -= l->step;
        steps++;
        l->ticks++;
    }
    if (steps >= l->max_steps) l->acc = 0; /* never spiral */

    if (draw_fn) draw_fn(ud, l->step > 0 ? l->acc / l->step : 0);
    return steps;
}

/* ---------- input ---------- */
typedef enum {
    GK_KEY_LEFT, GK_KEY_RIGHT, GK_KEY_UP, GK_KEY_DOWN,
    GK_KEY_ACTION, GK_KEY_START, GK_KEY_COUNT
} gk_key;

typedef struct {
    uint8_t down[GK_KEY_COUNT];
    uint8_t pressed[GK_KEY_COUNT];
} gk_input;

GK_API static void gk_input_down(gk_input *in, gk_key k) {
    if (k < 0 || k >= GK_KEY_COUNT) return;
    in->down[k] = 1;
    in->pressed[k] = 1;
}
GK_API static void gk_input_up(gk_input *in, gk_key k) {
    if (k < 0 || k >= GK_KEY_COUNT) return;
    in->down[k] = 0;
}
GK_API static int gk_input_is_down(const gk_input *in, gk_key k) {
    return (k >= 0 && k < GK_KEY_COUNT) ? in->down[k] : 0;
}
GK_API static int gk_input_was_pressed(const gk_input *in, gk_key k) {
    return (k >= 0 && k < GK_KEY_COUNT) ? in->pressed[k] : 0;
}
/* Call once at the end of a frame: "pressed" lasts exactly one frame. */
GK_API static void gk_input_end_frame(gk_input *in) {
    memset(in->pressed, 0, sizeof in->pressed);
}

/* ---------- scenes ---------- */
#define GK_MAX_SCENES 8

typedef struct gk_scene {
    const char *name;
    void (*enter)(void *ud, const void *data);
    void (*exit)(void *ud);
    void (*update)(void *ud, double step);
    void (*render)(void *ud, double alpha);
} gk_scene;

typedef struct {
    const gk_scene *list[GK_MAX_SCENES];
    int count;
    const gk_scene *current;
    const char *current_name;
} gk_scenes;

GK_API static void gk_scenes_init(gk_scenes *s) {
    memset(s, 0, sizeof *s);
}

GK_API static int gk_scenes_add(gk_scenes *s, const gk_scene *scene) {
    if (!scene || s->count >= GK_MAX_SCENES) return -1;
    s->list[s->count++] = scene;
    return 0;
}

GK_API static const gk_scene *gk_scenes_find(const gk_scenes *s, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < s->count; i++) {
        if (s->list[i]->name && strcmp(s->list[i]->name, name) == 0) return s->list[i];
    }
    return NULL;
}

/* Leave the current scene, then enter the named one. An unknown name leaves the
   registry untouched rather than blanking the screen. */
GK_API static int gk_scenes_go(gk_scenes *s, const char *name, void *ud, const void *data) {
    const gk_scene *next = gk_scenes_find(s, name);
    if (!next) return -1;
    if (s->current && s->current->exit) s->current->exit(ud);
    s->current = next;
    s->current_name = next->name;
    if (next->enter) next->enter(ud, data);
    return 0;
}

GK_API static void gk_scenes_update(gk_scenes *s, void *ud, double step) {
    if (s->current && s->current->update) s->current->update(ud, step);
}
GK_API static void gk_scenes_render(gk_scenes *s, void *ud, double alpha) {
    if (s->current && s->current->render) s->current->render(ud, alpha);
}
GK_API static const char *gk_scenes_current(const gk_scenes *s) { return s->current_name; }

/* ---------- save / load ---------- */
#define GK_SAVE_MAGIC 0x474B5331u /* "GKS1" */
#define GK_SAVE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t crc;
} gk_save_header;

enum {
    GK_SAVE_OK = 0,
    GK_SAVE_ERR_OPEN = -1,
    GK_SAVE_ERR_MAGIC = -2,
    GK_SAVE_ERR_VERSION = -3,
    GK_SAVE_ERR_SIZE = -4,
    GK_SAVE_ERR_CRC = -5
};

GK_API static uint32_t gk_crc32(const void *data, size_t n) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return ~crc;
}

GK_API static int gk_save(const char *path, const void *data, uint32_t size) {
    if (!path || (!data && size)) return GK_SAVE_ERR_OPEN;
    FILE *f = fopen(path, "wb");
    if (!f) return GK_SAVE_ERR_OPEN;
    gk_save_header h = { GK_SAVE_MAGIC, GK_SAVE_VERSION, size, gk_crc32(data, size) };
    int ok = fwrite(&h, sizeof h, 1, f) == 1 && (size == 0 || fwrite(data, size, 1, f) == 1);
    fclose(f);
    return ok ? GK_SAVE_OK : GK_SAVE_ERR_OPEN;
}

/* Refuses a file it did not write, one from a newer version, one of the wrong
   size, and one whose bytes changed - so a half-written save is caught rather
   than loaded. */
GK_API static int gk_load(const char *path, void *data, uint32_t size) {
    if (!path || (!data && size)) return GK_SAVE_ERR_OPEN;
    FILE *f = fopen(path, "rb");
    if (!f) return GK_SAVE_ERR_OPEN;
    gk_save_header h;
    if (fread(&h, sizeof h, 1, f) != 1) { fclose(f); return GK_SAVE_ERR_OPEN; }
    if (h.magic != GK_SAVE_MAGIC) { fclose(f); return GK_SAVE_ERR_MAGIC; }
    if (h.version != GK_SAVE_VERSION) { fclose(f); return GK_SAVE_ERR_VERSION; }
    if (h.size != size) { fclose(f); return GK_SAVE_ERR_SIZE; }
    if (size && fread(data, size, 1, f) != 1) { fclose(f); return GK_SAVE_ERR_OPEN; }
    fclose(f);
    if (gk_crc32(data, size) != h.crc) return GK_SAVE_ERR_CRC;
    return GK_SAVE_OK;
}

/* ---------- ecs ---------- */
/* Capacities are compile-time, so a bigger game raises them with -D and pays
   nothing when it does not. No allocation anywhere in the core. */
#ifndef GK_MAX_ENTITIES
#define GK_MAX_ENTITIES 256
#endif
#ifndef GK_MAX_COMPONENTS
#define GK_MAX_COMPONENTS 8
#endif
#ifndef GK_MAX_SYSTEMS
#define GK_MAX_SYSTEMS 8
#endif

struct gk_entity {
    uint32_t id;
    uint32_t mask;
    void *comp[GK_MAX_COMPONENTS];
};

typedef struct gk_world gk_world;

typedef struct gk_entity gk_entity;

typedef struct {
    uint32_t mask;
    void (*fn)(gk_world *, gk_entity *e);
} gk_system;

struct gk_world {
    gk_entity ents[GK_MAX_ENTITIES];
    int count;
    uint32_t next_id;
    gk_system systems[GK_MAX_SYSTEMS];
    int sys_count;
};

GK_API static void gk_world_init(gk_world *w) {
    memset(w, 0, sizeof *w);
    w->next_id = 1; /* 0 is never an entity */
}

GK_API static gk_entity *gk_ecs_slot(gk_world *w, uint32_t id) {
    for (int i = 0; i < w->count; i++) if (w->ents[i].id == id) return &w->ents[i];
    return NULL;
}

GK_API static uint32_t gk_ecs_create(gk_world *w) {
    if (w->count >= GK_MAX_ENTITIES) return 0;
    gk_entity *e = &w->ents[w->count++];
    memset(e, 0, sizeof *e);
    e->id = w->next_id++;
    return e->id;
}

GK_API static int gk_ecs_destroy(gk_world *w, uint32_t id) {
    for (int i = 0; i < w->count; i++) {
        if (w->ents[i].id == id) {
            w->ents[i] = w->ents[--w->count];
            return 0;
        }
    }
    return -1;
}

GK_API static int gk_ecs_add(gk_world *w, uint32_t id, int comp, void *data) {
    if (comp < 0 || comp >= GK_MAX_COMPONENTS) return -1;
    gk_entity *e = gk_ecs_slot(w, id);
    if (!e) return -1;
    e->comp[comp] = data;
    e->mask |= (1u << comp);
    return 0;
}

/* The fast path: a system already holds the entity, so there is nothing to look
   up. Every id-based accessor below is for callers that only have an id. */
GK_API static void *gk_ecs_comp(gk_entity *e, int comp) {
    if (!e || comp < 0 || comp >= GK_MAX_COMPONENTS) return NULL;
    return (e->mask & (1u << comp)) ? e->comp[comp] : NULL;
}

GK_API static int gk_ecs_comp_has(gk_entity *e, int comp) {
    if (!e || comp < 0 || comp >= GK_MAX_COMPONENTS) return 0;
    return (e->mask & (1u << comp)) ? 1 : 0;
}

GK_API static void *gk_ecs_get(gk_world *w, uint32_t id, int comp) {
    if (comp < 0 || comp >= GK_MAX_COMPONENTS) return NULL;
    gk_entity *e = gk_ecs_slot(w, id);
    return (e && (e->mask & (1u << comp))) ? e->comp[comp] : NULL;
}

GK_API static int gk_ecs_has(gk_world *w, uint32_t id, int comp) {
    if (comp < 0 || comp >= GK_MAX_COMPONENTS) return 0;
    gk_entity *e = gk_ecs_slot(w, id);
    return (e && (e->mask & (1u << comp))) ? 1 : 0;
}

GK_API static int gk_ecs_remove(gk_world *w, uint32_t id, int comp) {
    if (comp < 0 || comp >= GK_MAX_COMPONENTS) return -1;
    gk_entity *e = gk_ecs_slot(w, id);
    if (!e) return -1;
    e->mask &= ~(1u << comp);
    e->comp[comp] = NULL;
    return 0;
}

GK_API static int gk_ecs_count(const gk_world *w) { return w->count; }

GK_API static int gk_ecs_system(gk_world *w, uint32_t mask, void (*fn)(gk_world *, gk_entity *)) {
    if (!fn || w->sys_count >= GK_MAX_SYSTEMS) return -1;
    w->systems[w->sys_count].mask = mask;
    w->systems[w->sys_count].fn = fn;
    w->sys_count++;
    return 0;
}

/* Run every system in the order it was added, over the entities that hold the
   components it names. Returns the number of (system, entity) pairs run. */
GK_API static int gk_ecs_run(gk_world *w) {
    int ran = 0;
    for (int s = 0; s < w->sys_count; s++) {
        uint32_t need = w->systems[s].mask;
        for (int i = 0; i < w->count; i++) {
            if ((w->ents[i].mask & need) == need) {
                w->systems[s].fn(w, &w->ents[i]);
                ran++;
            }
        }
    }
    return ran;
}

/* ==================================================================== */
/* the game                                                             */
/* ==================================================================== */

#define GK_MAX_MOTES 12
#define GK_WORLD_W 1280.0
#define GK_WORLD_H 800.0
#define GK_TILE 40
#define GK_TILES_X 32          /* 32 x 40 = 1280 */
#define GK_TILES_Y 20          /* 20 x 40 = 800 */

typedef struct { float x, y; } gk_vel;
typedef struct { gk_rect box; } gk_body;
typedef struct { int value; } gk_worth;

enum { COMP_BODY = 0, COMP_VEL = 1, COMP_WORTH = 2 };

typedef struct {
    gk_loop loop;
    gk_scenes scenes;
    gk_input input;
    gk_world world;
    gk_rng rng;

    float px, py;            /* the player */
    int score, best, left;   /* left = motes still to collect */
    double pulse;            /* eases out when a mote is collected */
    gk_camera cam;           /* follows the player over a world bigger than the window */
    gk_tilemap map;          /* a solid border, so the player collides instead of bouncing */
    gk_anim mote_anim;       /* the motes pulse through four frames */
    unsigned char tiles[GK_TILES_X * GK_TILES_Y];
    double clock;
    uint32_t player_id;
    gk_body player_body;
    gk_vel player_vel;
} game;

static game G;

/* ---------- save file ---------- */
typedef struct { int best; } gk_progress;

GK_API static const char *gk_save_path(void) { return "gk-progress.bin"; }

GK_API static void gk_progress_load(void) {
    gk_progress p;
    if (gk_load(gk_save_path(), &p, sizeof p) == GK_SAVE_OK) G.best = p.best;
}
GK_API static void gk_progress_store(void) {
    gk_progress p = { G.best };
    gk_save(gk_save_path(), &p, sizeof p);
}

/* ---------- systems ---------- */
GK_API static void sys_move(gk_world *w, gk_entity *e) {
    (void)w;
    if (e->id == G.player_id) return;   /* the player collides with the tilemap instead */
    gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
    gk_vel *v = (gk_vel *)gk_ecs_comp(e, COMP_VEL);
    if (!b || !v) return;
    b->box.x += v->x * (float)G.loop.step;
    b->box.y += v->y * (float)G.loop.step;
    /* bounce off the field edges rather than leaving it */
    if (b->box.x < 0) { b->box.x = 0; v->x = -v->x; }
    if (b->box.y < 0) { b->box.y = 0; v->y = -v->y; }
    if (b->box.x + b->box.w > GK_WORLD_W) { b->box.x = (float)(GK_WORLD_W - b->box.w); v->x = -v->x; }
    if (b->box.y + b->box.h > GK_WORLD_H) { b->box.y = (float)(GK_WORLD_H - b->box.h); v->y = -v->y; }
}

/* ---------- scenes ---------- */
GK_API static void title_enter(void *ud, const void *data) { (void)ud; (void)data; }
GK_API static void title_update(void *ud, double step) {
    (void)ud; (void)step;
    if (gk_input_was_pressed(&G.input, GK_KEY_START) || gk_input_was_pressed(&G.input, GK_KEY_ACTION)) {
        gk_scenes_go(&G.scenes, "play", &G, NULL);
    }
}

GK_API static void play_enter(void *ud, const void *data) {
    (void)ud; (void)data;
    G.score = 0;
    G.clock = 0;
    G.left = 6;
    G.px = 320;
    G.py = 200;
    G.cam = gk_camera_make(640, 400);
    gk_camera_bounds(&G.cam, 0, 0, GK_WORLD_W, GK_WORLD_H);
    G.cam.x = G.px;
    G.cam.y = G.py;
    gk_world_init(&G.world);
    gk_rng_seed(&G.rng, (uint32_t)(G.best * 7919u + 12345u));

    /* one ring of solid tiles around the world; the middle is empty */
    for (int ty = 0; ty < GK_TILES_Y; ty++) {
        for (int tx = 0; tx < GK_TILES_X; tx++) {
            int edge = tx == 0 || ty == 0 || tx == GK_TILES_X - 1 || ty == GK_TILES_Y - 1;
            G.tiles[ty * GK_TILES_X + tx] = edge ? 1 : 0;
        }
    }
    static const unsigned char SOLID_IDS[256] = { 0, 1 };
    G.map = (gk_tilemap){ GK_TILES_X, GK_TILES_Y, GK_TILE, G.tiles, SOLID_IDS, 1 };
    G.mote_anim = (gk_anim){ { 12, 12, 2, 4 }, 6.0, 0, 3, 1 };

    G.player_id = gk_ecs_create(&G.world);
    G.player_body.box = (gk_rect){ G.px, G.py, 18, 18 };
    G.player_vel = (gk_vel){ 0, 0 };
    gk_ecs_add(&G.world, G.player_id, COMP_BODY, &G.player_body);
    gk_ecs_add(&G.world, G.player_id, COMP_VEL, &G.player_vel);

    for (int i = 0; i < G.left; i++) {
        uint32_t id = gk_ecs_create(&G.world);
        gk_body *b = (gk_body *)calloc(1, sizeof *b);
        gk_vel *v = (gk_vel *)calloc(1, sizeof *v);
        gk_worth *wr = (gk_worth *)calloc(1, sizeof *wr);
        b->box = (gk_rect){ (float)gk_rng_int(&G.rng, 20, (int)GK_WORLD_W - 20),
                            (float)gk_rng_int(&G.rng, 20, (int)GK_WORLD_H - 20), 12, 12 };
        v->x = (float)gk_rng_int(&G.rng, -60, 60);
        v->y = (float)gk_rng_int(&G.rng, -60, 60);
        wr->value = gk_rng_int(&G.rng, 1, 3);
        gk_ecs_add(&G.world, id, COMP_BODY, b);
        gk_ecs_add(&G.world, id, COMP_VEL, v);
        gk_ecs_add(&G.world, id, COMP_WORTH, wr);
    }
    gk_ecs_system(&G.world, (1u << COMP_BODY) | (1u << COMP_VEL), sys_move);
}

GK_API static void play_update(void *ud, double step) {
    (void)ud;
    G.clock += step;

    /* the player is an entity too, so it moves through the same system */
    float speed = 160.0f;
    G.player_vel.x = 0;
    G.player_vel.y = 0;
    if (gk_input_is_down(&G.input, GK_KEY_LEFT)) G.player_vel.x -= speed;
    if (gk_input_is_down(&G.input, GK_KEY_RIGHT)) G.player_vel.x += speed;
    if (gk_input_is_down(&G.input, GK_KEY_UP)) G.player_vel.y -= speed;
    if (gk_input_is_down(&G.input, GK_KEY_DOWN)) G.player_vel.y += speed;
    /* the player is moved by hand, through the tilemap; the motes bounce in the
       ECS system above, which is why the player is skipped there */
    G.player_body.box = gk_move(&G.map, G.player_body.box,
                                G.player_vel.x * step, G.player_vel.y * step);
    G.px = G.player_body.box.x;
    G.py = G.player_body.box.y;

    gk_ecs_run(&G.world);
    gk_camera_follow(&G.cam, G.player_body.box.x + G.player_body.box.w * 0.5,
                     G.player_body.box.y + G.player_body.box.h * 0.5, step, 0.12);
    gk_camera_clamp(&G.cam);
    if (G.pulse > 0) G.pulse -= step * 3.0;
    if (G.pulse < 0) G.pulse = 0;

    /* collect: the player against every mote */
    for (int i = 0; i < G.world.count; i++) {
        gk_entity *e = &G.world.ents[i];
        if (e->id == G.player_id) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        gk_worth *wr = (gk_worth *)gk_ecs_comp(e, COMP_WORTH);
        if (!b || !wr) continue;
        if (gk_aabb(G.player_body.box, b->box)) {
            G.score += wr->value;
            G.pulse = 1.0;
            gk_ecs_destroy(&G.world, e->id); /* swap-removes, so step back */
            G.left--;
            i--;
        }
    }

    if (G.left <= 0 || G.clock > 30.0) {
        if (G.score > G.best) { G.best = G.score; gk_progress_store(); }
        gk_scenes_go(&G.scenes, "over", &G, NULL);
    }
}

GK_API static void over_update(void *ud, double step) {
    (void)ud; (void)step;
    if (gk_input_was_pressed(&G.input, GK_KEY_START) || gk_input_was_pressed(&G.input, GK_KEY_ACTION)) {
        gk_scenes_go(&G.scenes, "title", &G, NULL);
    }
}

/* ---------- drawing (the only raylib-specific part of the game) ---------- */
#ifndef GK_NO_MAIN
#include "raylib.h"

GK_API static void title_render(void *ud, double alpha) {
    (void)ud; (void)alpha;
    DrawText("raylib-starter", 190, 120, 30, RAYWHITE);
    DrawText("arrows move, space starts", 200, 170, 16, GRAY);
    DrawText(TextFormat("best %d", G.best), 290, 210, 18, GREEN);
}

GK_API static void play_render(void *ud, double alpha) {
    (void)ud; (void)alpha;
    /* everything is drawn in world space, pushed through the camera */
    double sx, sy;
    gk_camera_world_to_screen(&G.cam, G.player_body.box.x, G.player_body.box.y, &sx, &sy);
    DrawRectangleRec((Rectangle){ (float)sx, (float)sy,
                                  (float)(G.player_body.box.w * G.cam.zoom),
                                  (float)(G.player_body.box.h * G.cam.zoom) }, RAYWHITE);
    /* the collect pulse: a ring that eases outward, the same curve a JS game uses */
    if (G.pulse > 0) {
        const gk_ease_t *e = gk_ease_named("out");
        float k = (float)gk_ease(e, 1.0 - G.pulse);
        float grow = 6.0f * k;
        DrawRectangleLines((int)(sx - grow), (int)(sy - grow),
                           (int)(G.player_body.box.w + grow * 2), (int)(G.player_body.box.h + grow * 2),
                           Fade(GREEN, 1.0f - k));
    }
    /* the border, drawn from the tiles the camera can actually see */
    gk_visible_tile vis[256];
    int n = gk_tiles_visible(&G.map, &G.cam, vis, 256);
    for (int i = 0; i < n; i++) {
        if (vis[i].id == 0) continue;               /* empty: the background shows through */
        double tx, ty, tw, th;
        gk_sprite_dst(&G.cam, vis[i].x, vis[i].y, vis[i].w, vis[i].h, &tx, &ty, &tw, &th);
        DrawRectangleRec((Rectangle){ (float)tx, (float)ty, (float)tw, (float)th }, DARKGRAY);
    }

    /* the motes pulse through four frames of an atlas */
    int frame = gk_anim_frame(&G.mote_anim, G.clock);
    float shrink = (float)(frame % 2) * 2.0f;
    for (int i = 0; i < G.world.count; i++) {
        gk_entity *e = &G.world.ents[i];
        if (e->id == G.player_id) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        if (!b) continue;
        double mx, my, mw, mh;
        gk_sprite_dst(&G.cam, b->box.x + shrink, b->box.y + shrink,
                      b->box.w - shrink * 2, b->box.h - shrink * 2, &mx, &my, &mw, &mh);
        DrawRectangleRec((Rectangle){ (float)mx, (float)my, (float)mw, (float)mh }, GREEN);
    }
    DrawText(TextFormat("score %d   left %d   %0.0fs", G.score, G.left, G.clock), 12, 10, 18, RAYWHITE);
}

GK_API static void over_render(void *ud, double alpha) {
    (void)ud; (void)alpha;
    DrawText("done", 290, 150, 30, RAYWHITE);
    DrawText(TextFormat("score %d", G.score), 290, 195, 20, GREEN);
    DrawText(TextFormat("best %d", G.best), 290, 220, 18, GRAY);
    DrawText("space to play again", 240, 265, 16, GRAY);
}

static const gk_scene SCENE_TITLE = { "title", title_enter, NULL, title_update, title_render };
static const gk_scene SCENE_PLAY = { "play", play_enter, NULL, play_update, play_render };
static const gk_scene SCENE_OVER = { "over", NULL, NULL, over_update, over_render };

/* The loop wants (void*, double); the scenes want (gk_scenes*, void*, double).
   Two adapters rather than a cast, so the function types actually match. */
GK_API static void loop_step(void *ud, double step) { gk_scenes_update(&G.scenes, ud, step); }
GK_API static void loop_draw(void *ud, double alpha) { gk_scenes_render(&G.scenes, ud, alpha); }

GK_API static void read_input(void) {
    struct { int key; gk_key gk; } map[] = {
        { KEY_LEFT, GK_KEY_LEFT }, { KEY_RIGHT, GK_KEY_RIGHT },
        { KEY_UP, GK_KEY_UP }, { KEY_DOWN, GK_KEY_DOWN },
        { KEY_SPACE, GK_KEY_ACTION }, { KEY_ENTER, GK_KEY_START },
    };
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (IsKeyPressed(map[i].key)) gk_input_down(&G.input, map[i].gk);
        else if (IsKeyReleased(map[i].key)) gk_input_up(&G.input, map[i].gk);
    }
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(640, 400, "raylib-starter");
    SetTargetFPS(60);

    memset(&G, 0, sizeof G);
    gk_loop_init(&G.loop, 1.0 / 60.0);
    gk_scenes_init(&G.scenes);
    gk_scenes_add(&G.scenes, &SCENE_TITLE);
    gk_scenes_add(&G.scenes, &SCENE_PLAY);
    gk_scenes_add(&G.scenes, &SCENE_OVER);
    gk_progress_load();
    gk_scenes_go(&G.scenes, "title", &G, NULL);

    while (!WindowShouldClose()) {
        read_input();
        BeginDrawing();
        ClearBackground((Color){ 20, 22, 28, 255 });
        gk_loop_tick(&G.loop, GetFrameTime(), loop_step, loop_draw, &G);
        gk_input_end_frame(&G.input);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
#endif /* GK_NO_MAIN */
