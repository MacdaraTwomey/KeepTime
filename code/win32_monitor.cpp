
#include <windows.h>
#undef min
#undef max

#include <commctrl.h>
#include <AtlBase.h>
#include <UIAutomation.h>
#include <shellapi.h>
#include <shlobj_core.h> // SHDefExtractIconA

#define ID_TRAY_APP_ICON 1001
#define ID_TRAY_APP_MENU_OPEN 2000
#define ID_TRAY_APP_MENU_EXIT 2001
#define CUSTOM_WM_TRAY_ICON (WM_APP + 1)
#define CUSTOM_WM_SHOW_WINDOW (WM_APP + 2)

//#define UNICODE 1
//#define _UNICODE 1

// Visual studio can generate manifest file
//#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Test.Research.SampleAssembly' version='6.0.0.0' processorArchitecture='X86' publicKeyToken='0000000000000000' language='*'\"")

struct Win32_Context
{
    NOTIFYICONDATA nid;
    LARGE_INTEGER performance_frequency;
    HWND window;
    // HINSTANCE instance = info.info.win.hinstance;
    
    HWND desktop_window;
    HWND shell_window;
    
    HMENU tray_menu;
    
    UINT TASKBARCREATED; // custom message id recieved when taskbar restarts
    
    IUIAutomation *uia;
    VARIANT navigation_name; // VT_BSTR
    VARIANT variant_offscreen_false; // VT_BOOL - don't need to initialise default is false
    
    IUIAutomationCondition *is_navigation;
    IUIAutomationCondition *is_toolbar;
    IUIAutomationCondition *and_cond;
    IUIAutomationCondition *is_combobox;
    IUIAutomationCondition *is_editbox;
    
    IUIAutomationCondition *is_document;
    IUIAutomationCondition *is_group;
    IUIAutomationCondition *is_not_offscreen;
    
    // TODO: Put statics in platform_get_firefox_url in here
    // and free any IUIAutomationElement remaining open before close
    
    
#if 0
    HWINEVENTHOOK window_changed_hook;
    HWINEVENTHOOK browser_title_changed_hook;
    HWND last_browser_window;
    bool window_changed;
    bool browser_title_changed;
#endif
    
};
static Win32_Context win32_context;

#if 0
s64
win32_get_wall_time_microseconds()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    
    now.QuadPart *= 1000000; // microseconds per second
    now.QuadPart /= win32_context.performance_frequency.QuadPart; // ticks per second
    
    return (s64)now.QuadPart;
}
#endif

LARGE_INTEGER
win32_get_time()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now;
}

// TODO: I would rather not pass in perf_freq (maybe make a global in this file)
s64
win32_get_microseconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    LARGE_INTEGER microseconds;
    microseconds.QuadPart = end.QuadPart - start.QuadPart;
    microseconds.QuadPart *= 1000000; // microseconds per second
    microseconds.QuadPart /= win32_context.performance_frequency.QuadPart; // ticks per second
    return microseconds.QuadPart;
}

