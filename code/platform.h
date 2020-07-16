#ifndef PLATFORM_H
#define PLATFORM_H

#include "graphics.h"

static constexpr i32 PLATFORM_MAX_PATH_LEN = 2000;
static constexpr i32 PLATFORM_MAX_URL_LEN  = 2000;

enum Window_Status
{
    Window_Just_Visible = 1,
    Window_Just_Hidden = 2,
    Window_Visible = 4,
    Window_Hidden = 8,
};

struct Platform_Window
{
    HWND handle;
    bool is_valid;
};

// TODO: Maybe platform should just get active window in the platform_get_program_from_window call
//       and hold on to it for use in subsequent call to platform_get_firefox_url.

// Ways to handle app getting the urls and path names it needs are:
//   1. app calls platform_get_active_window() and get_names()
//   2. app just calls get_names() but then platform has to do work of seeing if browser is part of the program path etc.
//   3. Pass Poll_Window_Result with all necessary names/urls to app on avery update, only cantain valid names
//      when timer has elapsed (similar to previous just app doesn't call just recieves when it needs it).

Platform_Window platform_get_active_window();

bool platform_get_program_from_window(Platform_Window window, char *buf, size_t *length);

bool platform_get_firefox_url(Platform_Window window, char *buf, size_t *length);

bool platform_get_icon_from_executable(char *path, u32 desired_size, 
                                       i32 *width, i32 *height, i32 *pitch, u32 **pixels, 
                                       bool load_default_on_failure);

// Temporary: only here so app can load ms icons
bool win32_get_bitmap_data_from_HICON(HICON icon, i32 *width, i32 *height, i32 *pitch, u32 **pixels);

#endif //PLATFORM_H
