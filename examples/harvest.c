/* examples/harvest.c — a top-down collect-and-avoid game on the whole core.
 *
 * This is the integration test that a suite cannot be: it uses the loop, scenes,
 * the phase machine, the tilemap, collide-and-slide, the camera with deadzone and
 * shake, atlas animation, a tween chain, the ECS with deferred destroys, the RNG
 * for a procedural map, the save file and the audio wrapper — in one game, the way
 * a game actually would.
 *
 *   make harvest              (needs raylib)
 *   make test-harvest         (needs only a C compiler)
 *
 * PolyForm Noncommercial 1.0.0. (c) 2026 Sohil Momin.
 */

/* the core only: main.c's own game and main() are excluded */
#define GK_NO_MAIN 1
#define GK_AUDIO_TEST 1   /* this game wants audio, so the wrapper is compiled in */
#include "../main.c"

#define HV_WORLD_W 1920.0
#define HV_WORLD_H 1280.0
#define HV_TILE 40
#define HV_COLS 48
#define HV_ROWS 32
#define HV_MAX_PICKUPS 10
#define HV_MAX_ENEMIES 4

typedef struct {
    gk_loop loop;
    gk_scenes scenes;
    gk_input input;
    gk_world world;
    gk_rng rng;
    gk_camera cam;
    gk_tilemap map;
    gk_machine phase;
    gk_anim sprite_anim;
    gk_tween_t pulse;
    gk_tween_chain death;
    gk_shake shake;
    gk_audio audio;

    unsigned char tiles[HV_COLS * HV_ROWS];
    int frames[256];

    /* the player is moved by hand through the tilemap, not by a system */
    gk_rect player;
    int hp, score, best, left;
    double clock;
    uint32_t player_id;
} harvest;

static harvest H;

/* ---------- map ---------- */
/* A procedural map: solid border, scattered rock tiles, and a clear middle so the
   player is never boxed in at spawn. Deterministic for a seed. */
static void hv_build_map(harvest *h, uint32_t seed) {
    gk_rng rng;
    gk_rng_seed(&rng, seed ? seed : 1u);
    for (int ty = 0; ty < HV_ROWS; ty++) {
        for (int tx = 0; tx < HV_COLS; tx++) {
            int edge = tx == 0 || ty == 0 || tx == HV_COLS - 1 || ty == HV_ROWS - 1;
            int middle = tx > HV_COLS / 2 - 4 && tx < HV_COLS / 2 + 4 &&
                         ty > HV_ROWS / 2 - 3 && ty < HV_ROWS / 2 + 3;
            int rock = !edge && !middle && gk_rng_next(&rng) < 0.08;
            h->tiles[ty * HV_COLS + tx] = (edge || rock) ? 1 : 0;
        }
    }
    static const unsigned char SOLID[256] = { 0, 1 };
    h->map = (gk_tilemap){ HV_COLS, HV_ROWS, HV_TILE, h->tiles, SOLID, 1 };
}

/* ---------- save ---------- */
static const char *hv_save_path(void) { return "harvest-best.bin"; }

static void hv_load_best(harvest *h) {
    gk_progress p;
    if (gk_load(hv_save_path(), &p, sizeof p) == GK_SAVE_OK) h->best = p.best;
}
static void hv_store_best(harvest *h) {
    gk_progress p = { h->best };
    gk_save(hv_save_path(), &p, sizeof p);
}

