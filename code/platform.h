#ifndef PLATFORM_H
#define PLATFORM_H

static constexpr u32 PLATFORM_MAX_PATH_LEN = 2000; // This must not too big, too small for all platforms
static constexpr u32 PLATFORM_MAX_URL_LEN  = 2000;

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

u32 platform_get_monitor_refresh_rate(); // using SDL functions

void platform_get_base_directory(char *directory, s32 capacity);

Platform_Window platform_get_active_window();

bool platform_get_program_from_window(Platform_Window window, char *buf, size_t *length);

bool platform_firefox_get_url(Platform_Window window, char *buf, size_t *length);

bool platform_get_default_icon(u32 desired_size, Bitmap *bitmap);

bool platform_get_icon_from_executable(char *path, u32 desired_size, Bitmap *bitmap, u32 *allocated_bitmap_memory);

void platform_set_sleep_time(u32 milliseconds);


struct Platform_Entire_File
{
    u8 *data;
    size_t size;
};

bool platform_file_exists(char *path);
s64 platform_get_file_size(FILE *file);
Platform_Entire_File platform_read_entire_file(char *file_name);


#endif //PLATFORM_H