bool
init_win32_context(HWND hwnd)
{
    win32_context = {};
    win32_context.window = hwnd;
    
    // TODO: Why don't we need to link ole32.lib?
    // Call this because we use CoCreateInstance in UIAutomation
    if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK)
    {
        return false;
    }
    
    QueryPerformanceFrequency(&win32_context.performance_frequency);
    
    // If these are null make sure platform_get_active_window doesn't compare a null window with these null windows
    win32_context.desktop_window = GetDesktopWindow();
    win32_context.shell_window = GetShellWindow();
    
    
    // Register a message that is sent when the taskbar restarts
    win32_context.TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
    if (win32_context.TASKBARCREATED == 0)
    {
        return false;
    }
    
    HRESULT err = CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_IUIAutomation,
                                   (void **)(&win32_context.uia));
    if (err != S_OK || win32_context.uia == nullptr)
    {
        // Don't get urls?
        Assert(0);
        return false;
    }
    
    win32_context.navigation_name.vt = VT_BSTR;
    win32_context.navigation_name.bstrVal = SysAllocString(L"Navigation");
    if (win32_context.navigation_name.bstrVal == NULL)
    {
        Assert(0);
        return 1;
    }
    
    // Default is false
    win32_context.variant_offscreen_false.vt = VT_BOOL;
    
    // For getting url editbox (when not fullscreen)
    HRESULT err1 = win32_context.uia->CreatePropertyCondition(UIA_NamePropertyId, win32_context.navigation_name,  &win32_context.is_navigation);
    HRESULT err2 = 
        win32_context.uia->CreatePropertyCondition(UIA_ControlTypePropertyId, CComVariant(UIA_ToolBarControlTypeId), &win32_context.is_toolbar);
    HRESULT err3 = win32_context.uia->CreateAndCondition(win32_context.is_navigation, win32_context.is_toolbar, &win32_context.and_cond);
    HRESULT err4 = win32_context.uia->CreatePropertyCondition(UIA_ControlTypePropertyId, CComVariant(UIA_ComboBoxControlTypeId), &win32_context.is_combobox);
    HRESULT err5 = win32_context.uia->CreatePropertyCondition(UIA_ControlTypePropertyId, CComVariant(UIA_EditControlTypeId), &win32_context.is_editbox);
    
    // // For getting url document (when fullscreen)
    HRESULT err6 = win32_context.uia->CreatePropertyCondition(UIA_ControlTypePropertyId, 
                                                              CComVariant(UIA_GroupControlTypeId), &win32_context.is_group);
    
    HRESULT err7 = win32_context.uia->CreatePropertyCondition(UIA_IsOffscreenPropertyId, 
                                                              win32_context.variant_offscreen_false, &win32_context.is_not_offscreen);
    HRESULT err8 = win32_context.uia->CreatePropertyCondition(UIA_ControlTypePropertyId, CComVariant(UIA_DocumentControlTypeId), &win32_context.is_document);
    
    if (err1 != S_OK || err2 != S_OK || err3 != S_OK || err4 != S_OK || err5 != S_OK || 
        err6 != S_OK || err7 != S_OK || err8 != S_OK)
    {
        Assert(0);
        return false;
    }
    
    
    // TODO: Does SDL automatically set icon to icon inside executable, or is that windows,
    // Should we set icon manually?
#if 0
    HICON h_icon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_MAIN_ICON));
    if (h_icon)
    {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)h_icon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)h_icon); // ??? both???
    }
#endif
    
    // TODO: To maintain compatibility with older and newer versions of shell32.dll while using
    // current header files may need to check which version of shell32.dll is installed so the
    // cbSize of NOTIFYICONDATA can be initialised correctly.
    // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa
    win32_context.nid;
    ZeroMemory(&win32_context.nid, sizeof(win32_context.nid));
    win32_context.nid.cbSize = sizeof(NOTIFYICONDATA);
    win32_context.nid.hWnd = hwnd;
    win32_context.nid.uID = ID_TRAY_APP_ICON; 
    win32_context.nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
    win32_context.nid.uCallbackMessage = CUSTOM_WM_TRAY_ICON;
    // Recommented you provide 32x32 icon for higher DPI systems
    // Can use LoadIconMetric to specify correct one with correct settings is used
    win32_context.nid.hIcon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_MAIN_ICON));
    
    TCHAR tooltip[] = {PROGRAM_NAME}; 
    static_assert(array_count(tooltip) <= 64, "Tooltip must be 64 characters");
    
    strncpy(win32_context.nid.szTip, tooltip, sizeof(tooltip));
    
    
    // TODO: If this doesn't work it can be impossible to access program without manually task manager killing the process and re-running, same goes for all tray icon code
    if (Shell_NotifyIconA(NIM_ADD, &win32_context.nid) == FALSE)
    {
        return false;
    }
    
    
#if 0    
    // Out of context hooks are asynchronous, but events are guaranteed to be in sequential order
    // TODO: Might want WINEVENT_SKIPOWNPROCESS
    // This might require CoInitialize
    // The client thread that calls SetWinEventHook must have a message loop in order to receive events.
    win32_context.window_changed_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
                                                        EVENT_SYSTEM_FOREGROUND,
                                                        NULL, window_changed_proc, 
                                                        0, 0, // If these are zero gets from all processes on desktop
                                                        WINEVENT_OUTOFCONTEXT);
    if (win32_context.window_changed_hook == 0)
    {
        Assert(0);
    }
