#ifndef UI_H
#define UI_H

#include "graphics.h"

typedef u32 UI_Id;

struct UI_Layout
{
    Rect rect;
    int spacing;
    int index;
    
    UI_Id start_id;
    
    int first_visible_bar;
    int visible_bar_count;
};

struct UI_Context
{
    // These can be set before struct it even initialised
    int mouse_x;
    int mouse_y;
    
    bool mouse_left_down;
    bool mouse_left_up;
    bool mouse_right_down;
    bool mouse_right_up;
    
    float mouse_wheel;
    
    bool ui_hidden;
    bool ui_shown;
    
    // Not sure when these should be set
    int width;
    int height;
    
    // These used after initialisation
    UI_Id hot;
    UI_Id active;
    UI_Id selected;
    
    UI_Layout layout;
    bool has_layout;
    
    Font *font;
    Bitmap *buffer;
    
    //temp
    int current_graph_scroll;
};

enum Mouse_Button_State
{
    Mouse_Move,
    Mouse_Left_Up,
    Mouse_Left_Down,
    Mouse_Right_Up,
    Mouse_Right_Down,
};

enum Window_Visibility
{
    Window_Hidden,
    Window_Shown,
};


#endif //GUI_H
