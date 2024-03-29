#ifndef GRAPHICS_H
#define GRAPHICS_H
#pragma once

#define RGBA(r, g, b, a) (((u8)(a) << 24) + ((u8)(r) << 16) + ((u8)(g) << 8) + (u8)(b))
#define A_COMP(col) ((u8)((col) >> 24))
#define R_COMP(col) ((u8)((col) >> 16))
#define G_COMP(col) ((u8)((col) >> 8))
#define B_COMP(col) ((u8)(col))

// I don't know if this is really necesary
typedef u32 Colour;

// Same as windows one in wingdi.h
struct MY_RGBQUAD
{
    u8 blue;
    u8 green;
    u8 red;
    u8 reserved;
};

struct Bitmap
{
    // Top Down bitmap with a positive height
    // Don't use outside of this program. Usually top down bitmaps have a negative height I think.
    static constexpr int BYTES_PER_PIXEL = 4;
    i32 width;
    i32 height;
    u32 *pixels;
    i32 pitch;
};

#if 0
#include "stb_truetype.h"
// TODO: This is debug only
#define RGB_NORMAL(r, g, b)\
(0xFF << 24|(u32)roundf((r) * 255.0f) << 16)|((u32)roundf((g) * 255.0f) << 8)|((u32)roundf((b) * 255.0f))

#define RGB_OPAQUE(r, g, b)  (((u8)0xFF << 24) + ((u8)(r) << 16) + ((u8)(g) << 8) + (u8)(b))
#define GREY(c) RGB_OPAQUE(c, c, c)

#define NICE_RED RGB_OPAQUE(244, 67, 54)
#define NICE_GREEN RGB_OPAQUE(102, 187, 106)
#define NICE_BLUE RGB_OPAQUE(79, 195, 247)

#define lerp(x, y, t) (((1.0f-(t))*(x)) + ((t)*(y)))

struct Font
{
    i32 atlas_width;
    i32 atlas_height;
    u8 *atlas;  // Not sure if should be 32-bit
    stbtt_bakedchar *glyphs;
    i32 glyphs_count;
    i32 max_ascent;
    i32 max_descent;
};

struct V2i
{
    int x, y;
};

struct Rect
{
    V2i pos;
    V2i dim;
};
#endif

#endif