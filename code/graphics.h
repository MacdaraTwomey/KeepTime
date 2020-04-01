#pragma once

#include "stb_truetype.h"

// TODO: This is debug only
#define RGB_NORMAL(r, g, b)\
(0xFF << 24|(u32)roundf((r) * 255.0f) << 16)|((u32)roundf((g) * 255.0f) << 8)|((u32)roundf((b) * 255.0f))

// TODO: THIS MIGHT BE MISNAMED, not actually rgb?
#define RGBA(r, g, b, a) (((u8)(a) << 24) + ((u8)(r) << 16) + ((u8)(g) << 8) + (u8)(b))
#define RGB_OPAQUE(r, g, b)  (((u8)0xFF << 24) + ((u8)(r) << 16) + ((u8)(g) << 8) + (u8)(b))
#define GREY(c) RGB_OPAQUE(c, c, c)

#define A_COMP(col) ((u8)((col) >> 24))
#define R_COMP(col) ((u8)((col) >> 16))
#define G_COMP(col) ((u8)((col) >> 8))
#define B_COMP(col) ((u8)(col))

#define lerp(x, y, t) (((1.0f-(t))*(x)) + ((t)*(y)))

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
