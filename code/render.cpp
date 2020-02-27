
struct Font
{
    i32 atlas_width;
    i32 atlas_height;
    u8 *atlas;  // Not sure if should be 32-bit
    stbtt_bakedchar *glyphs;
    i32 glyphs_count;
};

void draw_text(Screen_Buffer *buffer, Font *font, char *text, int baseline_x, int baseline_y, r32 r, r32 g, r32 b)
{
    // TODO: make pen_x and y float and accululate and round advance widths
    // TODO: font licensing
    // TODO: Maybe separate bitmap for each character
    int pen_x = rvl_clamp(baseline_x, 0, buffer->width);
    int pen_y = rvl_clamp(baseline_y, 0, buffer->height);
    
    for (char *c = text; *c; ++c)
    {
        if (*c < ' ' || *c > '~') continue;
        
        stbtt_bakedchar glyph = font->glyphs[*c - 32];
        
        int x = pen_x + roundf(glyph.xoff);
        int y = pen_y + roundf(glyph.yoff);
        
        int w = glyph.x1 - glyph.x0;
        int h = glyph.y1 - glyph.y0;
        
        if (x < 0) glyph.x0 += -x;
        if (x + w > buffer->width) glyph.x1 -= ((x + w) - buffer->width);
        if (y < 0) glyph.y0 += -y; 
        if (y + h > buffer->height) glyph.y1 -= (y + h) - buffer->height;
        
        x = rvl_clamp(x, 0, buffer->width);
        y = rvl_clamp(y, 0, buffer->height);
        glyph.x0 = rvl_clamp((int)glyph.x0, 0, buffer->width);
        glyph.y0 = rvl_clamp((int)glyph.y0, 0, buffer->height);
        glyph.x1 = rvl_clamp((int)glyph.x1, 0, buffer->width);
        glyph.y1 = rvl_clamp((int)glyph.y1, 0, buffer->height);
        
        //draw_rectangle(buffer, Rect2i{{pen_x, pen_y}, {pen_x+2, pen_y+50}}, 0, 0, 1); // draw start of pen
        //draw_rectangle(buffer, Rect2i{{pen_x, pen_y}, {pen_x+100, pen_y+1}}, 1, 1, 0.7f); // draw_baseline
        
        u8 *src_row = font->atlas + glyph.x0 + (glyph.y0 * font->atlas_width);
        u8 *dest_row = (u8 *)buffer->data + (x * buffer->BYTES_PER_PIXEL) + (y * buffer->pitch);
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
                
                u32 result_r = (u32)roundf(rvl_lerp(dest_r, r, a));
                u32 result_g = (u32)roundf(rvl_lerp(dest_g, g, a));
                u32 result_b = (u32)roundf(rvl_lerp(dest_b, b, a));
                
                *dest = (result_r << 16) | (result_g << 8) | (result_b << 0);
                
                ++dest;
                ++src;
            }
            
            dest_row += buffer->pitch;
            src_row += font->atlas_width;
        }
        
        pen_x += roundf(glyph.xadvance);
    }
}