#endif
    
    return true;
}

void
win32_free_context()
{
    Shell_NotifyIconA(NIM_DELETE, &win32_context.nid);
    
#if 0    
    if (win32_context.window_changed_hook) UnhookWinEvent(win32_context.window_changed_hook);
    if (win32_context.browser_name_change_hook) UnhookWinEvent(win32_context.browser_name_change_hook);
#endif
    
    // UIA
    
    if (win32_context.is_editbox) win32_context.is_editbox->Release();
    if (win32_context.is_combobox) win32_context.is_combobox->Release();
    if (win32_context.and_cond) win32_context.and_cond->Release();
    if (win32_context.is_toolbar) win32_context.is_toolbar->Release();
    if (win32_context.is_navigation) win32_context.is_navigation->Release();
    
    if (win32_context.is_document) win32_context.is_document->Release();
    if (win32_context.is_not_offscreen) win32_context.is_not_offscreen->Release();
    if (win32_context.is_group) win32_context.is_group->Release();
    
    VariantClear(&win32_context.navigation_name); 
    VariantClear(&win32_context.variant_offscreen_false); 
    if (win32_context.uia) win32_context.uia->Release();
    
    CoUninitialize();
}

void
platform_wait_for_event()
{
    // Only returns for new events in the queue
    // Doesn't return if there are unread events in the queue when called
    WaitMessage();
}

void
print_flags(SDL_Window *window)
{
    Uint32 f = SDL_GetWindowFlags(window);
    OutputDebugString("{\n");
    if (f & SDL_WINDOW_SHOWN)
    {
        OutputDebugString("\twindow is visible\n");
    }
    if (f & SDL_WINDOW_HIDDEN)
    {
        OutputDebugString("\twindow is not visible\n");
    }
    if (f & SDL_WINDOW_MINIMIZED)
    {
        OutputDebugString("\twindow is minimized\n");
    }
    if (f & SDL_WINDOW_MAXIMIZED)
    {
        OutputDebugString("\twindow is maximized\n");
    }
    if (f & SDL_WINDOW_INPUT_GRABBED)
    {
        OutputDebugString("\twindow has grabbed input focus\n");
    }
    if (f & SDL_WINDOW_INPUT_FOCUS)
    {
        OutputDebugString("\twindow has input focus\n");
    }
    if (f & SDL_WINDOW_MOUSE_FOCUS)
    {
        OutputDebugString("\twindow has mouse focus\n");
    }
    OutputDebugString("}\n");
}

void
win32_hide_win()
{
    ShowWindow(win32_context.window, SW_HIDE);
}

