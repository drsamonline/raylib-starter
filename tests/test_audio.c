/* tests/test_audio.c — the audio wrapper, routed and counted. No device.
 *
 *   cc -std=c11 -DGK_NO_MAIN -DGK_AUDIO_TEST -I. -Istub \
 *      -o gk-audio-tests main.c stub/raylib_stub.c tests/test_audio.c
 *
 * What this can prove: the wrapper calls the right engine function with the
 * right handle, clamps volume, and refuses a missing file. What it cannot
 * prove: that anything is audible.
 */
#define GK_NO_MAIN 1
#define GK_AUDIO_TEST 1

#include <stdio.h>
#include <math.h>

#include "stub/raylib.h"
#include "main.c"

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
    printf("audio tests\n\n");
    gk_audio a;

    OK("init succeeds", gk_audio_init(&a) == 0);
    OK("the device reports ready", IsAudioDeviceReady() == 1);
    OK("a fresh wrapper has nothing loaded", a.loaded == 0);
    OK("its volume is 1", a.volume == 1.0f);

    OK("a null path is refused", gk_audio_load(&a, NULL) == -1);
    OK("an empty path is refused", gk_audio_load(&a, "") == -1);
    OK("playing with nothing loaded is refused", gk_audio_play(&a) == -1);
    OK("stopping with nothing loaded is refused", gk_audio_stop(&a) == -1);
    OK("playing with nothing loaded reports not playing", gk_audio_playing(&a) == 0);

    OK("a real path loads", gk_audio_load(&a, "hit.wav") == 0);
    OKD("and it holds a handle", a.loaded > 0, "id %d", a.loaded);

    Stub_Reset();
    OK("playing succeeds", gk_audio_play(&a) == 0);
    OKD("and reached the engine once", Stub_SoundPlays() == 1, "%d plays", Stub_SoundPlays());
    OK("the engine reports it playing", gk_audio_playing(&a) == 1);

    OK("stopping succeeds", gk_audio_stop(&a) == 0);
    OK("and the engine reports it stopped", gk_audio_playing(&a) == 0);

    /* volume is clamped, not passed through */
    OKD("volume above 1 clamps to 1", gk_audio_volume(&a, 1.5f) == 1.0f, "%.2f", gk_audio_volume(&a, 1.5f));
    OKD("volume below 0 clamps to 0", gk_audio_volume(&a, -2.0f) == 0.0f, "%.2f", gk_audio_volume(&a, -2.0f));
    OKD("volume in range is kept", fabsf(gk_audio_volume(&a, 0.4f) - 0.4f) < 1e-6, "%.2f", a.volume);
    OKD("and reaches the engine", fabsf(Stub_SoundVolume(a.loaded) - 0.4f) < 1e-6, "%.2f", Stub_SoundVolume(a.loaded));

    /* a null wrapper is refused everywhere rather than crashing */
    OK("a null wrapper cannot init", gk_audio_init(NULL) == -1);
    OK("a null wrapper cannot load", gk_audio_load(NULL, "x.wav") == -1);
    OK("a null wrapper cannot play", gk_audio_play(NULL) == -1);
    OK("a null wrapper cannot stop", gk_audio_stop(NULL) == -1);
    OK("a null wrapper reports volume 0", gk_audio_volume(NULL, 1) == 0);
    OK("a null wrapper reports not playing", gk_audio_playing(NULL) == 0);
    gk_audio_close(NULL);
    OK("closing a null wrapper is safe", 1);

    gk_audio_close(&a);
    OK("closing closes the device", IsAudioDeviceReady() == 0);

    printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
