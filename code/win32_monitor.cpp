#include "cian.h"
//#include "monitor_string.h" // more of a library file

#include <windows.h>
#undef min
#undef max

#include <AtlBase.h>
#include <UIAutomation.h>

#include <shellapi.h>
#include <shlobj_core.h> // SHDefExtractIconA

#include "win32_monitor.h"
#include "resource.h"
#include "platform.h"
#include "graphics.h"
#include "ui.h"
#include "monitor.h"

#include <chrono>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

#include "utilities.cpp" // xalloc, string copy, concat string, make filepath, get_filename_from_path
#include "monitor.cpp" // This deals with id, days, databases, websites, bitmap icons

#define CONSOLE_ON 1

static Bitmap global_screen_buffer;
static char *global_savefile_path;
static char *global_debug_savefile_path;
static bool global_running = false;

// NOTE: This is only used for toggling with the tray icon.
// Use pump messages result to check if visible to avoid the issues with value unexpectly changing.
// Still counts as visible if minimised to the task bar.
static NOTIFYICONDATA global_nid = {};

static constexpr int WindowWidth = 960;
static constexpr int WindowHeight = 540;

// -----------------------------------------------------------------
// TODO:
// * !!! If GUI is visible don't sleep. But we still want to poll infrequently. So maybe check elapsed poll time.
// * Remember window width and height
// * Unicode correctness
// * Path length correctness
// * Dynamic window, button layout with resizing
// * OpenGL graphing?
// * Stop repeating work getting the firefox URL, maybe use UIAutomation cache?

// * Finalise GUI design (look at task manager and nothings imgui for inspiration)
// * Make api better/clearer, especially graphics api
// * BUG: Program time slows down when mouse is moved quickly within window or when a key is held down
// -----------------------------------------------------------------

// Top bit of id specifies is the 'program' is a normal executable or a website
// where rest of the bits are the actual id of its name.
// For websites whenever a user creates a new keyword that keyword gets an id and the id count increases by 1
// If a keyword is deleted the records with that website id can be given firefox's id.

HANDLE
create_console()
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


void
platform_change_wakeup_frequency(int wakeup_milliseconds)
{
    
}

void
win32_resize_screen_buffer(Bitmap *buffer, int new_width, int new_height)
{
    if (buffer->pixels) free(buffer->pixels);
    
    buffer->width = new_width;
    buffer->height = new_height;
    buffer->pitch = buffer->width * Bitmap::BYTES_PER_PIXEL;
    buffer->pixels = (u32 *)xalloc(buffer->pitch * buffer->height);
}