/* ---------- reset ---------- */
static void hv_reset(harvest *h, uint32_t seed) {
    h->hp = 3;
    h->score = 0;
    h->left = 0;
    h->clock = 0;
    h->pulse = gk_tween_make(0, 0, 400, gk_ease_named("out"));
    h->death = gk_chain_make();
    gk_chain_add(&h->death, 0, 1, 120, gk_ease_named("exit"));   /* freeze */
    gk_chain_add(&h->death, 1, 0, 240, gk_ease_named("soft"));   /* flash out */
    gk_chain_add(&h->death, 0, 0, 180, gk_ease_named("linear")); /* beat */
    h->shake = gk_shake_make(200, 5, 10);

    hv_build_map(h, seed);

    h->cam = gk_camera_make(800, 480);
    gk_camera_bounds(&h->cam, 0, 0, HV_WORLD_W, HV_WORLD_H);

    gk_rng_seed(&h->rng, seed ? seed : 1u);
    gk_world_init(&h->world);

    /* the player, centred in the clear middle */
    h->player = (gk_rect){ HV_WORLD_W / 2, HV_WORLD_H / 2, 22, 22 };
    h->player_id = gk_ecs_create(&h->world);

    /* pickups: on free tiles */
    for (int i = 0; i < HV_MAX_PICKUPS; i++) {
        int tx, ty, guard = 0;
        do {
            tx = gk_rng_int(&h->rng, 2, HV_COLS - 3);
            ty = gk_rng_int(&h->rng, 2, HV_ROWS - 3);
            guard++;
        } while (gk_tilemap_is_solid(&h->map, tx, ty) && guard < 50);
        if (gk_tilemap_is_solid(&h->map, tx, ty)) continue;
        uint32_t id = gk_ecs_create(&h->world);
        gk_body *b = (gk_body *)calloc(1, sizeof *b);
        gk_worth *wr = (gk_worth *)calloc(1, sizeof *wr);
        b->box = (gk_rect){ (float)(tx * HV_TILE + 12), (float)(ty * HV_TILE + 12), 14, 14 };
        wr->value = gk_rng_int(&h->rng, 1, 3);
        gk_ecs_add(&h->world, id, COMP_BODY, b);
        gk_ecs_add(&h->world, id, COMP_WORTH, wr);
    }
    h->left = 0;
    for (int i = 0; i < h->world.count; i++) {
        if (h->world.ents[i].id != h->player_id) h->left++;
    }

    /* enemies: drifting, with a velocity component so the ECS system moves them */
    for (int i = 0; i < HV_MAX_ENEMIES; i++) {
        uint32_t id = gk_ecs_create(&h->world);
        gk_body *b = (gk_body *)calloc(1, sizeof *b);
        gk_vel *v = (gk_vel *)calloc(1, sizeof *v);
        b->box = (gk_rect){ (float)gk_rng_int(&h->rng, 100, (int)HV_WORLD_W - 100),
                            (float)gk_rng_int(&h->rng, 100, (int)HV_WORLD_H - 100), 18, 18 };
        v->x = (float)gk_rng_int(&h->rng, -70, 70);
        v->y = (float)gk_rng_int(&h->rng, -70, 70);
        gk_ecs_add(&h->world, id, COMP_BODY, b);
        gk_ecs_add(&h->world, id, COMP_VEL, v);
    }
}

/* ---------- enemies move through the tilemap too ---------- */
static void hv_sys_drift(gk_world *w, gk_entity *e) {
    (void)w;
    if (e->id == H.player_id) return;
    gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
    gk_vel *v = (gk_vel *)gk_ecs_comp(e, COMP_VEL);
    if (!b || !v) return;
    gk_rect moved = gk_move(&H.map, b->box, v->x * (1.0 / 60.0), v->y * (1.0 / 60.0));
    /* a wall stopped it: turn around, so an enemy never grinds into a rock */
    if (moved.x == b->box.x && v->x != 0) v->x = -v->x;
    if (moved.y == b->box.y && v->y != 0) v->y = -v->y;
    b->box = moved;
}

/* ---------- the round ---------- */
static void hv_begin(harvest *h) {
    hv_reset(h, (uint32_t)(h->best * 7919u + 7u));
    gk_ecs_system(&h->world, (1u << COMP_BODY) | (1u << COMP_VEL), hv_sys_drift);

    gk_machine_init(&h->phase, h);
    gk_machine_state(&h->phase, "warmup", NULL, NULL);
    gk_machine_state(&h->phase, "live", NULL, NULL);
    gk_machine_state(&h->phase, "finished", NULL, NULL);
    gk_machine_transition(&h->phase, "warmup", "go", "live");
    gk_machine_transition(&h->phase, "live", "end", "finished");
    gk_machine_start(&h->phase, "warmup");
}

static int hv_live(const harvest *h) {
    return gk_str_eq(gk_machine_current(&h->phase), "live");
}

