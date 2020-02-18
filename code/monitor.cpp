#include "ravel.h"

#include "monitor.h"
#include "resource.h"

#include <chrono>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <AtlBase.h>
#include <UIAutomation.h>

#include <shellapi.h>
//#define NOMINMAX
#include <windows.h>

#include <algorithm>

//#undef max
//#undef min

#include "date.h" 

using namespace date;

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");

#include "utilities.cpp"
#include "helper.cpp"
#include "date.cpp"
#include "file.cpp"


#define CONSOLE_ON 1


static char *global_savefile_path;
static char *global_debug_savefile_path;
static bool Global_Running = false;
static bool Global_GUI_Visible = true; // Still counts as visible if minimised to the task bar
static HWND Global_Text_Window;
static NOTIFYICONDATA Global_Nid = {};

static constexpr char MutexName[] = "RVLmonitor143147";
static constexpr int WindowWidth = 1000;
static constexpr int WindowHeight = 900;

static constexpr u32 MaxDailyRecords = 1000;
static constexpr u32 MaxDays = 10000;

// -----------------------------------------------------------------
// TODO:
// * !!! If GUI is visible don't sleep. But we still want to poll infrequently. So maybe check elapsed poll time.
// * Remember window width and height
// * Unicode correctness
// * Path length correctnedd
// * Dynamic window, button layout with resizing
// * OpenGL graphing?
// * Stop repeating work getting the firefox URL
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
    
    bool program_in_today = false;
    u32 ID = 0;
    bool in_table = all_programs->search(program_name, &ID);
    
    if (in_table)
    {
        for (u32 i = 0; i < day->record_count; ++i)
        {
            if (ID == day->records[i].ID)
            {
                program_in_today = true;
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
    
    if (!program_in_today)
    {
        day->records[day->record_count] = {ID, dt};
        day->record_count += 1;
    }
}

u32
get_forground_window_program(HWND foreground_win, char *program_name)
{
    size_t len = 0;
    
    if (foreground_win)
    {
        DWORD process_id;
        GetWindowThreadProcessId(foreground_win, &process_id); 
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_id);
        if (process)
        {
            // filename_len is overwritten with number of characters written to buf
            char buf[MaxPathLen+1] = {};
            DWORD filename_len = array_count(buf);
            if (QueryFullProcessImageNameA(process, 0, buf, &filename_len))
            {
                rvl_assert(filename_len > 0);
                
                // Returns a pointer to filename in buf
                char *filename = get_filename_from_path(buf);
                len = strlen(filename);
                rvl_assert(len > 4 && strcmp(&filename[len-4], ".exe") == 0);
                
                // Remove '.exe' from end
                len -= 4;
                filename[len] = '\0';
                
                strcpy(program_name, filename);
            }
        }
        
        CloseHandle(process); 
    }
    else
    {
        strcpy(program_name, "No Window");
        len = strlen("No Window");
    }
    
    return len;
}

LRESULT CALLBACK
WinProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        // Close, Destroy and Quit ------------------------------------------------------
        // When main window or taskbar preview [x] is pressed we recieve CLOSE, DESTROY and QUIT in that order
        // Same for double clicking main window icon on top left, and clicking close on titlebar option under icon.
        // When we close from task manager all messages are recieved.
        // When we handle ESCAPE key, none of these are recieved
        case WM_CLOSE:
        {
            // If this message is ignored DefWindowProc calls DestroyWindow
            
            // Can Hide window here and not call DestroyWindow
            
            //tprint("WM_CLOSE\n");
            DestroyWindow(window); // Have to call this or main window X won't close program
        } break;
        
        case WM_DESTROY: 
        {
            // Recieved when window is removed from screen but before destruction occurs 
            // and before child windows are destroyed.
            // Typicall call PostQuitMessage(0) to send WM_QUIT to message loop
            // So typically goes CLOSE -> DESTROY -> QUIT
            
            //tprint("WM_DESTROY\n");
            
            // When the window is destroyed by pressing the X, not by quiting with escape
            Shell_NotifyIconA(NIM_DELETE, &Global_Nid);
            
            PostQuitMessage(0);
            
        } break;
        
        case WM_QUIT:
        {
            // Handled by message loop
            
            rvl_assert(0);
        } break;
        // ----------------------------------------------------------------------------
        
        
        // WM_NCCREATE WM_CREATE send before window becomes visible
        
        // When window created these messages are sent in this order:
        // WM_NCCREATE
        // WM_CREATE
        
        case WM_CREATE:
        {
#define ID_DAY_BUTTON 200
#define ID_WEEK_BUTTON 201
#define ID_MONTH_BUTTON 202
            SetWindowText(window, "This is the Title Bar");
            
            // TODO: Should I prefix strings with an L?   L"string here"
            
            // Alse creating a static control to allow printing text
            // This needs its own message loop
            HINSTANCE instance = GetModuleHandle(NULL);
            Global_Text_Window = CreateWindowA("STATIC",
                                               "TextWindow",
                                               WS_VISIBLE|WS_CHILD|SS_LEFT,
                                               100, 0,
                                               900, 900,
                                               window, 
                                               NULL, instance, NULL);
            
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
#define ID_TRAY_APP_ICON 1001
#define CUSTOM_WM_TRAY_ICON (WM_USER + 1)
            
            // TODO: To maintain compatibility with older and newer versions of shell32.dll while using
            // current header files may need to check which version of shell32.dll is installed so the
            // cbSize of NOTIFYICONDATA can be initialised correctly. 
            // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa
            
            Global_Nid.cbSize = sizeof(NOTIFYICONDATA);
            Global_Nid.hWnd = window;
            // Not sure why we need this
            Global_Nid.uID = ID_TRAY_APP_ICON;
            Global_Nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
            // This user invented message is sent when mouse event occurs or hovers over tray icon, or when icon selected or activated from keyboard
            Global_Nid.uCallbackMessage = CUSTOM_WM_TRAY_ICON;
            // Recommented you provide 32x32 icon for higher DPI systems
            // Can use LoadIconMetric to specify correct one with correct settings is used
            Global_Nid.hIcon = (HICON)LoadIcon(instance, MAKEINTRESOURCE(MAIN_ICON_ID));
            
            // Maybe this should say "running"?
            TCHAR tooltip[] = {"Tooltip dinosaur"}; // Use max 64 chars
            strncpy(Global_Nid.szTip, tooltip, sizeof(tooltip));
            
            
            // Adds icon to status area (I think)
            // TODO: Should this be in WM_ACTIVATE?
            BOOL success = Shell_NotifyIconA(NIM_ADD, &Global_Nid);
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
            PostMessageA(window, msg, wParam, lParam);
#if 0
            switch (HIWORD(wParam))
            {
                // TODO: Better way to achieve same effect? Maybe using globals and just pass data out.
                // Recieved by parent windproc if button proc doesn't handle BM_CLICK (I think)
                case BN_CLICKED:
                {
                    if (ID_DAY_BUTTON == LOWORD(wParam))
                    {
                        tprint("WNDPROCDay");
                    }
                    else if (ID_WEEK_BUTTON == LOWORD(wParam))
                    {
                        tprint("WNDPROCWeek");
                    }
                    else if (ID_MONTH_BUTTON == LOWORD(wParam))
                    {
                        tprint("WNDPROCMonth");
                    }
                    else
                    {
                        rvl_assert(0);
                    }
                } break;
            }
#endif
        } break;
        
        
        // Much copied from:
        //https://github.com/microsoft/Windows-classic-samples/blob/master/Samples/Win7Samples/winui/shell/appshellintegration/NotificationIcon/NotificationIcon.cpp
        case CUSTOM_WM_TRAY_ICON:
        {
            //print_tray_icon_message(lParam);
            switch (LOWORD(lParam))
            {
                // Want to use button up so user has to finish click on icon, not just start it.
                case WM_RBUTTONUP:                  
                {
                    // Cound open a context menu here with option to quit etc.
                } break;
                case WM_LBUTTONUP:
                {
                    // NOTE: For now we just toggle with tray icon but, we will in future toggle with X button 
                    if (Global_GUI_Visible)
                    {
                        tprint("Hiding\n\n");
                        Global_GUI_Visible = !Global_GUI_Visible;
                        ShowWindow(window, SW_HIDE);
                    }
                    else
                    {
                        // TODO: This doesn't seem to 'fully activate' the window. The title bar is in focus but cant input escape key. So seem to need to call SetForegroundWindow
                        tprint("SHOWING\n");
                        Global_GUI_Visible = !Global_GUI_Visible;
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
            // we should handle these in pump_messages
            rvl_assert(0);
        } break;
        
        
#if 0
        case WM_ACTIVATEAPP:
        {
        } break;
        
        case WM_PAINT:
        {
            // Sometimes your program will initiate painting, sometimes will get send WM_PATIN
            // Only responsible for repainting client area
            
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(window, &ps);
            
            // All painting occurs here, between BeginPaint and EndPaint.
            
            FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));
            
            EndPaint(window, &ps);
        } break;
#endif
        
        
        default:
        result = DefWindowProcA(window, msg, wParam, lParam);
        break;
    }
    
    return result;
}

