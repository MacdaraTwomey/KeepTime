#include "ravel.h"

#include "helper.h"
#include "monitor.h"
#include "resource.h"

#include <chrono>
#include <vector>
#include <algorithm>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <AtlBase.h>
#include <UIAutomation.h>

#include <shellapi.h>
#include <shlobj_core.h> // SHDefExtractIconA 

#include <windows.h>

#include "date.h" 

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

using namespace date;

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");


int bar_width = 40;
int bar_spacing = 30;
int line_thickness = 2;
int backtail = 10;
int x_axis_length = 500;
int y_axis_height = 300;


#include "utilities.cpp"
#include "helper.cpp"
#include "file.cpp"
#include "icon.cpp"
#include "render.cpp"

#define CONSOLE_ON 1

static Screen_Buffer global_screen_buffer;
static char *global_savefile_path;
static char *global_debug_savefile_path;
static bool global_running = false;
static Queue<Event> global_event_queue;

// NOTE: This is only used for toggling with the tray icon.
// Use pump messages result to check if visible to avoid the issues with value unexpectly changing.
// Still counts as visible if minimised to the task bar.

static NOTIFYICONDATA global_nid = {};

static constexpr int WindowWidth = 960;
static constexpr int WindowHeight = 540;

// TODO: Think of better way than just having error prone max amounts.
static constexpr u32 MaxDailyRecords = 1000;
static constexpr u32 MaxDays = 10000;
static constexpr u32 DefaultDayAllocationCount = 30;

#define ID_DAY_BUTTON 200
#define ID_WEEK_BUTTON 201
#define ID_MONTH_BUTTON 202

// -----------------------------------------------------------------
// TODO:
// * !!! If GUI is visible don't sleep. But we still want to poll infrequently. So maybe check elapsed poll time.
// * Remember window width and height
// * Unicode correctness
// * Path length correctnedd
// * Dynamic window, button layout with resizing
// * OpenGL graphing?
// * Stop repeating work getting the firefox URL
// * Get favicon from webpages, maybe use libcurl and a GET request for favicon.ico
// -----------------------------------------------------------------

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

bool get_firefox_url(HWND window, char *URL, size_t *URL_len)
{
    // Error or warning in VS output:
    // mincore\com\oleaut32\dispatch\ups.cpp(2125)\OLEAUT32.dll!00007FFCC7DC1D33: (caller: 00007FFCC7DC1EAA) ReturnHr(1) tid(1198) 8002801D Library not registered.
    // This error can be caused by the application requesting a newer version of oleaut32.dll than you have.
    // To fix I think you can just download newer version of oleaut32.dll.
    
    CComQIPtr<IUIAutomation> uia;
    if(FAILED(uia.CoCreateInstance(CLSID_CUIAutomation)) || !uia)
        return false;
    
    CComPtr<IUIAutomationElement> element;
    if(FAILED(uia->ElementFromHandle(window, &element)) || !element)
        return false;
    
    //initialize conditions
    CComPtr<IUIAutomationCondition> toolbar_cond;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                 CComVariant(UIA_ToolBarControlTypeId), &toolbar_cond);
    
    CComPtr<IUIAutomationCondition> combobox_cond;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId, 
                                 CComVariant(UIA_ComboBoxControlTypeId), &combobox_cond);
    
    CComPtr<IUIAutomationCondition> editbox_cond;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId, 
                                 CComVariant(UIA_EditControlTypeId), &editbox_cond);
    
    // Find the top toolbars
    // Throws an exception on my machine, but doesn't crash program. This may just be a debug printout only
    // and not a problem.
    CComPtr<IUIAutomationElementArray> toolbars;
    if (FAILED(element->FindAll(TreeScope_Children, toolbar_cond, &toolbars)) || !toolbars)
        return false;
    
    int toolbars_count = 0;
    toolbars->get_Length(&toolbars_count);
    for(int i = 0; i < toolbars_count; i++)
    {
        CComPtr<IUIAutomationElement> toolbar;
        if(FAILED(toolbars->GetElement(i, &toolbar)) || !toolbar)
            continue;
        
        //find the comboxes for each toolbar
        CComPtr<IUIAutomationElementArray> comboboxes;
        if(FAILED(toolbar->FindAll(TreeScope_Children, combobox_cond, &comboboxes)) || !comboboxes)
            return false;
        
        int combobox_count = 0;
        comboboxes->get_Length(&combobox_count);
        for(int j = 0; j < combobox_count; j++)
        {
            CComPtr<IUIAutomationElement> combobox;
            if(FAILED(comboboxes->GetElement(j, &combobox)) || !combobox)
                continue;
            
            CComVariant test;
            if(FAILED(combobox->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &test)))
                continue;
            
            //we are interested in a combobox which has no lable
            if(wcslen(test.bstrVal))
                continue;
            
            //find the first editbox
            CComPtr<IUIAutomationElement> edit;
            if(FAILED(combobox->FindFirst(TreeScope_Descendants, editbox_cond, &edit)) || !edit)
                continue;
            
            // CComVariant contains a bstr in a union
            // CComVariant calls destructor, which calls VariantClear (which may free BSTR?)
            CComVariant bstr;
            if(FAILED(edit->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &bstr)))
                continue;
            
            // A null pointer is the same as an empty string to a BSTR. An empty BSTR is a pointer to a zero-length string. It has a single null character to the right of the address being pointed to, and a long integer containing zero to the left. A null BSTR is a null pointer pointing to nothing. There can't be any characters to the right of nothing, and there can't be any length to the left of nothing. Nevertheless, a null pointer is considered to have a length of zero (that's what SysStringLen returns). 
            
            // On my firefox a empty URL bar gives a length of 0
            
            
            //https://docs.microsoft.com/en-us/previous-versions/87zae4a3(v=vs.140)?redirectedfrom=MSDN#atl70stringconversionclassesmacros
            // From MSVC: To convert from a BSTR, use COLE2[C]DestinationType[EX], such as COLE2T.
            // An OLE character is a Wide (W) character, a TCHAR is a Wide char if _UNICODE defined, elsewise it is an ANSI char
            
            // BSTR is a pointer to a wide character, with a byte count before the pointer and a (2 byte?) null terminator
            if (bstr.bstrVal)
            {
                size_t len = wcslen(bstr.bstrVal);
                // Probably just use stack, but maybe not on ATL 7.0 (on my PC calls alloca)
                // On my PC internally uses WideCharToMultiByte
                USES_CONVERSION; // disable warnings for conversion macro
                if (len > 0)
                {
                    strcpy(URL, OLE2CA(bstr.bstrVal));
                    rvl_assert(len == strlen(URL));
                }
                
                *URL_len = len;
            }
            else
            {
                *URL_len = 0;
            }
            
            // need to call SysFreeString?
            
            return true;
        }
    }
    return false;
}

