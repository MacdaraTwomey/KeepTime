#include "ui.h"
#include "graphics.h"

// API calls prefixed with ui

UI_Context ui_context; // should this be in monitor?


#define gen_id() (__LINE__)

UI_Id
get_id(char *str)
{
    // Need better way to avoid collisions
    u32 id = djb2((unsigned char *)str);
    if (id == 0) id += 1;
    return id;
}

void
ui_init(Bitmap *buffer, Font *font)
{
    // TODO: Maybe ui opens fonts files and rasterises itself.
    ui_context.font = font;
    ui_context.buffer = buffer;
    ui_context.width = buffer->width;
    ui_context.height = buffer->height;
    ui_context.active = 0; // invalid id
    ui_context.hot = 0; // invalid id
}

void
ui_begin()
{
    // It seems we allow ui_context input to continue to be set while we are using it.
    // I think this is fine, and input events might not even bypass GetMessage anyway
    ui_context.layout = {};
    ui_context.has_layout = false;
}

void
ui_end()
{
    ui_context.ui_shown = false;
    ui_context.ui_hidden = false;
    ui_context.mouse_left_up = false;
    ui_context.mouse_left_down = false;
    ui_context.mouse_right_up = false;
    ui_context.mouse_right_down = false;
    ui_context.mouse_wheel = 0;
    
    // Maybe want to set hot = 0;
    ui_context.hot = 0;
}

// Api
bool ui_was_shown() { return ui_context.ui_shown; }
bool ui_was_hidden() { return ui_context.ui_hidden; }

// Internal calls
bool is_active(UI_Id id) { return ui_context.active == id; }
bool is_hot(UI_Id id) { return ui_context.hot == id; }
bool is_selected(UI_Id id) { return ui_context.selected == id; }
void set_active(UI_Id id) { ui_context.active = id; }

void
maybe_set_hot(UI_Id id)
{
    // Only set hot if mouse has not been pressed down, on another item, and
    // maybe moved off item but still held down, so the active item is being
    // interacted with.
    if (ui_context.active == 0 || ui_context.active == id) ui_context.hot = id;
}

bool
in_item_rect(int rect_x, int rect_y, int rect_w, int rect_h)
{
    return (ui_context.mouse_x >= rect_x && ui_context.mouse_x <= rect_x+rect_w &&
            ui_context.mouse_y >= rect_y && ui_context.mouse_y <= rect_y+rect_h);
}

bool
do_item(UI_Id id, int x, int y, int w, int h)
{
    // Could instead pass in a bool, that says in rect or not
    
    // Doesn't handle click and hold outside box, then move into box and release, properly
    // Doesn't handle release and click in one update loop
    // Does handle click and release in one update loop
    
    bool result = false;
    
    if (in_item_rect(x, y, w, h)) maybe_set_hot(id);
    
    if (ui_context.active == 0) // Nothing is active
    {
        if (ui_context.mouse_left_down)
        {
            if (is_hot(id)) set_active(id);
            ui_context.selected = 0;
        }
    }
    
    if (is_active(id)) // button is active
    {
        if (ui_context.mouse_left_up) // user released mouse button
        {
            if (is_hot(id)) result = true; // users mouse was still over us
            ui_context.active = 0; // Button went up so nothing is active
            ui_context.selected = 0;
        }
    }
    
    if (result) ui_context.selected = id;
    
    return result;
}

void
ui_button_row_begin(int x, int y, int w)
{
    UI_Layout layout = {};
    layout.x = x;
    layout.y = y;
    layout.spacing = 10;
    layout.w = w;
    layout.index = 0;
    int ypad = 3;
    int font_height = ui_context.font->max_ascent + ui_context.font->max_descent;
    layout.h = font_height;
    
    ui_context.layout = layout;
    ui_context.has_layout = true;
}


void
ui_button_row_end()
{
    ui_context.has_layout = false;
}

bool
ui_button(char *text, int x = 0, int y = 0)
{
    Assert(x >= 0 && y >= 0);
    // Calculate w/h position from layout rules (maybe also x y if we want)
    Font *font = ui_context.font;
    Bitmap *buffer = ui_context.buffer;
    
    int xpad = 0; // make these mode dynamic, for different text sizes
    int ypad = 0;
    int text_w = text_width(text, ui_context.font);
    int w = text_w + xpad*2;
    int h = font->max_ascent + font->max_descent + ypad*2;
    
    if (ui_context.has_layout)
    {
        UI_Layout *layout = &ui_context.layout;
        x = layout->x;
        y = layout->y;
        w = layout->w;
        h = layout->h;
        
        layout->x += w + layout->spacing;
    }
    
    UI_Id id = get_id(text);
    
    bool pressed = do_item(id, x, y, w, h);
    
    draw_rectangle(buffer, x, y, w, h, GREY(190));
    
    int centre_x = x + (w/2);
    int centre_y = y + (int)(h*0.75f);
    
    int baseline_x = centre_x - text_w/2;
    int baseline_y = centre_y;
    
    u32 colour = RGB_OPAQUE(0, 0, 0);
    if (is_hot(id)) colour = RGB_OPAQUE(255, 0, 0);
    if (is_active(id))
    {
        draw_rect_outline(buffer, x, y, w, h, RGB_OPAQUE(255, 255, 255));
        draw_rect_outline(buffer, x-1, y-1, w+2, h+2, RGB_OPAQUE(0, 0, 0));
        draw_rect_outline(buffer, x+1, y+1, w-2, h-2, RGB_OPAQUE(0, 0, 0));
    }
    else
    {
        draw_rect_outline(buffer, x, y, w, h, RGB_OPAQUE(0, 0, 0));
    }
    
    if (is_selected(id)) colour = RGB_OPAQUE(0, 0, 255);
    
    draw_text(buffer, font, text, baseline_x, baseline_y, colour);
    
    return pressed;
}

