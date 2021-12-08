
#include <stdlib.h>
#include <stdio.h>

#include "imgui.h"
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"
// imgui will perform loading for OpenGL functions it uses automatically

#include <windows.h>
#include <commctrl.h>
#include <AtlBase.h>
#include <UIAutomation.h>
#include <shellapi.h>
#include <shlobj_core.h> // SHDefExtractIconA
#include <gl/gl.h> // windows OpenGL

#include "resource.h" // ID_MAIN_ICON

#include "base.h"
#include "platform.h"
#include "monitor_string.h"
#include "helper.h"
#include "helper.cpp"
#include "base.cpp"
#include "monitor.cpp"

#define ID_TRAY_APP_ICON 1001
#define ID_TRAY_APP_MENU_OPEN 2000
#define ID_TRAY_APP_MENU_EXIT 2001
#define CUSTOM_WM_TRAY_ICON (WM_APP + 1)
#define CUSTOM_WM_SHOW_WINDOW (WM_APP + 2)

#define PROGRAM_NAME "Keeptime"
#define WINDOW_CLASS_NAME "Keeptime2WindowClass"

struct win32_ui_automation
{
    IUIAutomation *Interface;
    
    VARIANT NavigationName; // VT_BSTR
    VARIANT VariantOffscreenFalse; // VT_BOOL - don't need to initialise default is false
    
    IUIAutomationCondition *IsNavigation;
    IUIAutomationCondition *IsToolbar;
    IUIAutomationCondition *AndCond;
    IUIAutomationCondition *IsCombobox;
    IUIAutomationCondition *IsEditbox;
    
    IUIAutomationCondition *IsDocument;
    IUIAutomationCondition *IsGroup;
    IUIAutomationCondition *IsNotOffscreen;
};

static UINT GlobalTaskbarCreatedMessage = 0;
static NOTIFYICONDATA GlobalNotifyIconData = {};
static HWND GlobalDesktopWindow = NULL;
static HWND GlobalShellWindow = NULL;
static HMENU GlobalTrayMenu = NULL;
static LARGE_INTEGER GlobalPerformanceFrequency = {};
static bool GlobalAppRunning = false;
static win32_ui_automation GlobalUIAutomation = {};
static HWND GlobalWindowHandle = NULL;

typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hshareContext, const int *attribList);
typedef BOOL WINAPI wgl_swap_interval_ext(int interval);

//
// TODO:
// PLATFORM_ERROR_MESSAGE to make message box
//

char *UTF8FromUTF16(arena *Arena, wchar_t *String16, u32 String16Length)
{
    Assert(String16Length > 0);
    
    int Capacity = (String16Length * 4) + 1;
    char *Buffer = Allocate(Arena, Capacity, char);
    int ByteCount = WideCharToMultiByte(CP_UTF8, 0, 
                                        String16, String16Length, 
                                        Buffer, Capacity, 
                                        NULL, NULL);
    
    Buffer[ByteCount] = 0;
    return Buffer;
}

wchar_t *UTF16FromUTF8(arena *Arena, char *String8, int String8Length)
{
    Assert(String8Length > 0);
    
    int Capacity = (String8Length * 2) + 2;
    wchar_t *Buffer = Allocate(Arena, Capacity, wchar_t);
    
    // Can be used with temp_arena or arena
    int CharacterCount = MultiByteToWideChar(CP_UTF8, 0, 
                                             String8, String8Length, 
                                             Buffer, Capacity);
    Buffer[CharacterCount] = 0;
    return Buffer;
}

void Win32InitOpenGL(HWND Window)
{
    HDC WindowDC = GetDC(Window);
    
    // We fill out some portion of pixel format then ask OS to find us a pixel format it supports
    // that most closely matches our format.
    PIXELFORMATDESCRIPTOR PixelFormat = {};
    PixelFormat.nSize = sizeof(PixelFormat);
    PixelFormat.nVersion = 1;
    PixelFormat.iPixelType = PFD_TYPE_RGBA;
    PixelFormat.dwFlags = PFD_SUPPORT_OPENGL|PFD_DRAW_TO_WINDOW|PFD_DOUBLEBUFFER;
    PixelFormat.cColorBits = 32; // The Docs say this is excluding alpha (so should be 23), but ChoosePixelFormat returns 32 bits... It should be correct to pass 32.
    PixelFormat.cAlphaBits = 8;
    PixelFormat.iLayerType = PFD_MAIN_PLANE;
    // Z-Buffer, Stencil buffer, aux buffer?
    
    int SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &PixelFormat);
    PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
    DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex, sizeof(SuggestedPixelFormat), &SuggestedPixelFormat);
    SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);
    
    // Handle to a GL Render Context 
    HGLRC OpenGLContext = wglCreateContext(WindowDC);
    
    // Make this thread have the render context
    // All OpenGL calls made by this thread are drawn to this DC
    if (wglMakeCurrent(WindowDC, OpenGLContext))
    {
        // Defined by OpenGL Spec
#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB             0x2093
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126
        
#define WGL_CONTEXT_DEBUG_BIT_ARB               0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
        
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
        // Context escalation:
        // Context versions guarantee that a set of functionality is available, rather that requiring us to 
        // get query extensions for each bit of functionality piecemeal, we can just check for version 3.0 then
        // get a 3.0 context.
        // This is the win32 create context which is only guaranteed to give us gl version 1.0 or higher (it should give us higher if it is available but only 1.0 is guaranteed). 
        // This is banannas because windows doesn't support OpenGL much, so graphics card hardare vendors had to do
        // it this way.
        wgl_create_context_attribs_arb *wglCreateContextAttribsARB = (wgl_create_context_attribs_arb * )wglGetProcAddress("wglCreateContextAttribsARB");
        
        if (wglCreateContextAttribsARB)
        {
            // We have a context for a modern version of OpenGL
            int Attribs[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 0,
                // Forward compatibility is only 3.0 or greater, and must not support functionality marked
                // as deprecated by that version of the api.
                WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB|WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0,
            };
            
            // A modern OpenGL context is not necesarily needed (as currently we only call glViewport, glClear, glClearColor) and ImGui loads pointers for all it's calls.
            HGLRC ModernOpenGLContext = wglCreateContextAttribsARB(WindowDC, 0, Attribs);
            if (ModernOpenGLContext)
            {
                if (wglMakeCurrent(WindowDC, ModernOpenGLContext))
                {
                    wglDeleteContext(OpenGLContext);
                    OpenGLContext = ModernOpenGLContext;
                }
                else 
                {
                    Assert(0);
                }
            }
            else
            {
                Assert(0);
            }
        }
        else
        {
            Assert(0);
        }
        
        
        wgl_swap_interval_ext *wglSwapInterval = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
        if (wglSwapInterval)
        {
            wglSwapInterval(1); // TODO: Correct value
        }
    }
    else
    {
        Assert(0);
    }
    
    
    ReleaseDC(Window, WindowDC);
}