void
add_program_duration(Hash_Table *all_programs, Day *day, double dt, char *program_name)
{
    rvl_assert(all_programs && day && dt > 0 && program_name && strlen(program_name) > 0);
    
    bool program_in_day = false;
    u32 ID = 0;
    bool in_table = all_programs->search(program_name, &ID);
    
    if (in_table)
    {
        for (u32 i = 0; i < day->record_count; ++i)
        {
            if (ID == day->records[i].ID)
            {
                program_in_day = true;
                day->records[i].duration += dt;
                break;
            }
        }
    }
    else
    {
        ID = all_programs->count;
        all_programs->add_item(program_name, ID);
    }
    
    if (!program_in_day)
    {
        rvl_assert(day->record_count < MaxDailyRecords);
        day->records[day->record_count] = {ID, dt};
        day->record_count += 1;
    }
}

void
resize_screen_buffer(Screen_Buffer *buffer, int new_width, int new_height)
{
    if (buffer->data)
    {
        free(buffer->data);
    }
    
    buffer->width = new_width;
    buffer->height = new_height;
    
    buffer->bitmap_info = {}; 
    buffer->bitmap_info.bmiHeader.biSize = sizeof(buffer->bitmap_info.bmiHeader);
    buffer->bitmap_info.bmiHeader.biWidth = buffer->width;
    buffer->bitmap_info.bmiHeader.biHeight = -buffer->height; // Negative height to tell Windows this is a top-down bitmap
    buffer->bitmap_info.bmiHeader.biPlanes = 1;
    buffer->bitmap_info.bmiHeader.biBitCount = 32;
    buffer->bitmap_info.bmiHeader.biCompression = BI_RGB;
    
    buffer->pitch = buffer->width * Screen_Buffer::BYTES_PER_PIXEL;
    buffer->data = xalloc((buffer->width * buffer->height) * Screen_Buffer::BYTES_PER_PIXEL);
}



