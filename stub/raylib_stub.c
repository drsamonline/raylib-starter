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
#include <string.h>

static double g_frame_time = 1.0 / 60.0;
static int g_draw_calls = 0;
static int g_should_close = 0;
static int g_key_down[512];
static int g_key_pressed[512];
static int g_key_released[512];
static int g_width = 640, g_height = 400;

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
}
void CloseWindow(void) { g_should_close = 1; }
bool WindowShouldClose(void) { return g_should_close != 0; }
void SetTargetFPS(int fps) { (void)fps; }

void BeginDrawing(void) {}
void EndDrawing(void) {
    /* a frame is over: "pressed" and "released" last exactly one frame */
    memset(g_key_pressed, 0, sizeof g_key_pressed);
    memset(g_key_released, 0, sizeof g_key_released);
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