void
win32_handle_message(UINT msg, LPARAM lParam, WPARAM wParam)
{
    HWND window = win32_context.window;
    switch (msg)
    {
        case WM_QUERYENDSESSION:
        case WM_ENDSESSION:
        {
            // save to file
            // return true; // but we can't return true
        } break;
        case CUSTOM_WM_SHOW_WINDOW:
        {
            // received when user tried to run another instance of the program
            ShowWindow(window, SW_SHOW);
            ShowWindow(window, SW_RESTORE); // un-minimise
            SetForegroundWindow(window);
        } break;
        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (id == 0)
            {
                return;
            }
            else if (id == ID_TRAY_APP_MENU_OPEN)
            {
                //Uint32 f = SDL_GetWindowFlags(sdl_win);
                //if (f & SDL_WINDOW_SHOWN)
                //SDL_ShowWindow(sdl_win);
                //SDL_RaiseWindow(sdl_win);
                //SDL_SetWindowInputFocus(sdl_win);
                
                ShowWindow(window, SW_SHOW);
                //ShowWindow(window, SW_RESTORE); 
                //SetForegroundWindow(window);
                //BringWindowToTop(window);
                //SetFocus(window);
                //EnableWindow(window, true);
                //SetFocus(window);
                //SetActiveWindow(window);
                //SetCapture(window);
            }
            else if (id == ID_TRAY_APP_MENU_EXIT)
            {
                SendMessage(window, WM_CLOSE, 0, 0);
            }
        }
        case CUSTOM_WM_TRAY_ICON:
        {
            if (LOWORD(lParam) == WM_RBUTTONUP)
            {
                HMENU menu = CreatePopupMenu();
                AppendMenu(menu, MF_STRING, ID_TRAY_APP_MENU_OPEN, TEXT("Open Barplot Window"));
                AppendMenu(menu, MF_STRING, ID_TRAY_APP_MENU_EXIT, TEXT("Exit"));
                
                POINT mouse;
                GetCursorPos(&mouse);
                SetForegroundWindow(window); 
                
                BOOL button = TrackPopupMenu(menu,
                                             0,
                                             mouse.x,
                                             mouse.y,
                                             0,
                                             window,
                                             NULL);
                PostMessage(window, WM_NULL, 0, 0);
                DestroyMenu(menu);
            }
            else if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
            {
                if (!IsWindowVisible(window))
                {
                    ShowWindow(window, SW_SHOW);
                    //ShowWindow(window, SW_RESTORE);
                    //SetForegroundWindow(window);
                }
            }
        } break;
        
        default:
        {
            if (msg == win32_context.TASKBARCREATED)
            {
                // TODO: re add tray icon
                // wait 3-5 seconds to allow explorer to initialise
                // then can try to re-add icon
                
                // TODO: If this doesn't work it can be impossible to access program without manually task manager killing the process and re-running, same goes for all tray icon code
                Sleep(6000);
                for (int i = 0; i < 10; ++i)
                {
                    if (Shell_NotifyIconA(NIM_ADD, &win32_context.nid))
                    {
                        return;
                    }
                    else
                    {
                        Sleep(100);
                    }
                }
                
                // Want to try to restart program here if we couldn't recreate the icon
            }
        } break;
    }
}

struct Process_Ids
{
    DWORD parent;
    DWORD child;
};

