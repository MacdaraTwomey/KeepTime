#ifndef PLATFORM_H
#define PLATFORM_H

static constexpr i32 PLATFORM_MAX_PATH_LEN = 2000;
static constexpr i32 PLATFORM_MAX_URL_LEN  = 2000;

struct Platform_Window
{
    HWND handle;
    bool is_valid;
};

// TODO: Maybe platform should just get active window in the platform_get_program_from_window call
//       and hold on to it for use in subsequent call to platform_get_firefox_url.

bool platform_get_program_from_window(Platform_Window window, char *buf, size_t *length);

bool platform_get_firefox_url(Platform_Window window, char *buf, size_t *length);

Platform_Window platform_get_active_window();


#endif //PLATFORM_H
