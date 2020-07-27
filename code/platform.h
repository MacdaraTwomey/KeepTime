#ifndef PLATFORM_H
#define PLATFORM_H

#include "graphics.h"

static constexpr i32 PLATFORM_MAX_PATH_LEN = 2000;
static constexpr i32 PLATFORM_MAX_URL_LEN  = 2000;

enum Window_Event
{
    Window_No_Change = 0,
    Window_Shown = 1,
    Window_Hidden = 2,
    Window_Closed = 4,
};

struct Platform_Window
{
    HWND handle;
    bool is_valid;
};

u32 platform_SDL_get_monitor_refresh_rate();

Platform_Window platform_get_active_window();

bool platform_get_program_from_window(Platform_Window window, char *buf, size_t *length);

bool platform_get_firefox_url(Platform_Window window, char *buf, size_t *length);

bool platform_get_default_icon(u32 desired_size, i32 *width, i32 *height, i32 *pitch, u32 **pixels);

bool platform_get_icon_from_executable(char *path, u32 desired_size, 
                                       i32 *width, i32 *height, i32 *pitch, u32 **pixels, 
                                       bool load_default_on_failure);

#endif //PLATFORM_H