void
paint_window(Screen_Buffer *buffer, HDC device_context, int window_width, int window_height)
{
    // Causing window flickering
#if 0
    PatBlt(device_context, 
           0, 0,
           buffer->width, buffer->height,
           BLACKNESS);
#endif 
    // GetClientRect gets slightly smaller dimensions
    //rvl_assert(window_width == buffer->width && window_height == buffer->height);
    
    // Don't stretch for now, just to keep things simple
    StretchDIBits(device_context,
                  0, 0,                          // dest rect upper left coordinates
                  buffer->width, buffer->height, // dest rect width height
                  0, 0,                          // src upper left coordinates
                  buffer->width, buffer->height, // src rect width height
                  buffer->data,
                  &buffer->bitmap_info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}


size_t
get_forground_window_path(HWND foreground_win, char *buffer, size_t buf_size)
{
    // Must have room for program name
    size_t len = 0;
    if (foreground_win)
    {
        DWORD process_id;
        GetWindowThreadProcessId(foreground_win, &process_id); 
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_id);
        if (process)
        {
            // TODO: Try resizing full_path buffer if too small, and retrying.
            // filename_len is overwritten with number of characters written to full_path
            rvl_assert(buf_size <= UINT32_MAX);
            DWORD filename_len = (DWORD)buf_size;
            if (QueryFullProcessImageNameA(process, 0, buffer, &filename_len))
            {
                CloseHandle(process); 
                return (size_t)filename_len;
            }
            else
            {
                tprint("Couldn't get executable path");
            }
            
            CloseHandle(process); 
        }
    }
    
    strcpy(buffer, "No Window.exe");
    len = strlen("No Window.exe");
    
    return len;
}

LRESULT CALLBACK
WinProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        
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
            // TODO: This may not be true in final version
            Event e;
            e.type = Event_GUI_Open;
            global_event_queue.enqueue(e);
            
            SetWindowText(window, "This is the Title Bar");
            
            // TODO: Should I prefix strings with an L?   L"string here"
            
            // Alse creating a static control to allow printing text
            // This needs its own message loop
            HINSTANCE instance = GetModuleHandle(NULL);
#if 0
            HWND day_button = CreateWindow("BUTTON",
                                           "DAY",
                                           WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
                                           10, 10,
                                           70, 50,
                                           window,
                                           (HMENU)ID_DAY_BUTTON, 
                                           instance, NULL);
            
            HWND week_button = CreateWindow("BUTTON",
                                            "WEEK",
                                            WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
                                            10, 90,
                                            70, 50,
                                            window,
                                            (HMENU)ID_WEEK_BUTTON, 
                                            instance, NULL);
            
            HWND month_button = CreateWindow("BUTTON",
                                             "MONTH",
                                             WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
                                             10, 170,
                                             70, 50,
                                             window,
                                             (HMENU)ID_MONTH_BUTTON, 
                                             instance, NULL);
            
#endif 
            
            
#define ID_TRAY_APP_ICON 1001
#define CUSTOM_WM_TRAY_ICON (WM_USER + 1)
            
            // TODO: To maintain compatibility with older and newer versions of shell32.dll while using
            // current header files may need to check which version of shell32.dll is installed so the
            // cbSize of NOTIFYICONDATA can be initialised correctly. 
            // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa
            
            global_nid.cbSize = sizeof(NOTIFYICONDATA);
            global_nid.hWnd = window;
            // Not sure why we need this
            global_nid.uID = ID_TRAY_APP_ICON;
            global_nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
            // This user invented message is sent when mouse event occurs or hovers over tray icon, or when icon selected or activated from keyboard
            global_nid.uCallbackMessage = CUSTOM_WM_TRAY_ICON;
            // Recommented you provide 32x32 icon for higher DPI systems
            // Can use LoadIconMetric to specify correct one with correct settings is used
            global_nid.hIcon = (HICON)LoadIcon(instance, MAKEINTRESOURCE(MAIN_ICON_ID));
            
            // Maybe this should say "running"?
            TCHAR tooltip[] = {"Tooltip dinosaur"}; // Use max 64 chars
            strncpy(global_nid.szTip, tooltip, sizeof(tooltip));
            
            
            // Adds icon to status area (I think)
            // TODO: Should this be in WM_ACTIVATE?
            BOOL success = Shell_NotifyIconA(NIM_ADD, &global_nid);
            if (!success) 
            {
                tprint("Error: Couldn't create notify icon\n");;
            }
            
            
            // Only available on Windows 2000 and up. Allows you to recieve WM_CONTEXTMENU, NIN_KEYSELECT and NIN_SELECT etc message from icon instead of WM LEFT/RIGHT BUTTON UP/DOWN.
            // Prefer version 4
            //nid.uVersion = NOTIFYICON_VERSION_4;
            //Shell_NotifyIconA(NIM_SETVERSION, &nid);
        } break;
        
        
        // Seems you can't wait to handle all messages and they are just sent directly to WinProc.
        // So pump messages can't see them in queue.
        case WM_COMMAND:
        {
            // TODO: This may not play nice with Waiting for a message in loop then pumping, if we:
            // wait -> msg recieved -> end sleep -> pump msgs (no message posted yet) -> winproc -> wait
            // Resend message so pump can remove it from queue.
            switch (HIWORD(wParam))
            {
                case BN_CLICKED:
                {
                    
                    Event e;
                    e.type = Event_Button_Click;
                    if (ID_DAY_BUTTON == LOWORD(wParam))
                    {
                        e.button = Button_Day;
                    }
                    else if (ID_WEEK_BUTTON == LOWORD(wParam))
                    {
                        e.button = Button_Week;
                    }
                    else if (ID_MONTH_BUTTON == LOWORD(wParam))
                    {
                        e.button = Button_Month;
                    }
                    else
                    {
                        rvl_assert(0);
                    }
                    
                    global_event_queue.enqueue(e);
                } break;
            }
        } break;
        
        
        // Much copied from:
        //https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/winui/shell/appshellintegration/NotificationIcon/NotificationIcon.cpp
        case CUSTOM_WM_TRAY_ICON:
        {
            //print_tray_icon_message(lParam);
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
                        Event e;
                        e.type = Event_GUI_Close;
                        global_event_queue.enqueue(e);
                        
                        ShowWindow(window, SW_HIDE);
                    }
                    else
                    {
                        Event e;
                        e.type = Event_GUI_Open;
                        global_event_queue.enqueue(e);
                        
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
        
        
        // WM_SIZE and WM_PAINT messages recieved when window resized,
        case WM_PAINT:
        {
            // Sometimes your program will initiate painting, sometimes will get sent WM_PAINT
            // Only responsible for repainting client area
            
            PAINTSTRUCT ps;
            HDC device_context = BeginPaint(window, &ps);
            
            RECT rect;
            GetClientRect(window, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            
            // When window is resized, the process is suspended and recieves WM_PAINT
            paint_window(&global_screen_buffer, device_context, width, height);
            
            EndPaint(window, &ps);
        } break;
        
        
        
        default:
        result = DefWindowProcA(window, msg, wParam, lParam);
        break;
    }
    
    return result;
}