Platform_Window
platform_get_active_window()
{
    // TODO: Use GetShellWindow GetShellWindow to detect when not doing anything on desktop, if foreground == desktop etc
    
#ifdef RECORD_ALL_ACTIVE_WINDOWS
    
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
    //window.is_valid = (foreground_win != 0 && foreground_win != win32_context.desktop_window && foreground_win != win32_context.shell_window);
    window.is_valid = (foreground_win != 0); // TODO: For now we say desktop and shell are valid so we can get their paths, in future make them invalid.
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
    // TODO: check against desktop and shell windows
    // TODO: Return utf8 path
    
    // On success fills buf will contiain null terminated string, and sets length to string length (not including null terminator)
    HWND handle = window.handle;
    
    // TODO: Just for debugging
    if (handle == win32_context.desktop_window)
    {
        char *win = "Desktop Window: debug";
        strcpy(buf, win);
        *length = strlen(win);
        return true;
    }
    if (handle == win32_context.shell_window)
    {
        char *win = "Shell Window: debug";
        strcpy(buf, win);
        *length = strlen(win);
        return true;
    }
    
    
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
            // This converted_len includes the null terminator, so subtract 1 from length
            *length = converted_len - 1;
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
win32_firefox_get_url_editbox(HWND hwnd)
{
    // If people have different firefox setup than me, (e.g. multiple comboboxes in navigation somehow), this doesn't work
    
    IUIAutomationElement *editbox = nullptr;
    
    IUIAutomationElement *firefox_window = nullptr;
    IUIAutomationElement *navigation_toolbar = nullptr;
    IUIAutomationElement *combobox = nullptr;
    
    HRESULT err = win32_context.uia->ElementFromHandle(hwnd, &firefox_window);
    if (err != S_OK || !firefox_window) goto CLEANUP;
    
    // oleaut.dll Library not registered - may just be app requesting newer oleaut than is available
    // This seems to throw exception
    err = firefox_window->FindFirst(TreeScope_Children, win32_context.and_cond, &navigation_toolbar);
    if (err != S_OK || !navigation_toolbar) goto CLEANUP;
    
    err = navigation_toolbar->FindFirst(TreeScope_Children, win32_context.is_combobox, &combobox);
    if (err != S_OK || !combobox) goto CLEANUP;
    
    err = combobox->FindFirst(TreeScope_Descendants, win32_context.is_editbox, &editbox);
    if (err != S_OK) editbox = nullptr;
    
    CLEANUP:
    
    if (combobox) combobox->Release();
    if (navigation_toolbar) navigation_toolbar->Release();
    if (firefox_window) firefox_window->Release();
    
    // If success dont release editbox, if failure editbox doesn't need releasing
    return editbox;
}

IUIAutomationElement *
win32_firefox_get_url_document_if_fullscreen(HWND hwnd)
{
    IUIAutomationElement *document = nullptr;
    
    IUIAutomationElement *firefox_window = nullptr;
    IUIAutomationElement *not_offscreen_element = nullptr;
    IUIAutomationElement *group = nullptr;
    
    HRESULT err = win32_context.uia->ElementFromHandle(hwnd, &firefox_window);
    if (err != S_OK || !firefox_window) goto CLEANUP;
    
    //0,0,w,h
    //VARIANT bounding_rect = {};
    //err = firefox_window->GetCurrentPropertyValue(UIA_BoundingRectanglePropertyId, &bounding_rect);
    
    // This doesn't seem to throw exception
    err = firefox_window->FindFirst(TreeScope_Children, win32_context.is_group, &group);
    if (err != S_OK || !group) goto CLEANUP;
    
    err = group->FindFirst(TreeScope_Children, win32_context.is_not_offscreen, &not_offscreen_element);
    if (err != S_OK || !not_offscreen_element) goto CLEANUP;
    
    // Try to skip over one child using TreeScope_Descendants
    err = not_offscreen_element->FindFirst(TreeScope_Descendants, win32_context.is_document, &document);
    if (err != S_OK || !document) document = nullptr;
    
    CLEANUP:
    if (not_offscreen_element) not_offscreen_element->Release();
    if (group) group->Release();
    if (firefox_window) firefox_window->Release();
    
    return document;
}

bool
win32_window_is_fullscreen(HWND hwnd)
{
    RECT window_rect;
    if (GetWindowRect(hwnd, &window_rect) == 0)
    {
        Assert(0);
    }
    
    int window_width = window_rect.right - window_rect.left;
    int window_height = window_rect.bottom - window_rect.top;
    
    // Gets closest monitor to window
    MONITORINFO monitor_info = {sizeof(MONITORINFO)}; // must set size of struct
    if (GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor_info) == 0)
    {
        Assert(0);
    }
    
    // Expressed in virtual-screen coordinates.
    // NOTE: that if the monitor is not the primary display monitor, some of the rectangle's coordinates may be negative values.
    // I think this works if left and right are negative
    int monitor_width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
    int monitor_height =  monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
    
    return (window_width == monitor_width && window_height == monitor_height);
}

bool win32_firefox_get_url_from_element(IUIAutomationElement *element, char *buf, int buf_size, size_t *length)
{
    bool success = false;
    VARIANT url_bar = {};
    HRESULT err = element->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &url_bar);
    if (err == S_OK)
    {
        if (url_bar.bstrVal)
        {
            success = win32_BSTR_to_utf8(url_bar.bstrVal, buf, buf_size, length);
        }
    }
    
    VariantClear(&url_bar);
    return success;
}

