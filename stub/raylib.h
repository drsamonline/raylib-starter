/* stub/raylib.h — the slice of raylib this starter uses, so the game compiles
 * and runs with no engine, no window and no GPU.
 *
 * Signatures match raylib 5.x, so the real build and the stub build compile the
 * same source. Only what main.c actually calls is here; adding a call means
 * adding it here too, which is the point - the stub is the contract.
 */
#ifndef GK_STUB_RAYLIB_H
#define GK_STUB_RAYLIB_H

#include <stdbool.h>

typedef struct { unsigned char r, g, b, a; } Color;
typedef struct { float x, y; } Vector2;
typedef struct { float x, y, width, height; } Rectangle;

#define RAYWHITE  (Color){ 245, 245, 245, 255 }
#define WHITE     (Color){ 255, 255, 255, 255 }
#define BLACK     (Color){ 0, 0, 0, 255 }
#define BLANK     (Color){ 0, 0, 0, 0 }
#define GRAY      (Color){ 130, 130, 130, 255 }
#define LIGHTGRAY (Color){ 200, 200, 200, 255 }
#define DARKGRAY  (Color){ 80, 80, 80, 255 }
#define RED       (Color){ 230, 41, 55, 255 }
#define GREEN     (Color){ 0, 228, 48, 255 }
#define BLUE      (Color){ 0, 121, 241, 255 }

enum {
    KEY_LEFT = 263, KEY_RIGHT = 262, KEY_UP = 265, KEY_DOWN = 264,
    KEY_SPACE = 32, KEY_ENTER = 257
};

enum { FLAG_VSYNC_HINT = 0x00000040 };

/* ---- the host, driven by the test harness ---- */
void Stub_SetFrameTime(double dt);
void Stub_KeyDown(int key);
void Stub_KeyUp(int key);
int  Stub_DrawCalls(void);
void Stub_CloseWindow(void);
void Stub_Reset(void);

/* ---- the raylib surface main.c uses ---- */
void SetConfigFlags(unsigned int flags);
void InitWindow(int width, int height, const char *title);
void CloseWindow(void);
bool WindowShouldClose(void);
void SetTargetFPS(int fps);

void BeginDrawing(void);
void EndDrawing(void);
void ClearBackground(Color color);
void DrawRectangle(int x, int y, int w, int h, Color color);
void DrawRectangleRec(Rectangle rec, Color color);
void DrawRectangleLines(int x, int y, int w, int h, Color color);
void DrawCircle(int cx, int cy, float radius, Color color);
void DrawText(const char *text, int x, int y, int fontSize, Color color);

float GetFrameTime(void);
int GetScreenWidth(void);
int GetScreenHeight(void);

bool IsKeyDown(int key);
bool IsKeyPressed(int key);
bool IsKeyReleased(int key);

Color Fade(Color color, float alpha);
int MeasureText(const char *text, int fontSize);

/* raylib's TextFormat: a rotating ring of static buffers, printf-style. */
const char *TextFormat(const char *text, ...);

#endif