/* One step of play. Returns 1 when the round ended. */
static int hv_step(harvest *h, double step) {
    h->clock += step;
    gk_tween_step(&h->pulse, step * 1000.0);
    gk_shake_step(&h->shake, step * 1000.0);
    gk_chain_step(&h->death, step * 1000.0);

    if (gk_str_eq(gk_machine_current(&h->phase), "warmup") && h->clock >= 1.5) {
        gk_machine_send(&h->phase, "go");
    }
    if (!hv_live(h)) return 0;

    /* the player moves through the tilemap */
    double speed = 190.0;
    double dx = 0, dy = 0;
    if (gk_input_is_down(&h->input, GK_KEY_LEFT)) dx -= speed * step;
    if (gk_input_is_down(&h->input, GK_KEY_RIGHT)) dx += speed * step;
    if (gk_input_is_down(&h->input, GK_KEY_UP)) dy -= speed * step;
    if (gk_input_is_down(&h->input, GK_KEY_DOWN)) dy += speed * step;
    h->player = gk_move(&h->map, h->player, dx, dy);

    gk_ecs_run(&h->world);

    /* pickups */
    for (int i = 0; i < h->world.count; i++) {
        gk_entity *e = &h->world.ents[i];
        if (e->id == h->player_id) continue;
        gk_worth *wr = (gk_worth *)gk_ecs_comp(e, COMP_WORTH);
        if (!wr) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        if (b && gk_aabb(h->player, b->box)) {
            h->score += wr->value;
            h->left--;
            gk_ecs_queue_destroy(&h->world, e->id);
            gk_tween_retarget(&h->pulse, 0);
            h->pulse = gk_tween_make(1, 0, 300, gk_ease_named("out"));
            gk_shake_start(&h->shake);
            gk_audio_play(&h->audio);
        }
    }
    gk_ecs_flush(&h->world);

    /* enemies */
    for (int i = 0; i < h->world.count; i++) {
        gk_entity *e = &h->world.ents[i];
        if (e->id == h->player_id) continue;
        gk_vel *v = (gk_vel *)gk_ecs_comp(e, COMP_VEL);
        if (!v) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        if (b && gk_aabb(h->player, b->box)) {
            h->hp--;
            v->x = -v->x;
            v->y = -v->y;
            gk_shake_start(&h->shake);
        }
    }

    /* the camera: deadzone, then follow, then clamp */
    gk_camera_deadzone(&h->cam, h->player.x + h->player.w / 2, h->player.y + h->player.h / 2, 160, 100);
    gk_camera_follow(&h->cam, h->player.x + h->player.w / 2, h->player.y + h->player.h / 2, step, 0.14);
    gk_camera_clamp(&h->cam);

    if (h->left <= 0 || h->hp <= 0 || h->clock > 45.0) {
        if (gk_machine_send(&h->phase, "end")) {
            if (h->score > h->best) { h->best = h->score; hv_store_best(h); }
            return 1;
        }
    }
    return 0;
}

/* ---------- scenes ---------- */
static void hv_title_update(void *ud, double step) {
    (void)step;
    harvest *h = (harvest *)ud;
    if (gk_input_was_pressed(&h->input, GK_KEY_START) || gk_input_was_pressed(&h->input, GK_KEY_ACTION)) {
        gk_scenes_go(&h->scenes, "play", h, NULL);
    }
}

static void hv_play_enter(void *ud, const void *data) {
    (void)data;
    hv_begin((harvest *)ud);
}

static void hv_play_update(void *ud, double step) {
    harvest *h = (harvest *)ud;
    if (hv_step(h, step)) gk_scenes_go(&h->scenes, "over", h, NULL);
}

static void hv_over_update(void *ud, double step) {
    (void)step;
    harvest *h = (harvest *)ud;
    if (gk_input_was_pressed(&h->input, GK_KEY_START) || gk_input_was_pressed(&h->input, GK_KEY_ACTION)) {
        gk_scenes_go(&h->scenes, "title", h, NULL);
    }
}

/* Everything the game needs before its first frame, in the core: both the real
   build and the headless suite call this, so no callback is dead code. */
static void hv_boot(harvest *h) {
    memset(h, 0, sizeof *h);
    gk_loop_init(&h->loop, 1.0 / 60.0);
    gk_scenes_init(&h->scenes);
    gk_scenes_add(&h->scenes, &(gk_scene){ "title", NULL, NULL, hv_title_update, NULL });
    gk_scenes_add(&h->scenes, &(gk_scene){ "play", hv_play_enter, NULL, hv_play_update, NULL });
    gk_scenes_add(&h->scenes, &(gk_scene){ "over", NULL, NULL, hv_over_update, NULL });
    h->sprite_anim = (gk_anim){ { 14, 14, 2, 4 }, 5.0, 0, 3, 1 };
    for (int i = 0; i < 256; i++) h->frames[i] = -1;
    h->frames[1] = 0;
    hv_load_best(h);
    hv_begin(h);
    gk_audio_init(&h->audio);
    gk_audio_load(&h->audio, "pickup.wav");
    gk_scenes_go(&h->scenes, "title", h, NULL);
}

#ifndef HARVEST_HEADLESS
#include "raylib.h"

static void hv_title_render(void *ud, double alpha) {
    (void)alpha;
    harvest *h = (harvest *)ud;
    DrawText("harvest", 330, 150, 44, RAYWHITE);
    DrawText("arrows move · space starts · collect, don't get hit", 170, 210, 16, GRAY);
    DrawText(TextFormat("best %d", h->best), 360, 250, 20, GREEN);
}