// TODO: Knowning if the window is shown or hidden is just for reducing memory footprint when 
// not being displayed
LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    ImGui_ImplWin32_WndProcHandler(Window, Message, wParam, lParam);
    
    LRESULT Result = 0;
    switch (Message)
    {
        // I think this is not needed
        // WM_PAINT
        
        case WM_QUIT:
        {
            GlobalAppRunning = false;
        } break;
        
        case WM_CLOSE:
        {
            ShowWindow(Window, SW_HIDE);
        } break;
        
        case WM_SHOWWINDOW:
        {
            // Received when window is about to be shown or hidden
            
            // A window can be shown (or hidden) because another window is covering it due to maximisation (or minimisation) as well as the window itself being maximised (minimised).
            // See how SDL treats these cases.
            // LPARAM Status = Message.lParam;
            
            WPARAM Shown = wParam;
            if (Shown)
            {
                
            }
            else
            {
                // Hidden
            }
        } break;
        
        case CUSTOM_WM_SHOW_WINDOW:
        {
            // Received when an new instance of the app is executed with another instance already running
            ShowWindow(Window, SW_SHOW);
            ShowWindow(Window, SW_RESTORE); // un-minimise
            SetForegroundWindow(Window);
        } break;
        
        case WM_COMMAND:
        {
            int ID = LOWORD(wParam);
            if (ID == ID_TRAY_APP_MENU_OPEN)
            {
                // SW_SHOW alone doesn't unminimise if the program is minimised but not hidden
                ShowWindow(Window, SW_SHOW);
                ShowWindow(Window, SW_RESTORE); 
                
                //SetForegroundWindow(window);
                //BringWindowToTop(window);
                //SetFocus(window);
                //EnableWindow(window, true);
                //SetFocus(window);
                //SetActiveWindow(window);
                //SetCapture(window);
            }
            else if (ID == ID_TRAY_APP_MENU_EXIT)
            {
                GlobalAppRunning = false;
            }
        } break;
        
        case CUSTOM_WM_TRAY_ICON:
        {
            if (LOWORD(lParam) == WM_RBUTTONUP)
            {
                HMENU Menu = CreatePopupMenu();
                AppendMenu(Menu, MF_STRING, ID_TRAY_APP_MENU_OPEN, TEXT("Show Records Window"));
                AppendMenu(Menu, MF_STRING, ID_TRAY_APP_MENU_EXIT, TEXT("Exit"));
                
                POINT Mouse;
                GetCursorPos(&Mouse);
                SetForegroundWindow(Window); 
                
                BOOL button = TrackPopupMenu(Menu,
                                             0,
                                             Mouse.x,
                                             Mouse.y,
                                             0,
                                             Window,
                                             NULL);
                PostMessage(Window, WM_NULL, 0, 0);
                DestroyMenu(Menu);
            }
            else if (LOWORD(lParam) == WM_LBUTTONDBLCLK)
            {
                if (!IsWindowVisible(Window))
                {
                    ShowWindow(Window, SW_SHOW);
                    //ShowWindow(window, SW_RESTORE);
                    //SetForegroundWindow(window);
                }
            }
        } break;
        
        // TODO: Save to file because computer is turning off
        //case WM_QUERYENDSESSION:
        //case WM_ENDSESSION:
        
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            WPARAM VKCode = wParam;
            if (VKCode == VK_ESCAPE)
            {
                GlobalAppRunning = false;
            }
        } break;
        
        default:
        {
            // TASKBARCREATED is result of creating tray icon
            // cant do switch case bacause dynamic value
            if (Message == GlobalTaskbarCreatedMessage)
            {
                // TODO: re add tray icon
                // wait 3-5 seconds to allow explorer to initialise
                // then can try to re-add icon
                
                // TODO: If this doesn't work it can be impossible to access program without manually task manager killing the process and re-running, same goes for all tray icon code
                Sleep(6000);
                for (int i = 0; i < 10; ++i)
                {
                    if (Shell_NotifyIconA(NIM_ADD, &GlobalNotifyIconData))
                    {
                        // Success
                    }
                    else
                    {
                        Sleep(100);
                    }
                }
                
                // TODO: Maybe try to restart program here if we couldn't recreate the icon
            }
            
            Result = DefWindowProcA(Window, Message, wParam, lParam);
        } break;
    }
    
    return Result;
}

void Win32WaitMessageWithTimeout(u32 TimeoutMilliseconds)
{
    // NOTE: Waking on any event is not important when running 60fps for the UI
    // Don't wake on mouse move, stops task manager reporting excessive cpu usage on mouse moves, though only appears excessive because we are sleeping because GPU is waiting for a vblank.
    DWORD WakeMask = QS_ALLEVENTS ^ QS_MOUSEMOVE;
    DWORD Result = MsgWaitForMultipleObjects(0, nullptr, FALSE, TimeoutMilliseconds, WakeMask);
    if (Result == WAIT_OBJECT_0)
    {
        //strcpy(msg, "Wait object 0\n");
    }
    else if (Result == WAIT_ABANDONED_0)
    {
        //strcpy(msg, "Wait abandoned\n");
    }
    else if (Result == WAIT_TIMEOUT)
    {
        //strcpy(msg, "Wait Timeout\n");
    }
    else if (Result == WAIT_FAILED)
    {
        //strcpy(msg, "Wait failed\n");
        Assert(0);
    }
    else
    {
        //strcpy(msg, "other ...\n");
        Assert(0);
    }
    
    //auto after_wait = platform_get_time();
    //s64 time_waited = platform_get_microseconds_elapsed(new_time, after_wait);// / 1000;
    
    //OutputDebugString(msg);
    //sprintf(msg, "Time to update: %llims\n" "time waited: %llims\n", time_to_update, time_waited);
    //OutputDebugString(msg);
}