void
ui_graph_begin(int x, int y, int w, int h, UI_Id id)
{
    UI_Layout layout = {};
    // Width and height of whole barplot
    layout.w = w;
    layout.h = h;
    layout.x = x;
    layout.spacing = 10;
    layout.y = y;
    layout.index = 0;
    
    layout.start_id = id;
    ui_context.layout = layout;
    ui_context.has_layout = true;
    
    draw_rectangle(ui_context.buffer, x, y, w, h, GREY(200));
    draw_rect_outline(ui_context.buffer, x, y, w, h, GREY(0));
}

void ui_graph_end()
{
    ui_context.has_layout = false;
}

bool
ui_graph_bar(float length, char *text, Bitmap *bitmap)
{
    Assert(length >= 0.0f && length <= 1.0f);
    Assert(ui_context.has_layout);
    
    UI_Layout *layout = &ui_context.layout;
    Font *font = ui_context.font;
    Bitmap *buffer = ui_context.buffer;
    
    
    int right_pad = 100;
    int bar_height = 40;
    int x = layout->x;
    int h = bar_height;
    int w = (int)((layout->w - right_pad) * length);
    int y = layout->y + layout->spacing + (layout->index * (h + layout->spacing));
    
    if (y >= layout->y + layout->h) return false;
    
    y = clamp(y, layout->y, layout->y + layout->h-1);
    h = std::min(h, (layout->y + layout->h-1) - y);
    
    layout->index += 1;
    
    UI_Id id = layout->start_id++;
    
    
    
    bool pressed = do_item(id, x, y, w, h);
    
    if (is_hot(id))
    {
        draw_rectangle(buffer, x, y, w, h, RGB_OPAQUE(190, 20, 75));
    }
    else
    {
        draw_rectangle(buffer, x, y, w, h, RGB_NORMAL(0.8, 0.1, 0.3));
    }
    if (is_active(id))
    {
        draw_rect_outline(buffer, x, y, w, h, RGB_OPAQUE(255, 255, 255));
        draw_rect_outline(buffer, x-1, y-1, w+2, h+2, RGB_OPAQUE(0, 0, 0));
        draw_rect_outline(buffer, x+1, y+1, w-2, h-2, RGB_OPAQUE(0, 0, 0));
    }
    else
    {
        draw_rect_outline(buffer, x, y, w, h, RGB_OPAQUE(0, 0, 0));
    }
    
    
    int centre_y = y + (int)(h*0.75f);
    
    int baseline_x = x + w + 10;
    int baseline_y = centre_y;
    
    if (text) draw_text(buffer, font, text, baseline_x, baseline_y, GREY(0));
    
    if (bitmap)
    {
        int bitmap_x = x - (bitmap->width);
        int bitmap_y = centre_y - (bitmap->height/2) + 3;
        draw_bitmap(buffer, bitmap, x-50, y);
    }
    
    return pressed;
}


void
ui_set_mouse_button_state(int x, int y, Mouse_Button_State button_state)
{
    ui_context.mouse_x = x;
    ui_context.mouse_y = y;
    if (button_state == Mouse_Left_Up)         ui_context.mouse_left_up    = true;
    else if (button_state == Mouse_Left_Down)  ui_context.mouse_left_down  = true;
    else if (button_state == Mouse_Right_Up)   ui_context.mouse_right_up   = true;
    else if (button_state == Mouse_Right_Down) ui_context.mouse_right_down = true;
    else if (button_state == Mouse_Move)
    {
        // Do nothing
    }
}

void
ui_set_mouse_wheel_state(int x, int y, int delta)
{
    ui_context.mouse_x = x;
    ui_context.mouse_y = y;
    
    // TODO: Make sure this is a good number
    ui_context.mouse_wheel = delta;
}

void
ui_set_visibility_changed(Window_Visibility visibility)
{
    if (visibility == Window_Hidden) ui_context.ui_hidden = true;
    else if (visibility == Window_Shown) ui_context.ui_shown = true;
}
