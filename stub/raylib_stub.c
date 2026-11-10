/* stub/raylib_stub.c — the headless raylib.
 *
 * A window that never opens: the clock is set by the test, the keyboard is set
 * by the test, and drawing is counted instead of drawn. That is enough to run
 * the whole game loop, the scene machine, the ECS and the save file in CI on a
 * machine with no display.
 */
#include "raylib.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double g_frame_time = 1.0 / 60.0;
static int g_draw_calls = 0;
static int g_should_close = 0;
static int g_key_down[512];
static int g_key_pressed[512];
static int g_key_released[512];
static int g_width = 640, g_height = 400;
static int g_frame_budget = 0;   /* 0 = run until something closes the window */
static int g_frames = 0;

/* Read once, on the first frame: GK_FRAMES=n makes a headless run finite. */
static void stub_read_budget(void) {
    static int read_once = 0;
    if (read_once) return;
    read_once = 1;
    const char *env = getenv("GK_FRAMES");
    if (env && *env) g_frame_budget = atoi(env);
}

void Stub_SetFrameBudget(int frames) { g_frame_budget = frames; }
int  Stub_Frames(void) { return g_frames; }

void Stub_SetFrameTime(double dt) { g_frame_time = dt; }
void Stub_KeyDown(int key) {
    if (key < 0 || key >= 512) return;
    g_key_down[key] = 1;
    g_key_pressed[key] = 1;
}
void Stub_KeyUp(int key) {
    if (key < 0 || key >= 512) return;
    g_key_down[key] = 0;
    g_key_released[key] = 1;
}
int Stub_DrawCalls(void) { return g_draw_calls; }
void Stub_CloseWindow(void) { g_should_close = 1; }

void Stub_Reset(void) {
    g_frame_time = 1.0 / 60.0;
    g_draw_calls = 0;
    g_should_close = 0;
    g_width = 640;
    g_height = 400;
    memset(g_key_down, 0, sizeof g_key_down);
    memset(g_key_pressed, 0, sizeof g_key_pressed);
    memset(g_key_released, 0, sizeof g_key_released);
}

void SetConfigFlags(unsigned int flags) { (void)flags; }
void InitWindow(int width, int height, const char *title) {
    (void)title;
    g_width = width;
    g_height = height;
    stub_read_budget();
}
void CloseWindow(void) {
    g_should_close = 1;
    /* a one-line summary the link check can assert on */
    printf("frames=%d draws=%d\n", g_frames, g_draw_calls);
    fflush(stdout);
}
bool WindowShouldClose(void) { return g_should_close != 0; }
void SetTargetFPS(int fps) { (void)fps; }

void BeginDrawing(void) {}
/* GK_AUTOPLAY makes a headless run press keys on a schedule, so the link check
   reaches the play scene instead of sitting on the title. Test scaffolding, not
   part of the game: the game reads input exactly as it would from a keyboard. */
static void stub_autoplay(void) {
    static int on = -1;
    if (on < 0) { const char *e = getenv("GK_AUTOPLAY"); on = (e && *e) ? 1 : 0; }
    if (!on) return;
    if (g_frames == 4) Stub_KeyDown(KEY_SPACE);          /* start */
    if (g_frames == 5) Stub_KeyUp(KEY_SPACE);
    if (g_frames == 12) Stub_KeyDown(KEY_RIGHT);         /* move, so the camera follows */
    if (g_frames == 40) { Stub_KeyUp(KEY_RIGHT); Stub_KeyDown(KEY_DOWN); }
    if (g_frames == 70) { Stub_KeyUp(KEY_DOWN); Stub_KeyDown(KEY_LEFT); }
    if (g_frames == 100) Stub_KeyUp(KEY_LEFT);
}

void EndDrawing(void) {
    /* a frame is over: "pressed" and "released" last exactly one frame */
    memset(g_key_pressed, 0, sizeof g_key_pressed);
    memset(g_key_released, 0, sizeof g_key_released);
    g_frames++;
    stub_autoplay();
    if (g_frame_budget > 0 && g_frames >= g_frame_budget) g_should_close = 1;
}
void ClearBackground(Color color) { (void)color; g_draw_calls++; }
void DrawRectangle(int x, int y, int w, int h, Color color) {
    (void)x; (void)y; (void)w; (void)h; (void)color; g_draw_calls++;
}
void DrawRectangleRec(Rectangle rec, Color color) {
    (void)rec; (void)color; g_draw_calls++;
}
void DrawRectangleLines(int x, int y, int w, int h, Color color) {
    (void)x; (void)y; (void)w; (void)h; (void)color; g_draw_calls++;
}
void DrawCircle(int cx, int cy, float radius, Color color) {
    (void)cx; (void)cy; (void)radius; (void)color; g_draw_calls++;
}
void DrawText(const char *text, int x, int y, int fontSize, Color color) {
    (void)text; (void)x; (void)y; (void)fontSize; (void)color; g_draw_calls++;
}

float GetFrameTime(void) { return (float)g_frame_time; }
int GetScreenWidth(void) { return g_width; }
int GetScreenHeight(void) { return g_height; }

bool IsKeyDown(int key) { return key >= 0 && key < 512 && g_key_down[key]; }
bool IsKeyPressed(int key) { return key >= 0 && key < 512 && g_key_pressed[key]; }
bool IsKeyReleased(int key) { return key >= 0 && key < 512 && g_key_released[key]; }

Color Fade(Color color, float alpha) {
    color.a = (unsigned char)(alpha * 255.0f);
    return color;
}
int MeasureText(const char *text, int fontSize) {
    return text ? (int)strlen(text) * (fontSize / 2) : 0;
}

const char *TextFormat(const char *text, ...) {
    static char ring[4][256];
    static int slot = 0;
    char *buf = ring[slot];
    slot = (slot + 1) % 4;
    va_list args;
    va_start(args, text);
    vsnprintf(buf, sizeof ring[0], text, args);
    va_end(args);
    return buf;
}