static void hv_play_render(void *ud, double alpha) {
    (void)alpha;
    harvest *h = (harvest *)ud;

    /* the shake is a draw-time offset on a copy of the camera */
    gk_camera drawn = h->cam;
    drawn.x += h->shake.dx;
    drawn.y += h->shake.dy;

    /* the map, one walk, each tile with its atlas frame */
    gk_visible_tile vis[512];
    int n = gk_tiles_visible(&h->map, &drawn, h->frames, vis, 512);
    for (int i = 0; i < n; i++) {
        double tx, ty, tw, th;
        gk_sprite_dst(&drawn, vis[i].x, vis[i].y, vis[i].w, vis[i].h, &tx, &ty, &tw, &th);
        Color c = vis[i].id ? DARKGRAY : (Color){ 26, 30, 38, 255 };
        DrawRectangleRec((Rectangle){ (float)tx, (float)ty, (float)tw, (float)th }, c);
    }

    /* the player, with the collect pulse around it */
    double px, py, pw, ph;
    gk_sprite_dst(&drawn, h->player.x, h->player.y, h->player.w, h->player.h, &px, &py, &pw, &ph);
    DrawRectangleRec((Rectangle){ (float)px, (float)py, (float)pw, (float)ph }, RAYWHITE);
    if (!h->pulse.done) {
        float k = (float)(1.0 - gk_tween_value(&h->pulse));
        float grow = 8.0f * k;
        DrawRectangleLines((int)(px - grow), (int)(py - grow),
                           (int)(pw + grow * 2), (int)(ph + grow * 2), Fade(GREEN, 1.0f - k));
    }

    /* pickups and enemies, the pickups pulsing through an atlas */
    int frame = gk_anim_frame(&h->sprite_anim, h->clock);
    float shrink = (float)(frame % 2) * 2.0f;
    for (int i = 0; i < h->world.count; i++) {
        gk_entity *e = &h->world.ents[i];
        if (e->id == h->player_id) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        if (!b) continue;
        int is_pickup = gk_ecs_comp_has(e, COMP_WORTH);
        double sx, sy, sw, sh;
        gk_sprite_dst(&drawn, b->box.x + (is_pickup ? shrink : 0), b->box.y + (is_pickup ? shrink : 0),
                      b->box.w - (is_pickup ? shrink * 2 : 0), b->box.h - (is_pickup ? shrink * 2 : 0),
                      &sx, &sy, &sw, &sh);
        DrawRectangleRec((Rectangle){ (float)sx, (float)sy, (float)sw, (float)sh },
                         is_pickup ? GREEN : RED);
    }

    DrawText(TextFormat("score %d  left %d  hp %d  %0.0fs", h->score, h->left, h->hp, h->clock),
             12, 10, 18, RAYWHITE);
    if (gk_str_eq(gk_machine_current(&h->phase), "warmup")) {
        DrawText("ready", 370, 210, 26, Fade(RAYWHITE, 0.7f));
    }
}

static void hv_over_render(void *ud, double alpha) {
    (void)alpha;
    harvest *h = (harvest *)ud;
    DrawText("done", 360, 160, 36, RAYWHITE);
    DrawText(TextFormat("score %d   best %d", h->score, h->best), 300, 210, 20, GREEN);
    DrawText("space to play again", 320, 250, 16, GRAY);
}

static void hv_map_keys(harvest *h) {
    struct { int key; gk_key gk; } map[] = {
        { KEY_LEFT, GK_KEY_LEFT }, { KEY_RIGHT, GK_KEY_RIGHT },
        { KEY_UP, GK_KEY_UP }, { KEY_DOWN, GK_KEY_DOWN },
        { KEY_SPACE, GK_KEY_ACTION }, { KEY_ENTER, GK_KEY_START },
    };
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (IsKeyPressed(map[i].key)) gk_input_down(&h->input, map[i].gk);
        else if (IsKeyReleased(map[i].key)) gk_input_up(&h->input, map[i].gk);
    }
}

static void hv_loop_step(void *ud, double step) { gk_scenes_update(&H.scenes, ud, step); }
static void hv_loop_draw(void *ud, double alpha) { gk_scenes_render(&H.scenes, ud, alpha); }

int main(void) {
    InitWindow(800, 480, "harvest");
    SetTargetFPS(60);

    hv_boot(&H);
    /* the real build draws, so it swaps in the render callbacks */
    gk_scenes_set_render(&H.scenes, "title", hv_title_render);
    gk_scenes_set_render(&H.scenes, "play", hv_play_render);
    gk_scenes_set_render(&H.scenes, "over", hv_over_render);

    while (!WindowShouldClose()) {
        hv_map_keys(&H);
        BeginDrawing();
        ClearBackground((Color){ 18, 20, 26, 255 });
        gk_loop_tick(&H.loop, GetFrameTime(), hv_loop_step, hv_loop_draw, &H);
        gk_input_end_frame(&H.input);
        EndDrawing();
    }

    gk_audio_close(&H.audio);
    CloseWindow();
    return 0;
}
#endif /* HARVEST_HEADLESS */
