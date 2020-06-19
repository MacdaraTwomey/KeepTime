
HANDLE
win32_create_console()
{
    AllocConsole();
    
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONIN", "r", stdin);
    
    HANDLE con_out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD out_mode;
    GetConsoleMode(con_out, &out_mode);
    SetConsoleMode(con_out, out_mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    
    return GetStdHandle(STD_INPUT_HANDLE);
}

Platform_Window
platform_get_active_window()
{
    // TODO: Use GetShellWindow GetShellWindow to detect when not doing anything on desktop, if foreground == desktop etc
    
#ifdef CYCLE
    
    static HWND queue[100];
    static int count = 0;
    static bool first = true;
    static bool done = false;
    
    auto get_windows = [](int &count, HWND *queue) {
        for (HWND hwnd = GetTopWindow(NULL); hwnd != NULL && count < 100; hwnd = GetNextWindow(hwnd, GW_HWNDNEXT))
        {
            if (IsWindowVisible(hwnd))
            {
                queue[count++] = hwnd;
            }
        }
    };
    
    if (first)
    {
        get_windows(count, queue);
        first = false;
    }
    
    Platform_Window window = {};
    if (count > 0)
    {
        HWND foreground_win = queue[count-1];
        window = {};
        window.handle = foreground_win;
        window.is_valid = (foreground_win != 0);
        
        count -= 1;
    }
    else
    {
        done = true;
    }
    
    if (done)
    {
        HWND foreground_win = GetForegroundWindow();
        window = {};
        window.handle = foreground_win;
        window.is_valid = (foreground_win != 0);
    }
#else
    
    HWND foreground_win = GetForegroundWindow();
    Platform_Window window = {};
    window.handle = foreground_win;
    window.is_valid = (foreground_win != 0);
#endif
    
    
    return window;
}

BOOL CALLBACK
MyEnumChildWindowsCallback(HWND hwnd, LPARAM lParam)
{
    // From // https://stackoverflow.com/questions/32360149/name-of-process-for-active-window-in-windows-8-10
    Process_Ids *process_ids = (Process_Ids *)lParam;
    DWORD pid  = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != process_ids->parent)
    {
        process_ids->child = pid;
    }
    
    return TRUE; // Continue enumeration
}

bool
platform_get_program_from_window(Platform_Window window, char *buf, size_t *length)
{
    // On success fills buf will null terminated string, and sets length to string length (not including null terminator)
    HWND handle = window.handle;
    Process_Ids process_ids = {0, 0};
    GetWindowThreadProcessId(handle, &process_ids.parent);
    process_ids.child = process_ids.parent; // In case it is a normal process
    
    // Modern universal windows apps in 8 and 10 have the process we want inside a parent process called
    // WWAHost.exe on Windows 8 and ApplicationFrameHost.exe on Windows 10.
    // So we search for a child pid that is different to its parent's pid, to get the process we want.
    // https://stackoverflow.com/questions/32360149/name-of-process-for-active-window-in-windows-8-10
    
    EnumChildWindows(window.handle, MyEnumChildWindowsCallback, (LPARAM)&process_ids);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_ids.child);
    if (process)
    {
        // filename_len is overwritten with number of characters written to full_path
        // Null terminates
        DWORD filename_len = (DWORD)PLATFORM_MAX_PATH_LEN-1;
        if (QueryFullProcessImageNameA(process, 0, buf, &filename_len))
        {
            CloseHandle(process);
            *length = filename_len;
            return true;
        }
        else
        {
            tprint("Couldn't get executable path");
        }
        
        CloseHandle(process);
    }
    
    *length = 0;
    return false;
}

bool
win32_BSTR_to_utf8(BSTR url, char *buf, int buf_size, size_t *length)
{
    bool result = false;
    size_t len = SysStringLen(url);
    if (len > 0)
    {
        int converted_len = WideCharToMultiByte(CP_UTF8, 0,
                                                url, len+1, // +1 to include null terminator
                                                buf, buf_size, NULL, NULL);
        if (converted_len == len+1)
        {
            *length = converted_len;
            result = true;
        }
    }
    else
    {
        // Url bar was just empty
        *length = 0;
        result = true;
    }
    
    return result;
}