void Win32PumpMessages()
{
    MSG Message = {};
    while (true)
    {
        if (PeekMessage(&Message, NULL, NULL, NULL, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessageA(&Message);
        }
        else
        {
            break;
        }
    }
}

s64 Win32GetTime()
{
    LARGE_INTEGER Now;
    QueryPerformanceCounter(&Now);
    return Now.QuadPart;
}

s64 Win32GetMicrosecondsElaped(s64 start, s64 end)
{
    s64 Microseconds;
    Microseconds = end - start;
    Microseconds *= 1000000; // microseconds per second
    Microseconds /= GlobalPerformanceFrequency.QuadPart; // ticks per second
    return Microseconds;
}

void Win32FreeUIAutomation()
{
    if (GlobalUIAutomation.IsEditbox) GlobalUIAutomation.IsEditbox->Release();
    if (GlobalUIAutomation.IsCombobox) GlobalUIAutomation.IsCombobox->Release();
    if (GlobalUIAutomation.AndCond) GlobalUIAutomation.AndCond->Release();
    if (GlobalUIAutomation.IsToolbar) GlobalUIAutomation.IsToolbar->Release();
    if (GlobalUIAutomation.IsNavigation) GlobalUIAutomation.IsNavigation->Release();
    
    if (GlobalUIAutomation.IsDocument) GlobalUIAutomation.IsDocument->Release();
    if (GlobalUIAutomation.IsNotOffscreen) GlobalUIAutomation.IsNotOffscreen->Release();
    if (GlobalUIAutomation.IsGroup) GlobalUIAutomation.IsGroup->Release();
    
    VariantClear(&GlobalUIAutomation.NavigationName); 
    VariantClear(&GlobalUIAutomation.VariantOffscreenFalse); 
    if (GlobalUIAutomation.Interface) GlobalUIAutomation.Interface->Release();
    
    GlobalUIAutomation = {};
    
    CoUninitialize();
}

bool Win32InitUIAutomation()
{
    // TODO: Why don't we need to link ole32.lib?
    // Call this because we use CoCreateInstance in UIAutomation
    if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) == S_OK)
    {
        if (CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, IID_IUIAutomation, (void **)(&GlobalUIAutomation.Interface)) == S_OK)
        {
            IUIAutomation *Interface = GlobalUIAutomation.Interface;
            
            GlobalUIAutomation.NavigationName.vt = VT_BSTR;
            GlobalUIAutomation.NavigationName.bstrVal = SysAllocString(L"Navigation");
            if (GlobalUIAutomation.NavigationName.bstrVal)
            {
                GlobalUIAutomation.VariantOffscreenFalse.vt = VT_BOOL;
                GlobalUIAutomation.VariantOffscreenFalse.boolVal = 0;
                
                // For getting url editbox (when not fullscreen)
                HRESULT err1 = Interface->CreatePropertyCondition(UIA_NamePropertyId, 
                                                                  GlobalUIAutomation.NavigationName, &GlobalUIAutomation.IsNavigation);
                
                HRESULT err2 = Interface->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                                                  CComVariant(UIA_ToolBarControlTypeId), 
                                                                  &GlobalUIAutomation.IsToolbar);
                
                HRESULT err3 = Interface->CreateAndCondition(GlobalUIAutomation.IsNavigation, 
                                                             GlobalUIAutomation.IsToolbar, &GlobalUIAutomation.AndCond);
                
                HRESULT err4 = Interface->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                                                  CComVariant(UIA_ComboBoxControlTypeId), 
                                                                  &GlobalUIAutomation.IsCombobox);
                
                HRESULT err5 = Interface->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                                                  CComVariant(UIA_EditControlTypeId),
                                                                  &GlobalUIAutomation.IsEditbox);
                
                // // For getting url document (when fullscreen)
                HRESULT err6 = Interface->CreatePropertyCondition(UIA_ControlTypePropertyId, 
                                                                  CComVariant(UIA_GroupControlTypeId),
                                                                  &GlobalUIAutomation.IsGroup);
                
                HRESULT err7 = Interface->CreatePropertyCondition(UIA_IsOffscreenPropertyId, 
                                                                  GlobalUIAutomation.VariantOffscreenFalse,
                                                                  &GlobalUIAutomation.IsNotOffscreen);
                
                HRESULT err8 = Interface->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                                                  CComVariant(UIA_DocumentControlTypeId),
                                                                  &GlobalUIAutomation.IsDocument);
                
                if (err1 == S_OK && err2 == S_OK && err3 == S_OK && err4 == S_OK && err5 == S_OK &&
                    err6 == S_OK && err7 == S_OK && err8 == S_OK)
                {
                    return true;
                }
            }
        }
    }
    
    Win32FreeUIAutomation();
    return false;
}

struct win32_process_ids
{
    DWORD Parent;
    DWORD Child;
};

BOOL CALLBACK
Win32MyEnumChildWindowsCallback(HWND Window, LPARAM lParam)
{
    // From // https://stackoverflow.com/questions/32360149/name-of-process-for-active-window-in-windows-8-10
    win32_process_ids *ProcessIDs = (win32_process_ids *)lParam;
    DWORD PID = 0;
    GetWindowThreadProcessId(Window, &PID);
    if (PID != ProcessIDs->Parent)
    {
        ProcessIDs->Child = PID;
    }
    
    return TRUE; // Continue enumeration
}

