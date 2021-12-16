
#ifndef GRAPHICS_H
#define GRAPHICS_H

#define RGBA(r, g, b, a) (((u8)(a) << 24) + ((u8)(r) << 16) + ((u8)(g) << 8) + (u8)(b))
#define A_COMP(col) ((u8)((col) >> 24))
#define R_COMP(col) ((u8)((col) >> 16))
#define G_COMP(col) ((u8)((col) >> 8))
#define B_COMP(col) ((u8)(col))

// Same as windows one in wingdi.h
struct MY_RGBQUAD
{
    u8 blue;
    u8 green;
    u8 red;
    u8 reserved;
};

struct bitmap
{
    // Top Down bitmap with a positive height
    // Don't use outside of this program. Usually top down bitmaps have a negative height I think.
    static constexpr int BYTES_PER_PIXEL = 4;
    s32 Width;
    s32 Height;
    u32 *Pixels;
    s32 Pitch;
};

#endif //GRAPHICS_H
