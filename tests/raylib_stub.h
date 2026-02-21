// Minimal raylib stub for unit testing — replaces <raylib.h> via forced include.
// Uses the same include guard so #include <raylib.h> becomes a no-op.
#ifndef RAYLIB_H
#define RAYLIB_H

typedef struct Vector2 {
    float x;
    float y;
} Vector2;

typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

typedef struct Rectangle {
    float x;
    float y;
    float width;
    float height;
} Rectangle;

typedef struct Camera2D {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;

typedef struct Texture2D {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} Texture2D;

typedef struct Font {
    int baseSize;
} Font;

typedef struct Music {
    int dummy;
} Music;

typedef struct Sound {
    int dummy;
} Sound;

#define WHITE   (Color){255, 255, 255, 255}
#define RED     (Color){255, 0, 0, 255}
#define GREEN   (Color){0, 255, 0, 255}
#define BLUE    (Color){0, 0, 255, 255}
#define BLACK   (Color){0, 0, 0, 255}
#define YELLOW  (Color){255, 255, 0, 255}
#define GRAY    (Color){130, 130, 130, 255}
#define BLANK   (Color){0, 0, 0, 0}

#endif // RAYLIB_H