// Return NULL on error
char *PlatformGetProgramFromWindow(arena *Arena, platform_window_handle WindowHandle)
{
    char *Result = nullptr;
    
    HWND Handle = (HWND)WindowHandle;
    
#if MONITOR_DEBUG
    // TODO: Just for debugging
    if (Handle == GlobalDesktopWindow)
    {
        return PushCString(Arena, "Desktop Window: debug");
    }
    else if (Handle == GlobalShellWindow)
    {
        return PushCString(Arena, "Shell Window: debug");
    }
#endif
    
    win32_process_ids ProcessIDs = {0, 0};
    GetWindowThreadProcessId(Handle, &ProcessIDs.Parent);
    ProcessIDs.Child = ProcessIDs.Parent; // In case it is a normal process
    
    // Modern universal windows apps in 8 and 10 have the process we want inside a parent process called
    // WWAHost.exe on Windows 8 and ApplicationFrameHost.exe on Windows 10.
    // So we search for a child pid that is different to its parent's pid, to get the process we want.
    // https://stackoverflow.com/questions/32360149/name-of-process-for-active-window-in-windows-8-10
    
    EnumChildWindows(Handle, Win32MyEnumChildWindowsCallback, (LPARAM)&ProcessIDs);
    HANDLE Process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, ProcessIDs.Child);
    if (Process)
    {
        u32 Capacity = 2048;
        wchar_t *Buffer = Allocate(Arena, Capacity, wchar_t);
        
        for (int Tries = 0; Tries < 4; ++Tries)
        {
            // Null terminates the string
            // Returned value in FilenameLength does not count the null terminator
            DWORD FilenameLength = (DWORD)Capacity;
            if (QueryFullProcessImageNameW(Process, 0, Buffer, &FilenameLength))
            {
                Result = UTF8FromUTF16(Arena, Buffer, FilenameLength);
                break;
            }
            else
            {
                Capacity *= 2;
            }
        }
        
        CloseHandle(Process);
    }
    
    return Result;
}


platform_window_handle PlatformGetActiveWindow()
{
#if RECORD_ALL_ACTIVE_WINDOWS
    static HWND Queue[100];
    static int Count = 0;
    static bool First = true;
    static bool Done = false;
    
    auto GetWindows = [](int &Count, HWND *Queue) {
        for (HWND Window = GetTopWindow(NULL); 
             Window != NULL && Count < 100; 
             Window = GetNextWindow(Window, GW_HWNDNEXT))
        {
            if (IsWindowVisible(Window))
            {
                Queue[Count++] = Window;
            }
        }
    };
    
    if (First)
    {
        GetWindows(Count, Queue);
        First = false;
    }
    
    HWND ForeGroundWindow = {};
    if (Count > 0)
    {
        ForeGroundWindow = Queue[Count-1];
        Count -= 1;
    }
    else
    {
        Done = true;
    }
    
    if (Done)
    {
        // Can return 0 is certain circumstances, such as when a window is losing activation
        ForeGroundWindow = GetForegroundWindow();
    }
#else
    
    HWND ForeGroundWindow = GetForegroundWindow();
    //window.is_valid = (foreground_win != 0 && foreground_win != win32_context.desktop_window && foreground_win != win32_context.shell_window);
    
    // TODO: If foreground_win == shell or desktop set to 0
#endif
    
    return (platform_window_handle)ForeGroundWindow;
}

// CheckError() for PlatformAPI (only actual errors e.g. File Read Error are errors, passing invalid params is
// not an 'error')
// This allows multiple flatform calls to happen from user side of API and then an CheckError() called at the end
// The platform side can have if (!Error) at the start of all of its functions

bool PlatformIsWindowHidden()
{
    // IsWindowVisible returns True if the vindow is totally obscured by other windows
    return !IsWindowVisible(GlobalWindowHandle);
}

IUIAutomationElement *
Win32FirefoxGetUrlEditbox(HWND Window)
{
    // If people have different firefox setup than me, (e.g. multiple comboboxes in navigation somehow), this doesn't work
    
    IUIAutomationElement *Editbox = nullptr;
    
    IUIAutomationElement *FirefoxWindow = nullptr;
    IUIAutomationElement *NavigationToolbar = nullptr;
    IUIAutomationElement *Combobox = nullptr;
    
    HRESULT err = GlobalUIAutomation.Interface->ElementFromHandle(Window, &FirefoxWindow);
    if (err != S_OK || !FirefoxWindow) goto CLEANUP;
    
    // oleaut.dll Library not registered - may just be app requesting newer oleaut than is available
    // This seems to throw exception
    err = FirefoxWindow->FindFirst(TreeScope_Children, GlobalUIAutomation.AndCond, &NavigationToolbar);
    if (err != S_OK || !NavigationToolbar) goto CLEANUP;
    
    err = NavigationToolbar->FindFirst(TreeScope_Children, GlobalUIAutomation.IsCombobox, &Combobox);
    if (err != S_OK || !Combobox) goto CLEANUP;
    
    err = Combobox->FindFirst(TreeScope_Descendants, GlobalUIAutomation.IsEditbox, &Editbox);
    if (err != S_OK) Editbox = nullptr;
    
    CLEANUP:
    
    if (Combobox) Combobox->Release();
    if (NavigationToolbar) NavigationToolbar->Release();
    if (FirefoxWindow) FirefoxWindow->Release();
    
    // If success dont release editbox, if failure editbox doesn't need releasing
    return Editbox;
}

