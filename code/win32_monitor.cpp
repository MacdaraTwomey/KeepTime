#include "ravel.h"

#include "helper.h"
#include "monitor.h"
#include "resource.h"
#include "win32_monitor.h"

#include <chrono>
#include <vector>
#include <algorithm>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <wchar.h>

#include <AtlBase.h>
#include <UIAutomation.h>

#include <shellapi.h>
#include <shlobj_core.h> // SHDefExtractIconA

#include <winhttp.h>

#include <windows.h>

#include "date.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace date;

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");

// NOTE: Debug use only
Simple_Bitmap global_ms_icons[5];

#include "utilities.cpp" // General programming and graphics necessaries
#include "helper.cpp"  // General ADTs and useful classes
#include "icon.cpp"    // Deals with win32 icon interface
#include "file.cpp"    // Deals with savefile and file operations
#include "monitor.cpp" // This deals with id, days, databases, websites, bitmap icons
#include "render.cpp"  // Rendering code

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


// -----------------------------------------------------------------
// TODO:
// * !!! If GUI is visible don't sleep. But we still want to poll infrequently. So maybe check elapsed poll time.
// * Remember window width and height
// * Unicode correctness
// * Path length correctness
// * Dynamic window, button layout with resizing
// * OpenGL graphing?
// * Stop repeating work getting the firefox URL, maybe use UIAutomation cache?
// * Get favicon from webpages, maybe use libcurl and a GET request for favicon.ico. Seems possible to use win32 api for this.

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

