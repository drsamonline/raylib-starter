/* tests/test_snake.c — the second game, tested the same way: no engine, no window.
 *
 *   cc -std=c11 -DGK_NO_MAIN -I. -Istub -o snake-tests examples/snake.c stub/raylib_stub.c tests/test_snake.c
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "examples/snake.c"

static int pass = 0, fail = 0;
static void ok(const char *name, int cond, const char *detail) {
    if (cond) pass++; else fail++;
    printf("%s  %s", cond ? "PASS" : "FAIL", name);
    if (detail) printf("  (%s)", detail);
    printf("\n");
}
#define OK(n, c) ok(n, (c), NULL)
static char d[128];
#define OKD(n, c, fmt, ...) do { snprintf(d, sizeof d, fmt, __VA_ARGS__); ok(n, (c), d); } while (0)

int main(void) {
    printf("snake tests\n\n");

    /* ---------- start state ---------- */
    snake_reset(&S, 1);
    OK("starts with three segments", S.len == 3);
    OK("starts alive", S.alive == 1);
    OK("starts heading right", S.dir == SNAKE_RIGHT);
    OK("starts at zero", S.score == 0);
    OKD("the head is centred", S.x[0] == SNAKE_COLS / 2 && S.y[0] == SNAKE_ROWS / 2, "%d,%d", S.x[0], S.y[0]);
    OK("the body trails behind", S.x[1] == S.x[0] - 1 && S.x[2] == S.x[0] - 2);

    /* ---------- food ---------- */
    OK("food is on the board", S.food_x >= 0 && S.food_x < SNAKE_COLS && S.food_y >= 0 && S.food_y < SNAKE_ROWS);
    OK("food is not on the snake", snake_occupies(&S, S.food_x, S.food_y) == 0);

    /* A crowded board still never puts food on the snake. The linear-scan
       fallback cannot be reached at this capacity: SNAKE_MAX (64) is smaller
       than the board (240 cells), so the retry loop always finds a free cell
       before the scan is needed. The scan stays as the guard for a future
       SNAKE_MAX that fills the board. */
    {
        snake_state t;
        memset(&t, 0, sizeof t);
        gk_rng_seed(&t.rng, 5);
        t.len = SNAKE_MAX;
        for (int i = 0; i < SNAKE_MAX; i++) { t.x[i] = i % SNAKE_COLS; t.y[i] = i / SNAKE_COLS; }
        int on_snake = 0;
        for (int i = 0; i < 500; i++) {
            snake_place_food(&t);
            if (snake_occupies(&t, t.food_x, t.food_y)) on_snake++;
        }
        OKD("food never lands on a crowded snake", on_snake == 0, "%d of 500", on_snake);
    }

    /* ---------- turning ---------- */
    snake_reset(&S, 1);
    OK("a turn up is allowed", snake_turn(&S, SNAKE_UP) == 0 && S.dir == SNAKE_UP);
    OK("reversing down is refused", snake_turn(&S, SNAKE_DOWN) == -1 && S.dir == SNAKE_UP);
    OK("a turn left is allowed", snake_turn(&S, SNAKE_LEFT) == 0);
    OK("reversing right is refused", snake_turn(&S, SNAKE_RIGHT) == -1);

    /* ---------- movement ---------- */
    snake_reset(&S, 1);
    {
        int hx = S.x[0], hy = S.y[0];
        OK("a step does not kill on open ground", snake_move(&S) == 0);
        OKD("the head moved one cell right", S.x[0] == hx + 1 && S.y[0] == hy, "%d,%d", S.x[0], S.y[0]);
        OK("the body follows", S.x[1] == hx);
        OK("the length is unchanged", S.len == 3);
    }

    /* the ticker only moves on its interval */
    snake_reset(&S, 1);
    OK("a short tick does not move", snake_tick(&S, 0.05) == 0);
    OKD("a full interval moves once", snake_tick(&S, 0.10) == 1, "steps %d", 1);
    OKD("a long tick moves several times", snake_tick(&S, 0.36) == 3, "steps %d", 3);

    /* ---------- eating ---------- */
    snake_reset(&S, 1);
    S.food_x = S.x[0] + 1;
    S.food_y = S.y[0];
    {
        int before = S.score, len = S.len;
        snake_move(&S);
        OK("eating scores", S.score == before + 1);
        OK("eating grows the snake", S.len == len + 1);
        OK("and the best follows the score", S.best >= S.score);
        OK("food moves after being eaten", !(S.food_x == S.x[0] && S.food_y == S.y[0]));
    }

    /* ---------- dying ---------- */
    snake_reset(&S, 1);
    {
        /* run into the right wall */
        for (int i = 0; i < SNAKE_COLS + 2 && S.alive; i++) snake_move(&S);
        OK("the right wall kills", S.alive == 0);
    }
    snake_reset(&S, 1);
    {
        snake_turn(&S, SNAKE_UP);
        for (int i = 0; i < SNAKE_ROWS + 2 && S.alive; i++) snake_move(&S);
        OK("the top wall kills", S.alive == 0);
    }
    snake_reset(&S, 1);
    {
        /* a long body laid out to the left, then a three-turn spiral back into
           itself: right, down, left, up lands on its own fourth segment */
        S.len = 6;
        for (int i = 0; i < 6; i++) { S.x[i] = 5 - i; S.y[i] = 5; }
        S.dir = SNAKE_RIGHT;
        S.food_x = -1; S.food_y = -1;
        snake_move(&S);                    /* head 6,5 */
        snake_turn(&S, SNAKE_DOWN);  snake_move(&S);   /* 6,6 */
        snake_turn(&S, SNAKE_LEFT);  snake_move(&S);   /* 5,6 */
        snake_turn(&S, SNAKE_UP);    snake_move(&S);   /* 5,5 - its own body */
        OKD("biting itself kills", S.alive == 0, "alive=%d", S.alive);
    }
    {
        /* following the tail into the cell it is leaving is not a bite */
        snake_reset(&S, 1);
        S.len = 4;
        S.x[0] = 5; S.y[0] = 5;
        S.x[1] = 5; S.y[1] = 6;
        S.x[2] = 4; S.y[2] = 6;
        S.x[3] = 4; S.y[3] = 5;  /* the tail sits where the head is about to go */
        S.food_x = -1; S.food_y = -1;
        S.dir = SNAKE_LEFT;
        snake_turn(&S, SNAKE_UP);
        S.dir = SNAKE_LEFT;
        snake_move(&S);
        OK("moving into the vacated tail cell is legal", S.alive == 1);
    }

    /* ---------- the scenes and the save ---------- */
    memset(&S, 0, sizeof S);
    gk_loop_init(&S.loop, 1.0 / 60.0);
    gk_scenes_init(&S.scenes);
    gk_scenes_add(&S.scenes, &(gk_scene){ "title", NULL, NULL, snake_title_update, NULL });
    gk_scenes_add(&S.scenes, &(gk_scene){ "play", snake_play_enter, NULL, snake_play_update, NULL });
    gk_scenes_add(&S.scenes, &(gk_scene){ "over", NULL, NULL, snake_over_update, NULL });
    S.best = 4;
    remove(snake_save_path());
    gk_scenes_go(&S.scenes, "title", &S, NULL);
    OK("the title scene is current", strcmp(gk_scenes_current(&S.scenes), "title") == 0);

    gk_input_down(&S.input, GK_KEY_START);
    gk_scenes_update(&S.scenes, &S, 1.0 / 60.0);
    gk_input_end_frame(&S.input);
    OK("start goes to play", strcmp(gk_scenes_current(&S.scenes), "play") == 0);
    OK("play resets the snake", S.len == 3 && S.score == 0 && S.alive == 1);
    OK("the best survives the reset", S.best == 4);

    /* die, and the best is written to disk */
    for (int i = 0; i < 400 && S.alive; i++) gk_scenes_update(&S.scenes, &S, 1.0 / 60.0);
    OK("the round ends when the snake dies", strcmp(gk_scenes_current(&S.scenes), "over") == 0);
    {
        gk_progress p = { -1 };
        OK("the best was saved", gk_load(snake_save_path(), &p, sizeof p) == GK_SAVE_OK);
        OKD("and holds the run's score", p.best == S.best, "saved %d, best %d", p.best, S.best);
    }
    remove(snake_save_path());

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
