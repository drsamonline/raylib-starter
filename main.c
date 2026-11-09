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

/* ---------- maths ---------- */
static inline float gk_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float gk_lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float gk_dist(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return sqrtf(dx * dx + dy * dy);
}

typedef struct { float x, y, w, h; } gk_rect;
typedef struct { float x, y, r; } gk_circle;

static inline int gk_aabb(gk_rect a, gk_rect b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}
static inline int gk_circle_hit(gk_circle a, gk_circle b) {
    return gk_dist(a.x, a.y, b.x, b.y) <= (a.r + b.r);
}

/* ---------- rng: mulberry32, bit-identical to game-kit ---------- */
typedef struct { uint32_t s; } gk_rng;

static void gk_rng_seed(gk_rng *r, uint32_t seed) {
    r->s = seed ? seed : 0x9e3779b9u;
}

static double gk_rng_next(gk_rng *r) {
    uint32_t a = r->s + 0x6D2B79F5u;
    r->s = a;
    uint32_t t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}

static int gk_rng_int(gk_rng *r, int a, int b) {
    return a + (int)(gk_rng_next(r) * (double)(b - a + 1));
}

static int gk_rng_pick(gk_rng *r, int n) {
    int i = (int)(gk_rng_next(r) * (double)n);
    return i < 0 ? 0 : (i >= n ? n - 1 : i);
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

static void gk_loop_init(gk_loop *l, double step) {
    l->step = step > 0 ? step : 1.0 / 60.0;
    l->max_frame = 0.25;
    l->max_steps = 240;
    l->acc = 0;
    l->ticks = 0;
}

/* Run as many fixed steps as dt earned, then draw once with the leftover as an
   alpha in [0,1). Returns the number of steps run. */
static int gk_loop_tick(gk_loop *l, double dt, gk_step_fn step_fn, gk_draw_fn draw_fn, void *ud) {
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

static void gk_input_down(gk_input *in, gk_key k) {
    if (k < 0 || k >= GK_KEY_COUNT) return;
    in->down[k] = 1;
    in->pressed[k] = 1;
}
static void gk_input_up(gk_input *in, gk_key k) {
    if (k < 0 || k >= GK_KEY_COUNT) return;
    in->down[k] = 0;
}
static int gk_input_is_down(const gk_input *in, gk_key k) {
    return (k >= 0 && k < GK_KEY_COUNT) ? in->down[k] : 0;
}
static int gk_input_was_pressed(const gk_input *in, gk_key k) {
    return (k >= 0 && k < GK_KEY_COUNT) ? in->pressed[k] : 0;
}
/* Call once at the end of a frame: "pressed" lasts exactly one frame. */
static void gk_input_end_frame(gk_input *in) {
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

static void gk_scenes_init(gk_scenes *s) {
    memset(s, 0, sizeof *s);
}

static int gk_scenes_add(gk_scenes *s, const gk_scene *scene) {
    if (!scene || s->count >= GK_MAX_SCENES) return -1;
    s->list[s->count++] = scene;
    return 0;
}

static const gk_scene *gk_scenes_find(const gk_scenes *s, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < s->count; i++) {
        if (s->list[i]->name && strcmp(s->list[i]->name, name) == 0) return s->list[i];
    }
    return NULL;
}

/* Leave the current scene, then enter the named one. An unknown name leaves the
   registry untouched rather than blanking the screen. */
static int gk_scenes_go(gk_scenes *s, const char *name, void *ud, const void *data) {
    const gk_scene *next = gk_scenes_find(s, name);
    if (!next) return -1;
    if (s->current && s->current->exit) s->current->exit(ud);
    s->current = next;
    s->current_name = next->name;
    if (next->enter) next->enter(ud, data);
    return 0;
}

static void gk_scenes_update(gk_scenes *s, void *ud, double step) {
    if (s->current && s->current->update) s->current->update(ud, step);
}
static void gk_scenes_render(gk_scenes *s, void *ud, double alpha) {
    if (s->current && s->current->render) s->current->render(ud, alpha);
}
static const char *gk_scenes_current(const gk_scenes *s) { return s->current_name; }

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

static uint32_t gk_crc32(const void *data, size_t n) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return ~crc;
}

static int gk_save(const char *path, const void *data, uint32_t size) {
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
static int gk_load(const char *path, void *data, uint32_t size) {
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

static void gk_world_init(gk_world *w) {
    memset(w, 0, sizeof *w);
    w->next_id = 1; /* 0 is never an entity */
}

static gk_entity *gk_ecs_slot(gk_world *w, uint32_t id) {
    for (int i = 0; i < w->count; i++) if (w->ents[i].id == id) return &w->ents[i];
    return NULL;
}

static uint32_t gk_ecs_create(gk_world *w) {
    if (w->count >= GK_MAX_ENTITIES) return 0;
    gk_entity *e = &w->ents[w->count++];
    memset(e, 0, sizeof *e);
    e->id = w->next_id++;
    return e->id;
}

static int gk_ecs_destroy(gk_world *w, uint32_t id) {
    for (int i = 0; i < w->count; i++) {
        if (w->ents[i].id == id) {
            w->ents[i] = w->ents[--w->count];
            return 0;
        }
    }
    return -1;
}

static int gk_ecs_add(gk_world *w, uint32_t id, int comp, void *data) {
    if (comp < 0 || comp >= GK_MAX_COMPONENTS) return -1;
    gk_entity *e = gk_ecs_slot(w, id);
    if (!e) return -1;
    e->comp[comp] = data;
    e->mask |= (1u << comp);
    return 0;
}

/* The fast path: a system already holds the entity, so there is nothing to look
   up. Every id-based accessor below is for callers that only have an id. */
static void *gk_ecs_comp(gk_entity *e, int comp) {
    if (!e || comp < 0 || comp >= GK_MAX_COMPONENTS) return NULL;
    return (e->mask & (1u << comp)) ? e->comp[comp] : NULL;
}

static int gk_ecs_comp_has(gk_entity *e, int comp) {
    if (!e || comp < 0 || comp >= GK_MAX_COMPONENTS) return 0;
    return (e->mask & (1u << comp)) ? 1 : 0;
}

static void *gk_ecs_get(gk_world *w, uint32_t id, int comp) {
    if (comp < 0 || comp >= GK_MAX_COMPONENTS) return NULL;
    gk_entity *e = gk_ecs_slot(w, id);
    return (e && (e->mask & (1u << comp))) ? e->comp[comp] : NULL;
}

static int gk_ecs_has(gk_world *w, uint32_t id, int comp) {
    if (comp < 0 || comp >= GK_MAX_COMPONENTS) return 0;
    gk_entity *e = gk_ecs_slot(w, id);
    return (e && (e->mask & (1u << comp))) ? 1 : 0;
}

static int gk_ecs_remove(gk_world *w, uint32_t id, int comp) {
    if (comp < 0 || comp >= GK_MAX_COMPONENTS) return -1;
    gk_entity *e = gk_ecs_slot(w, id);
    if (!e) return -1;
    e->mask &= ~(1u << comp);
    e->comp[comp] = NULL;
    return 0;
}

static int gk_ecs_count(const gk_world *w) { return w->count; }

static int gk_ecs_system(gk_world *w, uint32_t mask, void (*fn)(gk_world *, gk_entity *)) {
    if (!fn || w->sys_count >= GK_MAX_SYSTEMS) return -1;
    w->systems[w->sys_count].mask = mask;
    w->systems[w->sys_count].fn = fn;
    w->sys_count++;
    return 0;
}

/* Run every system in the order it was added, over the entities that hold the
   components it names. Returns the number of (system, entity) pairs run. */
static int gk_ecs_run(gk_world *w) {
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
    double clock;
    uint32_t player_id;
    gk_body player_body;
    gk_vel player_vel;
} game;

static game G;

/* ---------- save file ---------- */
typedef struct { int best; } gk_progress;

static const char *gk_save_path(void) { return "gk-progress.bin"; }

static void gk_progress_load(void) {
    gk_progress p;
    if (gk_load(gk_save_path(), &p, sizeof p) == GK_SAVE_OK) G.best = p.best;
}
static void gk_progress_store(void) {
    gk_progress p = { G.best };
    gk_save(gk_save_path(), &p, sizeof p);
}

/* ---------- systems ---------- */
static void sys_move(gk_world *w, gk_entity *e) {
    (void)w;
    gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
    gk_vel *v = (gk_vel *)gk_ecs_comp(e, COMP_VEL);
    if (!b || !v) return;
    b->box.x += v->x * (float)G.loop.step;
    b->box.y += v->y * (float)G.loop.step;
    /* bounce off the field edges rather than leaving it */
    if (b->box.x < 0) { b->box.x = 0; v->x = -v->x; }
    if (b->box.y < 0) { b->box.y = 0; v->y = -v->y; }
    if (b->box.x + b->box.w > 640) { b->box.x = 640 - b->box.w; v->x = -v->x; }
    if (b->box.y + b->box.h > 400) { b->box.y = 400 - b->box.h; v->y = -v->y; }
}

/* ---------- scenes ---------- */
static void title_enter(void *ud, const void *data) { (void)ud; (void)data; }
static void title_update(void *ud, double step) {
    (void)ud; (void)step;
    if (gk_input_was_pressed(&G.input, GK_KEY_START) || gk_input_was_pressed(&G.input, GK_KEY_ACTION)) {
        gk_scenes_go(&G.scenes, "play", &G, NULL);
    }
}

static void play_enter(void *ud, const void *data) {
    (void)ud; (void)data;
    G.score = 0;
    G.clock = 0;
    G.left = 6;
    G.px = 320;
    G.py = 200;
    gk_world_init(&G.world);
    gk_rng_seed(&G.rng, (uint32_t)(G.best * 7919u + 12345u));

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
        b->box = (gk_rect){ (float)gk_rng_int(&G.rng, 20, 600), (float)gk_rng_int(&G.rng, 20, 360), 12, 12 };
        v->x = (float)gk_rng_int(&G.rng, -60, 60);
        v->y = (float)gk_rng_int(&G.rng, -60, 60);
        wr->value = gk_rng_int(&G.rng, 1, 3);
        gk_ecs_add(&G.world, id, COMP_BODY, b);
        gk_ecs_add(&G.world, id, COMP_VEL, v);
        gk_ecs_add(&G.world, id, COMP_WORTH, wr);
    }
    gk_ecs_system(&G.world, (1u << COMP_BODY) | (1u << COMP_VEL), sys_move);
}

static void play_update(void *ud, double step) {
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
    G.px = G.player_body.box.x;
    G.py = G.player_body.box.y;

    gk_ecs_run(&G.world);

    /* collect: the player against every mote */
    for (int i = 0; i < G.world.count; i++) {
        gk_entity *e = &G.world.ents[i];
        if (e->id == G.player_id) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        gk_worth *wr = (gk_worth *)gk_ecs_comp(e, COMP_WORTH);
        if (!b || !wr) continue;
        if (gk_aabb(G.player_body.box, b->box)) {
            G.score += wr->value;
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

static void over_update(void *ud, double step) {
    (void)ud; (void)step;
    if (gk_input_was_pressed(&G.input, GK_KEY_START) || gk_input_was_pressed(&G.input, GK_KEY_ACTION)) {
        gk_scenes_go(&G.scenes, "title", &G, NULL);
    }
}

/* ---------- drawing (the only raylib-specific part of the game) ---------- */
#ifndef GK_NO_MAIN
#include "raylib.h"

static void title_render(void *ud, double alpha) {
    (void)ud; (void)alpha;
    DrawText("raylib-starter", 190, 120, 30, RAYWHITE);
    DrawText("arrows move, space starts", 200, 170, 16, GRAY);
    DrawText(TextFormat("best %d", G.best), 290, 210, 18, GREEN);
}

static void play_render(void *ud, double alpha) {
    (void)ud; (void)alpha;
    DrawRectangleRec((Rectangle){ G.player_body.box.x, G.player_body.box.y,
                                  G.player_body.box.w, G.player_body.box.h }, RAYWHITE);
    for (int i = 0; i < G.world.count; i++) {
        gk_entity *e = &G.world.ents[i];
        if (e->id == G.player_id) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        if (!b) continue;
        DrawRectangleRec((Rectangle){ b->box.x, b->box.y, b->box.w, b->box.h }, GREEN);
    }
    DrawText(TextFormat("score %d   left %d   %0.0fs", G.score, G.left, G.clock), 12, 10, 18, RAYWHITE);
}

static void over_render(void *ud, double alpha) {
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
static void loop_step(void *ud, double step) { gk_scenes_update(&G.scenes, ud, step); }
static void loop_draw(void *ud, double alpha) { gk_scenes_render(&G.scenes, ud, alpha); }

static void read_input(void) {
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