IUIAutomationElement *
Win32FirefoxGetUrlDocumentIfFullscreen(HWND Window)
{
    IUIAutomationElement *Document = nullptr;
    
    IUIAutomationElement *FirefoxWindow = nullptr;
    IUIAutomationElement *NotOffscreenElement = nullptr;
    // NOTE: After firefox update there is now no group
    IUIAutomationElement *Group = nullptr;
    
    HRESULT err = GlobalUIAutomation.Interface->ElementFromHandle(Window, &FirefoxWindow);
    if (err != S_OK || !FirefoxWindow) goto CLEANUP;
    
    //0,0,w,h
    //VARIANT bounding_rect = {};
    //err = FirefoxWindow->GetCurrentPropertyValue(UIA_BoundingRectanglePropertyId, &bounding_rect);
    
    
    // TODO: Make sure this is only Done once and stuff is freed
    VARIANT EnabledTrue;
    EnabledTrue.vt = VT_BOOL;
    EnabledTrue.boolVal = -1; // -1 is true
    // TODO: Variant clear
    
    
    IUIAutomationCondition *IsEnabled = nullptr; 
    IUIAutomationCondition *IsEnabledNotOffscreen = nullptr; 
    
    HRESULT err2 = 
        GlobalUIAutomation.Interface->CreatePropertyCondition(UIA_IsEnabledPropertyId, 
                                                              EnabledTrue, &IsEnabled);
    HRESULT err6 = GlobalUIAutomation.Interface->CreateAndCondition(IsEnabled, GlobalUIAutomation.IsNotOffscreen, &IsEnabledNotOffscreen);
    
    
    err = FirefoxWindow->FindFirst(TreeScope_Children, IsEnabledNotOffscreen, &NotOffscreenElement);
    if (err != S_OK || !NotOffscreenElement) goto CLEANUP;
    
    // Try to skip over one child using TreeScope_Descendants
    err = NotOffscreenElement->FindFirst(TreeScope_Descendants, GlobalUIAutomation.IsDocument, &Document);
    if (err != S_OK || !Document) Document = nullptr;
    
    CLEANUP:
    if (NotOffscreenElement) NotOffscreenElement->Release();
    //if (Group) Group->Release();
    if (FirefoxWindow) FirefoxWindow->Release();
    
    return Document;
}


bool
Win32WindowIsFullscreen(HWND Window)
{
    RECT WindowRect;
    if (GetWindowRect(Window, &WindowRect) == 0)
    {
        Assert(0);
    }
    
    int WindowWidth = WindowRect.right - WindowRect.left;
    int WindowHeight = WindowRect.bottom - WindowRect.top;
    
    // Gets closest monitor to window
    MONITORINFO MonitorInfo = {sizeof(MONITORINFO)}; // must set size of struct
    if (GetMonitorInfoA(MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST), &MonitorInfo) == 0)
    {
        Assert(0);
    }
    
    // Expressed in virtual-screen coordinates.
    // NOTE: that if the monitor is not the primary display monitor, some of the rectangle's coordinates may be negative values.
    // I think this works if left and right are negative
    int MonitorWidth = MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left;
    int MonitorHeight =  MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top;
    
    return (WindowWidth == MonitorWidth && WindowHeight == MonitorHeight);
}

bool
Win32FirefoxElementHasKeyboardFocus(IUIAutomationElement *Element) 
{
    bool Result = false;
    VARIANT HasFocus;
    HasFocus.vt = VT_BOOL;
    HasFocus.boolVal = 0;
    
    HRESULT err = Element->GetCurrentPropertyValue(UIA_HasKeyboardFocusPropertyId, &HasFocus);
    if (err == S_OK)
    {
        if (HasFocus.boolVal)
        {
            Result = (HasFocus.boolVal == -1); // -1 == true
        }
    }
    
    VariantClear(&HasFocus);
    if (Result)
        OutputDebugStringA("Has keyboard focus\n");
    else
        OutputDebugStringA("No keyboard focus\n");
    
    return Result;
}

char *Win32FirefoxGetUrlFromElement(arena *Arena, IUIAutomationElement *Element)
{
    char *Url = nullptr;
    
    bool Success = false;
    VARIANT UrlBar = {};
    HRESULT Err = Element->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &UrlBar);
    if (Err == S_OK)
    {
        if (UrlBar.bstrVal)
        {
            size_t UrlLength = SysStringLen(UrlBar.bstrVal);
            if (UrlLength > 0)
            {
                // bstrVal is a BSTR which is a pointer to a WCHAR
                Url = UTF8FromUTF16(Arena, UrlBar.bstrVal, UrlLength);
            }
        }
    }
    
    VariantClear(&UrlBar);
    return Url;
}

// Non-caching version
char *PlatformFirefoxGetUrl(arena *Arena, platform_window_handle WindowHandle)
{
    char *Url = nullptr;
    
    HWND hwnd = (HWND)WindowHandle;
    bool IsFullscreen = Win32WindowIsFullscreen(hwnd);
    
    IUIAutomationElement *Element = nullptr;
    if (IsFullscreen)
        Element = Win32FirefoxGetUrlDocumentIfFullscreen(hwnd);
    else
        Element = Win32FirefoxGetUrlEditbox(hwnd);
    
    if (Element)
    {
        if (Win32FirefoxElementHasKeyboardFocus(Element))
        {
            // Don't get URL because URL bar is being edited
        }
        else 
        {
            Url = Win32FirefoxGetUrlFromElement(Arena, Element);
        }
        
        Element->Release();
    }
    
    return Url;
}

