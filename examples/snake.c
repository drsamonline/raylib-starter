/* examples/snake.c — a second game on the same core.
 *
 * Proof that the starter is a starter: this file includes main.c for the core
 * only (-DGK_NO_MAIN), and brings its own game, its own scenes and its own
 * save. Nothing from the demo game is reused except the machinery.
 *
 *   cc -std=c11 -O2 -DSNAKE_HEADLESS -I. -Istub -o snake-tests examples/snake.c ...
 *   make snake      (raylib)
 *
 * PolyForm Noncommercial 1.0.0. (c) 2026 Dr Sohil Momin.
 */
/* The core only: main.c's own game and main() are excluded, always, because this
   file brings its own. SNAKE_HEADLESS is what hides this game's raylib half. */
#define GK_NO_MAIN 1
#include "../main.c"

#define SNAKE_COLS 20
#define SNAKE_ROWS 12
#define SNAKE_CELL 24
#define SNAKE_MAX 64

typedef enum { SNAKE_UP, SNAKE_RIGHT, SNAKE_DOWN, SNAKE_LEFT } snake_dir;

typedef struct {
    int x[SNAKE_MAX], y[SNAKE_MAX]; /* body[0] is the head */
    int len;
    snake_dir dir;
    int food_x, food_y;
    int score, best;
    double timer, step_every;
    int alive;
    gk_rng rng;
    gk_loop loop;
    gk_scenes scenes;
    gk_input input;
} snake_state;

GK_API static snake_state S;   /* the game state; the headless build has no main() */

/* ---------- rules ---------- */
GK_API static int snake_occupies(const snake_state *s, int x, int y) {
    for (int i = 0; i < s->len; i++) if (s->x[i] == x && s->y[i] == y) return 1;
    return 0;
}

/* Food goes on a free cell. Bounded retries, then a linear scan, so a nearly
   full board still finds the last free cell instead of looping forever. */
GK_API static void snake_place_food(snake_state *s) {
    for (int attempt = 0; attempt < 64; attempt++) {
        int x = gk_rng_int(&s->rng, 0, SNAKE_COLS - 1);
        int y = gk_rng_int(&s->rng, 0, SNAKE_ROWS - 1);
        if (!snake_occupies(s, x, y)) { s->food_x = x; s->food_y = y; return; }
    }
    for (int y = 0; y < SNAKE_ROWS; y++) {
        for (int x = 0; x < SNAKE_COLS; x++) {
            if (!snake_occupies(s, x, y)) { s->food_x = x; s->food_y = y; return; }
        }
    }
    s->food_x = -1; /* the board is full: nothing left to eat */
    s->food_y = -1;
}

GK_API static void snake_reset(snake_state *s, uint32_t seed) {
    s->len = 3;
    s->x[0] = SNAKE_COLS / 2;     s->y[0] = SNAKE_ROWS / 2;
    s->x[1] = SNAKE_COLS / 2 - 1; s->y[1] = SNAKE_ROWS / 2;
    s->x[2] = SNAKE_COLS / 2 - 2; s->y[2] = SNAKE_ROWS / 2;
    s->dir = SNAKE_RIGHT;
    s->score = 0;
    s->timer = 0;
    s->step_every = 0.12;
    s->alive = 1;
    gk_rng_seed(&s->rng, seed ? seed : 1u);
    snake_place_food(s);
}

/* A turn into the opposite direction is ignored: the snake cannot reverse. */
GK_API static int snake_turn(snake_state *s, snake_dir d) {
    if ((d + 2) % 4 == (int)s->dir) return -1;
    s->dir = d;
    return 0;
}

/* One step of the snake. Returns 1 when the step killed it. */
GK_API static int snake_move(snake_state *s) {
    int hx = s->x[0], hy = s->y[0];
    switch (s->dir) {
        case SNAKE_UP:    hy--; break;
        case SNAKE_RIGHT: hx++; break;
        case SNAKE_DOWN:  hy++; break;
        case SNAKE_LEFT:  hx--; break;
    }

    /* walls */
    if (hx < 0 || hy < 0 || hx >= SNAKE_COLS || hy >= SNAKE_ROWS) { s->alive = 0; return 1; }

    int ate = (hx == s->food_x && hy == s->food_y);

    /* the tail moves out of the way on the same step, so following it is safe */
    int tail_x = s->x[s->len - 1], tail_y = s->y[s->len - 1];
    for (int i = s->len - 1; i > 0; i--) { s->x[i] = s->x[i - 1]; s->y[i] = s->y[i - 1]; }

    int body = snake_occupies(s, hx, hy);
    if (body && !(hx == tail_x && hy == tail_y && !ate)) { s->alive = 0; return 1; }

    s->x[0] = hx;
    s->y[0] = hy;

    if (ate) {
        if (s->len < SNAKE_MAX) {
            s->x[s->len] = tail_x;
            s->y[s->len] = tail_y;
            s->len++;
        }
        s->score++;
        if (s->score > s->best) s->best = s->score;
        snake_place_food(s);
    }
    return 0;
}

/* Advance by dt, moving one cell every step_every. Returns the steps taken. */
GK_API static int snake_tick(snake_state *s, double dt) {
    if (!s->alive) return 0;
    s->timer += dt;
    int steps = 0;
    while (s->timer + 1e-9 >= s->step_every && s->alive) {
        s->timer -= s->step_every;
        steps++;
        snake_move(s);
    }
    return steps;
}

/* ---------- save ---------- */
GK_API static const char *snake_save_path(void) { return "snake-best.bin"; }

GK_API static void snake_progress_load(snake_state *s) {
    gk_progress p;
    if (gk_load(snake_save_path(), &p, sizeof p) == GK_SAVE_OK) s->best = p.best;
}
GK_API static void snake_progress_store(snake_state *s) {
    gk_progress p = { s->best };
    gk_save(snake_save_path(), &p, sizeof p);
}