// TODO: Do we even need a pump messages or just handle everying in def window proc with GetMessage loop
void
pump_messages(Button_State *button_state)
{
    rvl_assert(button_state);
    
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        WPARAM wParam = msg.wParam;
        LPARAM lParam = msg.lParam;
        switch (msg.message)
        {
            
            // From MSDN: Closing the window  // https://docs.microsoft.com/en-us/windows/win32/learnwin32/closing-the-window
            // Clicking close or using alt-f4 causes window to recieve WM_CLOSE message
            // Allowing you to prompt the user before closing the window
            
            // If you want to destroy the window you can call DestroyWindow when WM_CLOSE recieved
            // or can return 0 from case statement and the OS will ignore message
            
            // If you ignore WM_CLOSE entirely DefWindowProc will call DestroyWindow for you
            
            // When window is about to be destroyed it will recieve WM_DESTROY message
            // Typically you call PostQuitMessage(0) from case statement to post WM_QUIT message to
            // end the message loop
            case WM_QUIT:
            {
                // GetMessage will never recieve this message because it will return false and stop looping (I think)
                
                // minimise to tray
                Global_Running = false;
                
                // Removing tray icon doesn't seem to make it disappear any faster than it does by itself.
                // So seems to be no need for Shell_NotifyIconA(NIM_DELETE, &nid) stuff.
                //tprint("WM_QUIT\n");
            } break;
            
            case WM_COMMAND:
            {
                switch (HIWORD(wParam))
                {
                    case BN_CLICKED:
                    {
                        // TODO: Do we want this type of polling, or do we want to try to respond to 
                        // messages as soon as they come, i.e. winproc function only.
                        
                        // Looping over all messages, only the last pressed button is accepted as input
                        button_state->clicked = true;
                        if (ID_DAY_BUTTON == LOWORD(wParam))
                        {
                            button_state->button = Button_Day;
                            tprint("Day");
                        }
                        else if (ID_WEEK_BUTTON == LOWORD(wParam))
                        {
                            button_state->button = Button_Week;
                            tprint("Week");
                        }
                        else if (ID_MONTH_BUTTON == LOWORD(wParam))
                        {
                            button_state->button = Button_Month;
                            tprint("Month");
                        }
                        else
                        {
                            rvl_assert(0);
                        }
                        
                        // See if this is even a problem
                        rvl_assert(button_state->clicked);
                    } break;
                    
                    default:
                    {
                        tprint("OTHER");
                    } break;
                }
                
            } break;
            
            
            case WM_SYSKEYDOWN:
            case WM_KEYDOWN:
            {
                if (wParam == VK_ESCAPE)
                {
                    Global_Running = false;
                }
            } break;
            
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
#if CONSOLE_ON
    HANDLE con_in = create_console();
#endif
    
    
    
    char *exe_path = (char *)xalloc(MaxPathLen);
    if (!exe_path)
    {
        tprint("Error: Could not allocate memory");
        return 1;
    }
    
    DWORD filepath_len = GetModuleFileNameA(nullptr, exe_path, 1024);
    if (filepath_len == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        tprint("Error: Could not get executable path");
        return 1;
    }
    
    global_savefile_path = make_filepath(exe_path, SaveFileName);
    if (!global_savefile_path)
    {
        tprint("Error: Could not initialise filepath");
        return 1;
    }
    
    global_debug_savefile_path = make_filepath(exe_path, DebugSaveFileName);
    if (!global_debug_savefile_path)
    {
        tprint("Error: Could not initialise debug filepath");
        return 1;
    }
    
    free(exe_path);
    exe_path = nullptr;
    
    if (strlen(global_savefile_path) > MAX_PATH)
    {
        tprint("Error: Find first file ANSI version limited to MATH_PATH");
        return 1;
    }
    
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
    
    // On icons and resources from Forger's tutorial
    // The icon shown on executable file in explorer is the icon with lowest numerical ID in the program's resources. Not necessarily the one we load while running.
    
    // You can create menus by specifying them in the .rc file or by specifying them at runtime, with AppendMenu etc
    
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
                                  WS_VISIBLE|WS_OVERLAPPEDWINDOW,
                                  800, 100,  // Screen pos, TODO: Change to CS_USEDEFAULT
                                  WindowWidth, WindowHeight,
                                  NULL, NULL, 
                                  instance, NULL);
    
    if (!window)
    {
        return 1;
    }
    
    ShowWindow(window, cmd_show);
    
    Global_GUI_Visible = (bool)IsWindowVisible(window);
    rvl_assert(Global_GUI_Visible == (cmd_show != SW_HIDE));
    UpdateWindow(window);
    
    remove(global_savefile_path);
    remove(global_debug_savefile_path);
    
    if (!file_exists(global_savefile_path))
    {
        // Create database
        make_empty_savefile(global_savefile_path);
        rvl_assert(global_savefile_path);
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
        
        u32 max_days = std::max(MaxDays, (header.day_count*2) + 30);
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
    
    String_Builder sb = create_string_builder();
    
    time_type added_times = 0;
    time_type duration_accumulator = 0;
    
    auto old_time = Steady_Clock::now();
    auto loop_start = Steady_Clock::now();
    
    //
    // Unsleep on message, and sleep remaining time?
    //
    
    bool GUI_opened = false; // must start as false, for toggling to work.
    
    Global_Running = true;
    while (Global_Running)
    {
        Button_State button_state = {};
        pump_messages(&button_state);
        
        if (!GUI_opened && Global_GUI_Visible) GUI_opened = true;
        else GUI_opened = false;
        
        auto new_time = Steady_Clock::now();
        std::chrono::duration<time_type> diff = new_time - old_time; 
        old_time = new_time;
        time_type dt = diff.count();
        
        // Steady clock also accounts for time paused in debugger etc, so can introduce bugs that aren't there
        // normally when debugging. 
        duration_accumulator += dt;
        
        // speed up time!!!
        int seconds_per_day = 5;
        sys_days current_date = floor<date::days>(System_Clock::now()) + date::days{(int)duration_accumulator / seconds_per_day};
        
        // I prefer if this wasn't here, and i could better query for name, and then possibly get URL
        HWND foreground_win = GetForegroundWindow();
        char program_name[MaxPathLen + 1] = {};
        u32 len = get_forground_window_program(foreground_win, program_name);
        
        if (strcmp(program_name, "firefox") == 0)
        {
            // for now just overwrite firefox window name and add url as a program
            char *URL = program_name;
            size_t URL_len = 0;
            bool success = get_firefox_url(foreground_win, URL, &URL_len);
        }
        
        if (current_date != days[cur_day].date)
        {
            ++cur_day;
            ++day_count;
            days[cur_day].records = (Program_Record *)xalloc(sizeof(Program_Record) * MaxDailyRecords);
            days[cur_day].record_count = 0;
            days[cur_day].date = current_date;
        }
        
        add_program_duration(&all_programs, &days[cur_day], dt, program_name);
        
        
        static char window_str[10000];
        if (button_state.clicked)
        {
            rvl_assert(button_state.button != Button_Invalid);
            
            u32 period = 
                (button_state.button == Button_Day) ? 1 :
            (button_state.button == Button_Week) ? 7 : 30; 
            
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
        
        SetWindowTextA(Global_Text_Window, window_str);
        
#if 0
        sb.appendf("Num programs: %lu\n", today.record_count);
        sb.appendf("Accumulated duration: %lf\n\n", duration_accumulator);
        added_times = 0;
        for (u32 i = 0; i < today.record_count; ++i)
        {
            // Can store all names in array to quickly get name associated with ID
            char *name = all_programs.search_by_value(today.records[i].ID);
            rvl_assert(name);
            sb.appendf("%s %lu: %lf\n", name, today.records[i].ID, today.records[i].duration);
            
            added_times += today.records[i].duration;
        }
        
        sb.appendf("\nAdded records duration: %lf\n", added_times);
        SetWindowTextA(Global_Text_Window, sb.str);
        
        if (Global_Running) sb.clear();
#endif
        
        Sleep(60);
        
    } // END LOOP
    
    auto loop_end = Steady_Clock::now();
    std::chrono::milliseconds sleeptime(60); // TODO:!
    std::chrono::duration<time_type> loop_time = loop_end - loop_start -  std::chrono::duration<time_type>(sleeptime);
    
    sb.appendf("\nLoop Time: %lf\n", loop_time.count());
    // NOTE: Maybe be slightly different because of instructions between loop finishing and taking the time here, also Sleep may not be exact. Probably fine to ignore.
    sb.appendf("Diff: %lf\n", loop_time.count() - duration_accumulator);
    
    
    //printf(sb.str);
    
    
    
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
    
    
    // Can open new background instance when this is closed
    rvl_assert(mutex);
    CloseHandle(mutex);
    
    // Just in case we exit using ESCAPE
    Shell_NotifyIconA(NIM_DELETE, &Global_Nid);
    
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
