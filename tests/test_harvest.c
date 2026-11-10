/* tests/test_harvest.c — the second game, tested headless.
 *
 *   cc -std=c11 -O2 -Wall -Wextra -Werror -DHARVEST_HEADLESS -I. -Istub \
 *      -o harvest-tests examples/harvest.c stub/raylib_stub.c tests/test_harvest.c -lm
 *
 * This is the integration check the suites cannot be: one game using the loop,
 * scenes, the phase machine, the tilemap, collide-and-slide, the camera, atlas
 * animation, a tween chain, the ECS with deferred destroys, the RNG, the save file
 * and the audio wrapper - together.
 */
#include <stdio.h>
#include <math.h>

#include "../examples/harvest.c"

static int pass = 0, fail = 0;
static void ok(const char *name, int cond, const char *detail) {
    if (cond) pass++; else fail++;
    printf("%s  %s", cond ? "PASS" : "FAIL", name);
    if (detail) printf("  (%s)", detail);
    printf("\n");
}
#define OK(n, c) ok(n, (c), NULL)
static char d[160];
#define OKD(n, c, fmt, ...) do { snprintf(d, sizeof d, fmt, __VA_ARGS__); ok(n, (c), d); } while (0)

static void hold_key(gk_input *in, gk_key k, int on) {
    if (on) gk_input_down(in, k); else gk_input_up(in, k);
}

