// Minimal raylib stand-in for unit tests: the include guard matches raylib's, so game
// headers that include <raylib.h> get these plain structs and no window or GL calls.
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

#define WHITE (Color){255, 255, 255, 255}
#define BLACK (Color){0, 0, 0, 255}

#endif // RAYLIB_H
