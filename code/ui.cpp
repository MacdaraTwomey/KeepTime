#include "ui.h"
#include "graphics.h"

UI_Context ui_context; // should this be in monitor?

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

UI_Id
get_id(char *str)
{
    // Need better way to avoid collisions
    u32 id = djb2((unsigned char *)str);
    if (id == 0) id += 1;
    return id;
}

bool
is_active(UI_Id id)
{
    return ui_context.active == id;
}

bool
is_hot(UI_Id id)
{
    return ui_context.hot == id;
}

bool
is_selected(UI_Id id)
{
    return ui_context.selected == id;
}

void
set_active(UI_Id id)
{
    ui_context.active = id;
}

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
    
    draw_rectangle(buffer, x, y, w, h, RGB_OPAQUE(180, 180, 180));
    
    int centre_x = x + (w/2);
    int centre_y = y + (int)(h*0.75f);
    
    int baseline_x = centre_x - text_w/2;
    int baseline_y = centre_y;
    
    u32 colour = RGB_OPAQUE(0, 0, 0);
    if (is_hot(id)) colour = RGB_OPAQUE(255, 0, 0);
    if (is_active(id))
    {
        colour = RGB_OPAQUE(0, 255, 0);
        draw_rect_outline(buffer, x, y, w, h, RGB_OPAQUE(0, 0, 0));
    }
    if (is_selected(id)) colour = RGB_OPAQUE(0, 0, 255);
    draw_text(buffer, font, text, baseline_x, baseline_y, colour);
    
    return pressed;
    
    // Get widget's id
    // UI_Id id = get_id(text);
    
    // Draw the button
    
    // Layout is updated
    
    // Update UI_Context (compares mouse and button with button pos/dimensions to see if becomes hot)
    // use
}