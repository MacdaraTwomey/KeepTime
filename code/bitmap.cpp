
#include "graphics.h"

bool
file_is_png(u8 *file_data, size_t length)
{
    u8 PNG_signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (length < sizeof(PNG_signature)) return false;
    
    return (memcmp(file_data, PNG_signature, sizeof(PNG_signature)) == 0);
}

void
render_icon_to_bitmap_32(int width, int height, u8 *XOR, Simple_Bitmap *bitmap)
{
    int pitch = width * 4;
    
    u32 *dest = bitmap->pixels;
    u8 *src_row = (u8 *)XOR + ((height-1) * pitch);
    for (int y = 0; y < bitmap->height; ++y)
    {
        u32 *src = (u32 *)src_row;
        for (int x = 0; x < bitmap->width; ++x)
        {
            *dest++ = *src++;
        }
        
        src_row -= pitch;
    }
}


void
render_icon_to_bitmap_32_no_alpha(int width, int height, u8 *XOR, u8 *AND, Simple_Bitmap *bitmap)
{
    int xor_pitch = width * 4;
    int and_pitch = width / 8;
    
    u32 *dest = bitmap->pixels;
    
    u8 *xor_row = (u8 *)XOR + ((height-1) * xor_pitch);
    u8 *and_row = (u8 *)AND + ((height-1) * and_pitch);
    
    int bit = 7;
    for (int y = 0; y < bitmap->height; ++y)
    {
        u32 *src_xor = (u32 *)xor_row;
        u8 *src_and = and_row;
        for (int x = 0; x < bitmap->width; ++x)
        {
            u32 alpha = (*src_and & (1 << bit)) ? 0xFF : 0x00;
            *dest++ = *src_xor++ | (alpha << 24);
            bit -= 1;
            if (bit == -1)
            {
                bit = 7;
                ++src_and;
            }
        }
        
        xor_row -= xor_pitch;
        and_row -= and_pitch;
    }
}

void
render_icon_to_bitmap_16(int width, int height, u8 *XOR, u8 *AND, MY_RGBQUAD *table, Simple_Bitmap *bitmap)
{
    u32 *dest = bitmap->pixels;
    u16 *src_xor = (u16 *)XOR;
    u8 *src_and = (u8 *)AND;
    int bit = 7;
    for (int y = 0; y < bitmap->height; ++y)
    {
        for (int x = 0; x < bitmap->width; ++x)
        {
            u32 alpha = (*src_and & (1 << bit)) ? 0xFF : 0x00;
            MY_RGBQUAD col = table[*src_xor++];
            u32 colour = RGBA(col.red, col.green, col.blue, alpha);
            *dest++ = colour;
            bit -= 1;
            if (bit == -1)
            {
                bit = 7;
                ++src_and;
            }
        }
    }
}


void
render_icon_to_bitmap_8(int width, int height,  u8 *XOR, u8 *AND, MY_RGBQUAD *table, Simple_Bitmap *bitmap)
{
    int xor_pitch = width;
    int and_pitch = width / 8;
    
    u32 *dest = bitmap->pixels;
    u8 *xor_row = (u8 *)XOR + ((height-1) * xor_pitch);
    u8 *and_row = (u8 *)AND + ((height-1) * and_pitch);
    
    for (int y = 0; y < bitmap->height; ++y)
    {
        u8 *xor_src = xor_row;
        u8 *and_src = and_row;
        for (int x = 0; x < bitmap->width; ++x)
        {
            int bit = 7 - (x % 8);
            u32 alpha = (*and_src & (1 << bit)) ? 0xFF : 0x00;
            alpha = 0xff;
            
            MY_RGBQUAD col = table[*xor_src++];
            u32 colour = RGBA(col.red, col.green, col.blue, alpha);
            *dest++ = colour;
            
            if (bit == 0) ++and_src;
        }
        
        xor_row -= xor_pitch;
        and_row -= and_pitch;
    }
}


void
render_icon_to_bitmap_4(int width, int height, u8 *XOR, u8 *AND, MY_RGBQUAD *table, Simple_Bitmap *bitmap)
{
    u32 *dest = bitmap->pixels;
    int xor_pitch = (width * 4) / 8;
    int and_pitch = width / 8;
    
    u8 *xor_row = (u8 *)XOR + ((height-1) * xor_pitch);
    u8 *and_row = (u8 *)AND + ((height-1) * and_pitch);
    
    for (int y = 0; y < bitmap->height; ++y)
    {
        u8 *src_xor = xor_row;
        u8 *src_and = and_row;
        for (int x = 0; x < bitmap->width; ++x)
        {
            int and_bit = 7 - (x % 8);
            u32 alpha = (*src_and & (1 << and_bit)) ? 0x00 : 0xFf;
            //alpha = 0xFF;
            
            bool high_part = x % 2;
            u8 table_index = (high_part) ? *src_xor & 0x0F : *src_xor & 0xF0 >> 4;
            
            MY_RGBQUAD col = table[table_index];
            u32 colour = RGBA(col.red, col.green, col.blue, alpha);
            *dest++ = colour;
            
            
            if (and_bit == 0) ++src_and;
            if (high_part) ++src_xor;
        }
        
        xor_row -= xor_pitch;
        and_row -= and_pitch;
    }
}


Simple_Bitmap
make_empty_bitmap(int width, int height)
{
    Assert(width > 0 && height > 0);
    Simple_Bitmap bitmap = {};
    bitmap.width = width;
    bitmap.height = height;
    bitmap.pixels = (u32 *)xalloc(width * height * bitmap.BYTES_PER_PIXEL);
    memset(bitmap.pixels, 0, width*height*bitmap.BYTES_PER_PIXEL);
    
    return bitmap;
}


Simple_Bitmap
make_bitmap(int width, int height, u32 colour)
{
    Assert(width > 0 && height > 0);
    Simple_Bitmap bitmap = {};
    bitmap.width = width;
    bitmap.height = height;
    bitmap.pixels = (u32 *)xalloc(width * height * bitmap.BYTES_PER_PIXEL);
    u32 *dest = bitmap.pixels;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            *dest++ = colour;
        }
    }
    
    return bitmap;
}