// TODO: Do we even need a pump messages or just handle everying in def window proc with GetMessage loop
void
pump_messages()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        WPARAM wParam = msg.wParam;
        LPARAM lParam = msg.lParam;
        switch (msg.message)
        {
#if 0
            case WM_COMMAND:
            {
                switch (HIWORD(wParam))
                {
                    case BN_CLICKED:
                    {
                    } break;
                    
                    default:
                    {
                    } break;
                }
                
            } break;
            
            case WM_SYSKEYDOWN:
            case WM_KEYDOWN:
            {
                
            } break;
#endif
            
            default:
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            } break;
        }
    }
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
    
    {
        // Relative paths possible???
        char exe_path[MaxPathLen];
        DWORD filepath_len = GetModuleFileNameA(nullptr, exe_path, 1024);
        if (filepath_len == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER) return 1;
        
        global_savefile_path = make_filepath(exe_path, SaveFileName);
        if (!global_savefile_path) return 1;
        
        global_debug_savefile_path = make_filepath(exe_path, DebugSaveFileName);
        if (!global_debug_savefile_path) return 1;
        
        if (strlen(global_savefile_path) > MAX_PATH)
        {
            tprint("Error: Find first file ANSI version limited to MATH_PATH");
            return 1;
        }
    }
    
    // If already running just open/show GUI, or if already open do nothing.
    // FindWindowA();
    
    // On icons and resources from Forger's tutorial
    // The icon shown on executable file in explorer is the icon with lowest numerical ID in the program's resources. Not necessarily the one we load while running.
    
    // You can create menus by specifying them in the .rc file or by specifying them at runtime, with AppendMenu etc
    
    // Better to init this before we even can recieve messages
    resize_screen_buffer(&global_screen_buffer, WindowWidth, WindowHeight);
    
    init_queue(&global_event_queue, 100);
    
    WNDCLASS window_class = {};
    window_class.style = CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = WinProc;
    window_class.hInstance = instance;
    window_class.hIcon =  LoadIcon(instance, MAKEINTRESOURCE(MAIN_ICON_ID)); // Function looks for resource with this ID, 
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    // window_class_ex.hIconsm = ; OS may still look in ico file for small icon anyway
    // window_class.hbrBackground;
    // window_class.lpszMenuName
    window_class.lpszClassName = "MonitorWindowClassName";
    
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
    
    
    Font font = create_font("c:\\windows\\fonts\\times.ttf", 28);
    
    remove(global_savefile_path);
    remove(global_debug_savefile_path);
    
    if (!file_exists(global_savefile_path))
    {
        // Create database
        make_empty_savefile(global_savefile_path);
        rvl_assert(valid_savefile(global_savefile_path));
    }
    else
    {
        // check database in valid state
        if (!valid_savefile(global_savefile_path))
        {
            tprint("Error: savefile corrupted");
            return 1;
        }
    }
    
    Header header = {};
    Hash_Table all_programs = {};
    Day *days = nullptr;
    u32 day_count = 0;
    u32 cur_day = 0;
    
    // TODO: This is bad and error prone
    // Consider a separate array, or linked list of chunks to hold records, when the days then point into.
    // Easy to just allocate a big chunk add all the days, then easily add more days, with no resizing/reallocating
    {
        // Can there be no records for a day in the file, or there be days with no programs and vice versa?
        FILE *savefile = fopen(global_savefile_path, "rb");
        fread(&header, sizeof(Header), 1, savefile);
        
        // If there are zero days or progams it sets size to 30
        init_hash_table(&all_programs, header.total_program_count*2 + 30);
        
        u32 max_days = std::max(MaxDays, (header.day_count*2) + DefaultDayAllocationCount);
        days = (Day *)xalloc(sizeof(Day) * max_days);
        
        read_programs_from_savefile(savefile, header, &all_programs);
        read_all_days_from_savefile(savefile, header, days);
        
        day_count = header.day_count;
        
        sys_days now = floor<date::days>(System_Clock::now());
        
        // Leave space for 30 days
        if (day_count > 0)
        {
            if (now == days[day_count-1].date)
            {
                cur_day = day_count-1;
                days[day_count-1].records = (Program_Record *)realloc(days[day_count-1].records, sizeof(Program_Record) * MaxDailyRecords); 
            }
            else
            {
                cur_day = day_count;
                day_count += 1;
                days[cur_day].records = (Program_Record *)xalloc(sizeof(Day) * MaxDailyRecords);
                days[cur_day].date = now;
                days[cur_day].record_count = 0;
            }
        }
        else
        {
            cur_day = 0;
            day_count = 1;
            days[cur_day].records = (Program_Record *)xalloc(sizeof(Program_Record) * MaxDailyRecords);
            days[cur_day].date = now;
            days[cur_day].record_count = 0;
        }
        
        fclose(savefile);
    }
    
    // Must have slot for all hash table ids
    u32 program_paths_count = 0;
    Program_Path program_paths[200] = {}; // NOTE: Must be zeroed out.
    
    u32 icon_count = 0;
    Simple_Bitmap icons[200] = {};
    
    String_Builder sb = create_string_builder();
    
    time_type added_times = 0;
    time_type duration_accumulator = 0;
    
    auto old_time = Steady_Clock::now();
    auto loop_start = Steady_Clock::now();
    
    DWORD sleep_milliseconds = 2000;
    //
    // Unsleep on message, and sleep remaining time?
    //
    
    Day *stored_days = nullptr;
    u32 stored_day_count = 0;
    u32 stored_cur_day = 0;
    
    
    bool gui_visible = (bool)IsWindowVisible(window);
    
    // Create a freeze frame of data when GUI opened, and just add data to aside, and merge when closed.
    bool first_run = true;
    global_running = true;
    while (global_running)
    {
        // When MsgWaitForMultipleObjects tells you there is message, you have to process
        // all messages
        // Returns when there is a message no one knows about
        DWORD wait_result = MsgWaitForMultipleObjects(0, nullptr, false, sleep_milliseconds,
                                                      QS_SENDMESSAGE);
        if (wait_result == WAIT_FAILED)
        {
            
        }
        else if (wait_result == WAIT_TIMEOUT)
        {
            
        }
        else if (wait_result == WAIT_OBJECT_0)
        {
            // Can return this even if no input available, if system event (like foreground
            // activation) occurs.
        }
        else
        {
            // Should not get other return values
        }
        
        pump_messages(); // Need this to translate and dispatch messages.
        
        Button button = Button_Invalid;;
        bool button_clicked = false;
        bool gui_opened = false;
        bool gui_closed = false;
        
        while (!global_event_queue.empty())
        {
            Event e = global_event_queue.dequeue();
            if (e.type == Event_Button_Click) 
            {
                button_clicked = true;
                button = e.button;
            }
            else if (e.type == Event_GUI_Open)
            {
                gui_opened = true;
                gui_closed = false;
                gui_visible = true;
            }
            else if (e.type == Event_GUI_Close)
            {
                gui_opened = false;
                gui_closed = true;
                gui_visible = false;
            }
        }
        
        auto new_time = Steady_Clock::now();
        std::chrono::duration<time_type> diff = new_time - old_time; 
        old_time = new_time;
        time_type dt = diff.count();
        
        // Steady clock also accounts for time paused in debugger etc, so can introduce bugs that aren't there
        // normally when debugging. 
        duration_accumulator += dt;
        
        // speed up time!!!
        // int seconds_per_day = 5;
        //sys_days current_date = floor<date::days>(System_Clock::now()) + date::days{(int)duration_accumulator / seconds_per_day};
        
        sys_days current_date = floor<date::days>(System_Clock::now());
        
        if (current_date != days[cur_day].date)
        {
            ++cur_day;
            ++day_count;
            days[cur_day].records = (Program_Record *)xalloc(sizeof(Program_Record) * MaxDailyRecords);
            days[cur_day].record_count = 0;
            days[cur_day].date = current_date;
        }
        
        {
            char   full_path[MaxPathLen + 1]     = {};
            char   *name_start                   = nullptr; 
            char   program_name[MaxPathLen + 1]  = {};
            size_t full_path_len                 = 0;
            size_t name_len                      = 0;
            
            HWND foreground_win = GetForegroundWindow();
            
            full_path_len = get_forground_window_path(foreground_win, full_path, array_count(full_path));
            name_start    = get_filename_from_path(full_path); // Returns a pointer to filename in full_path
            name_len      = strlen(name_start);
            
            rvl_assert(name_len > 4 && strcmp(name_start + (name_len - 4), ".exe") == 0);
            
            // TODO: Does "No Window" ever appear???
            
            name_len -= 4;
            memcpy(program_name, name_start, name_len); // Don't copy '.exe' from end
            program_name[name_len] = '\0';
            
            if (strcmp(program_name, "firefox") == 0)
            {
                // for now just overwrite firefox window name and add url as a program
                // TODO: If we get a URL but it has no url features like www. or https:// etc
                // it is probably someone just writing into the url bar, and we don't want to save these
                // as urls.
                char *URL = program_name;
                size_t URL_len = 0;
                bool success = get_firefox_url(foreground_win, URL, &URL_len);
            }
            
            // NOTE: Adds to the table and the days array, maybe these should be separate
            // Mayne this could return an ID
            add_program_duration(&all_programs, &days[cur_day], dt, program_name);
            
            // TODO: Table must be in sync with program_paths array, so ID slot must exist, is this bad?
            // Maybe just put whole thing in hash table.
            u32 ID = 0;
            bool in_table = all_programs.search(program_name, &ID);
            if (in_table)
            {
                if (program_paths[ID].occupied)
                {
                    if (!program_paths[ID].updated_recently)
                    {
                        if (strcmp(program_paths[ID].full_path, full_path) != 0)
                        {
                            free(program_paths[ID].full_path);
                            program_paths[ID].full_path = clone_string(full_path, full_path_len+1);
                            program_paths[ID].updated_recently = true;
                        }
                    }
                }
                else
                {
                    program_paths[ID].full_path = clone_string(full_path, full_path_len+1);
                    program_paths[ID].updated_recently = true;
                    program_paths[ID].occupied = true;
                    program_paths_count += 1;
                    
                    rvl_assert(program_paths_count == all_programs.count);
                }
            }
            else
            {
                rvl_assert(0);
            }
            
            
            if (in_table && strcmp(program_name, "firefox") != 0)
            {
                if (!icons[ID].pixels) 
                {
                    Simple_Bitmap icon_bitmap = get_icon_from_executable(full_path, 64);
                    icons[ID] = icon_bitmap;
                    ++icon_count;
                }
            }
        }
        
        static char window_str[10000];
        if (button_clicked)
        {
            Button button_clicked = Button_Day;
            
            u32 period = 
                (button_clicked == Button_Day) ? 1 :
            (button_clicked == Button_Week) ? 7 : 30; 
            
            sys_days period_start_date = current_date - date::days{period - 1};
            
            rvl_assert(day_count > 0);
            
            u32 end_range = cur_day;
            u32 start_range = end_range;
            i32 start_search = std::max(0, (i32)end_range - (i32)(period-1));
            
            for (u32 i = (u32)start_search; i <= end_range; ++i)
            {
                if (days[i].date >= period_start_date)
                {
                    start_range = i;
                    break;
                }
            }
            
            String_Builder display = create_string_builder();
            display.appendf("Period %u\n", period);
            for (u32 i = start_range; i <= end_range; ++i)
            {
                display.appendf("\nDay %u\n", i);
                Day &d = days[i];
                for (int j = 0; j < d.record_count; ++j)
                {
                    Program_Record &pr = d.records[j];
                    char *name = all_programs.search_by_value(pr.ID);
                    display.appendf("%s: %lfs\n", name, pr.duration);
                }
            }
            
            strcpy(window_str, display.str);
        }
        
        if (gui_opened)
        {
            // Save a freeze frame of the currently saved days.
            stored_days = days;
            stored_day_count = day_count;
            stored_cur_day = cur_day;
            
            // It is unnecessary to save the hash table of programs as new programs will not be able to
            // be referenced by existing IDs.
            
            days = (Day *)xalloc(sizeof(Day) * DefaultDayAllocationCount);
            day_count = 1;
            cur_day = 0;
            
            // Copy back todays records
            days[cur_day] = stored_days[stored_cur_day];
            days[cur_day].records = (Program_Record *)xalloc(sizeof(Program_Record) * MaxDailyRecords);
            if (days[cur_day].record_count > 0)
            {
                memcpy(days[cur_day].records, 
                       stored_days[stored_cur_day].records, sizeof(Program_Record) * stored_days[stored_cur_day].record_count);
            }
        }
        
        // TODO: Do you need cur day, as it is always the last day
        
        if (gui_closed)
        {
            // Merge new days and records with the stored.
            
            // stored_cur_day                 v = 6
            // stored_days [0][1][2][3][4][5][6][][][]
            // stored_day_count = 7
            
            // cur_day  v = 1
            // days [0][1][][]
            // day_count = 2
            
            free(stored_days[stored_cur_day].records);
            
            // TODO: just use a vector
            if (stored_day_count + day_count < MaxDays)
            {
                rvl_assert(stored_day_count > 0);
                
                // NOTE: This doesn't work if we have one big record array.
                memcpy(&stored_days[stored_cur_day], days, sizeof(Day) * day_count);
                
                days = stored_days;
                day_count += stored_day_count - 1; // Because we always have one overlapping day I think.
                cur_day += stored_cur_day;
                
                stored_days = nullptr;
                stored_day_count = 0;
                stored_cur_day = 0;
            }
            else
            {
                rvl_assert(0);
            }
        }
        
        
        if (gui_visible)
        {
            // Fill Background
            draw_rectangle(&global_screen_buffer, 
                           Rect2i{{0, 0}, {global_screen_buffer.width, global_screen_buffer.height}},
                           1.0f, 1.0f, 1.0f);
            
            int origin_x = 200;
            int origin_y = 400;
            
            
            rvl_assert(stored_days);
            rvl_assert(stored_day_count > 0);
            
            Day &today = stored_days[cur_day];
            
            Program_Record sorted_records[MaxDailyRecords];
            memcpy(sorted_records, today.records, sizeof(Program_Record) * today.record_count);
            
            std::sort(sorted_records, sorted_records+today.record_count, [](Program_Record &a, Program_Record &b){ return a.duration > b.duration; });
            
            int max_bars = x_axis_length / (bar_width + bar_spacing);
            int bar_count = std::min(max_bars, (int)today.record_count);
            
            for (int i = 0; i < bar_count; ++i)
            {
                Program_Record &record = sorted_records[i];
                rvl_assert(icons[record.ID].pixels);
                
                Simple_Bitmap &icon = icons[record.ID];
                
                int bar_centre_line_x = origin_x + ((bar_spacing + bar_width) * (i+1)) - (bar_width/2);
                int icon_pen_x = bar_centre_line_x - (icon.width/2);
                int icon_pen_y = origin_y + 10;
                
                draw_simple_bitmap(&global_screen_buffer, &icon, icon_pen_x, icon_pen_y);
            }
            
            draw_bar_plot_from_records(&global_screen_buffer, &font, sorted_records, bar_count, origin_x, origin_y);
        }
        
        
        // Don't sleep if GUI opened
        // Just wait for messages I guess
        {
            HDC device_context = GetDC(window);
            
            RECT rect;
            GetClientRect(window, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            paint_window(&global_screen_buffer, device_context, width, height);
            
            ReleaseDC(window, device_context);
        }
        
        
        Sleep(60);
        
    } // END LOOP
    
    auto loop_end = Steady_Clock::now();
    std::chrono::milliseconds sleeptime(60); // TODO:!
    std::chrono::duration<time_type> loop_time = loop_end - loop_start -  std::chrono::duration<time_type>(sleeptime);
    
    
    
#if 0
    
    
    sb.appendf("\nLoop Time: %lf\n", loop_time.count());
    // NOTE: Maybe be slightly different because of instructions between loop finishing and taking the time here, also Sleep may not be exact. Probably fine to ignore.
    sb.appendf("Diff: %lf\n", loop_time.count() - duration_accumulator);
    
    SetWindowTextA(global_text_window, sb.str);
    
    
    Global_running = true;
    while (Global_running)
    {
        Button_State button_state = {};
        pump_messages(&button_state);
    }
#endif
    
    {
        // NOTE: Always create a new savefile for now
        Header new_header = header;
        
        String_Builder sb = create_string_builder(); // We will build strings will add_string to allow null terminators to be interspersed
        
        u32 total_record_count = 0;
        for (u32 i = 0; i < day_count; ++i)
        {
            total_record_count += days[i].record_count;
        }
        
        std::vector<u32> program_ids;
        std::vector<sys_days> dates;
        std::vector<u32> counts;
        std::vector<Program_Record> records;
        
        program_ids.reserve(all_programs.count);
        dates.reserve(day_count);
        counts.reserve(day_count);
        records.reserve(total_record_count);
        
        u32 test = 0; // TODO: remove this
        for (s64 i = 0; i < all_programs.size; ++i)
        {
            if (all_programs.occupancy[i] == Hash_Table::OCCUPIED)
            {
                // add null terminator
                sb.add_bytes(all_programs.buckets[i].key, strlen(all_programs.buckets[i].key) + 1);
                
                program_ids.push_back(all_programs.buckets[i].value);
                
                test += strlen(all_programs.buckets[i].key) + 1;
            }
        }
        
        rvl_assert(test == sb.len);
        rvl_assert(program_ids.size() == all_programs.count);
        
        // TODO: Not sure whether to keep days as AOS or SOA
        // SOA wouldn't need to do this kind of thing when saving to file
        
        new_header.program_names_block_size = sb.len;
        new_header.total_program_count = all_programs.count;
        new_header.day_count = day_count;
        new_header.total_record_count = total_record_count;
        
        u32 index = 0;
        for (u32 i = 0; i < day_count; ++i)
        {
            dates.push_back(days[i].date);
            counts.push_back(days[i].record_count);
            for (u32 j = 0; j < days[i].record_count; ++j)
            {
                records.push_back(days[i].records[j]);
            }
        }
        
        update_savefile(global_savefile_path, 
                        &new_header, 
                        sb.str, sb.len, 
                        program_ids.data(), program_ids.size(), 
                        dates.data(), dates.size(),
                        counts.data(), counts.size(),
                        records.data(), records.size());
        
        convert_savefile_to_text_file(global_savefile_path, global_debug_savefile_path);
    }
    
    
    free_table(&all_programs);
    
    free(font.atlas);
    free(font.glyphs);
    
    // Just in case we exit using ESCAPE
    Shell_NotifyIconA(NIM_DELETE, &global_nid);
    
    FreeConsole();
    
    return 0;
}


