#ifndef UI_H
#define UI_H

typedef u32 UI_Id;

struct UI_Context
{
    int mouse_x;
    int mouse_y;
    
    bool mouse_left_down;
    bool mouse_left_up;
    bool mouse_right_down;
    bool mouse_right_up;
    
    bool timer_elapsed;
    
    int mouse_wheel;
    
    bool hide_ui;
    bool show_ui;
    
    int width;
    int height;
    
    
    
    UI_Id hot_id
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
