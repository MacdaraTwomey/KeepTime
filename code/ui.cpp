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
ui_start()
{
    // It seems we allow ui_context input to continue to be set while we are using it.
    // I think this is fine, and input events might not even bypass GetMessage anyway
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
}

UI_Id
get_id(char *str)
{
    u32 id = djb2((unsigned char *)str);
    return id;
}

// maybe this is a good way?
#define ui_button(text) ui_button_call_(text, __COUNTER__)

void
ui_button_call_(char *text, int id)
{
    // Calculate w/h position from layout rules
    
    // Get widget's id
    // UI_Id id = get_id(text);
    
    
    // Draw the button
    
    // Layout is updated
    
    // Update UI_Context (compares mouse and button with button pos/dimensions to see if becomes hot)
    // use
}