#if 0
// So if we want fully real time application, where we make changes regularly
// probably want to peek messages before dispatching to windows
// Else we want to just wait for message before proceeding with GUI stuff
// Get message blocks, peek message (with PM_REMOVE) doesn't block

// Some windows bypass queue and are sent direclt to WindowProc
// Returns 0 on WM_QUIT

MSG msg = {};
BOOL result;
while ((result = GetMessage(&msg, window, 0, 0)) != 0)
{
    if (result == -1)
    {
        // Error
        tprint("Error: GetMessage failed");
        return false;
    }
    
    TranslateMessage(&msg); 
    DispatchMessage(&msg); 
}
#endif

#if 0
// NOTE: Can maybe use this to be alerted for messages when sleeping
// When MsgWaitForMultipleObjects tells you there is message, you have to process
// all messages
// Returns when there is a message no one knows about
DWORD wait_result = MsgWaitForMultipleObjects(0, nullptr, false, sleep_milliseconds,
                                              QS_ALLEVENTS);
if (wait_result == WAIT_FAILED)
{
    
}
else if (wait_result == WAIT_TIMEOUT)
{
    
}
else if (wait_result == WAIT_OBJECT_0)
{
    // Can return this even if no input available, if system event (like foreground
    // activation) occurs.
}
else
{
    // Should not get other return values
}
#endif


#if 0

// a name called s, and records being added without corresponding name in hash table

Day &today = days[cur_day];
double sum_duration = 0;
for (u32 i = 0; i < today.record_count; ++i)
{
    // Can store all names in array to quickly get name associated with ID
    char *name = all_programs.search_by_value(today.records[i].ID);
    rvl_assert(name);
    sb.appendf("%s %lu: %lf\n", name, today.records[i].ID, today.records[i].duration);
    sum_duration += today.records[i].duration;
}
sb.appendf("\nAccumulated duration: %lf\n\n", duration_accumulator);
sb.appendf("Sum records duration: %lf\n\n", sum_duration);
SetWindowTextA(global_text_window, sb.str);

if (Global_running) sb.clear();

//
//
//

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
    
    Simple_Bitmap icon_bitmap = get_icon_from_executable(names[i], size);
#endif