IUIAutomationElement *
win32_firefox_get_url_bar_editbox(HWND window)
{
    IUIAutomationElement *editbox = nullptr;
    
    IUIAutomation *uia = nullptr;
    HRESULT err = CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_IUIAutomation,
                                   (void **)(&uia));
    if (err != S_OK || !uia) goto CLEANUP;
    
    IUIAutomationElement *firefox_window = nullptr;
    err = uia->ElementFromHandle(window, &firefox_window);
    if (err != S_OK || !firefox_window) goto CLEANUP;
    
    VARIANT navigation_name;
    navigation_name.vt = VT_BSTR;
    navigation_name.bstrVal = SysAllocString(L"Navigation");
    if (navigation_name.bstrVal == NULL) goto CLEANUP;
    
    IUIAutomationCondition *is_navigation = nullptr;
    IUIAutomationCondition *is_toolbar = nullptr;
    IUIAutomationCondition *and_cond = nullptr;
    IUIAutomationCondition *is_combobox = nullptr;
    IUIAutomationCondition *is_editbox = nullptr;
    uia->CreatePropertyCondition(UIA_NamePropertyId, navigation_name, &is_navigation);
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId, CComVariant(UIA_ToolBarControlTypeId), &is_toolbar);
    uia->CreateAndCondition(is_navigation, is_toolbar, &and_cond);
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId, CComVariant(UIA_ComboBoxControlTypeId), &is_combobox);
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId, CComVariant(UIA_EditControlTypeId), &is_editbox);
    
    // exception?
    IUIAutomationElement *navigation_toolbar = nullptr;
    err = firefox_window->FindFirst(TreeScope_Children, and_cond, &navigation_toolbar);
    if (err != S_OK || !navigation_toolbar) goto CLEANUP;
    
    IUIAutomationElement *combobox = nullptr;
    err = navigation_toolbar->FindFirst(TreeScope_Children, is_combobox, &combobox);
    if (err != S_OK || !combobox) goto CLEANUP;
    
    err = combobox->FindFirst(TreeScope_Descendants, is_editbox, &editbox);
    if (err != S_OK) editbox = nullptr;
    
    CLEANUP:
    
    if (combobox) combobox->Release();
    if (navigation_toolbar) navigation_toolbar->Release();
    if (is_editbox) is_editbox->Release();
    if (is_combobox) is_combobox->Release();
    if (and_cond) and_cond->Release();
    if (is_toolbar) is_toolbar->Release();
    if (is_navigation) is_navigation->Release();
    if (navigation_name.bstrVal) VariantClear(&navigation_name);
    if (firefox_window) firefox_window->Release();
    if (uia) uia->Release();
    
    return editbox;
}

bool
platform_get_firefox_url(Platform_Window window, char *buf, int buf_size, size_t *length)
{
    // To get the URL of the active tab in Firefox Window:
    // Mozilla firefox window          ControlType: UIA_WindowControlTypeId (0xC370) LocalizedControlType: "window"
    //   "Navigation" tool bar         UIA_ToolBarControlTypeId (0xC365) LocalizedControlType: "tool bar"
    //     "" combobox                 UIA_ComboBoxControlTypeId (0xC353) LocalizedControlType: "combo box"
    //       "Search with Google or enter address"   UIA_EditControlTypeId (0xC354) LocalizedControlType: "edit"
    //        which has Value.Value = URL
    
    // TODO: Does this handle multiple firefox windows correctly??
    
    HWND handle = window.handle;
    bool success = false;
    *length = 0;
    
    // NOTE: window will always be the up to date firefox window, even if our editbox pointer is old.
    // NOTE: Even when everything else is released it seems that you can still use editbox pointer, unless the window is closed. Documentation does not confirm this, so i'm not 100% sure it's safe.
    
    // NOTE: This is duplicated because the editbox pointer can be valid, but just stale, and we only know that if GetCurrentPropertyValue fails, in which case we free the pointer and try to reacquire the editbox.
    static IUIAutomationElement *editbox = nullptr;
    if (editbox)
    {
        // If we have a editbox pointer we can assume its the same window (likely) and try to get url bar
        VARIANT url_bar = {};
        HRESULT err = editbox->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &url_bar);
        if (err == S_OK)
        {
            if (url_bar.bstrVal)
            {
                success = win32_BSTR_to_utf8(url_bar.bstrVal, buf, buf_size, length);
                VariantClear(&url_bar);
            }
            
            return success;
        }
        else
        {
            // The editbox pointer probably isn't valid anymore
            editbox->Release();
            editbox = nullptr;
        }
    }
    
    editbox = win32_firefox_get_url_bar_editbox(handle);
    if (editbox)
    {
        VARIANT url_bar = {};
        HRESULT err = editbox->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &url_bar);
        if (err == S_OK)
        {
            if (url_bar.bstrVal)
            {
                success = win32_BSTR_to_utf8(url_bar.bstrVal, buf, buf_size, length);
                VariantClear(&url_bar);
            }
            
            return success;
        }
        else
        {
            editbox->Release();
            editbox = nullptr;
        }
    }
    
    return success;
}