void
win32_paint_window(Bitmap *buffer, HDC device_context)
{
    // TODO: GetClientRect gets slightly smaller dimensions, why??
    
    BITMAPINFO bitmap_info = {};
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = buffer->width;
    bitmap_info.bmiHeader.biHeight = -buffer->height; // Negative height to tell Windows this is a top-down bitmap
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    
    // Could use this instead
    // SetDIBitsToDevice
    StretchDIBits(device_context,
                  0, 0,                          // dest rect upper left coordinates
                  buffer->width, buffer->height, // dest rect width height
                  0, 0,                          // src upper left coordinates
                  buffer->width, buffer->height, // src rect width height
                  buffer->pixels,
                  &bitmap_info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

LRESULT CALLBACK
WinProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        // We use windows timer so we can wake up waiting on input events
        case WM_TIMER:
        {
        } break;
        
        
        // WM_QUERYENDSESSION and WM_ENDSESSION for system shutdown
        // WM_TASKBARCREATED if explorer.exe (and taskbar) crashes, to recreate icon
        
        // Close, Destroy and Quit ------------------------------------------------------
        // When main window or taskbar preview [x] is pressed we recieve CLOSE, DESTROY and QUIT in that order
        // Same for double clicking main window icon on top left, and clicking close on titlebar option under icon.
        // When we close from task manager all messages are recieved.
        // When we handle ESCAPE key, none of these are recieved
        
        // From MSDN: Closing the window  // https://docs.microsoft.com/en-us/windows/win32/learnwin32/closing-the-window
        // Clicking close or using alt-f4 causes window to recieve WM_CLOSE message
        // Allowing you to prompt the user before closing the window
        
        // If you want to destroy the window you can call DestroyWindow when WM_CLOSE recieved
        // or can return 0 from case statement and the OS will ignore message
        
        case WM_CLOSE:
        {
            // If this message is ignored DefWindowProc calls DestroyWindow
            // Can Hide window here and not call DestroyWindow
            // This is recieved direcly by WinProc
            
            global_running = false;
            DestroyWindow(window); // Have to call this or main window X won't close program
        } break;
        
        case WM_DESTROY:
        {
            // Recieved when window is removed from screen but before destruction occurs
            // and before child windows are destroyed.
            // Typicall call PostQuitMessage(0) to send WM_QUIT to message loop
            // So typically goes CLOSE -> DESTROY -> QUIT
            
            
            // When the window is destroyed by pressing the X, not by quiting with escape
            Shell_NotifyIconA(NIM_DELETE, &global_nid);
            
            PostQuitMessage(0);
        } break;
        
        // When window is about to be destroyed it will recieve WM_DESTROY message
        // Typically you call PostQuitMessage(0) from case statement to post WM_QUIT message to
        // end the message loop
        
        case WM_QUIT:
        {
            global_running = false;
        } break;
        
        // WM_NCCREATE WM_CREATE send before window becomes visible
        
        // When window created these messages are sent in this order:
        // WM_NCCREATE
        // WM_CREATE
        
        case WM_CREATE:
        {
            SetWindowText(window, "This is the Title Bar");
            
            // TODO: To maintain compatibility with older and newer versions of shell32.dll while using
            // current header files may need to check which version of shell32.dll is installed so the
            // cbSize of NOTIFYICONDATA can be initialised correctly.
            // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa
            
            global_nid = {};
            global_nid.cbSize = sizeof(NOTIFYICONDATA);
            global_nid.hWnd = window;
            global_nid.uID = ID_TRAY_APP_ICON; // // Not sure why we need this
            global_nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
            global_nid.uCallbackMessage = CUSTOM_WM_TRAY_ICON;
            // Recommented you provide 32x32 icon for higher DPI systems
            // Can use LoadIconMetric to specify correct one with correct settings is used
            global_nid.hIcon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(MAIN_ICON_ID));
            
            // TODO: This should say "running"
            TCHAR tooltip[] = {"Tooltip dinosaur"}; // Use max 64 chars
            strncpy(global_nid.szTip, tooltip, sizeof(tooltip));
            
            Shell_NotifyIconA(NIM_ADD, &global_nid);
        } break;
        
        case CUSTOM_WM_TRAY_ICON:
        {
            switch (LOWORD(lParam))
            {
                case WM_RBUTTONUP:
                {
                    // Could open a context menu here with option to quit etc.
                } break;
                case WM_LBUTTONUP:
                {
                    // NOTE: For now we just toggle with tray icon but, we will in future toggle with X button
                    if (IsWindowVisible(window))
                    {
                        ui_set_visibility_changed(Window_Hidden);
                        
                        ShowWindow(window, SW_HIDE);
                    }
                    else
                    {
                        ui_set_visibility_changed(Window_Shown);
                        
                        // TODO: This doesn't seem to 'fully activate' the window. The title bar is in focus but cant input escape key. So seem to need to call SetForegroundWindow
                        ShowWindow(window, SW_SHOW);
                        ShowWindow(window, SW_RESTORE); // If window was minimised and hidden, also unminimise
                        SetForegroundWindow(window); // Need this to allow 'full focus' on window after showing
                    }
                } break;
            }
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            if (wParam == VK_ESCAPE)
            {
                global_running = false;
            }
        } break;
        
        // Windows says use GET_X_LPARAM/GET_Y_LPARAM instead of HIWORD LOWORD because these ignore signedness
        // that the lo and hi parts actually are, but internally the macros use HIWORD and LOWORD anyway, but cast from a (unsigned short) to a (short) then to a (int).
        // The problem is that these returned values can be negative when there are multiple monitors (not sure how to handle this issue).
        case WM_MOUSEMOVE:
        ui_set_mouse_button_state((short)LOWORD(lParam), (short)HIWORD(lParam), Mouse_Move);
        break;
        
        case WM_LBUTTONUP:
        ui_set_mouse_button_state((short)LOWORD(lParam), (short)HIWORD(lParam), Mouse_Left_Up);
        //tprint("Left mouse up");
        break;
        
        case WM_LBUTTONDOWN:
        ui_set_mouse_button_state((short)LOWORD(lParam), (short)HIWORD(lParam), Mouse_Left_Down);
        //tprint("Left mouse down");
        break;
        
        case WM_RBUTTONUP:
        ui_set_mouse_button_state((short)LOWORD(lParam), (short)HIWORD(lParam), Mouse_Right_Up);
        //tprint("Right mouse up");
        break;
        
        case WM_RBUTTONDOWN:
        ui_set_mouse_button_state((short)LOWORD(lParam), (short)HIWORD(lParam), Mouse_Right_Down);
        //tprint("Right mouse down");
        break;
        
        case WM_MOUSEWHEEL:
        ui_set_mouse_wheel_state((short)LOWORD(lParam), (short)HIWORD(lParam), GET_WHEEL_DELTA_WPARAM(wParam));
        //tprint("mouse wheel");
        break;
        
        // WM_SIZE and WM_PAINT messages recieved when window resized,
        // WM_SIZE can be sent directly to WndProc
        case WM_PAINT:
        {
            // Sometimes your program will initiate painting, sometimes will get sent WM_PAINT
            // Only responsible for repainting client area
            PAINTSTRUCT ps;
            HDC device_context = BeginPaint(window, &ps);
            // When window is resized, the process is suspended and recieves WM_PAINT
            win32_paint_window(&global_screen_buffer, device_context);
            EndPaint(window, &ps);
        } break;
        
#if 0
        case WM_SIZE:
        {
            // Is invalidate rect even needed/
            InvalidateRect();
            
            // NOTE: Very dangerous that this could be resized 'while' we're writing to it in the app layer.
            win32_resize_screen_buffer();
        } break;
#endif
        default:
        result = DefWindowProcA(window, msg, wParam, lParam);
        break;
    }
    
    return result;
}