bool
platform_get_firefox_url(Platform_Window window, char *buf, size_t *length)
{
    // To get the URL of the active tab in Firefox Window:
    // Mozilla firefox window          ControlType: UIA_WindowControlTypeId (0xC370) LocalizedControlType: "window"
    //   "Navigation" tool bar         UIA_ToolBarControlTypeId (0xC365) LocalizedControlType: "tool bar"
    //     "" combobox                 UIA_ComboBoxControlTypeId (0xC353) LocalizedControlType: "combo box"
    //       "Search with Google or enter address"   UIA_EditControlTypeId (0xC354) LocalizedControlType: "edit"
    //        which has Value.Value = URL
    
    // TODO: Could also use EVENT_OBJECT_NAMECHANGE to see if window title changed and then only look for new tabs at that point
    // TODO:?
    // AddPropertyChangedEventHandler
    // https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-howto-implement-event-handlers
    
    // TODO: make sure url bar doesn't have keyboard focus/is changing?
    
    bool success = false;
    *length = 0;
    
    static HWND last_hwnd = 0; // if != then re-get firefox_window and re get editbox or document etc
    //static IUIAutomationElement *firefox_window = nullptr;
    static IUIAutomationElement *editbox = nullptr;
    static IUIAutomationElement *document = nullptr;
    
    HWND hwnd = window.handle;
    bool is_fullscreen = win32_window_is_fullscreen(hwnd);
    
    // TODO: Get firefox window once-ish (is this even a big deal if we have to re_load element it is just one extra call anyway)
    // TODO: We are just redundantly trying twice to load element if it fails (when matching hwnd)
    
    // TODO: Possible cases
    // 1. alt tab switching from one fullscreen firefox window to another
    // 2. Switching between tabs that are both fullscreen (might not be possible)
    
    // NOTE: Maybe safest thing is to just do all the work everytime, and rely and events to tell us when
    // Counterpoint, try to do the fast thing and then fall back to slow thing (what i'm trying to do)
    
    // If screenmode changes load other element
    bool re_load_firefox_element = true;
    if (last_hwnd == hwnd)
    {
        if (is_fullscreen)
        {
            if (editbox) editbox->Release();
            editbox = nullptr;
            
            if (!document)
            {
                document = win32_firefox_get_url_document_if_fullscreen(hwnd);
            }
            if (document)
            {
                // TODO: Assert that this is onscreen (the current tab)
                
                if (win32_firefox_get_url_from_element(document, buf, PLATFORM_MAX_URL_LEN, length))
                {
                    // Don't release element (try to reuse next time)
                    success = true;
                    re_load_firefox_element = false;
                }
            }
        }
        else
        {
            if (document) document->Release();
            document = nullptr;
            
            if (!editbox)
            {
                editbox = win32_firefox_get_url_editbox(hwnd);
            }
            if (editbox)
            {
                if (win32_firefox_get_url_from_element(editbox, buf, PLATFORM_MAX_URL_LEN, length))
                {
                    // Don't release element (try to reuse next time)
                    success = true;
                    re_load_firefox_element = false;
                }
            }
        }
    }
    
    if (re_load_firefox_element)
    {
        // Free cached elements of other (old) window
        if (editbox) editbox->Release();
        if (document) document->Release();
        //if (firefox_window) firefox_window->Release();
        editbox = nullptr;
        document = nullptr;
        //firefox_window = nullptr;
        
        IUIAutomationElement *element = nullptr;
        if (is_fullscreen)
            element = win32_firefox_get_url_document_if_fullscreen(hwnd);
        else
            element = win32_firefox_get_url_editbox(hwnd);
        
        if (element)
        {
            if (win32_firefox_get_url_from_element(element, buf, PLATFORM_MAX_URL_LEN, length))
            {
                // Don't release element (try to reuse next time)
                success = true;
                if (is_fullscreen)
                    document = element;
                else
                    editbox = element;
            }
            else
            {
                // Could not get url from window
                element->Release();
            }
        }
        else
        {
            // Could not get url from window
        }
        
        last_hwnd = hwnd;
    }
    
    return success;
    
    
    // NOTE: window will always be the up to date firefox window, even if our editbox pointer is old.
    // NOTE: Even when everything else is released it seems that you can still use editbox pointer, unless the window is closed. Documentation does not confirm this, so i'm not 100% sure it's safe.
    
    // NOTE: This is duplicated because the editbox pointer can be valid, but just stale, and we only know that if GetCurrentPropertyValue fails, in which case we free the pointer and try to reacquire the editbox.
    
    // BUG: If we get a valid editbox from a normal firfox window, then go fullscreen, the editbox pointer is not set to null, so we continually read an empty string from the property value, without an error (not sure why no error is set by windows).
}


