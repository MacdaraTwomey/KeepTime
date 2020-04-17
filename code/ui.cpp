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
    
    ui_context.current_graph_scroll = 0;
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
in_item_rect(Rect rect)
{
    return (ui_context.mouse_x >= rect.pos.x && ui_context.mouse_x <= rect.pos.x + rect.dim.x &&
            ui_context.mouse_y >= rect.pos.y && ui_context.mouse_y <= rect.pos.y + rect.dim.y);
}

bool
do_item(UI_Id id, Rect item_rect)
{
    // Could instead pass in a bool, that says in rect or not
    
    // Doesn't handle click and hold outside box, then move into box and release, properly
    // Doesn't handle release and click in one update loop
    // Does handle click and release in one update loop
    
    bool result = false;
    
    if (in_item_rect(item_rect)) maybe_set_hot(id);
    
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
ui_button_row_begin(int x, int y)
{
    UI_Layout layout = {};
    int ypad = 3;
    int button_width = 100;
    int font_height = ui_context.font->max_ascent + ui_context.font->max_descent;
    layout.rect = {x, y, button_width, font_height};
    layout.spacing = 10;
    layout.index = 0;
    ui_context.layout = layout;
    ui_context.has_layout = true;
}


void
ui_button_row_end()
{
    ui_context.has_layout = false;
}

bool
ui_button(char *text)
{
    Assert(ui_context.has_layout);
    
    UI_Layout *layout = &ui_context.layout;
    Font *font = ui_context.font;
    Bitmap *buffer = ui_context.buffer;
    
    int xpad = 0; // make these mode dynamic, for different text sizes
    int ypad = 0;
    
    Rect rect = layout->rect;
    rect.pos.x += layout->index * (layout->rect.dim.x + layout->spacing);
    layout->index += 1;
    
    UI_Id id = get_id(text);
    
    bool pressed = do_item(id, rect);
    
    draw_rectangle(buffer, rect, GREY(190));
    
    u32 colour = RGB_OPAQUE(0, 0, 0);
    if (is_hot(id)) colour = RGB_OPAQUE(255, 0, 0);
    if (is_active(id))
    {
        draw_rect_outline(buffer, rect, RGB_OPAQUE(255, 255, 255));
        draw_rect_outline(buffer, grow(rect, 1), RGB_OPAQUE(0, 0, 0));
        draw_rect_outline(buffer, shrink(rect, 1), RGB_OPAQUE(0, 0, 0));
    }
    else
    {
        draw_rect_outline(buffer, rect, RGB_OPAQUE(0, 0, 0));
    }
    
    if (is_selected(id)) colour = RGB_OPAQUE(0, 0, 255);
    
    int centre_x = rect.pos.x + (rect.dim.x/2);
    int centre_y = rect.pos.y + (int)(rect.dim.y*0.75f);
    
    int text_w = text_width(text, font);
    int baseline_x = centre_x - text_w/2;
    int baseline_y = centre_y;
    draw_text(buffer, font, text, baseline_x, baseline_y, colour);
    
    return pressed;
}

int bar_height = 60;
int pixels_per_scroll = 1;

void
ui_graph_begin(int x, int y, int w, int h, int bar_count, UI_Id id)
{
    UI_Layout layout = {};
    // Width and height of whole barplot
    layout.rect = {x, y, w, h};
    layout.spacing = 10;
    layout.index = 0;
    layout.start_id = id;
    layout.first_visible_bar = 0;
    layout.visible_bar_count = bar_count;
    
    draw_rectangle(ui_context.buffer, layout.rect, GREY(200));
    //draw_rect_outline(ui_context.buffer, layout.rect, GREY(0));
    
    // TODO: If on scroll max last frame, keep on scroll max, even if bar count increases
    
    int body_height = bar_count * (bar_height + layout.spacing);
    if (body_height > layout.rect.dim.y)
    {
        int scroll_bar_w = 10;
        int scroll_bar_pad = 10;
        int scroll_bar_h = h - (2*scroll_bar_pad);
        int scroll_bar_x = x + w - scroll_bar_pad - scroll_bar_w;
        int scroll_bar_y = y + scroll_bar_pad;
        
        draw_rectangle(ui_context.buffer, scroll_bar_x, scroll_bar_y, scroll_bar_w, scroll_bar_h, NICE_GREEN);
        
        float inscreen = (float)layout.rect.dim.y;
        float offscreen = (float)body_height;
        
        // 5 scrolls
        // 10p per scroll
        // 2 is max scroll
        // cur scroll is 2
        // | - | - | o | o | o |
        
        int slider_h   = (int)roundf((inscreen / offscreen) * scroll_bar_h);
        
        // TODO: for now all this current scroll is in pixels along the scroll bar, which will become wrong is
        // the scrolbar is resized (e.g. when window resized).
        int max_scroll = scroll_bar_h - slider_h;
        // for now invert this
        ui_context.current_graph_scroll += (int)(-ui_context.mouse_wheel) * pixels_per_scroll;
        
        ui_context.current_graph_scroll = clamp(ui_context.current_graph_scroll, 0, max_scroll);
        
        // For user experience, don't allow annoying small last part of scrolling needed to reach bottom/top
        if (ui_context.mouse_wheel < 0 /* scrolling down */ &&
            max_scroll - ui_context.current_graph_scroll < 10)
        {
            ui_context.current_graph_scroll = max_scroll;
        }
        else if (ui_context.mouse_wheel > 0 /* scrolling up */ &&
                 ui_context.current_graph_scroll < 10)
        {
            ui_context.current_graph_scroll = 0;
        }
        
        int slider_x   = scroll_bar_x;
        int slider_y   = scroll_bar_y + ui_context.current_graph_scroll;
        int slider_w   = scroll_bar_w;
        
        draw_rectangle(ui_context.buffer, slider_x, slider_y, slider_w, slider_h, NICE_RED);
        
        // 20p / 5+2 = 2.8 (2  at 1.d.p)
        // 20p % 5+2 = 6p
        layout.first_visible_bar = ui_context.current_graph_scroll / (bar_height + layout.spacing);
        int first_visible_bar_clipped_pixels = ui_context.current_graph_scroll % (bar_height + layout.spacing);
        
        // Graph goes bar, spacing, bar, spacing ...
        // If only the spacing is visible don't count the bar
        int remaining_visible_pixels = layout.rect.dim.y;
        if (first_visible_bar_clipped_pixels >= bar_height)
        {
            // First real visible bar is not clipped, only an offscreen one's spacing is
            layout.first_visible_bar += 1;
            // So remove this fully visible bar's pixels from tally
            int first_visible_bar_visible_pixels = (bar_height + layout.spacing);
            remaining_visible_pixels -= first_visible_bar_visible_pixels;
        }
        else
        {
            // Remove clipped first bars pixels from tally
            int first_visible_bar_visible_pixels = (bar_height + layout.spacing) - first_visible_bar_clipped_pixels;
            remaining_visible_pixels -= first_visible_bar_visible_pixels;
        }
        
        // Assumes first bar is visible
        int visible_bars = 1 + remaining_visible_pixels / (bar_height + layout.spacing);
        if (remaining_visible_pixels % (bar_height + layout.spacing) != 0)
        {
            // Partially visible last bar
            visible_bars += 1;
        }
        
        layout.visible_bar_count = visible_bars;
    }
    
    
    ui_context.layout = layout;
    ui_context.has_layout = true;
}

void ui_graph_end()
{
    ui_context.has_layout = false;
}

void
clip_rect(Rect a, Rect *b)
{
    if (b->pos.x < a.pos.x) {
        b->dim.x -= a.pos.x - b->pos.x;
        b->pos.x = a.pos.x;
    }
    if (b->pos.x + b->dim.x > a.pos.x + a.dim.x) b->dim.x = a.pos.x + a.dim.x - b->pos.x;
    if (b->pos.y < a.pos.y)
    {
        b->dim.y -= a.pos.y - b->pos.y;
        b->pos.y = a.pos.y;
    }
    if (b->pos.y + b->dim.y > a.pos.y + a.dim.y) b->dim.y = a.pos.y + a.dim.y - b->pos.y;
}

bool
ui_graph_bar(float length, char *name, char *time_text, Bitmap *bitmap)
{
    Assert(length >= 0.0f && length <= 1.0f);
    Assert(ui_context.has_layout);
    
    UI_Layout *layout = &ui_context.layout;
    Font *font = ui_context.font;
    Bitmap *buffer = ui_context.buffer;
    
    if (layout->index < layout->first_visible_bar ||
        layout->index > (layout->first_visible_bar + layout->visible_bar_count) - 1)
    {
        layout->index += 1;
        return false;
    }
    
    // rect in is the pixel space of the whole extended visible and hidden portions of bar plot
    int right_pad = 100;
    int bar_len = (int)((layout->rect.dim.x - right_pad) * length);
    Rect bar = {
        0,
        layout->index * (bar_height + layout->spacing),
        bar_len,
        bar_height,
    };
    
    bar.pos += layout->rect.pos;
    
    Rect outline = bar;
    
    // Move layout into the 'body' pixel space to get visible rect then move back again into window space.
    Rect visible_rect = layout->rect;
    visible_rect.pos.y += ui_context.current_graph_scroll;
    
    clip_rect(visible_rect, &bar);
    bar.pos.y -= ui_context.current_graph_scroll;
    
    UI_Id id = layout->start_id++;
    
    bool pressed = do_item(id, bar);
    
    if (is_hot(id))
    {
        draw_rectangle(buffer, bar, RGB_OPAQUE(190, 20, 75));
    }
    else
    {
        draw_rectangle(buffer, bar, RGB_NORMAL(0.8, 0.1, 0.3));
    }
    if (is_active(id))
    {
        draw_rect_outline(buffer, bar, RGB_OPAQUE(255, 255, 255));
        draw_rect_outline(buffer, shrink(bar, 1), RGB_OPAQUE(0, 0, 0));
        draw_rect_outline(buffer, grow(bar, 1), RGB_OPAQUE(0, 0, 0));
    }
    else
    {
        // TODO: We also want to clip outlines if part of a bar goes offscreen, but this is not easy with current way of doing things
        draw_rect_outline(buffer, bar, RGB_OPAQUE(0, 0, 0));
    }
    
    
#if 1
    int centre_y = bar.pos.y + (int)(bar.dim.y*0.75f);
    
    int baseline_x = bar.pos.x + 10;
    int baseline_y = centre_y;
    
    if (name)
    {
        int max_width = layout->rect.dim.x - 10 - 100;
        draw_text(buffer, font, name, baseline_x, baseline_y, GREY(0), max_width);
        baseline_x += max_width + 20;
    }
    if (time_text)
    {
        draw_text(buffer, font, time_text, baseline_x, baseline_y, GREY(0));
    }
    
    if (bitmap)
    {
        int bitmap_x = bar.pos.x - (bitmap->width);
        int bitmap_y = centre_y - (bitmap->height/2) + 3;
        draw_bitmap(buffer, bitmap, bar.pos.x-50, bar.pos.y);
    }
#endif
    
    layout->index += 1;
    
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
    
    //printf("scroll %i\n", delta);
    
    // On windows
    // The high-order word indicates the distance the wheel is rotated, expressed in multiples or divisions of WHEEL_DELTA, which is 120.
    // Positive value indicates wheel rotated forward, away from user
    // Negative value indicates wheel rotated backward, toward from user
    
    //The wheel rotation will be a multiple of WHEEL_DELTA, which is set at 120. This is the threshold for action to be taken, and one such action (for example, scrolling one increment) should occur for each delta.
    
    // I observed delta or -120, +120, +240 etc on my computer
    // NOTE: This may mean that +60, or even +30 is possible with a free wheeling mouse, and you are supposed
    // to add it?
    
    // We add all wheel deltas
    ui_context.mouse_wheel += (float)delta / 120.0f;
}

void
ui_set_visibility_changed(Window_Visibility visibility)
{
    if (visibility == Window_Hidden) ui_context.ui_hidden = true;
    else if (visibility == Window_Shown) ui_context.ui_shown = true;
}
