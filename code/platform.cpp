

// ---------------------------------------------------------------------------------------
// Called by the platform layer

bool platform_init_context(SDL_Window* window)
{
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version); /* initialize info structure with SDL version info */
    if(!SDL_GetWindowWMInfo(window, &info)) 
    {
        return false;
    }
    
    HWND hwnd = info.info.win.window;
    if (!win32_init_context(hwnd))
    {
        return false;
    }
    
    return true;
}

void platform_free_context()
{
    win32_free_context();
}

void platform_handle_message(SDL_SysWMmsg *sys_msg) //UINT msg, LPARAM lParam, WPARAM wParam)
{
    win32_handle_message(sys_msg->msg.win.msg, sys_msg->msg.win.lParam, sys_msg->msg.win.wParam);
}

void platform_wait_for_event_with_timeout()
{
    win32_wait_for_event_with_timeout();
}

s64 platform_get_time()
{
    return win32_get_time();
}

s64 platform_get_microseconds_elapsed(s64 start, s64 end)
{
    return win32_get_microseconds_elapsed(start, end);
}

// ---------------------------------------------------------------------------------------
// Called by the app

u32
platform_get_monitor_refresh_rate()
{
    // or if 0 is not correct, could pass in sdl_window and get index with SDL_GetWindowDisplayIndex
    SDL_DisplayMode display_mode;
    int result = SDL_GetCurrentDisplayMode(0, &display_mode);
    if (result == 0)
    {
        return display_mode.refresh_rate;
    }
    else
    {
        return 16; // 60fps default
    }
}

void
platform_get_base_directory(char *directory, s32 capacity)
{
    char *exe_dir_path = SDL_GetBasePath();
    if (exe_dir_path) 
    {
        strncpy(directory, exe_dir_path, capacity);
        SDL_free(exe_dir_path);
    }
}

void platform_set_sleep_time(u32 milliseconds)
{
    win32_set_sleep_time(milliseconds);
}

Platform_Window platform_get_active_window()
{
    HWND hwnd = win32_get_active_window();
    
    Platform_Window window = {};
    window.handle = hwnd;
    window.is_valid = (hwnd != 0);
    
    return window;
}

bool platform_get_program_from_window(Platform_Window window, char *buf, size_t *length)
{
    return win32_get_program_from_window(window.handle, PLATFORM_MAX_PATH_LEN, buf, length);
}

bool platform_firefox_get_url(Platform_Window window, char *buf, size_t *length)
{
    return win32_get_firefox_url(window.handle, PLATFORM_MAX_URL_LEN, buf, length);
}

bool platform_get_default_icon(u32 desired_size, Bitmap *bitmap)
{
    return win32_get_default_icon(desired_size, bitmap);
}

bool platform_get_icon_from_executable(char *path, u32 desired_size, Bitmap *bitmap, u32 *allocated_bitmap_memory)
{
    return win32_get_icon_from_executable(path, desired_size, bitmap, allocated_bitmap_memory);
}

bool platform_show_other_instance_if_already_running(char *program_name)
{
    return win32_show_other_instance_if_already_running(program_name);
}

bool platform_file_exists(char *path)
{
    FILE *file = fopen(path, "rb");
    if (file)
    {
        fclose(file);
        return true;
    }
    
    return false;
}

// TODO: Wrap FILE handle
// TODO: Debug doesn't check if file exists etc
s64 
platform_get_file_size(FILE *file)  
{
    // Bad because messes with seek position
    // TODO: Pretty sure it is bad to seek to end of file
    fseek(file, 0, SEEK_END); // Not portable. TODO: Use os specific calls
    long int size = ftell(file);
    fseek(file, 0, SEEK_SET); // Not portable. TODO: Use os specific calls
    return (s64) size;
}

Platform_Entire_File
platform_read_entire_file(char *file_name)
{
    Platform_Entire_File result = {};
    
    FILE *file = fopen(file_name, "rb");
    if (file)
    {
        s64 file_size = platform_get_file_size(file);
        if (file_size > 0)
        {
            u8 *data = (u8 *)xalloc(file_size);
            if (data)
            {
                if (fread(data, 1, file_size, file) == file_size)
                {
                    result.data = data;
                    result.size = file_size;
                }
                else
                {
                    free(data);
                }
            }
        }
        
        // I don't think it matters if this fails as we already have the file data
        fclose(file);
    }
    
    return result;
}


// TODO: wrap ftell, fwrite, fseek, fwrite 