int main(void) {
    printf("harvest tests\n\n");

    /* ---------- boot ---------- */
    hv_boot(&H);
    OK("boot leaves the title scene current", gk_str_eq(H.scenes.current_name, "title"));
    OK("the round starts in warmup", gk_str_eq(gk_machine_current(&H.phase), "warmup"));
    OK("the player has three lives", H.hp == 3);
    OKD("there are pickups to collect", H.left > 0, "%d pickups", H.left);
    OK("the camera is bounded to the world", H.cam.bw == HV_WORLD_W && H.cam.bh == HV_WORLD_H);
    OK("the audio wrapper loaded its sound", H.audio.loaded == 1);

    /* ---------- the map is procedural and deterministic ---------- */
    int solid_first = 0, solid_again = 0;
    hv_build_map(&H, 1234u);
    for (int i = 0; i < HV_COLS * HV_ROWS; i++) solid_first += H.tiles[i] ? 1 : 0;
    hv_build_map(&H, 1234u);
    for (int i = 0; i < HV_COLS * HV_ROWS; i++) solid_again += H.tiles[i] ? 1 : 0;
    OKD("the same seed builds the same map", solid_first == solid_again, "%d vs %d", solid_first, solid_again);
    OKD("the map is mostly open", solid_first > 0 && solid_first < HV_COLS * HV_ROWS / 2, "%d solid tiles", solid_first);

    hv_build_map(&H, 999u);
    int solid_other = 0;
    for (int i = 0; i < HV_COLS * HV_ROWS; i++) solid_other += H.tiles[i] ? 1 : 0;
    OKD("a different seed builds a different map", solid_other != solid_first, "%d vs %d", solid_other, solid_first);

    /* the border is always solid */
    OK("the border is solid", gk_tilemap_is_solid(&H.map, 0, 0) && gk_tilemap_is_solid(&H.map, HV_COLS - 1, HV_ROWS - 1));
    OK("the middle is clear", gk_tilemap_is_solid(&H.map, HV_COLS / 2, HV_ROWS / 2) == 0);

    /* ---------- warmup then live ---------- */
    hv_boot(&H);
    gk_scenes_go(&H.scenes, "play", &H, NULL);
    OK("starting play enters warmup", gk_str_eq(gk_machine_current(&H.phase), "warmup"));
    hv_step(&H, 1.0);
    OK("one second in, still warmup", gk_str_eq(gk_machine_current(&H.phase), "warmup"));
    hv_step(&H, 0.6);
    OK("past 1.5s the round is live", gk_str_eq(gk_machine_current(&H.phase), "live"));

    /* ---------- the player collides with the world ---------- */
    gk_rect before = H.player;
    hold_key(&H.input, GK_KEY_LEFT, 1);
    for (int i = 0; i < 200; i++) hv_step(&H, 1.0 / 60.0);
    hold_key(&H.input, GK_KEY_LEFT, 0);
    OKD("walking left moves the player", H.player.x < before.x, "%.1f -> %.1f", before.x, H.player.x);
    OKD("but not out of the world", H.player.x >= 0, "x %.1f", H.player.x);

    /* the camera followed, and stayed inside its bounds */
    OKD("the camera moved with the player", H.cam.x < HV_WORLD_W / 2, "cam %.1f", H.cam.x);
    OK("the camera stayed in bounds", H.cam.x >= H.cam.vw / 2 - 0.01);

    /* ---------- collecting ---------- */
    hv_boot(&H);
    gk_scenes_go(&H.scenes, "play", &H, NULL);
    hv_step(&H, 2.0);   /* into live */
    /* walk the player onto the first pickup */
    int collected = 0;
    for (int i = 0; i < H.world.count && !collected; i++) {
        gk_entity *e = &H.world.ents[i];
        if (!gk_ecs_comp_has(e, COMP_WORTH)) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        H.player.x = b->box.x;
        H.player.y = b->box.y;
        int left_before = H.left, score_before = H.score;
        Stub_Reset();
        hv_step(&H, 1.0 / 60.0);
        OKD("a pickup is collected on contact", H.left == left_before - 1, "left %d -> %d", left_before, H.left);
        OKD("it adds its worth to the score", H.score > score_before, "score %d", H.score);
        OKD("it plays a sound", Stub_SoundPlays() >= 1, "%d plays", Stub_SoundPlays());
        OK("it starts the collect pulse", !H.pulse.done);
        OK("it starts the shake", gk_shake_running(&H.shake) == 1);
        collected = 1;
    }
    OK("there was a pickup to collect", collected == 1);

    /* ---------- damage ---------- */
    hv_boot(&H);
    gk_scenes_go(&H.scenes, "play", &H, NULL);
    hv_step(&H, 2.0);
    int hit = 0;
    for (int i = 0; i < H.world.count && !hit; i++) {
        gk_entity *e = &H.world.ents[i];
        if (!gk_ecs_comp_has(e, COMP_VEL)) continue;
        gk_body *b = (gk_body *)gk_ecs_comp(e, COMP_BODY);
        H.player.x = b->box.x;
        H.player.y = b->box.y;
        int hp_before = H.hp;
        hv_step(&H, 1.0 / 60.0);
        OKD("an enemy costs a life", H.hp == hp_before - 1, "hp %d -> %d", hp_before, H.hp);
        hit = 1;
    }
    OK("there was an enemy to hit", hit == 1);

    /* ---------- the round ends ---------- */
    hv_boot(&H);
    gk_scenes_go(&H.scenes, "play", &H, NULL);
    hv_step(&H, 2.0);
    H.hp = 0;
    int ended = hv_step(&H, 1.0 / 60.0);
    OK("losing the last life ends the round", ended == 1);
    OK("the phase is finished", gk_str_eq(gk_machine_current(&H.phase), "finished"));
    OK("a finished round cannot be ended again", gk_machine_send(&H.phase, "end") == 0);

    /* the clock also ends it */
    hv_boot(&H);
    gk_scenes_go(&H.scenes, "play", &H, NULL);
    hv_step(&H, 2.0);
    H.clock = 46.0;
    OK("the clock ends the round", hv_step(&H, 1.0 / 60.0) == 1);

    /* ---------- the death chain runs ---------- */
    hv_boot(&H);
    OK("the death chain is three steps", H.death.count == 3);
    OK("it starts at 0", gk_chain_value(&H.death) == 0);
    OK("it is not done", gk_chain_done(&H.death) == 0);
    gk_chain_step(&H.death, 120 + 240 + 180 + 50);
    OK("it finishes", gk_chain_done(&H.death) == 1);

    /* ---------- the save file round-trips ---------- */
    remove(hv_save_path());
    H.best = 0;
    hv_load_best(&H);
    OK("no save file leaves the best at 0", H.best == 0);
    H.best = 42;
    hv_store_best(&H);
    H.best = 0;
    hv_load_best(&H);
    OKD("the best score round-trips", H.best == 42, "best %d", H.best);
    remove(hv_save_path());

    /* ---------- scenes ---------- */
    hv_boot(&H);
    OK("title is current", gk_str_eq(H.scenes.current_name, "title"));
    gk_scenes_go(&H.scenes, "play", &H, NULL);
    OK("play is current", gk_str_eq(H.scenes.current_name, "play"));
    gk_scenes_go(&H.scenes, "over", &H, NULL);
    OK("over is current", gk_str_eq(H.scenes.current_name, "over"));
    gk_scenes_go(&H.scenes, "title", &H, NULL);
    OK("and back to title", gk_str_eq(H.scenes.current_name, "title"));
    OK("an unknown scene is refused", gk_scenes_go(&H.scenes, "nope", &H, NULL) != 0);

    /* ---------- the renderer can be attached later ---------- */
    OK("a renderer can be attached after registration",
       gk_scenes_set_render(&H.scenes, "play", NULL) == 0);
    OK("an unknown scene takes no renderer",
       gk_scenes_set_render(&H.scenes, "nope", NULL) == -1);

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