bool get_firefox_url(HWND window, char *URL, size_t *URL_len)
{
    // TODO: Firefox can have multiple windows and therefore a unique active tab for each, so caching
    // needs to account for possibility of different windows.
    
    // Error or warning in VS output:
    // mincore\com\oleaut32\dispatch\ups.cpp(2125)\OLEAUT32.dll!00007FFCC7DC1D33: (caller: 00007FFCC7DC1EAA) ReturnHrx(1) tid(1198) 8002801D Library not registered.
    // This error can be caused by the application requesting a newer version of oleaut32.dll than you have.
    // To fix I think you can just download newer version of oleaut32.dll.
    
    // Need to create a  CUIAutomation object and retrieve an IUIAutomation interface pointer to to to access UIAutomation functionality...
    CComQIPtr<IUIAutomation> uia;
    if(FAILED(uia.CoCreateInstance(CLSID_CUIAutomation)) || !uia)
        return false;
    
#if 0
    // Different style
    CComQIPtr<IUIAutomation> uia;
    CoCreateInstance(CLSID_CUIAutomation, NULL,
                     CLSCTX_INPROC_SERVER, IID_IUIAutomation,
                     reinterpret_cast<void**>(&uia));
#endif
    
    // Clients use methods exposed by the IUIAutomation interface to retrieve IUIAutomationElement interfaces for UI elements in the tree
    CComPtr<IUIAutomationElement> element;
    if(FAILED(uia->ElementFromHandle(window, &element)) || !element)
        return false;
    
    // NOTE:
    // When retrieving UI elements, clients can improve system performance by using the caching capabilities of UI Automation. Caching enables a client to specify a set of properties and control patterns to retrieve along with the element. In a single interprocess call, UI Automation retrieves the element and the specified properties and control patterns, and then stores them in the cache. Without caching, a separate interprocess call is required to retrieve each property or control pattern.
    
    
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
    
    
    
    
    // TODO:
    // This AND condition finds the navigation toolbar straight away, rather than other one looping
    // so can use find first instead of find all, but is this faster (does windows do a string compare on each element?)
    // TODO: Don't realloc this repeatedly.
    VARIANT variant;
    variant.vt = VT_BSTR;
    variant.bstrVal = SysAllocString(L"Navigation");
    if (!variant.bstrVal)
        return false;
    defer(SysFreeString(variant.bstrVal));
    
    CComPtr<IUIAutomationCondition> navigation_name_cond;
    uia->CreatePropertyCondition(UIA_NamePropertyId,
                                 variant, &navigation_name_cond);
    
    
    CComPtr<IUIAutomationCondition> and_cond;
    uia->CreateAndCondition(navigation_name_cond, toolbar_cond, &and_cond);
    
    
    // Find the top toolbars
    // Throws an exception on my machine, but doesn't crash program. This may just be a debug printout only
    // and not a problem.
    CComPtr<IUIAutomationElementArray> toolbars;
    if (FAILED(element->FindAll(TreeScope_Children, and_cond, &toolbars)) || !toolbars)
        return false;
    
    // NOTE: 27/02/2020:
    // (according to Inspect.exe)
    // To get the URLs of all open tabs in Firefox Window:
    // Mozilla firefox window ControlType: UIA_WindowControlTypeId (0xC370) LocalizedControlType: "window"
    // "" group               ControlType: UIA_GroupControlTypeId (0xC36A) LocalizedControlType: "group"
    //
    // containing multiple nestings of this (one for each tab):
    // ""                     no TypeId or Type
    //   ""                   no TypeId or Type
    //     "tab text"         ControlType: UIA_DocumentControlTypeId (0xC36E) LocalizedControlType: "document"
    //     which has Value.Value = URL
    
    // To get the URL of the active tab in Firefox Window:
    // Mozilla firefox window          ControlType: UIA_WindowControlTypeId (0xC370) LocalizedControlType: "window"
    //   "Navigation" tool bar         UIA_ToolBarControlTypeId (0xC365) LocalizedControlType: "tool bar"
    //     "" combobox                 UIA_ComboBoxControlTypeId (0xC353) LocalizedControlType: "combo box"
    //       "Search with Google or enter address"   UIA_EditControlTypeId (0xC354) LocalizedControlType: "edit"
    //        which has Value.Value = URL
    
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

size_t
get_forground_window_path(HWND foreground_win, char *buffer, size_t buf_size)
{
    // TODO: Use GetShellWindow GetShellWindow to detect when not doing anything on desktop, if foreground == desktop etc
    
    // Must have room for program name
    size_t len = 0;
    if (foreground_win)
    {
        Process_Ids process_ids = {0, 0};
        GetWindowThreadProcessId(foreground_win, &process_ids.parent);
        process_ids.child = process_ids.parent; // In case it is a normal process
        
        // Modern universal windows apps in 8 and 10 have the process we want inside a parent process called
        // WWAHost.exe on Windows 8 and ApplicationFrameHost.exe on Windows 10.
        // So we search for a child pid that is different to its parent's pid, to get the process we want.
        // https://stackoverflow.com/questions/32360149/name-of-process-for-active-window-in-windows-8-10
        
        EnumChildWindows(foreground_win, MyEnumChildWindowsCallback, (LPARAM)&process_ids);
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_ids.child);
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
    
    strcpy(buffer, "fairyland/No Window.exe");
    len = strlen("fairyland/No Window.exe");
    
    return len;
}

struct Window_Info
{
    char   full_path[MaxPathLen + 1];
    char   program_name[MaxPathLen + 1];
    size_t full_path_len;
    size_t program_name_len;
};

void
get_window_info(HWND window, Window_Info *info)
{
    size_t full_path_len     = get_forground_window_path(window, info->full_path, array_count(info->full_path));
    char *name_start         = get_filename_from_path(info->full_path);
    size_t  program_name_len = strlen(name_start);
    
    for (int i = 0; i < full_path_len; ++i)
    {
        info->full_path[i] = tolower(info->full_path[i]);
    }
    
    rvl_assert(program_name_len > 4 && strcmp(name_start + (program_name_len - 4), ".exe") == 0);
    
    program_name_len -= 4;
    memcpy(info->program_name, name_start, program_name_len); // Don't copy '.exe' from end
    info->program_name[program_name_len] = '\0';
    
    info->program_name_len = program_name_len;
    info->full_path_len = full_path_len;
}

void
poll_windows(Database *database, double dt)
{
    rvl_assert(database);
    
    HWND foreground_win = GetForegroundWindow();
    if (!foreground_win) return;
    
    Window_Info window_info = {};
    get_window_info(foreground_win, &window_info);
    
    // If this is a good feature then just pass in created HWND and test
    // if (strcmp(winndow_info->full_path, "c:\\dev\\projects\\monitor\\build\\monitor.exe") == 0) return;
    
    u32 record_id = 0;
    
    bool program_is_firefox = (strcmp(window_info.program_name, "firefox") == 0);
    bool add_to_executables = !program_is_firefox;
    
    if (program_is_firefox)
    {
        // TODO: Maybe cache last url to quickly get record without comparing with keywords, retrieving record etc
        
        // TODO: If we get a URL but it has no url features like www. or https:// etc
        // it is probably someone just writing into the url bar, and we don't want to save these
        // as urls.
        char url[2100];
        size_t url_len = 0;
        bool success = get_firefox_url(foreground_win, url, &url_len);
        
        bool keyword_match = false;
        Keyword *keyword = find_keywords(url, database->keywords, database->keyword_count);
        if (keyword)
        {
            bool website_is_stored = false;
            for (i32 website_index = 0; website_index < database->website_count; ++website_index)
            {
                if (database->websites[website_index].id == keyword->id)
                {
                    website_is_stored = true;
                    break;
                }
            }
            
            if (!website_is_stored)
            {
                rvl_assert(database->website_count < MaxWebsiteCount);
                
                Website *website = &database->websites[database->website_count];
                website->id = keyword->id;
                website->website_name = clone_string(keyword->str);
                
                database->website_count += 1;
            }
            
            record_id = keyword->id;
            
            // HACK: We would like to treat websites and programs the same and just index their icons
            // in a smarter way maybe.
            if (!database->firefox_path.updated_recently)
            {
                if (database->firefox_path.path) free(database->firefox_path.path);
                
                database->firefox_path.path = clone_string(window_info.full_path, window_info.full_path_len);
                database->firefox_path.updated_recently = true;
                database->firefox_id = keyword->id;
                
            }
        }
        else
        {
            add_to_executables = true;
        }
        
        if (!database->firefox_icon.pixels)
        {
            char *firefox_path = window_info.full_path;
            database->firefox_icon = {};
            bool success = get_icon_from_executable(firefox_path, ICON_SIZE, &database->firefox_icon, true);
            rvl_assert(success);
        }
    }
    
    if (add_to_executables)
    {
        u32 temp_id = 0;
        bool in_table = database->all_programs.search(window_info.program_name, &temp_id);
        if (!in_table)
        {
            temp_id = make_id(database, Record_Exe);
            database->all_programs.add_item(window_info.program_name, temp_id);
        }
        
        record_id = temp_id;
        
        // TODO: This is similar to what happens to firefox path and icon, needs collapsing.
        rvl_assert(!(temp_id & (1 << 31)));
        if (!in_table || (in_table && !database->paths[temp_id].updated_recently))
        {
            
            if (in_table && database->paths[temp_id].path)
            {
                free(database->paths[temp_id].path);
            }
            database->paths[temp_id].path = clone_string(window_info.full_path);
            database->paths[temp_id].updated_recently = true;
        }
    }
    
    rvl_assert(database->day_count > 0);
    Day *current_day = &database->days[database->day_count-1];
    
    update_days_records(current_day, record_id, dt);
}

LRESULT CALLBACK
WinProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        case WM_TIMER:
        {
            Event e = {};
            e.type = Event_Poll_Programs;
            global_event_queue.enqueue(e);
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
            // TODO: This may not be true in final version
            Event e = {};
            e.type = Event_GUI_Open;
            global_event_queue.enqueue(e);
            
            SetWindowText(window, "This is the Title Bar");
            
            // TODO: Should I prefix strings with an L?   L"string here"
            
            // Alse creating a static control to allow printing text
            // This needs its own message loop
            
            V2i button_pos = {30, 30};
            int button_width = 100;
            int button_height = 100;
            HINSTANCE instance = GetModuleHandle(NULL);
            HWND day_button = CreateWindow("BUTTON",
                                           "DAY",
                                           WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
                                           button_pos.x, button_pos.y,
                                           button_width, button_height,
                                           window,
                                           (HMENU)ID_DAY_BUTTON,
                                           instance, NULL);
            button_pos.y += button_height + 30;
            HWND week_button = CreateWindow("BUTTON",
                                            "WEEK",
                                            WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
                                            button_pos.x, button_pos.y,
                                            button_width, button_height,
                                            window,
                                            (HMENU)ID_WEEK_BUTTON,
                                            instance, NULL);
            button_pos.y += button_height + 30;
            HWND month_button = CreateWindow("BUTTON",
                                             "MONTH",
                                             WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
                                             button_pos.x, button_pos.y,
                                             button_width, button_height,
                                             window,
                                             (HMENU)ID_MONTH_BUTTON,
                                             instance, NULL);
            
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
        
        
        // It seems that pressing a button gives it the keyboard focus (shown by grey dotted border) and takes away the main windows keyboard focus.
        // These are sent directly to WinProc can't PeekMessage them.
        case WM_COMMAND:
        {
            switch (HIWORD(wParam))
            {
                case BN_CLICKED:
                {
                    
                    Event e = {};
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
                    
                    // Return keyboard focus to parent
                    SetFocus(window);
                    
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
                        Event e = {};
                        e.type = Event_GUI_Close;
                        global_event_queue.enqueue(e);
                        
                        ShowWindow(window, SW_HIDE);
                    }
                    else
                    {
                        Event e = {};
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
        switch (msg.message)
        {
            default:
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            } break;
        }
    }
}

struct Size_Data
{
    u8 *data;
    size_t size;
};

bool
read_data(HINTERNET request, u8 *read_data, size_t *read_bytes)
{
    u8 *recieved_data = read_data;
    size_t total_recieved = 0;
    
    DWORD available_bytes = 0;
    do
    {
        BOOL query_success =  WinHttpQueryDataAvailable(request, &available_bytes);
        if (!query_success)
        {
            tprint("No query");
            break;
        }
        
        if (available_bytes == 0) break;
        
        // TODO: Read directly into read_data
        u8 *response_buffer = (u8 *)xalloc(available_bytes + 1);
        memset(response_buffer, 0, available_bytes + 1);
        
        // If you are using WinHttpReadData synchronously, and the return value is TRUE and the number of bytes read is zero, the transfer has been completed and there are no more bytes to read on the handle.
        DWORD bytes_read = 0;
        BOOL read_success = WinHttpReadData(request, response_buffer, available_bytes, &bytes_read);
        if (!read_success)
        {
            tprint("No read");
            free(response_buffer);
            break;
        }
        
        if (bytes_read == 0)
        {
            tprint("0 bytes read, not sure if this is an error because can try to read when nothing is there and are at EOF");
        }
        
        memcpy(recieved_data + total_recieved, response_buffer, bytes_read);
        total_recieved += bytes_read;
        
        free(response_buffer);
    } while(available_bytes > 0);
    
    tprint("Read % bytes", total_recieved);
    
    *read_bytes = total_recieved;
    
    if (total_recieved == 0)
    {
        return false;
    }
    
    return true;
}


bool
get_icon_location(HINTERNET connection, char *location, size_t *length)
{
    wchar_t target[] = L"favicon.ico";
    
#if 1
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", NULL, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
#else
    HINTERNET request = WinHttpOpenRequest(connection,
                                           L"GET",
                                           target,
                                           NULL,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
#endif
    
    if (request == NULL)
    {
        tprint("No icon location request");
        return false;
    }
    
    defer(WinHttpCloseHandle(request));
    
    BOOL sent = WinHttpSendRequest(request,
                                   WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent)
    {
        tprint("No send");
        return false;
    }
    
    BOOL recieved = WinHttpReceiveResponse(request, NULL);
    if (!recieved)
    {
        tprint("No recieve");
        //print_error();
        return false;
    }
    
    DWORD dwStatusCode = 0;
    DWORD dwSize = sizeof(dwStatusCode);
    
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
    
    if (dwStatusCode != HTTP_STATUS_OK)
    {
        tprint("Not OK, status code: %", dwStatusCode);
        return false;
    }
    
    u8 *recieved_data = (u8 *)xalloc(Megabytes(8));
    size_t total_recieved = 0;
    bool read_success = read_data(request, recieved_data, &total_recieved);
    if (!read_success)
    {
        tprint("Could not read icon location data");
        return false;
    }
    
    // TODO: also type=""
    
    // check encoding but should be UTF-8
    char *match = strstr((char *)recieved_data, "rel=\"shortcut icon\"");
    if (!match)
    {
        match = strstr((char *)recieved_data, "rel=\"icon\"");
        if (!match)
        {
            return false;
        }
    }
    
    // TODO could also be behind, or absent if bad html, set upper limit on search
    char *href = strstr(match, "href=");
    if (!href)
    {
        return false;
    }
    
    char *start = href;
    size_t len = 0;
    
    for (char *at = href; *at; ++at)
    {
        if (*at == '"')
        {
            start = at + 1;
            break;
        }
    }
    
    for (char *at = start; *at; ++at)
    {
        if (*at == '"') break;
        len += 1;
    }
    
    if (len == 0)
    {
        return false;
    }
    
    memcpy(location, start, len);
    *length = len;
    return true;
}


bool
request_icon_from_url(wchar_t *url, u8 **icon_data, size_t *size)
{
    // https://docs.microsoft.com/en-us/windows/win32/winhttp/error-messages
    
    // many WinHTTP error codes not supported by FormatMessage
    DWORD error = 0;
    auto print_error = [&](){ error = GetLastError(); tprint("Error code: %", error); };
    
    HINTERNET session = WinHttpOpen(L"WinHttpPostSample",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == NULL)
    {
        tprint("No open");
        print_error();
        return false;
    }
    
    defer(WinHttpCloseHandle(session));
    
    // WinHttp does not accept international host names without converting them first to Punycode
    
    // Don't want https:// at start or a slash '/' at end both will give error
    HINTERNET connection = WinHttpConnect(session,
                                          url,
                                          INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection == NULL)
    {
        tprint("No connection");
        print_error();
        return false;
    }
    defer(WinHttpCloseHandle(connection));
    // From Wikipedia:
    // While the IANA-registered MIME type for ICO files is image/vnd.microsoft.icon,[8] it was submitted to IANA in 2003 by a third party and is not recognised by Microsoft software, which uses image/x-icon instead.[9] Erroneous types image/ico, image/icon, text/ico and application/ico have also been seen in use.[8]
    
    
    char location[400]; // utf8 because copied from html page // probably
    size_t location_len = 0;
    bool got_location = get_icon_location(connection, location, &location_len);
    if (!got_location)
    {
        tprint("Could not get favicon location");
        return false;
    }
    
    // TODO: Look at type="" and use this as icon_type
    // <link rel="icon" href="https://www.beginnerukuleles.com/wp-content/uploads/Beginner-Ukuleles-Blue-Favicon.png" type="image/png"/>
    // request failed for a .png icon at beginnerukuleles.com, mayube change mime type
    
    wchar_t target[400]; //  = L"favicon.ico"; // maybe need full path
    wchar_t icon_type[] = L"image/x-icon";//L"vnd.microsoft.icon";
    const wchar_t *media_types[] = {icon_type, NULL}; // maybe image/vnd.microsoft.icon or image/x-icon
    // can try HINTERNET hRequest = WinHttpOpenRequest( hConnect, L"GET", NULL, NULL, NULL, NULL, 0);
    
    // or this      hRequest = WinHttpOpenRequest( hConnect, L"GET", NULL, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    
    MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, location, location_len, target, array_count(target));
    
    // WINHTTP_FLAG_SECURE translates to using Secure Sockets Layer (SSL)/Transport Layer Security (TLS).
    HINTERNET request = WinHttpOpenRequest(connection,
                                           L"GET",
                                           target,
                                           NULL,
                                           WINHTTP_NO_REFERER,
                                           media_types,
                                           WINHTTP_FLAG_SECURE);
    if (request == NULL)
    {
        tprint("No request");
        return false;
    }
    
    
    defer(WinHttpCloseHandle(request));
    
    BOOL sent = WinHttpSendRequest(request,
                                   WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent)
    {
        tprint("No send");
        return false;
    }
    
    // when in synchronous mode can recieve response when sendrequest returns
    
    // When WinHttpReceiveResponse completes successfully, the status code and response headers have been received and are available for the application to inspect using WinHttpQueryHeaders. An application must call WinHttpReceiveResponse before it can use WinHttpQueryDataAvailable and WinHttpReadData to access the response entity body (if any).
    BOOL recieved = WinHttpReceiveResponse(request, NULL);
    if (!recieved)
    {
        tprint("No recieve");
        print_error();
        return false;
    }
    
    DWORD dwStatusCode = 0;
    DWORD dwSize = sizeof(dwStatusCode);
    
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
    
    if (dwStatusCode != HTTP_STATUS_OK)
    {
        tprint("Not OK, status code: %", dwStatusCode);
        return false;
    }
    
    // WINHTTP_QUERY_CONTENT_LENGTH to see how much to allocate?
    
    u8 *recieved_data = (u8 *)xalloc(Megabytes(8));
    size_t total_recieved = 0;
    
    read_data(request, recieved_data, &total_recieved);
    
    tprint("Read % bytes", total_recieved);
    
    if (total_recieved == 0)
    {
        return false;
    }
    
    tprint("Success");
    
    //if (request) WinHttpCloseHandle(request);
    //if (connection) WinHttpCloseHandle(connection);
    //if (session) WinHttpCloseHandle(session);
    
    *icon_data = recieved_data, *size = total_recieved;
    
    return true;
}

void
render_icon_to_bitmap_32(int width, int height, u8 *XOR, Simple_Bitmap *bitmap)
{
    int pitch = width * 4;
    
    u32 *dest = bitmap->pixels;
    u8 *src_row = (u8 *)XOR + ((height-1) * pitch);
    for (int y = 0; y < bitmap->height; ++y)
    {
        u32 *src = (u32 *)src_row;
        for (int x = 0; x < bitmap->width; ++x)
        {
            *dest++ = *src++;
        }
        
        src_row -= pitch;
    }
}


void
render_icon_to_bitmap_32_no_alpha(int width, int height, u8 *XOR, u8 *AND, Simple_Bitmap *bitmap)
{
    int xor_pitch = width * 4;
    int and_pitch = width / 8;
    
    u32 *dest = bitmap->pixels;
    
    u8 *xor_row = (u8 *)XOR + ((height-1) * xor_pitch);
    u8 *and_row = (u8 *)AND + ((height-1) * and_pitch);
    
    int bit = 7;
    for (int y = 0; y < bitmap->height; ++y)
    {
        u32 *src_xor = (u32 *)xor_row;
        u8 *src_and = and_row;
        for (int x = 0; x < bitmap->width; ++x)
        {
            u32 alpha = (*src_and & (1 << bit)) ? 0xFF : 0x00;
            *dest++ = *src_xor++ | (alpha << 24);
            bit -= 1;
            if (bit == -1)
            {
                bit = 7;
                ++src_and;
            }
        }
        
        xor_row -= xor_pitch;
        and_row -= and_pitch;
    }
}

void
render_icon_to_bitmap_16(int width, int height, u8 *XOR, u8 *AND, MY_RGBQUAD *table, Simple_Bitmap *bitmap)
{
    u32 *dest = bitmap->pixels;
    u16 *src_xor = (u16 *)XOR;
    u8 *src_and = (u8 *)AND;
    int bit = 7;
    for (int y = 0; y < bitmap->height; ++y)
    {
        for (int x = 0; x < bitmap->width; ++x)
        {
            u32 alpha = (*src_and & (1 << bit)) ? 0xFF : 0x00;
            MY_RGBQUAD col = table[*src_xor++];
            u32 colour = RGBA(col.red, col.green, col.blue, alpha);
            *dest++ = colour;
            bit -= 1;
            if (bit == -1)
            {
                bit = 7;
                ++src_and;
            }
        }
    }
}


void
render_icon_to_bitmap_8(int width, int height,  u8 *XOR, u8 *AND, MY_RGBQUAD *table, Simple_Bitmap *bitmap)
{
    int xor_pitch = width;
    int and_pitch = width / 8;
    
    u32 *dest = bitmap->pixels;
    u8 *xor_row = (u8 *)XOR + ((height-1) * xor_pitch);
    u8 *and_row = (u8 *)AND + ((height-1) * and_pitch);
    
    for (int y = 0; y < bitmap->height; ++y)
    {
        u8 *xor_src = xor_row;
        u8 *and_src = and_row;
        for (int x = 0; x < bitmap->width; ++x)
        {
            int bit = 7 - (x % 8);
            u32 alpha = (*and_src & (1 << bit)) ? 0xFF : 0x00;
            alpha = 0xff;
            
            MY_RGBQUAD col = table[*xor_src++];
            u32 colour = RGBA(col.red, col.green, col.blue, alpha);
            *dest++ = colour;
            
            if (bit == 0) ++and_src;
        }
        
        xor_row -= xor_pitch;
        and_row -= and_pitch;
    }
}


void
render_icon_to_bitmap_4(int width, int height, u8 *XOR, u8 *AND, MY_RGBQUAD *table, Simple_Bitmap *bitmap)
{
    u32 *dest = bitmap->pixels;
    int xor_pitch = (width * 4) / 8;
    int and_pitch = width / 8;
    
    u8 *xor_row = (u8 *)XOR + ((height-1) * xor_pitch);
    u8 *and_row = (u8 *)AND + ((height-1) * and_pitch);
    
    for (int y = 0; y < bitmap->height; ++y)
    {
        u8 *src_xor = xor_row;
        u8 *src_and = and_row;
        for (int x = 0; x < bitmap->width; ++x)
        {
            int and_bit = 7 - (x % 8);
            u32 alpha = (*src_and & (1 << and_bit)) ? 0x00 : 0xFf;
            //alpha = 0xFF;
            
            bool high_part = x % 2;
            u8 table_index = (high_part) ? *src_xor & 0x0F : *src_xor & 0xF0 >> 4;
            
            MY_RGBQUAD col = table[table_index];
            u32 colour = RGBA(col.red, col.green, col.blue, alpha);
            *dest++ = colour;
            
            
            if (and_bit == 0) ++src_and;
            if (high_part) ++src_xor;
        }
        
        xor_row -= xor_pitch;
        and_row -= and_pitch;
    }
}


Simple_Bitmap
get_bitmap_from_ico_file(u8 *file_data, u32 file_size, int max_icon_size)
{
    // We don't support varied file types or sophisticated searching for faviocn, just plain vanilla .ico
    // found at /favicon.ico
    
    Simple_Bitmap bitmap = make_bitmap(32, 32, RGBA(193, 84, 193, 255));
    
    if (file_size < sizeof(ICONDIR) + sizeof(ICONDIRENTRY) + sizeof(BITMAPINFOHEADER))
    {
        tprint("File size too small");
        return bitmap;
    }
    
    // favicon.ico file can also be a png I think
    u8 PNG_signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (memcmp(file_data, PNG_signature, sizeof(PNG_signature) == 0))
    {
        tprint("PNG not supported");
        return bitmap;
    }
    
    ICONDIR *dir = (ICONDIR *)file_data;
    if (!(dir->reserved == 0 && dir->type == 1 && dir->entry_count > 0))
    {
        tprint("Not icon file");
        return bitmap;
    }
    
    if (file_size < sizeof(ICONDIR) +
        (sizeof(ICONDIRENTRY)*dir->entry_count) +
        (sizeof(BITMAPINFOHEADER)*dir->entry_count))
    {
        tprint("File size too small for entries and headers");
        return bitmap;
    }
    
    ICONDIRENTRY *entries = (ICONDIRENTRY *)(file_data + sizeof(ICONDIR));
    
    // int closest_smaller_size_index = 0;
    int closest_size_index = 0;
    int min_diff = INT_MAX;
    for (int i = 0; i < dir->entry_count; ++i)
    {
        int diff = abs(max_icon_size - (int)entries[i].width);
        if (diff < min_diff)
        {
            closest_size_index = i;
            min_diff = diff;
        }
    }
    
    ICONDIRENTRY *entry = entries + closest_size_index;
    
    // Make sure that we at least got data for our chosen bitmap
    if (file_size < entry->offset + entry->size - 1)
    {
        tprint("File is incomplete");
        return bitmap;
    }
    
    tprint("Entry colour_count is %", entry->colour_count); // should be 0 if image doesn't use colour palette
    tprint("Entry bit_count is %", entry->bit_count);       // should be zero but isn't always
    tprint("Entry planes is %", entry->planes);             // should be zero but isn't always
    
    BITMAPINFOHEADER *header = (BITMAPINFOHEADER *)(file_data + entry->offset);
    
    // There can also be PNG embedded instead of bitmap I think
    if (memcmp(header, PNG_signature, sizeof(PNG_signature) == 0))
    {
        tprint("embedded PNG not supported");
        return bitmap;
    }
    
    int width = header->biWidth;
    int height = header->biHeight/2;
    int bpp = header->biBitCount;
    rvl_assert(height == entry->height && width == entry->width);
    
    // Firefox says this is right
    // firefox says colour table is present only if bpp is >= 8
    // can have non square icons
    // 24bpp not supported in ico files
    // BIBITFIELDS only valid with 16 and 32bpp (firefox)
    
    // biSizeImage can be 0 for uncompressed RGB images, but then the image size is just 4*width*height I guess
    
    // GIMP makes sure xor/and rows have 32 bit padding boundaries
    
    int table_size = 0;
    if (bpp <= 8)
    {
        table_size = pow(2, bpp) * sizeof(MY_RGBQUAD);
    }
    tprint("Table size: %", table_size);
    
    
    int image_data_size = entry->size - header->biSize - table_size;
    
    int xor_stride = ((((width * bpp) + 31) & ~31) >> 3);
    
    rvl_assert(xor_stride == width*bpp/8);
    
    // Is this padded?
    int xor_size = (width * height * bpp) / 8;
    int xor_size_2 = xor_stride * height;
    
    rvl_assert(xor_size == xor_size_2);
    
    // just assume there is an and mask
    bool has_and_mask = image_data_size != xor_size;
    tprint("Image data size: %", image_data_size);
    tprint("XOR size: %", xor_size);
    tprint("has_and_mask: %", has_and_mask);
    
    bool has_and_mask_2 = xor_size + table_size + header->biSize < entry->size;
    tprint("has_and_mask_2: %", has_and_mask_2);
    
    // mask rows are padded to 32 bits
    int and_mask_row_size = ((width + 31) / 32) * 4;
    
    // is this padded?
    int apparent_and_mask_size = entry->size - (header->biSize + table_size + xor_size);
    int and_mask_size = and_mask_row_size * height;
    
    MY_RGBQUAD *table = (MY_RGBQUAD *)((u8 *)header + header->biSize);
    u8 *xor = (u8 *)table + table_size;
    u8 *and = xor + xor_size;
    
    // Ignrore compression for now
    
    // maybe iterate and mask and if no set bits set all alpha to 0xFF.
    
    free(bitmap.pixels);
    bitmap = make_bitmap(width, height, RGBA(193, 84, 193, 255));
    
    tprint("bpp = %", bpp);
    switch (bpp)
    {
        case 1: {
            
        } break;
        case 4: {
            render_icon_to_bitmap_4(width, height, xor, and, table, &bitmap);
        } break;
        case 8: {
            render_icon_to_bitmap_8(width, height, xor, and, table, &bitmap);
        } break;
        case 16: {
            render_icon_to_bitmap_16(width, height, xor, and, table, &bitmap);
        } break;
        case 32: {
            // TODO: Width == stride?
            if (bitmap_has_alpha_component((u32 *)xor, width, height, width))
            {
                // ARGB format
                tprint("With alpha");
                render_icon_to_bitmap_32(width, height, xor, &bitmap);
            }
            else
            {
                // 0RGB format
                tprint("Without alpha");
                render_icon_to_bitmap_32_no_alpha(width, height, xor, and, &bitmap);
            }
        } break;
        
        default: {
            rvl_assert(0);
        } break;
    }
    
    return bitmap;
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
    HRESULT gg = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
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
    
    for (int i = 0; i < array_count(global_ms_icons); ++i)
    {
        HICON ico = LoadIconA(NULL, MAKEINTRESOURCE(32513 + i));
        global_ms_icons[i] = get_icon_bitmap(ico);
    }
    
    resize_screen_buffer(&global_screen_buffer, WindowWidth, WindowHeight);
    
    init_queue(&global_event_queue, 100);
    
    Font font = create_font("c:\\windows\\fonts\\times.ttf", 28);
    
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
    
    //remove(global_savefile_path);
    //remove(global_debug_savefile_path);
    
    // mit very screwed
    // stack overflow, google has extra row on top?
    // onesearch.library.uwa.edu.au is 8 bit
    // teaching.csse.uwa.edu.au hangs forever, maybe because not SSL?
    // scotthyoung.com 8 bit bitmap, seems fully transparent, maybe aren't using AND mask right?
    // http://ukulelehunt.com/ not SSL, getting Success read 0 bytes
    // forum.ukuleleunderground.com 15x16 icon, seemed to render fine though...
    // onesearch has a biSizeImage = 0, and bitCount = 8, and did set the biClrUsed field to 256, when icons shouldn't
    // voidtools.com
    // getmusicbee.com
    // mozilla.org   8bpp seems the and mask is empty the way i'm trying to read it
    
    // lots of smaller websites dont have an icon under /favicon.ico, e.g. ukulele sites
    
    // maths doesn't seem to work out calcing and_mask size ourmachinery.com
    
    
    u8 *icon_file_data = nullptr;
    size_t icon_file_size = 0;
    bool request_success = request_icon_from_url(L"beginnerukuleles.com", &icon_file_data, &icon_file_size);
    
    Simple_Bitmap favicon = {};
    if (request_success)
    {
        favicon = get_bitmap_from_ico_file(icon_file_data, icon_file_size, 128);
    }
    else
    {
        tprint("Request failure");
        favicon = make_empty_bitmap(10, 10);
    }
    
    Header header = {};
    
    Database database;
    init_database(&database);
    add_keyword(&database, "CITS3003");
    add_keyword(&database, "youtube");
    add_keyword(&database, "docs.microsoft");
    add_keyword(&database, "eso-community");
    
    start_new_day(&database, floor<date::days>(System_Clock::now()));
    
    // Strings:
    // name of program
    // full path of program
    // full URL of browser tab
    // keywords specified by user
    
    time_type added_times = 0;
    time_type duration_accumulator = 0;
    
    auto old_time = Steady_Clock::now();
    auto loop_start = Steady_Clock::now();
    
    DWORD sleep_milliseconds = 60;
    
    // Can't set lower than 10ms, not that you'd want to.
    // Can specify callback to call instead of posting message.
    SetTimer(window, 0, (UINT)sleep_milliseconds, NULL);
    
    Day_View day_view = {};
    bool gui_visible = (bool)IsWindowVisible(window);
    
    global_running = true;
    while (global_running) // main loop
    {
        WaitMessage();
        pump_messages(); // Need this to translate and dispatch messages.
        
        Button button = Button_Invalid;
        bool button_clicked = false;
        bool gui_opened = false;
        bool gui_closed = false;
        bool poll_programs = false;
        
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
            else if (e.type == Event_Poll_Programs)
            {
                poll_programs = true;
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
        if (current_date != database.days[database.day_count-1].date)
        {
            start_new_day(&database, current_date);
        }
        if (poll_programs)
        {
            poll_windows(&database, dt);
        }
        
#if 0
        if (gui_opened)
        {
            // Save a freeze frame of the currently saved days.
            init_day_view(&database, &day_view);
        }
        if (gui_closed)
        {
            destroy_day_view(&day_view);
        }
#endif
        init_day_view(&database, &day_view);
        
        
        
        if (button_clicked)
        {
            Button button_clicked = Button_Day;
            
            i32 period =
                (button_clicked == Button_Day) ? 1 :
            (button_clicked == Button_Week) ? 7 : 30;
            
            set_range(&day_view, period, current_date);
        }
        
        
        if (gui_visible)
        {
            render_gui(&global_screen_buffer, &database, &day_view, &font);
        }
        destroy_day_view(&day_view);
        
        if (favicon.pixels)
        {
            draw_simple_bitmap(&global_screen_buffer, &favicon, 200, 200);
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
    } // END LOOP
    
    auto loop_end = Steady_Clock::now();
    std::chrono::duration<time_type> loop_time = loop_end - loop_start;
    
    // Save to file TODO: (implement when file structure is not changing)
    
    free_table(&database.all_programs);
    
    free(font.atlas);
    free(font.glyphs);
    
    // Just in case we exit using ESCAPE
    Shell_NotifyIconA(NIM_DELETE, &global_nid);
    
    FreeConsole();
    
    CoUninitialize();
    
    return 0;
}





#if 0
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