#if 0
char *PlatformFirefoxGetUrl(arena *Arena, platform_window_handle WindowHandle)
{
    // NOTE: IMPORTANT: Inspect has different views, 'raw view' doesn't seem to correspond to the way i'm using the api currently, for example there is no Navigation toolbar element in raw view.
    
    // If could search for no name elements then would be able to have single method that works for fullscreen and normal, by looking for no name, enabled, not offscreen. 
    
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
    
    HWND hwnd = (HWND)WindowHandle;
    
    bool Success = false;
    
    static HWND LastHwnd = 0; // if != then re-get firefox_window and re get editbox or document etc
    //static IUIAutomationElement *firefox_window = nullptr;
    static IUIAutomationElement *Editbox = nullptr;
    static IUIAutomationElement *Document = nullptr;
    
    bool IsFullscreen = Win32WindowIsFullscreen(hwnd);
    
    // TODO: Get firefox window once-ish (is this even a big deal if we have to re_load element it is just one extra call anyway)
    // TODO: We are just redundantly trying twice to load element if it fails (when matching hwnd)
    
    // TODO: Possible cases
    // 1. alt tab switching from one fullscreen firefox window to another
    // 2. Switching between tabs that are both fullscreen (might not be possible)
    
    // NOTE: Maybe safest thing is to just do all the work everytime, and rely and events to tell us when
    // Counterpoint, try to do the fast thing and then fall back to slow thing (what i'm trying to do)
    
    // If screenmode changes load other element
    bool ReLoadFirefoxElement = true;
    if (LastHwnd == hwnd)
    {
        if (IsFullscreen)
        {
            if (Editbox) Editbox->Release();
            Editbox = nullptr;
            
            if (!Document)
            {
                Document = Win32FirefoxGetURLDocumentIfFullscreen(hwnd);
            }
            if (Document)
            {
                // TODO: Assert that this is onscreen (the current tab)
                
                if (Win32FirefoxElementHasKeyboardFocus(Document))
                {
                    // Dont get urls that may be currently being edited
                    Success = false;
                    ReLoadFirefoxElement = false;
                }
                else if (Win32FirefoxGetURLFromElement(Document, Buf, MaxUrlLen, Length))
                {
                    // Don't release element (try to reuse next time)
                    Success = true;
                    ReLoadFirefoxElement = false;
                }
            }
        }
        else
        {
            if (Document) Document->Release();
            Document = nullptr;
            
            if (!Editbox)
            {
                Editbox = Win32FirefoxGetURLEditbox(hwnd);
            }
            if (Editbox)
            {
                if (Win32FirefoxElementHasKeyboardFocus(Editbox))
                {
                    Success = false;
                    ReLoadFirefoxElement = false;
                }
                else if (Win32FirefoxGetURLFromElement(Editbox, Buf, MaxUrlLen, Length))
                {
                    // Don't release element (try to reuse next time)
                    Success = true;
                    ReLoadFirefoxElement = false;
                }
            }
        }
    }
    
    if (ReLoadFirefoxElement)
    {
        // Free cached elements of other (old) window
        if (Editbox) Editbox->Release();
        if (Document) Document->Release();
        //if (firefox_window) firefox_window->Release();
        Editbox = nullptr;
        Document = nullptr;
        //firefox_window = nullptr;
        
        IUIAutomationElement *Element = nullptr;
        if (IsFullscreen)
            Element = Win32FirefoxGetURLDocumentIfFullscreen(hwnd);
        else
            Element = Win32FirefoxGetURLEditbox(hwnd);
        
        if (Element)
        {
            if (Win32FirefoxElementHasKeyboardFocus(Element))
            {
                Success = false;
            }
            else if (Win32FirefoxGetURLFromElement(Element, Buf, MaxUrlLen, Length))
            {
                // Don't release element (try to reuse next time)
                Success = true;
                if (IsFullscreen)
                    Document = Element;
                else
                    Editbox = Element;
            }
            else
            {
                // Could not get url from window
                Element->Release();
            }
        }
        else
        {
            // Could not get url from window
        }
        
        LastHwnd = hwnd;
    }
    
    return Success;
    
    
    // NOTE: window will always be the up to date firefox window, even if our editbox pointer is old.
    // NOTE: Even when everything else is released it seems that you can still use editbox pointer, unless the window is closed. Documentation does not confirm this, so i'm not 100% sure it's safe.
    
    // NOTE: This is duplicated because the editbox pointer can be valid, but just stale, and we only know that if GetCurrentPropertyValue fails, in which case we free the pointer and try to reacquire the editbox.
    
    // BUG: If we get a valid editbox from a normal firfox window, then go fullscreen, the editbox pointer is not set to null, so we continually read an empty string from the property value, without an error (not sure why no error is set by windows).
}
#endif

//-
// Icon API