bool
win32_get_bitmap_data_from_HICON(HICON icon, i32 *width, i32 *height, i32 *pitch, u32 **pixels)
{
    bool success = false;
    
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
    
    if (icon && GetIconInfo(icon, &ii))
    {
        // It is necessary to call DestroyObject for icons and cursors created with CopyImage
        colours_handle = (HBITMAP)CopyImage(ii.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        and_mask_handle = (HBITMAP)CopyImage(ii.hbmMask, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (colours_handle && and_mask_handle)
        {
            int result_1 = GetObject(colours_handle, sizeof(BITMAP), &colour_mask) == sizeof(BITMAP);
            int result_2 = GetObject(and_mask_handle, sizeof(BITMAP), &and_mask) == sizeof(BITMAP);
            if (result_1 && result_2)
            {
                *width = colour_mask.bmWidth;
                *height = colour_mask.bmHeight;
                *pitch = *width * 4;
                *pixels = (u32 *)malloc(*pitch * *height);
                if (*pixels)
                {
                    memset(*pixels, 0, *pitch * *height);
                    
                    b32 had_alpha = false;
                    u32 *dest = *pixels;
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
                        u32 *dest = *pixels;
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
                    
                    success = true;
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


#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
bool
platform_get_default_icon(u32 desired_size, i32 *width, i32 *height, i32 *pitch, u32 **pixels)
{
    bool success = false;
    HICON hico = (HICON)LoadIcon(NULL, IDI_APPLICATION);
    if (hico)
    {
#if 0
        s32 big_w = 0;
        s32 big_h = 0;
        s32 big_pitch = 0;
        u32 *big_pixels = 0;
        if (win32_get_bitmap_data_from_HICON(hico, &big_w, &big_h, &big_pitch, &big_pixels))
        {
            u32 *out_pixels = (u32 *)malloc(desired_size * desired_size * 4);
            if (out_pixels)
            {
                if (stbir_resize_uint8((u8 *)big_pixels, big_w , big_h, 0, // packed in memory
                                       (u8 *)out_pixels, desired_size, desired_size, 0, // packed in memory
                                       4))
                {
                    *width = desired_size;
                    *height = desired_size;
                    *pitch = desired_size*4;
                    *pixels = out_pixels;
                    success = true;
                }
            }
        }
#endif
        
        if (win32_get_bitmap_data_from_HICON(hico, width, height, pitch, pixels))
        { 
            success = true;
        }
    }
    
    return success;
}

bool
platform_get_icon_from_executable(char *path, u32 desired_size, 
                                  i32 *width, i32 *height, i32 *pitch, u32 **pixels)
{
    // path must be null terminated
    
    HICON small_icon_handle = 0;
    HICON icon_handle = 0;
    
    // TODO: maybe just use extract icon, or manually extract to avoid shellapi.h maybe shell32.dll
    if(SHDefExtractIconA(path, 0, 0, &icon_handle, &small_icon_handle, desired_size) != S_OK)
    {
        return false;
    }
    
    bool success = false;
    if (icon_handle)
    {
        success = win32_get_bitmap_data_from_HICON(icon_handle, width, height, pitch, pixels);
    }
    else if (small_icon_handle)
    {
        success = win32_get_bitmap_data_from_HICON(small_icon_handle, width, height, pitch, pixels);
    }
    
    // NOTE: Don't need to destroy LoadIcon icons I'm pretty sure, but SHDefExtractIconA does need to be destroyed
    if (icon_handle) DestroyIcon(icon_handle);
    if (small_icon_handle) DestroyIcon(small_icon_handle);
    return success;
}

bool
platform_show_other_instance_if_already_running()
{
    
#if 0    
    wchar_t class_name[1000];
    UINT len = RealGetWindowClassW(win32_context.window, class_name, 1000);
    
    char text[1000];
    int count = GetWindowTextA(win32_context.window, text, 1000);
#endif
    
    
    HWND other_window = FindWindowA("SDL_app", PROGRAM_NAME);
    if (other_window)
    {
        SendMessage(other_window, CUSTOM_WM_SHOW_WINDOW, 0, 0); 
        return true;
    }
    
    return false;
}

#if 0
void
platform_get_changed_window()
{
    if (win32_context.window_changed)
    {
        HWND foreground_win = GetForegroundWindow();
        char buf[2000];
        size_t len;
        if (platform_get_program_from_window(Platform_Window{foreground_win, true}, buf, &len))
        {
            String full_path = make_string_size_cap(buf, len, array_count(buf));
            String program_name = get_filename_from_path(full_path);
            if (program_name.length == 0) return 0;
            
            remove_extension(&program_name);
            if (program_name.length == 0) return 0;
            
            if (string_equals(program_name, "firefox"))
            {
                if (win32_context.last_browser_window == foreground_win)
                {
                    // We changed to a browser window that is already hooked
                }
                else
                {
                    if (win32_context.browser_title_changed_hook) UnhookWinEvent(win32_context.browser_name_change_hook);
                    
                    DWORD process_id = 0; // 0 can only be system idle process
                    GetWindowThreadProcessId(foreground_win, &process_id);
                    if (process_id != 0)
                    {
                        win32_context.browser_title_changed_hook = SetWinEventHook(EVENT_OBJECT_NAMECHANGE,
                                                                                   EVENT_OBJECT_NAMECHANGE,
                                                                                   NULL, browser_title_changed_proc, 
                                                                                   process_id, 0, 
                                                                                   WINEVENT_OUTOFCONTEXT);
                        if (win32_context.browser_title_changed_hook)
                        {
                            win32_context.last_browser_window = foreground_win;
                        }
                    }
                    
                }
            }
        }
        
        
        win32_context.browser_title_changed = false;
        win32_context.window_changed = false;
        
        // If window change occured we assume it takes precedence over tab change, so don't process tab change
        return;
    }
    
    if (win32_context.browser_title_changed)
    {
        // make sure we are still on browser
        HWND foreground_win = GetForegroundWindow();
        if (foreground_win != win32_context.last_browser_window)
        {
            // This shouldn't happen because if the foreground win is not the hooked browser window that set .browser_title_changed, then we should have also set and handled .window_changed and returned above.
            Assert(0);
        }
        
        char buf[2000];
        size_t len;
        if (platform_get_active_window(Platform_Window{foreground_win, true}, buf, &len))
        {
            String full_path = make_string_size_cap(buf, len, array_count(buf));
            String program_name = get_filename_from_path(full_path);
            if (program_name.length == 0) return 0;
            
            remove_extension(&program_name);
            if (program_name.length == 0) return 0;
            
            Assert(string_equals(program_name, "firefox"));
        }
    }
    
    // ... do normal updating time stuff...
    
    win32_context.browser_title_changed = false;
    win32_context.window_changed = false;
    return;
}
#endif


#if 0
// While a hook function processes an event, additional events may be triggered, which may cause the hook function to reenter before the processing for the original event is finished. The problem with reentrancy in hook functions is that events are completed out of sequence unless the hook function handles this situation.
// The only solution is for client developers to add code in the hook function that detects reentrancy and take appropriate action if the hook function is reentered.
void CALLBACK window_changed_proc(HWINEVENTHOOK hWinEventHook,
                                  DWORD event,
                                  HWND hwnd,
                                  LONG idObject,
                                  LONG idChild,
                                  DWORD dwEventThread,
                                  DWORD dwmsEventTime)
{
    // Can be sent even if foregound window changes to another window in the same thread
    if (event == EVENT_SYSTEM_FOREGROUND &&
        idObject == OBJID_WINDOW &&
        idChild == CHILDID_SELF)
    {
        // Since the notification is asynchronous, the foreground window may have been destroyed by the time the notification is received, so we have to be prepared for that case.
        
        win32_context.window_changed = true;
        win32_context.browser_title_changed = false; // not sure if should do this or not
        
        // send message
    }
    else
    {
        Assert(0);
    }
}

void CALLBACK browser_title_changed_proc(HWINEVENTHOOK hWinEventHook,
                                         DWORD event,
                                         HWND hwnd,
                                         LONG idObject,
                                         LONG idChild,
                                         DWORD dwEventThread,
                                         DWORD dwmsEventTime)
{
    if (event == EVENT_OBJECT_NAMECHANGE) 
        //idObject == OBJID_WINDOW &&
        //idChild == CHILDID_SELF)
    {
        // Doesn't matter if multiple tabs or windows are changed in a small space of time, we would have missed this anyway with > 1 second polling, so just know about last change
        win32_context.browser_title_changed = true;
        win32_context.window_changed = true;
    }
    else
    {
        Assert(0);
    }
}
#endif

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