int WINAPI
WinMain(HINSTANCE instance,
        HINSTANCE prev_instance,
        LPTSTR    cmd_line,
        int       cmd_show)
{
    
#if CONSOLE_ON
    HANDLE con_in = create_console();
#endif
    
    // TODO: Why don't we need to link ole32.lib?
    // Call this because we use CoCreateInstance in UIAutomation
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); // equivalent to CoInitialize(NULL)
    
    {
        // TODO: How to give the app layer its full path?
        // Relative paths possible???
        char exe_path[PLATFORM_MAX_PATH_LEN];
        DWORD filepath_len = GetModuleFileNameA(nullptr, exe_path, 1024);
        if (filepath_len == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER) return 1;
        
        global_savefile_path = make_filepath(exe_path, "savefile.mpt");
        if (!global_savefile_path) return 1;
        
        global_debug_savefile_path = make_filepath(exe_path, "debug_savefile.txt");
        if (!global_debug_savefile_path) return 1;
        
        if (strlen(global_savefile_path) > PLATFORM_MAX_PATH_LEN)
        {
            tprint("Error: Find first file ANSI version limited to MATH_PATH");
            return 1;
        }
    }
    
    // If already running just open/show GUI, or if already open do nothing.
    // FindWindowA();
    
    win32_resize_screen_buffer(&global_screen_buffer, WindowWidth, WindowHeight);
    
    WNDCLASS window_class = {};
    window_class.style         = CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc   = WinProc;
    window_class.hInstance     = instance;
    window_class.hIcon         =  LoadIcon(instance, MAKEINTRESOURCE(MAIN_ICON_ID));
    window_class.hCursor       = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszMenuName  = "Monitor";
    window_class.lpszClassName = "MonitorWindowClassName";
    //window_classex.hIconsm   = LoadIcon(instance, MAKEINTRESOURCE(MAIN_ICON_ID)); // OS may still look in ico file for small icon anyway
    
    if (!RegisterClassA(&window_class))
    {
        return 1;
    }
    
    HWND window = CreateWindowExA(0, window_class.lpszClassName, "Monitor",
                                  WS_VISIBLE|WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                                  800, 100,  // Screen pos, TODO: Change to CS_USEDEFAULT
                                  WindowWidth, WindowHeight,
                                  NULL, NULL,
                                  instance, NULL);
    if (!window)
    {
        return 1;
    }
    
    ShowWindow(window, cmd_show);
    UpdateWindow(window); // This sends WM_PAINT message so make sure screen buffer is initialised
    
    if (IsWindowVisible(window)) ui_set_visibility_changed(Window_Shown);
    
    // Can't set lower than 10ms, not that you'd want to.
    // Can specify callback to call instead of posting message.
    DWORD poll_milliseconds = 10;
    SetTimer(window, 0, (UINT)poll_milliseconds, NULL);
    
    auto old_time = Steady_Clock::now();
    
    // THis may just be temporary.
    Monitor_State monitor_state = {};
    monitor_state.is_initialised = false;
    
    global_running = true;
    while (global_running)
    {
        WaitMessage();
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        
        // Steady clock also accounts for time paused in debugger etc, so can introduce bugs that aren't there normally when debugging.
        auto new_time = Steady_Clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(new_time - old_time);
        old_time = new_time;
        time_type dt = diff.count();
        
        // Maybe pass in poll stuff here which would allow us to avoid timer stuff in app layer,
        // or could call poll windows from app layer, when we recieve a timer elapsed flag.
        // We might want to change frequency that we poll, so may need platform_change_wakeup_frequency()
        update(&monitor_state, &global_screen_buffer, dt);
        
        // Maybe we want to call this from in the app layer because we might not always wan't to update on an event
        {
            HDC device_context = GetDC(window);
            win32_paint_window(&global_screen_buffer, device_context);
            ReleaseDC(window, device_context);
        }
    }
    
    // Save to file TODO: (implement when file structure is not changing)
    
    //free_table(&database.all_programs);
    //free(font.atlas);
    //free(font.glyphs);
    
    // Just in case we exit using ESCAPE
    Shell_NotifyIconA(NIM_DELETE, &global_nid);
    
    FreeConsole();
    
    CoUninitialize();
    
    return 0;
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
    
    
    void
        print_tray_icon_message(LPARAM lParam)
    {
        static int counter = 0;
        switch (LOWORD(lParam))
        {
            case WM_RBUTTONDOWN:
            {
                tprint("%. WM_RBUTTONDOWN", counter++);
            }break;
            case WM_RBUTTONUP:
            {
                tprint("%. WM_RBUTTONUP", counter++);
            } break;
            case WM_LBUTTONDOWN:
            {
                tprint("%. WM_LBUTTONDOWN", counter++);
            }break;
            case WM_LBUTTONUP:
            {
                tprint("%. WM_LBUTTONUP", counter++);
            }break;
            case WM_LBUTTONDBLCLK:
            {
                tprint("%. WM_LBUTTONDBLCLK", counter++);
            }break;
            case NIN_BALLOONSHOW:
            {
                tprint("%. NIN_BALLOONSHOW", counter++);
            }break;
            case NIN_POPUPOPEN:
            {
                tprint("%. NIN_POPUPOPEN", counter++);
            }break;
            case NIN_KEYSELECT:
            {
                tprint("%. NIN_KEYSELECT", counter++);
            }break;
            case NIN_BALLOONTIMEOUT:
            {
                tprint("%. NIN_BALLOONTIMEOUT", counter++);
            }break;
            case NIN_BALLOONUSERCLICK:
            {
                tprint("%. NIN_BALLOONUSERCLICK", counter++);
            }break;
            case NIN_SELECT:
            {
                tprint("%. NIN_SELECT\n", counter++);
            } break;
            case WM_CONTEXTMENU:
            {
                tprint("%. WM_CONTEXTMENU\n", counter++);
            } break;
            default:
            {
                tprint("%. OTHER", counter++);
            }break;
        }
    }
    
#endif