bool
Win32GetBitmapDataFromHICON(HICON Icon, s32 Dimension, u32 *Pixels)
{
    bool Success = false;
    
    // TODO: Get Icon from UWP:
    // Process is wrapped in a parent process, and can't extract icons from the child, not sure about parent
    // SO might need to use SHLoadIndirectString, GetPackageInfo could be helpful.
    
    //GetIconInfoEx creates bitmaps for the hbmMask and hbmCol or members of ICONINFOEX. The calling application must manage these bitmaps and delete them when they are no longer necessary.
    
    // These bitmaps are stored as bottom up bitmaps, so the bottom row of image is stored in the first 'row' in memory.
    ICONINFO ii;
    BITMAP ColourMask;
    BITMAP AndMask;
    HBITMAP AndMaskHandle = 0;
    HBITMAP ColoursHandle = 0;
    
    if (Icon && GetIconInfo(Icon, &ii))
    {
        // It is necessary to call DestroyObject for icons and cursors created with CopyImage
        ColoursHandle = (HBITMAP)CopyImage(ii.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        AndMaskHandle = (HBITMAP)CopyImage(ii.hbmMask, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        if (ColoursHandle && AndMaskHandle)
        {
            // GetObject uses Gdi
            int Result1 = GetObject(ColoursHandle, sizeof(BITMAP), &ColourMask) == sizeof(BITMAP);
            int Result2 = GetObject(AndMaskHandle, sizeof(BITMAP), &AndMask) == sizeof(BITMAP);
            if (Result1 && Result2)
            {
                if (ColourMask.bmWidth == Dimension && ColourMask.bmHeight == Dimension && Pixels)
                {
                    b32 HadAlpha = false;
                    u32 *Dest = Pixels;
                    u8 *SrcRow = (u8 *)ColourMask.bmBits + (ColourMask.bmWidthBytes * (ColourMask.bmHeight-1));
                    for (int y = 0; y < ColourMask.bmHeight; ++y)
                    {
                        u32 *Src = (u32 *)SrcRow;
                        for (int x = 0; x < ColourMask.bmWidth; ++x)
                        {
                            u32 Col = *Src++;
                            *Dest++ = Col;
                            HadAlpha |= (u8)(Col >> 24); // Alpha Component
                        }
                        
                        SrcRow -= ColourMask.bmWidthBytes;
                    }
                    
                    // Some icons have no set alpha channel in the colour mask, so specify it in the and mask.
                    if (!HadAlpha)
                    {
                        int AndMaskPitch = AndMask.bmWidthBytes;
                        u32 *Dest = Pixels;
                        u8 *SrcRow = (u8 *)AndMask.bmBits + (AndMask.bmWidthBytes * (AndMask.bmHeight-1));
                        for (int y = 0; y < AndMask.bmHeight; ++y)
                        {
                            u8 *Src = SrcRow;
                            for (int x = 0; x < AndMaskPitch; ++x)
                            {
                                for (int i = 7; i >= 0; --i)
                                {
                                    if (!(Src[x] & (1 << i))) *Dest |= 0xFF000000;
                                    Dest++;
                                }
                            }
                            
                            SrcRow -= AndMaskPitch;
                        }
                    }
                    
                    Success = true;
                }
            }
        }
    }
    
    if (ColoursHandle)  DeleteObject(ColoursHandle);
    if (AndMaskHandle) DeleteObject(AndMaskHandle);
    
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    
    return Success;
}

bool
Win32GetDefaultIcon(u32 DesiredSize, bitmap *Bitmap)
{
    // NOTE: You don't need to delete LoadIcon hicon
    bool Success = false;
    HICON IconHandle = (HICON)LoadIcon(NULL, IDI_APPLICATION);
    if (IconHandle )
    {
        // TODO: Not sure how exactly to handle platform icon getting with checking sizes, resizing, allocating
        u32 *AllocatedPixels = (u32 *)calloc(1, 4*ICON_SIZE*ICON_SIZE);
        if (Win32GetBitmapDataFromHICON(IconHandle, DesiredSize, AllocatedPixels))
        {
            Bitmap->Width = DesiredSize;
            Bitmap->Height = DesiredSize;
            Bitmap->Pitch = DesiredSize*4;
            Bitmap->Pixels = AllocatedPixels;
            Success = true;
        }
    }
    
    return Success;
}

bool
PlatformGetIconFromExecutable(char *Path, u32 DesiredSize, bitmap *Bitmap, u32 *AllocatedBitmapMemory)
{
    Assert(AllocatedBitmapMemory);
    
    // path must be null terminated
    // If icon is not desired size returns false
    // Pass in memory to write icon bitmap to
    
    // NOTE: These must be destroyed
    HICON SmallIconHandle = 0;
    HICON IconHandle = 0;
    
    // TODO: maybe just use extract icon and resize
    if(SHDefExtractIconA(Path, 0, 0, &IconHandle, &SmallIconHandle, DesiredSize) != S_OK)
    {
        return false;
    }
    
    bool Success = false;
    if (IconHandle)
    {
        Success = Win32GetBitmapDataFromHICON(IconHandle, DesiredSize, AllocatedBitmapMemory);
    }
    else if (SmallIconHandle)
    {
        Success = Win32GetBitmapDataFromHICON(SmallIconHandle, DesiredSize, AllocatedBitmapMemory);
    }
    
    // NOTE: Don't need to destroy LoadIcon icons I'm pretty sure, but SHDefExtractIconA does need to be destroyed
    if (IconHandle) DestroyIcon(IconHandle);
    if (SmallIconHandle) DestroyIcon(SmallIconHandle);
    
    if (Success)
    {
        Bitmap->Width = DesiredSize;
        Bitmap->Height = DesiredSize;
        Bitmap->Pitch = DesiredSize*4;
        Bitmap->Pixels = AllocatedBitmapMemory;
    }
    
    return Success;
}


//-
// File API

// Returns null terminated string
// Returns NULL on error (or maybe sets PlatformError = true?)
// May be prefixed with \? stuff
char *PlatformGetExecutablePath(arena *Arena)
{
    u64 Capacity = 2048;
    wchar_t *Buffer = Allocate(Arena, Capacity, wchar_t);
    
    u32 Size = 0;
    for (int Tries = 0; Tries < 4; ++Tries)
    {
        Size = GetModuleFileNameW(NULL, Buffer, Capacity);
        if (Size == Capacity && (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        {
            // String truncated to Size characters (including null terminator)
            // Try again with larger Buffer
            Capacity *= 2;
            Buffer = Allocate(Arena, Capacity, wchar_t);
        }
        else
        {
            // Error or Success
            break;
        }
    }
    
    // If succeeded then Size is the length of the Path (NOT including the null terminator). 
    // If failed then Size is 0.
    // Or if we failed 4 times Size will == Capacity (TODO: What happens if size is capacity but wan't truncated)?
    // will GetLastError just not be set and we can use path.
    
    // From docs:
    // "If the length of the path is less than the size that the nSize parameter specifies, the function succeeds and the path is returned as a null-terminated string."
    // "And if exceeds then truncates..."
    
    
    char *Result = nullptr;
    if (Size > 0)
    {
        Result = UTF8FromUTF16(Arena, Buffer, Size);
    }
    
    return Result;
}



bool PlatformFileExists(arena *Arena, char *Filename)
{
    // I think you need to prepend \\?\ to exceed 260 Characters
    // TODO: Test with a long path
    // This null terminates for us
    wchar_t *Filename16 = UTF16FromUTF8(Arena, Filename, StringLength(Filename));
    DWORD Result = GetFileAttributesW(Filename16);
    bool IsDirectory = Result & FILE_ATTRIBUTE_DIRECTORY;
    bool Exists = Result != INVALID_FILE_ATTRIBUTES; 
    return Exists && !IsDirectory;
}

s64 
PlatformGetFileSize(FILE *file)  
{
    // Bad because messes with seek position
    // TODO: Pretty sure it is bad to seek to end of file
    fseek(file, 0, SEEK_END); // Not portable. TODO: Use os specific calls
    long int size = ftell(file);
    fseek(file, 0, SEEK_SET); // Not portable. TODO: Use os specific calls
    return (s64) size;
}

platform_file_contents
PlatformReadEntireFile(char *FileName)
{
    platform_file_contents Result = {};
    
    FILE *File = fopen(FileName, "rb");
    if (File)
    {
        s64 FileSize = PlatformGetFileSize(File);
        if (FileSize > 0)
        {
            u8 *Data = (u8 *)malloc(FileSize);
            if (Data)
            {
                if (fread(Data, 1, FileSize, File) == FileSize)
                {
                    Result.Data = Data;
                    Result.Size = FileSize;
                }
                else
                {
                    free(Data);
                }
            }
        }
        
        // I don't think it matters if this fails as we already have the file data
        fclose(File);
    }
    
    return Result;
}

// It is possible to reserve and commit at once on windows

void *PlatformMemoryReserve(u64 Size)
{
    void *Memory = VirtualAlloc(0, Size, MEM_RESERVE, PAGE_READWRITE);
    return Memory;
}

void *PlatformMemoryCommit(void *Address, u64 Size)
{
    void *Memory = VirtualAlloc(0, Size, MEM_COMMIT, PAGE_READWRITE);
    return Memory;
}

// Frees all pages with a byte in the range from Address to Address+Size
void PlatformMemoryDecommit(void *Address, u64 Size)
{
    VirtualFree(Address, Size, MEM_DECOMMIT);
}

void PlatformMemoryFree(void *Address)
{
    VirtualFree(Address, 0, MEM_RELEASE);
}

//-


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    HWND AlreadyRunningInstance = FindWindowA(WINDOW_CLASS_NAME, PROGRAM_NAME);
    if (AlreadyRunningInstance)
    {
        SendMessage(AlreadyRunningInstance, CUSTOM_WM_SHOW_WINDOW, 0, 0); 
        return 0;
    }
    
    if (!Win32InitUIAutomation())
    {
        return 1;
    }
    
    QueryPerformanceFrequency(&GlobalPerformanceFrequency);
    
    // If these are null make sure platform_get_active_window doesn't compare a null window with these null windows
    GlobalDesktopWindow = GetDesktopWindow();
    GlobalShellWindow = GetShellWindow();
    
    // Register a message that is sent when the taskbar restarts
    GlobalTaskbarCreatedMessage = RegisterWindowMessage(TEXT("TaskbarCreated"));
    if (GlobalTaskbarCreatedMessage == 0)
    {
        return 1;
    }
    
    
    // Should we set icon manually?
#if 0
    HICON h_icon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_MAIN_ICON));
    if (h_icon)
    {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)h_icon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)h_icon); // ??? both???
    }