/* ---------- scenes ---------- */
GK_API static void snake_read_input(snake_state *s) {
    if (gk_input_was_pressed(&s->input, GK_KEY_UP)) snake_turn(s, SNAKE_UP);
    if (gk_input_was_pressed(&s->input, GK_KEY_RIGHT)) snake_turn(s, SNAKE_RIGHT);
    if (gk_input_was_pressed(&s->input, GK_KEY_DOWN)) snake_turn(s, SNAKE_DOWN);
    if (gk_input_was_pressed(&s->input, GK_KEY_LEFT)) snake_turn(s, SNAKE_LEFT);
}

GK_API static void snake_title_update(void *ud, double step) {
    (void)step;
    snake_state *s = (snake_state *)ud;
    if (gk_input_was_pressed(&s->input, GK_KEY_START) || gk_input_was_pressed(&s->input, GK_KEY_ACTION)) {
        gk_scenes_go(&s->scenes, "play", s, NULL);
    }
}

GK_API static void snake_play_enter(void *ud, const void *data) {
    (void)data;
    snake_state *s = (snake_state *)ud;
    snake_reset(s, (uint32_t)(s->best * 7919u + 1u));
}

GK_API static void snake_play_update(void *ud, double step) {
    snake_state *s = (snake_state *)ud;
    snake_read_input(s);
    snake_tick(s, step);
    if (!s->alive) {
        snake_progress_store(s);
        gk_scenes_go(&s->scenes, "over", s, NULL);
    }
}

GK_API static void snake_over_update(void *ud, double step) {
    (void)step;
    snake_state *s = (snake_state *)ud;
    if (gk_input_was_pressed(&s->input, GK_KEY_START) || gk_input_was_pressed(&s->input, GK_KEY_ACTION)) {
        gk_scenes_go(&s->scenes, "title", s, NULL);
    }
}

#ifndef SNAKE_HEADLESS
#include "raylib.h"

GK_API static void snake_title_render(void *ud, double alpha) {
    (void)alpha;
    snake_state *s = (snake_state *)ud;
    DrawText("snake", 200, 110, 40, RAYWHITE);
    DrawText("arrows turn, space starts", 150, 165, 16, GRAY);
    DrawText(TextFormat("best %d", s->best), 205, 200, 18, GREEN);
}

GK_API static void snake_play_render(void *ud, double alpha) {
    (void)alpha;
    snake_state *s = (snake_state *)ud;
    for (int i = 0; i < s->len; i++) {
        Color c = i == 0 ? RAYWHITE : GREEN;
        DrawRectangle(s->x[i] * SNAKE_CELL, s->y[i] * SNAKE_CELL, SNAKE_CELL - 2, SNAKE_CELL - 2, c);
    }
    if (s->food_x >= 0) {
        DrawRectangle(s->food_x * SNAKE_CELL + 4, s->food_y * SNAKE_CELL + 4,
                      SNAKE_CELL - 10, SNAKE_CELL - 10, RED);
    }
    DrawText(TextFormat("score %d", s->score), 8, 6, 16, RAYWHITE);
}

GK_API static void snake_over_render(void *ud, double alpha) {
    (void)alpha;
    snake_state *s = (snake_state *)ud;
    DrawText("over", 200, 120, 34, RAYWHITE);
    DrawText(TextFormat("score %d   best %d", s->score, s->best), 150, 170, 18, GREEN);
    DrawText("space to try again", 170, 210, 16, GRAY);
}

GK_API static void snake_map_keys(snake_state *s) {
    struct { int key; gk_key gk; } map[] = {
        { KEY_UP, GK_KEY_UP }, { KEY_DOWN, GK_KEY_DOWN },
        { KEY_LEFT, GK_KEY_LEFT }, { KEY_RIGHT, GK_KEY_RIGHT },
        { KEY_SPACE, GK_KEY_ACTION }, { KEY_ENTER, GK_KEY_START },
    };
    for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (IsKeyPressed(map[i].key)) gk_input_down(&s->input, map[i].gk);
        else if (IsKeyReleased(map[i].key)) gk_input_up(&s->input, map[i].gk);
    }
}

GK_API static void snake_loop_step(void *ud, double step) { gk_scenes_update(&S.scenes, ud, step); }
GK_API static void snake_loop_draw(void *ud, double alpha) { gk_scenes_render(&S.scenes, ud, alpha); }

int main(void) {
    InitWindow(SNAKE_COLS * SNAKE_CELL, SNAKE_ROWS * SNAKE_CELL, "snake");
    SetTargetFPS(60);

    memset(&S, 0, sizeof S);
    gk_loop_init(&S.loop, 1.0 / 60.0);
    gk_scenes_init(&S.scenes);
    gk_scenes_add(&S.scenes, &(gk_scene){ "title", NULL, NULL, snake_title_update, snake_title_render });
    gk_scenes_add(&S.scenes, &(gk_scene){ "play", snake_play_enter, NULL, snake_play_update, snake_play_render });
    gk_scenes_add(&S.scenes, &(gk_scene){ "over", NULL, NULL, snake_over_update, snake_over_render });
    snake_progress_load(&S);
    snake_reset(&S, 1u);
    gk_scenes_go(&S.scenes, "title", &S, NULL);

    while (!WindowShouldClose()) {
        snake_map_keys(&S);
        BeginDrawing();
        ClearBackground((Color){ 16, 18, 24, 255 });
        gk_loop_tick(&S.loop, GetFrameTime(), snake_loop_step, snake_loop_draw, &S);
        gk_input_end_frame(&S.input);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
#endif /* SNAKE_HEADLESS */