bool
win32_get_bitmap_from_HICON(HICON icon, Bitmap *bitmap)
{
    bool success = false;
    *bitmap = {};
    
    // TODO: Get icon from UWP:
    // Process is wrapped in a parent process, and can't extract icons from the child, not sure about parent
    // SO might need to use SHLoadIndirectString, GetPackageInfo could be helpful.
    
    //GetIconInfoEx creates bitmaps for the hbmMask and hbmCol or members of ICONINFOEX. The calling application must manage these bitmaps and delete them when they are no longer necessary.
    
    // These bitmaps are stored as bottom up bitmaps, so the bottom row of image is stored in the first 'row' in memory.
    ICONINFO ii;
    BITMAP colour_mask;
    BITMAP and_mask;
    HBITMAP and_mask_handle = 0;
    HBITMAP colours_handle = 0;
    
    if (GetIconInfo(icon, &ii))
    {
        // It is necessary to call DestroyObject for icons and cursors created with CopyImage
        colours_handle = (HBITMAP)CopyImage(ii.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        and_mask_handle = (HBITMAP)CopyImage(ii.hbmMask, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        
        // When this is done without CopyImage the bitmaps don't have any .bits memory allocated
        //int result_1 = GetObject(ii.hbmColor, sizeof(BITMAP), &bitmap);
        //int result_2 = GetObject(ii.hbmMask, sizeof(BITMAP), &mask);
        if (colours_handle && and_mask_handle)
        {
            int result_1 = GetObject(colours_handle, sizeof(BITMAP), &colour_mask) == sizeof(BITMAP);
            int result_2 = GetObject(and_mask_handle, sizeof(BITMAP), &and_mask) == sizeof(BITMAP);
            if (result_1 && result_2)
            {
                init_bitmap(bitmap, colour_mask.bmWidth, colour_mask.bmHeight);
                success = true;
                
                bool32 had_alpha = false;
                u32 *dest = bitmap->pixels;
                u8 *src_row = (u8 *)colour_mask.bmBits + (colour_mask.bmWidthBytes * (colour_mask.bmHeight-1));
                for (int y = 0; y < colour_mask.bmHeight; ++y)
                {
                    u32 *src = (u32 *)src_row;
                    for (int x = 0; x < colour_mask.bmWidth; ++x)
                    {
                        u32 col = *src++;
                        *dest++ = col;
                        had_alpha |= A_COMP(col);
                    }
                    
                    src_row -= colour_mask.bmWidthBytes;
                }
                
                // Some icons have no set alpha channel in the colour mask, so specify it in the and mask.
                if (!had_alpha)
                {
                    int and_mask_pitch = and_mask.bmWidthBytes;
                    u32 *dest = bitmap->pixels;
                    u8 *src_row = (u8 *)and_mask.bmBits + (and_mask.bmWidthBytes * (and_mask.bmHeight-1));
                    for (int y = 0; y < and_mask.bmHeight; ++y)
                    {
                        u8 *src = src_row;
                        for (int x = 0; x < and_mask_pitch; ++x)
                        {
                            for (int i = 7; i >= 0; --i)
                            {
                                if (!(src[x] & (1 << i))) *dest |= 0xFF000000;
                                dest++;
                            }
                        }
                        
                        src_row -= and_mask_pitch;
                    }
                }
            }
        }
    }
    
    if (colours_handle)  DeleteObject(colours_handle);
    if (and_mask_handle) DeleteObject(and_mask_handle);
    
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    
    return success;
}

bool
platform_get_icon_from_executable(char *path, u32 desired_size, Bitmap *icon_bitmap,
                                  bool load_default_on_failure = true)
{
    // path must be null terminated
    
    HICON small_icon_handle = 0;
    HICON icon_handle = 0;
    
    // TODO: maybe just use extract icon, or manually extract to avoid shellapi.h maybe shell32.dll
    if(SHDefExtractIconA(path, 0, 0, &icon_handle, &small_icon_handle, desired_size) != S_OK)
    {
        // NOTE: Show me that path was actually wrong and it wasn't just failed icon extraction.
        // If we can load the executable, it means we probably should be able to get the icon
        //Assert(!LoadLibraryA(path)); // TODO: Enable this when we support UWP icons
        if (load_default_on_failure)
        {
            icon_handle = LoadIconA(NULL, IDI_APPLICATION);
            if (!icon_handle) return false;
        }
    }
    
    bool success = false;
    if (icon_handle)
    {
        success = win32_get_bitmap_from_HICON(icon_handle, icon_bitmap);
    }
    else if (small_icon_handle)
    {
        success = win32_get_bitmap_from_HICON(small_icon_handle, icon_bitmap);
    }
    
    if (icon_handle) DestroyIcon(icon_handle);
    if (small_icon_handle) DestroyIcon(small_icon_handle);
    return success;
}


#if 0
// TRUE means thread owns the mutex, must have name to be visible to other processes
// CreateMutex opens mutex if it exists, and creates it if it doesn't
// ReleaseMutex gives up ownership ERROR_ALREADY_EXISTS and returns handle but not ownership
// If mutex already exists then LastError gives ERROR_ALREADY_EXISTS
// Dont need ownership to close handle
// NOTE:
// * Each process should create mutex (maybe open mutex instead) once, and hold onto handle
//   using WaitForSingleObject to wait for it and ReleaseMutex to release the lock.
// * A mutex can be in two states: signaled (when no thread owns the mutex) or unsignaled
//
// Can also run FindWindow ?
HANDLE mutex = CreateMutexA(nullptr, TRUE, MutexName);
DWORD error = GetLastError();
if (mutex == nullptr ||
    error == ERROR_ACCESS_DENIED ||
    error == ERROR_INVALID_HANDLE)
{
    tprint("Error creating mutex\n");
    return 1;
}

bool already_running = (error == ERROR_ALREADY_EXISTS);
if (already_running)
{
    //CloseHandle(mutex); // this right?
    // maximise GUI, unless already maximise
}

CloseHandle(mutex);
#endif

#if 0
char *names[] = {
    "C:\\Program Files\\Krita (x64)\\bin\\krita.exe",
    "C:\\Program Files\\Mozilla Firefox\\firefox.exe",
    "C:\\dev\\projects\\monitor\\build\\monitor.exe",
    "C:\\Program Files\\7-Zip\\7zFM.exe",  // normal 7z.exe had no icon
    "C:\\Qt\\5.12.2\\msvc2017_64\\bin\\designer.exe",
    "C:\\Qt\\5.12.2\\msvc2017_64\\bin\\assistant.exe",
    "C:\\Program Files (x86)\\Dropbox\\Client\\Dropbox.exe",
    "C:\\Program Files (x86)\\Vim\\vim80\\gvim.exe",
    "C:\\Windows\\notepad.exe",
    "C:\\Program Files\\CMake\\bin\\cmake-gui.exe",
    "C:\\Program Files\\Git\\git-bash.exe",
    "C:\\Program Files\\Malwarebytes\\Anti-Malware\\mbam.exe",
    "C:\\Program Files\\Sublime Text 3\\sublime_text.exe",
    "C:\\Program Files\\Typora\\bin\\typora.exe",
    "C:\\files\\applications\\cmder\\cmder.exe",
    "C:\\files\\applications\\4coder\\4ed.exe",
    "C:\\files\\applications\\Aseprite\\aseprite.exe",
    "C:\\dev\\projects\\shell\\shell.exe",  // No icon in executable just default windows icon showed in explorer, so can't load anything
};

UINT size = 64;
int n = size * 1.2f;
int row = 0;
int col = 0;
for (int i = 0; i < array_count(names); ++i, ++col)
{
    if (col == 3) {
        ++row;
        col = 0;
    }
    
    Bitmap icon_bitmap = get_icon_from_executable(names[i], size);
    
    
    
    bool
        esc_key_pressed(HANDLE con_in)
    {
        DWORD events_count;
        GetNumberOfConsoleInputEvents(con_in, &events_count);
        
        if (events_count > 0)
        {
            INPUT_RECORD input;
            for (int i = 0; i < events_count; ++i)
            {
                DWORD read_event_count;
                ReadConsoleInput(con_in, &input, 1, &read_event_count);
                if (input.EventType == KEY_EVENT)
                {
                    if (input.Event.KeyEvent.bKeyDown &&
                        input.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }
#endif