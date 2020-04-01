
#include "monitor.h"
#include "graphics.h"
#include <algorithm>

template<class T>
constexpr const T& clamp( const T& v, const T& lo, const T& hi )
{
    Assert(!(hi < lo));
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

int
text_width(char *text, Font *font)
{
    float total_w = 0;
    for (char *c = text; *c; ++c)
    {
        if (*c < ' ' || *c > '~') continue;
        stbtt_bakedchar glyph = font->glyphs[*c - 32];
        total_w += glyph.xadvance;
    }
    
    return (int)total_w;
}

void draw_text(Bitmap *buffer, Font *font, char *text, int baseline_x, int baseline_y, u32 colour)
{
    // TODO: make pen_x and y float and accululate and round advance widths
    // TODO: font licensing
    // TODO: Maybe separate bitmap for each character
    float pen_x = (float)clamp(baseline_x, 0, buffer->width);
    float pen_y = (float)clamp(baseline_y, 0, buffer->height);
    
    for (char *c = text; *c; ++c)
    {
        if (*c < ' ' || *c > '~') continue;
        
        stbtt_bakedchar glyph = font->glyphs[*c - 32];
        
        int x = (int)roundf(pen_x + glyph.xoff);
        int y = (int)roundf(pen_y + glyph.yoff);
        
        int w = glyph.x1 - glyph.x0;
        int h = glyph.y1 - glyph.y0;
        
        if (x < 0) glyph.x0 += -x;
        if (x + w > buffer->width) glyph.x1 -= ((x + w) - buffer->width);
        if (y < 0) glyph.y0 += -y;
        if (y + h > buffer->height) glyph.y1 -= (y + h) - buffer->height;
        
        x = clamp(x, 0, buffer->width);
        y = clamp(y, 0, buffer->height);
        glyph.x0 = clamp((int)glyph.x0, 0, buffer->width);
        glyph.y0 = clamp((int)glyph.y0, 0, buffer->height);
        glyph.x1 = clamp((int)glyph.x1, 0, buffer->width);
        glyph.y1 = clamp((int)glyph.y1, 0, buffer->height);
        
        //draw_rectangle(buffer, Rect2i{{pen_x, pen_y}, {pen_x+2, pen_y+50}}, 0, 0, 1); // draw start of pen
        //draw_rectangle(buffer, Rect2i{{pen_x, pen_y}, {pen_x+100, pen_y+1}}, 1, 1, 0.7f); // draw_baseline
        
        u8 *src_row = font->atlas + glyph.x0 + (glyph.y0 * font->atlas_width);
        u8 *dest_row = (u8 *)buffer->pixels + (x * buffer->BYTES_PER_PIXEL) + (y * buffer->pitch);
        for (int min_y = glyph.y0; min_y < glyph.y1; ++min_y)
        {
            u32 *dest = (u32 *)dest_row;
            u8 *src = src_row;
            for (int min_x = glyph.x0; min_x < glyph.x1; ++min_x)
            {
                r32 a = *src / 255.0f;
                
                r32 dest_r = (r32)((*dest >> 16) & 0xFF);
                r32 dest_g = (r32)((*dest >> 8) & 0xFF);
                r32 dest_b = (r32)((*dest >> 0) & 0xFF);
                
                // TODO: Not sure how to pass in colour properly, it seems bad to pass in three floats as well.
                r32 r = (r32)R_COMP(colour);
                r32 g = (r32)G_COMP(colour);
                r32 b = (r32)B_COMP(colour);
                u32 result_r = (u32)roundf(lerp(dest_r, r, a));
                u32 result_g = (u32)roundf(lerp(dest_g, g, a));
                u32 result_b = (u32)roundf(lerp(dest_b, b, a));
                
                // This leaves dest with same alpha
                *dest = (result_r << 16) | (result_g << 8) | (result_b << 0);
                
                ++dest;
                ++src;
            }
            
            dest_row += buffer->pitch;
            src_row += font->atlas_width;
        }
        
        pen_x += glyph.xadvance;
    }
}

void draw_rectangle(Bitmap *buffer, Rect2i rect, Colour colour)
{
    int x0 = clamp(rect.min.x, 0, buffer->width);
    int y0 = clamp(rect.min.y, 0, buffer->height);
    int x1 = clamp(rect.max.x, 0, buffer->width);
    int y1 = clamp(rect.max.y, 0, buffer->height);
    
    u8 *row = (u8 *)buffer->pixels + x0*Bitmap::BYTES_PER_PIXEL + y0*buffer->pitch;
    for (int y = y0; y < y1; ++y)
    {
        u32 *dest = (u32 *)row;
        for (int x = x0; x < x1; ++x)
        {
            *dest++ = colour;
        }
        
        row += buffer->pitch;
    }
}

void
draw_rectangle(Bitmap *buffer, int x, int y, int w, int h, Colour colour)
{
    int x0 = clamp(x, 0, buffer->width);
    int y0 = clamp(y, 0, buffer->height);
    int x1 = clamp(x + w, 0, buffer->width);
    int y1 = clamp(y + h, 0, buffer->height);
    
    u8 *row = (u8 *)buffer->pixels + x0*Bitmap::BYTES_PER_PIXEL + y0*buffer->pitch;
    for (int y = y0; y < y1; ++y)
    {
        u32 *dest = (u32 *)row;
        for (int x = x0; x < x1; ++x)
        {
            *dest++ = colour;
        }
        
        row += buffer->pitch;
    }
}


void
draw_horizontal_line(Bitmap *buffer, int x0, int x1, int y, Colour colour)
{
    x0 = clamp(x0, 0, buffer->width-1);
    x1 = clamp(x1, 0, buffer->width);
    y  = clamp(y, 0, buffer->height-1);
    if (x0 <= x1)
    {
        u32 *dest = (u32 *)((u8 *)buffer->pixels + x0*Bitmap::BYTES_PER_PIXEL + y*buffer->pitch);
        for (int x = x0; x < x1; ++x)
        {
            *dest++ = colour;
        }
    }
}

void
draw_vertical_line(Bitmap *buffer, int y0, int y1, int x, Colour colour)
{
    y0 = clamp(y0, 0, buffer->height-1);
    y1 = clamp(y1, 0, buffer->height);
    x  = clamp(x, 0, buffer->width-1);
    
    if (y0 <= y1)
    {
        u8 *dest = (u8 *)buffer->pixels + x*Bitmap::BYTES_PER_PIXEL + y0*buffer->pitch;
        for (int y = y0; y < y1; ++y)
        {
            *(u32 *)dest = colour;
            dest += buffer->pitch;
        }
    }
}

void
draw_rect_outline(Bitmap *buffer, int x, int y, int w, int h, Colour colour)
{
    int x0 = clamp(x, 0, buffer->width);
    int y0 = clamp(y, 0, buffer->height);
    int x1 = clamp(x + w, 0, buffer->width);
    int y1 = clamp(y + h, 0, buffer->height);
    
    draw_horizontal_line(buffer, x0, x1, y0, colour);
    draw_horizontal_line(buffer, x0, x1, y1-1, colour);
    draw_vertical_line(buffer,   y0, y1, x0, colour);
    draw_vertical_line(buffer,   y0, y1, x1-1, colour);
}

void
draw_bitmap(Bitmap *buffer, Bitmap *bitmap, int buffer_x, int buffer_y)
{
    if (!bitmap->pixels || bitmap->width == 0 || bitmap->height == 0)
    {
        Assert(0);
        return;
    }
    
    int x0 = buffer_x;
    int y0 = buffer_y;
    int x1 = buffer_x + bitmap->width;
    int y1 = buffer_y + bitmap->height;
    
    if (x0 < 0)
    {
        x0 = 0;
    }
    if (y0 < 0)
    {
        y0 = 0;
    }
    if (x1 > buffer->width)
    {
        x1 = buffer->width;
    }
    if (y1 > buffer->height)
    {
        y1 = buffer->height;
    }
    
    u32 *src = bitmap->pixels;
    u8 *dest_row = (u8 *)buffer->pixels + (x0 * buffer->BYTES_PER_PIXEL) + (y0 * buffer->pitch);
    for (int y = y0; y < y1; ++y)
    {
        u32 *dest = (u32 *)dest_row;
        for (int x = x0; x < x1; ++x)
        {
            // Linear blend with destination colour based on alpha value
            r32 A = ((*src >> 24) & 0xFF) / 255.0f;
            
            r32 SR = (r32)((*src >> 16) & 0xFF);
            r32 SG = (r32)((*src >> 8) & 0xFF);
            r32 SB = (r32)((*src >> 0) & 0xFF);
            
            r32 DR = (r32)((*dest >> 16) & 0xFF);
            r32 DG = (r32)((*dest >> 8) & 0xFF);
            r32 DB = (r32)((*dest >> 0) & 0xFF);
            
            r32 R = (1.0f-A)*DR + A*SR;
            r32 G = (1.0f-A)*DG + A*SG;
            r32 B = (1.0f-A)*DB + A*SB;
            
            *dest = (((u32)(R + 0.5f) << 16) |
                     ((u32)(G + 0.5f) << 8) |
                     ((u32)(B + 0.5f) << 0));
            
            ++dest;
            ++src;
        }
        
        dest_row += buffer->pitch;
    }
}

void
render_gui(Bitmap *buffer, Database *database, Day_View *day_view, Font *font)
{
#if 0
    Assert(day_view);
    Assert(buffer);
    Assert(database);
    Assert(font);
    
    // Fill Background
    
    // TODO: This is bad api, remove the rect etc
    draw_rectangle(buffer, Rect2i{{0, 0}, {buffer->width, buffer->height}}, RGB(255, 255, 255));
    
    int canvas_x = 200;
    int canvas_y = 0;
    int canvas_width = 700;
    int canvas_height = 540;
    
    Rect2i canvas = {{canvas_x, canvas_y}, {canvas_x + canvas_width, canvas_y + canvas_height}};
    draw_rectangle(buffer, canvas, RGB_NORMAL(0.9f, 0.9f, 0.9f));
    
    int bar_thickness = 30;
    int bar_spacing = 10;
    int line_thickness = 1;
    
    int x_axis_length = 500;
    int y_axis_length = 400;
    
    int icon_margin = 50;
    int text_margin = 70;
    
    V2i bar_origin = canvas.min + V2i{icon_margin, 0};
    
    Rect2i x_axis = {
        bar_origin,
        bar_origin + V2i{x_axis_length, line_thickness}
    };
    Rect2i y_axis = {
        bar_origin,
        bar_origin + V2i{line_thickness, y_axis_length}
    };
    
    draw_rectangle(buffer, x_axis, 0.0f, 0.0f, 0.0f);
    draw_rectangle(buffer, y_axis, 0.0f, 0.0f, 0.0f);
    
    for (int i = 0; i < bar_count; ++i)
    {
        Program_Record &record = sorted_records[i];
        
        double scale = (record.duration / max_duration);
        int bar_length = (int)(max_bar_length * scale);
        Rect2i bar = {{0, 0}, {bar_length, bar_thickness}};
        
        bar.min += bar_origin;
        bar.max += bar_origin;
        
        int bar_y_offset = bar_spacing + i*(bar_thickness + bar_spacing);
        bar.min.y += bar_y_offset;
        bar.max.y += bar_y_offset;
        
        {
            r32 t = (r32)i/bar_count;
            r32 red = lerp(0.0f, 1.0f, t);
            
            draw_rectangle(buffer, bar, RGB_NORMAL(red, 0.0f, 0.0f));
            
            
            draw_text(buffer, font, text, bar.max.x + 5, bar.max.y - 5, 0.0f, 0.0f, 0.0f);
        }
        
        // TODO: line between icon and bar?
        
        // TODO: I want renderer to know less about program, immediate mode rendering might help with this
        Bitmap *icon = 0;
        //icon = get_icon_from_database(database, record.id);
        if (database->icons[record.id].pixels)
        {
            Assert(database->icons[record.id].width > 0 && database->icons[record.id].pixels > 0);
            icon = &database->icons[record.id];
        }
        if (icon)
        {
            int icon_centre_x = canvas_x + (bar_origin.x - canvas_x) / 2;
            int icon_centre_y = bar.min.y + (bar.max.y - bar.min.y) / 2;
            
            int icon_top_left_x = icon_centre_x - (icon->width/2);
            int icon_top_left_y = icon_centre_y - (icon->height/2);
            
            draw_simple_bitmap(buffer, icon, icon_top_left_x, icon_top_left_y);
        }
    }
#endif
}




struct Glyph
{
    i32 x;
    i32 y;
    i32 width;
    i32 height;
    i32 bitmap_left;
    i32 bitmap_top;
    i32 advance;
    u32 charcode;
};
Font create_font(char *font_name, int pixel_height)
{
    // TODO: Do we even need a texture atlas, can just have a bitmap for each char
    // TODO: Can we just write the whole rasterised font data to a file and convert it to a c-array and
    // put into source code, to avoid rasterising at run time. Because we don't really need to change fonts
    // at run time.
    char *times = "c:\\dev\\projects\\monitor\\build\\times.ttf";
    char *liberation = "c:\\dev\\projects\\monitor\\build\\LiberationMono-Regular.ttf";
    FILE* font_file = fopen(font_name, "rb");
    fseek(font_file, 0, SEEK_END);
    long size = ftell(font_file);
    fseek(font_file, 0, SEEK_SET);
    
    u8 *font_data = (u8 *)xalloc(size);
    
    fread(font_data, size, 1, font_file);
    fclose(font_file);
    
    //stbtt_fontinfo font;
    //stbtt_InitFont(&font, font_data, stbtt_GetFontOFfsetForIndex(font_data, 0));
    
    u8 *atlas_bitmap = (u8 *)xalloc(512 * 512);
    stbtt_bakedchar *chars = (stbtt_bakedchar *)xalloc(sizeof(stbtt_bakedchar) * 96);
    stbtt_BakeFontBitmap(font_data, 0, (float)pixel_height, atlas_bitmap, 512, 512, 32, 96, chars); // no guarantee this fits!
    
    Font font = {};
    font.atlas_width = 512;
    font.atlas_height = 512;
    font.atlas = atlas_bitmap;
    font.glyphs = chars;
    font.glyphs_count = 95;
    
    int max_descent = 0;
    int max_ascent = 0;
    for (int i = 0; i < font.glyphs_count; ++i)
    {
        stbtt_bakedchar glyph = font.glyphs[i];
        int height = (int)(glyph.y1 - glyph.y0);
        int ascent = (int)-glyph.yoff;
        max_ascent = std::max(max_ascent, ascent); // Can't figure how to get ascent and descent in stb_tt
        
        // w and h are width and height of bitmap
        // xoff/yoff are the offset it pixel space from the glyph origin to the top-left of the bitmap
        // advance includes width, and is the distance to get start of next glyph (ignoring kerning)
        // However it seems that ascent can be -18 (because it's counting with y as down), but height only 2
        // so don't know how to calculare descent.
    }
    
    font.max_ascent = max_ascent;
    font.max_descent = max_ascent;  // HACK: Assuming max descent <= max_ascent
    
    return font;
}

#if 0
int ascent = 0;
int descent = 0;
int linegap = 0;
int baseline = 0;
float scale, xpos=2; // leave a little padding in case the character extends left
char *text = "Heljo World!"; // intentionally misspelled to show 'lj' brokenness

scale = stbtt_ScaleForPixelHeight(&font, 15);
stbtt_GetFontVMetrics(&font, &ascent, &descent, &linegap);
baseline = (int) (ascent*scale);

i32 atlas_width = 512;
i32 atlas_height = 512;
u8 *atlas_pixels = (u8 *)xalloc(atlas_width * atlas_height);

int pen_x = 0;
int pen_y = 0;
int max_y = 0;

i32 glyphs_desired = 25;
for (i32 i = 0; i < glyphs_desired; ++i)
{
    int width = 0;
    int height = 0;
    int xoff = 0;
    int yoff = 0;
    unsigned char *glyph_bitmap = stbtt_GetCodepointBitmap(&font, 1, 1, 'a' + i, &width, &height, &xoff, &yoff);
    
    if (!is_space_in_atlas(atlas_width, atlas_height, width, height, pen_x, pen_y))
    {
        pen_x = 0;
        pen_y = max_y;
        Assert(is_space_in_atlas(atlas_width, atlas_height, width, height, pen_x, pen_y));
    }
    
    u8 *src = glyph_bitmap;
    u8 *dest_row = atlas_pixels + pen_x + (pen_y * atlas_width);
    
    for (i32 y = 0; y < height; ++y)
    {
        u8* dest = dest_row;
        for (i32 x = 0; x < width; ++x)
        {
            *dest++ = *src++;
        }
        
        dest_row += atlas_width;
    }
    
    max_y = std::max(max_y, pen_y + height);
    pen_x += width;
    
    // Not sure why this takes two arguments so will just free using free
    // stbtt_FreeBitmap(glyph_bitmap);
    free(glyph_bitmap);
}

return atlas_pixels;
#endif