#endif
    
    
    WNDCLASSA WindowClass = {};
    WindowClass.style         = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc   = WindowProc;
    WindowClass.hInstance     = hInstance;
    WindowClass.lpszClassName = WINDOW_CLASS_NAME;
    WindowClass.hCursor       = LoadCursor(0, IDC_ARROW);
    if (!RegisterClassA(&WindowClass))
    {
        return 1;
    }
    
    HWND Window = CreateWindowExA(0,                              // Optional window styles.
                                  WindowClass.lpszClassName,      // Window class
                                  PROGRAM_NAME,                   // Window text
                                  WS_OVERLAPPEDWINDOW,            // Window style
                                  // Size and position
                                  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                  NULL,       // Parent window    
                                  NULL,       // Menu
                                  hInstance,  // Instance handle
                                  NULL);      // Additional application data
    if (Window == NULL)
    {
        return 1;
    }
    
    // TODO: Proper Defers...
    
    GlobalWindowHandle = Window;
    
    ShowWindow(Window, nCmdShow);
    
    // TODO: To maintain compatibility with older and newer versions of shell32.dll while using
    // current header files may need to check which version of shell32.dll is installed so the
    // cbSize of NOTIFYICONDATA can be initialised correctly.
    // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa
    ZeroMemory(&GlobalNotifyIconData, sizeof(GlobalNotifyIconData));
    GlobalNotifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    GlobalNotifyIconData.hWnd = Window;
    GlobalNotifyIconData.uID = ID_TRAY_APP_ICON; 
    GlobalNotifyIconData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
    GlobalNotifyIconData.uCallbackMessage = CUSTOM_WM_TRAY_ICON;
    // Recommented you provide 32x32 icon for higher DPI systems
    // Can use LoadIconMetric to specify correct one with correct settings is used
    GlobalNotifyIconData.hIcon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_MAIN_ICON));
    
    TCHAR tooltip[] = {PROGRAM_NAME}; 
    static_assert(ArrayCount(tooltip) <= 64, "Tooltip must be 64 characters");
    strncpy(GlobalNotifyIconData.szTip, tooltip, sizeof(tooltip));
    
    // TODO: If this doesn't work it can be impossible to access program without manually task manager killing the process and re-running, same goes for all tray icon code
    if (Shell_NotifyIconA(NIM_ADD, &GlobalNotifyIconData) == FALSE)
    {
        return 1;
    }
    
    Win32InitOpenGL(Window);
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGui_ImplWin32_Init(Window);
    ImGui_ImplOpenGL3_Init("#version 130"); // GLSL 130 (OpenGL 3.0)
    
    HDC DeviceContext = GetDC(Window);
    
    // TODO: Get monitor refresh rate
    static constexpr f32 UPDATES_PER_SECOND_WINDOW_VISIBLE = 60.0f; 
    static constexpr f32 UPDATES_PER_SECOND_BACKGROUND = 1.0f;
    
    f32 UpdatesPerSecond = 0.0f;
    auto SecondsToMilliseconds = [](f32 Seconds) { return (u32)(Seconds * 1000.0f); };
    
    s64 OldTime = 0;
    
    GlobalAppRunning = true;
    while (GlobalAppRunning)
    {
        // Uses last frames update time
        f32 SecondsPerUpdate = 1.0f / UpdatesPerSecond; // TODO: BUG! UpdatesPerSecond = 0
        Win32WaitMessageWithTimeout(SecondsToMilliseconds(SecondsPerUpdate)); 
        Win32PumpMessages();
        
        bool WindowHidden = PlatformIsWindowHidden();
        UpdatesPerSecond = WindowHidden ? UPDATES_PER_SECOND_BACKGROUND : UPDATES_PER_SECOND_WINDOW_VISIBLE;
        
        ImGui_ImplWin32_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        
        // ------------
        // Non-platform stuff
        
        s64 NewTime = Win32GetTime();
        s64 dt  = Win32GetMicrosecondsElaped(OldTime, NewTime); 
        OldTime = NewTime;
        
        
        // Update(dt);
        
        
        ImGui::NewFrame();
        ImGui::Render();
        
        ImGuiIO& io = ImGui::GetIO();
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        // Using Windows OpenGL bindings
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // ------------
        
        if (!WindowHidden)
        {
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); 
            SwapBuffers(DeviceContext);
        }
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    
    ImGui::DestroyContext();
    
    Win32FreeUIAutomation();
    
    // Delete tray icon
    Shell_NotifyIconA(NIM_DELETE, &GlobalNotifyIconData);
    
    return 0;
}