void draw_rectangle(Screen_Buffer *buffer, Rect2i rect, r32 R, r32 G, r32 B)
{
    int x0 = rvl_clamp(rect.min.x, 0, buffer->width);
    int y0 = rvl_clamp(rect.min.y, 0, buffer->height);
    int x1 = rvl_clamp(rect.max.x, 0, buffer->width);
    int y1 = rvl_clamp(rect.max.y, 0, buffer->height);
    
    u32 colour = 
        ((u32)roundf(R * 255.0f) << 16) |
        ((u32)roundf(G * 255.0f) << 8) |
        ((u32)roundf(B * 255.0f));
    
    u8 *row = (u8 *)buffer->data + x0*Screen_Buffer::BYTES_PER_PIXEL + y0*buffer->pitch;
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


void draw_bar_plot(Screen_Buffer *buffer, Day *days, u32 cur_day, int origin_x, int origin_y)
{
    V2i origin = {origin_x, origin_y};
    
    Rect2i x_axis = {
        origin - V2i{backtail, 0},
        origin + V2i{x_axis_length, line_thickness}
    };
    Rect2i y_axis = {
        origin - V2i{0, y_axis_height},
        origin + V2i{line_thickness, backtail+line_thickness}
    };
    
    draw_rectangle(buffer, 
                   x_axis,
                   0.0f, 0.0f, 0.0f);
    
    draw_rectangle(buffer, 
                   y_axis,
                   0.0f, 0.0f, 0.0f);
    
    
    Day &today = days[cur_day];
    double max_duration = 0;
    for (int j = 0; j < today.record_count; ++j)
    {
        max_duration = std::max(max_duration, today.records[j].duration);
    }
    
    int max_bars = x_axis_length / (bar_width + bar_spacing);
    int bars = std::min(max_bars, (int)today.record_count);
    
    for (int i = 0; i < bars; ++i)
    {
        double scale = (today.records[i].duration / max_duration);
        int bar_height = y_axis_height * scale;
        
        Rect2i bar = {
            {0, 0},
            {bar_width, bar_height}
        };
        
        V2i bar_offset = V2i{bar_spacing + i*(bar_width + bar_spacing), 0};
        V2i top_left = origin + bar_offset - V2i{0, bar_height};
        
        bar.min += top_left;
        bar.max += top_left;
        
        r32 t = (r32)i/bars;
        r32 red = (1 - t) * 0.0f + t * 1.0f;
        draw_rectangle(buffer, 
                       bar,
                       red, 0.0f, 0.0f);
    }
}


void draw_bar_plot_from_records(Screen_Buffer *buffer, Font *font,  Program_Record *records, u32 record_count, 
                                int origin_x, int origin_y)
{
    V2i origin = {origin_x, origin_y};
    
    Rect2i x_axis = {
        origin - V2i{backtail, 0},
        origin + V2i{x_axis_length, line_thickness}
    };
    Rect2i y_axis = {
        origin - V2i{0, y_axis_height},
        origin + V2i{line_thickness, backtail+line_thickness}
    };
    
    draw_rectangle(buffer, 
                   x_axis,
                   0.0f, 0.0f, 0.0f);
    
    draw_rectangle(buffer, 
                   y_axis,
                   0.0f, 0.0f, 0.0f);
    
    
    // Max should be the first but leave this for now
    double max_duration = 0;
    for (int j = 0; j < record_count; ++j)
    {
        max_duration = std::max(max_duration, records[j].duration);
    }
    
    int max_bars = x_axis_length / (bar_width + bar_spacing);
    int bar_count = std::min(max_bars, (int)record_count);
    
    for (int i = 0; i < bar_count; ++i)
    {
        double scale = (records[i].duration / max_duration);
        int bar_height = y_axis_height * scale;
        
        Rect2i bar = {
            {0, 0},
            {bar_width, bar_height}
        };
        
        V2i bar_offset = V2i{bar_spacing + i*(bar_width + bar_spacing), 0};
        V2i top_left = origin + bar_offset - V2i{0, bar_height};
        
        bar.min += top_left;
        bar.max += top_left;
        
        r32 t = (r32)i/bar_count;
        r32 red = (1 - t) * 0.0f + t * 1.0f;
        
        char text[512];
        snprintf(text, array_count(text), "%.2lfs", records[i].duration);
        draw_rectangle(buffer, 
                       bar,
                       red, 0.0f, 0.0f);
        draw_text(buffer, font, text, bar.min.x - 6, bar.min.y - 4, 0.0f, 0.0f, 0.0f);
    }
}


void
draw_win32_bitmap(Screen_Buffer *buffer, BITMAP *bitmap, int buffer_x, int buffer_y)
{
    int x0 = buffer_x;
    int y0 = buffer_y;
    int x1 = buffer_x + bitmap->bmWidth;
    int y1 = buffer_y + bitmap->bmHeight;
    
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
    
    u8 *src_row = (u8 *)bitmap->bmBits;
    u8 *dest_row = (u8 *)buffer->data + (x0 * buffer->BYTES_PER_PIXEL) + (y0 * buffer->pitch);
    for (int y = y0; y < y1; ++y)
    {
        u32 *src = (u32 *)src_row;
        u32 *dest = (u32 *)dest_row;
        for (int x = x0; x < x1; ++x)
        {
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
        
        src_row += bitmap->bmWidthBytes;
        dest_row += buffer->pitch;
    }
}

void
draw_simple_bitmap(Screen_Buffer *buffer, Simple_Bitmap *bitmap, int buffer_x, int buffer_y)
{
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
    u8 *dest_row = (u8 *)buffer->data + (x0 * buffer->BYTES_PER_PIXEL) + (y0 * buffer->pitch);
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
    return font;
    
    
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
            rvl_assert(is_space_in_atlas(atlas_width, atlas_height, width, height, pen_x, pen_y));
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
}

