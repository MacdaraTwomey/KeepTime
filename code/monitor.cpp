#include "ravel.h"

#include "monitor.h"
#include "resource.h"

#include <chrono>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <shellapi.h>
#define NOMINMAX
#include <windows.h>

#include <algorithm>

#undef max
#undef min

#include "date.h" 

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");

#include "date.cpp"
#include "helper.cpp"

#define CONSOLE_ON 1


static char *global_savefile_path;
static char *global_debug_savefile_path;
static bool Global_Running = false;
static bool Global_Visible = true; // Still counts as visible if minimised to the task bar
static HWND Global_Text_Window;
static NOTIFYICONDATA Global_Nid = {};

static constexpr char MutexName[] = "RVLmonitor143147";
static constexpr i32 MaxPathLen = 2048;
static constexpr char SaveFileName[] = "monitor_save.pmd";
static constexpr char DebugSaveFileName[] = "debug_monitor_save.txt";
static constexpr int WindowWidth = 400;
static constexpr int WindowHeight = 400;

static constexpr i32 MaxDailyRecords = 1000;
static constexpr i32 MaxDays = 10000;

// On windows Steady clock is based on QueryPerformanceCounter
// Steady clock typically uses system startup time as epoch, and system clock uses systems epoch like 1970-1-1 00:00 
using Clock = std::chrono::steady_clock;

// Clocks have a starting point (epoch) and tick rate (e.g. 1 tick per second)
// A Time Point is a duration of time that has passed since a clocks epoch
// A Duration consists of a span of time, defined as a number of ticks of some time unit (e.g. 12 ticks in millisecond unit)

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

#include <AtlBase.h>
#include <UIAutomation.h>

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
concat_strings(char *dest, size_t dest_len,
               const char *str1, size_t len1,
               const char *str2, size_t len2)
{
    rvl_assert(dest && str1 && str2);
    if ((len1 + len2 + 1) > dest_len) return;
    
    memcpy(dest, str1, len1);
    memcpy(dest + len1, str2, len2);
    dest[len1 + len2] = '\0';
}

static char *
get_filename_from_path(const char *filepath)
{
    // Returns pointer to filename part of filepath
    char *char_after_last_slash = (char *)filepath;
    for (char *at = char_after_last_slash; at[0]; ++at)
    {
        if (*at == '\\')
        {
            char_after_last_slash = at + 1;
        }
    }
    
    return char_after_last_slash;
}

char *
make_filepath(char *exe_path, const char *filename)
{
    char *buf = (char *)xalloc(MaxPathLen);
    if (!buf)
    {
        return nullptr;
    }
    
    char *exe_name = get_filename_from_path(exe_path);
    ptrdiff_t dir_len = exe_name - exe_path;
    if (dir_len + strlen(filename) + 1 > MaxPathLen)
    {
        free(buf);
        return nullptr;
    }
    
    concat_strings(buf, MaxPathLen,
                   exe_path, dir_len,
                   filename, strlen(filename));
    
    realloc(buf, dir_len + strlen(filename) + 1);
    if (!buf)
    {
        return nullptr;
    }
    
    return buf;
}


bool
make_empty_savefile(char *filepath)
{
    FILE *file = fopen(filepath, "wb");
    Header header = {};
    fwrite(&header, sizeof(header), 1, file);
    fclose(file);
    return true;
}

bool file_exists(char *path)
{
    FILE *file = fopen(path, "rb");
    if (file)
    {
        fclose(file);
        return true;
    }
    
    return false;
}

s64 get_file_size(char *filepath)
{
    // TODO: Pretty sure it is bad to seek to end of file
    FILE *file = fopen(filepath, "rb");
    if (file)
    {
        fseek(file, 0, SEEK_END); // Not portable
        long int size = ftell(file);
        fclose(file);
        return (s64) size;
    }
    
    return -1;
}


bool valid_savefile(char *filepath)
{
    // Assumes file exists
    s64 file_size = get_file_size(filepath);
    if (file_size < sizeof(Header))
    {
        return false;
    }
    
    FILE *savefile = fopen(filepath, "rb");
    if (!savefile)
    {
        return false;
    }
    
    // Header
    // null terminated names, in a block    # programs (null terminated strings)
    // corresponding ids                    # programs (u32)
    // Dates of each day                    # days     (u32)
    // Index of end of each day             # days     (u32)
    // Array of all prorgam records clumped by day      # total records (Program_Record)
    
    Header header = {};
    if (fread(&header, sizeof(Header), 1, savefile) != 1)
    {
        fclose(savefile);
        return false;
    }
    
    bool valid = true;
    
    if (header.program_names_block_size > 0 &&
        header.total_program_count > 0 &&
        header.day_count > 0 &&
        header.program_record_count > 0)
    {
        // All are non-zero
        s64 expected_size = 
            sizeof(Header) + 
            header.program_names_block_size + 
            (sizeof(u32) * header.total_program_count) + 
            (sizeof(Date) * header.day_count) + 
            (sizeof(u32) * header.day_count) + 
            (sizeof(Program_Record) * header.program_record_count);
        if (expected_size != file_size)
        {
            valid = false;
        }
    }
    else
    {
        if (header.program_names_block_size == 0 &&
            header.total_program_count == 0 &&
            header.day_count == 0 &&
            header.program_record_count == 0)
        {
            // All are zero
            if (file_size > sizeof(Header))
            {
                valid = false;
            }
        }
        else
        {
            // Only some are zero
            valid = false;
        }
    }
    
    // TODO: check values of saved data (e.g. ID must be smaller than program count)
    
    fclose(savefile);
    
    return valid;
}

void convert_savefile_to_text_file(char *savefile_path, char *text_file_path)
{
    FILE *savefile = fopen(savefile_path, "rb");
    
    // Header
    // null terminated names, in a block    # programs (null terminated strings)
    // corresponding ids                    # programs (u32)
    // Dates of each day                    # days     (u32)
    // Index of end of each day             # days     (u32)
    // Array of all prorgam records clumped by day      # total records (Program_Record)
    
    Header header = {};
    fread(&header, sizeof(Header), 1, savefile);
    
    char *program_names = (char *)xalloc(header.program_names_block_size);
    u32 *program_ids = (u32 *)xalloc(header.total_program_count * sizeof(u32));
    Date *dates = (Date *)xalloc(header.day_count * sizeof(Date));
    u32 *indexes = (u32 *)xalloc(header.day_count * sizeof(u32));
    Program_Record *records = (Program_Record *)xalloc(header.program_record_count * sizeof(Program_Record));
    
    rvl_assert(&header.program_record_count > &header.day_count &&
               &header.day_count > &header.total_program_count &&
               &(header.total_program_count) > &(header.program_names_block_size));
    
    String_Builder sb = create_string_builder();
    
    auto datetime = std::chrono::system_clock::now();
    time_t time = std::chrono::system_clock::to_time_t(datetime);
    sb.append(ctime(&time));
    
    sb.appendf("\nHeader //-----------------------------------------\n"
               "%-25s %4lu \n"
               "%-25s %4lu \n"
               "%-25s %4lu \n"
               "%-25s %4lu \n",
               "program_names_block_size", header.program_names_block_size,
               "total_program_count",       header.total_program_count,
               "day_count",                 header.day_count,
               "program_record_count",      header.program_record_count);
    
    if (header.total_program_count > 0)
    {
        fread(program_names, 1, header.program_names_block_size, savefile);
        fread(program_ids, sizeof(u32), header.total_program_count, savefile);
        fread(dates, sizeof(Date), header.day_count, savefile);
        fread(indexes, sizeof(u32), header.day_count, savefile);
        fread(records, sizeof(Program_Record), header.program_record_count, savefile);
        
        sb.appendf("\nPrograms //---------------------------------\n");
        char *p = program_names;
        u32 name_index = 0;
        while (name_index < header.total_program_count)
        {
            if (*p == '\0')
            {
                // Weird symbols to align to columns
                sb.appendf("%-15s %lu \n", program_names, program_ids[name_index]);
                ++name_index;
                program_names = p+1;
            }
            
            ++p;
        }
    }
    
    sb.appendf("\nDates //--------------------------------------\n");
    for (u32 i = 0; i < header.day_count; ++i)
    {
        Date d = dates[i];
        sb.appendf("%i: [%i/%i/%i]   ", i, d.dd, d.mm+1, d.yy);
    }
    sb.appendf("\n\nIndexes //------------------------------------\n");
    for (u32 i = 0; i < header.day_count; ++i)
    {
        sb.appendf("%lu,  ", indexes[i]);
    }
    
    sb.appendf("\n\nRecords //------------------------------------\n");
    sb.appendf("[ID:  Duration]\n");
    u32 cur_index = 0;
    u32 n = (cur_index == header.day_count-1) ? 
        header.program_record_count - indexes[cur_index] : indexes[cur_index+1] - indexes[cur_index];
    for (u32 i = 0; i < header.program_record_count; ++i)
    {
        sb.appendf("[%lu:  %lf]   Day %lu \n", records[i].ID, records[i].duration, cur_index);
        n -= 1;
        if (n == 0)
        {
            cur_index += 1;
            if (cur_index > header.day_count-1) 
            {
                break;
            }
            if (cur_index == header.day_count-1) 
            {
                n = header.program_record_count - indexes[cur_index];
            }
            else
            {
                n = indexes[cur_index+1] - indexes[cur_index];
            }
        }
    }
    
    FILE *text_file = fopen(text_file_path, "wb");
    fwrite(sb.str, 1, sb.len, text_file);
    fclose(text_file);
    
    free_string_builder(&sb);
    // TODO: This
#if 0
    if (header.total_program_count > 0)
    {
        free(program_names);
        free(program_ids);
        free(dates);
        free(indexes);
        free(records);
    }
#endif
    fclose(savefile);
}


bool 
update_savefile(char *filepath, 
                Header *header, 
                char *program_names, u32 program_names_block_size,
                u32 *program_ids, u32 id_count,
                Date *dates, u32 date_count,
                u32 *indexes, u32 index_count,
                Program_Record *records, u32 record_count)
{
    FILE *file = fopen(global_savefile_path, "wb+"); // Creates an empty file
    
    fwrite(header, sizeof(Header), 1, file);
    fwrite(program_names, 1, program_names_block_size, file);
    fwrite(program_ids, sizeof(u32), id_count, file);
    fwrite(dates, sizeof(Date), date_count, file);
    fwrite(indexes, sizeof(u32), index_count, file);
    fwrite(records, sizeof(Program_Record), record_count, file);
    
    fclose(file);
    return true;
}

char *clone_string(char *str, size_t len)
{
    char *clone = (char *)xalloc(len);
    if (clone)
    {
        strcpy(clone, str);
    }
    return clone;
}

struct Active_Program 
{
    char *name;
    size_t len;
    bool is_URL; // assumes firefox for now
    bool success;
};

Active_Program 
get_active_program()
{
    Active_Program active_program = {};
    HWND foreground_win = GetForegroundWindow();
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
                size_t len = strlen(filename);
                if (len > 4 && strcmp(&filename[len-4], ".exe") == 0)
                {
                    // Remove '.exe' from end
                    len -= 4;
                    filename[len] = '\0';
                    
                    rvl_assert(filename[0]);
                    
                    if (strcmp(filename, "firefox") == 0)
                    {
                        char *URL = buf;
                        size_t URL_len = 0;
                        bool success = get_firefox_url(foreground_win, URL, &URL_len);
                        if (success)
                        {
                            active_program.success = true;
                            active_program.is_URL = true;
                            active_program.len = URL_len;
                            active_program.name = clone_string(URL, URL_len+1);
                        }
                    }
                    else
                    {
                        active_program.success = true;
                        active_program.is_URL = false;
                        active_program.len = len;
                        active_program.name = clone_string(filename, len+1);
                    }
                }
            }
            
            CloseHandle(process); 
        }
        
    }
    else
    {
        // TODO: What to do in this case
        // No window
        active_program.success = true;
        active_program.is_URL = false;
        active_program.name = clone_string("No Window", strlen("No Window")+1);
        active_program.len = strlen(active_program.name);
    }
    
    return active_program;
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
                                               300, 300,
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
                    if (Global_Visible)
                    {
                        tprint("Hiding\n\n");
                        Global_Visible = !Global_Visible;
                        ShowWindow(window, SW_HIDE);
                    }
                    else
                    {
                        // TODO: This doesn't seem to 'fully activate' the window. The title bar is in focus but cant input escape key. So seem to need to call SetForegroundWindow
                        tprint("SHOWING\n");
                        Global_Visible = !Global_Visible;
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
                                  1000, 100, WindowWidth, WindowHeight, // Change  1000,100 to CS_USEDEFAULT
                                  NULL, NULL, 
                                  instance, NULL);
    
    if (!window)
    {
        return 1;
    }
    
    ShowWindow(window, cmd_show);
    
    Global_Visible = (bool)IsWindowVisible(window);
    rvl_assert(Global_Visible == (cmd_show != SW_HIDE));
    UpdateWindow(window);
    
    //
    //
    remove(global_savefile_path);
    remove(global_debug_savefile_path);
    //
    //
    
    if (!file_exists(global_savefile_path))
    {
        // Create database
        make_empty_savefile(global_savefile_path);
        rvl_assert(global_savefile_path);
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
    
    Monitor_State state = {};
    Hash_Table all_programs = {};
    Day days[MaxDays] = {};
    u32 day_count = 1;
    u32 cur_day = 0;
    
    days[0].records = (Program_Record *)xalloc(sizeof(Program_Record) * MaxDailyRecords);
    days[0].record_count = 0;
    days[0].date = get_current_date();
    
    // Can there be no records for a day?
    
    FILE *savefile = fopen(global_savefile_path, "rb");
    
    // Header
    // null terminated names, in a block    # programs (null terminated strings)
    // corresponding ids                    # programs (u32)
    // Dates of each day                    # days     (u32)
    // Index of end of each day             # days     (u32)
    // Array of all prorgam records clumped by day      # total records (Program_Record)
    
    Header header = {};
    fread(&header, sizeof(Header), 1, savefile);
    
    if (header.total_program_count == 0)
    {
        init_hash_table(&all_programs, 30);
    }
    else
    {
        init_hash_table(&all_programs, header.total_program_count*2);
        
        char *program_names = (char *)xalloc(header.program_names_block_size);
        u32 *program_ids = (u32 *)xalloc(header.total_program_count * sizeof(u32));
        fread(program_names, 1, header.program_names_block_size, savefile);
        fread(program_ids, sizeof(u32), header.total_program_count, savefile);
        
        char *p = program_names;
        u32 name_index = 0;
        while (name_index < header.total_program_count)
        {
            if (*p == '\0')
            {
                all_programs.add_item(program_names, program_ids[name_index]);
                ++name_index;
                program_names = p+1;
            }
            
            ++p;
        }
        
        free(program_names);
        free(program_ids);
    }
    {
        long int dates_pos_in_file = sizeof(Header) + header.program_names_block_size + (sizeof(u32) * header.total_program_count);
        
        long int indexes_pos_in_file = dates_pos_in_file + (sizeof(Date) * header.day_count);
        
        long int records_pos_in_file = indexes_pos_in_file + (sizeof(u32) * header.day_count);
        
        // Get last saved date
        Date last_date = {};
        fseek(savefile, dates_pos_in_file + (sizeof(Date) * (header.day_count-1)), SEEK_SET);
        fread(&last_date, sizeof(Date), 1, savefile);
        
        // Copy records if it is still the same day
        Date current_date = days[0].date;
        if (current_date == last_date)
        {
            // Get index of todays records
            u32 last_day_records_index = 0;
            fseek(savefile, indexes_pos_in_file + (sizeof(u32) * (header.day_count-1)), SEEK_SET);
            fread(&last_day_records_index, sizeof(u32), 1, savefile);
            
            rvl_assert(last_day_records_index < header.program_record_count);
            days[0].record_count = header.program_record_count - last_day_records_index;
            
            // Copy to todays records
            fseek(savefile, records_pos_in_file + (sizeof(Program_Record) * last_day_records_index), SEEK_SET);
            fread(&days[0].records, sizeof(Program_Record), days[0].record_count, savefile);
        }
    }
    
    state.saved_header = header;
    
    fclose(savefile);
    
    String_Builder sb = create_string_builder();
    
    time_type added_times = 0;
    time_type duration_accumulator = 0;
    
    auto old_time = Clock::now();
    auto loop_start = Clock::now();
    
    Global_Running = true;
    while (Global_Running)
    {
        Button_State button_state = {};
        pump_messages(&button_state);
        
        Active_Program active_program = get_active_program();
        if (!active_program.success) tprint("No active window");
        
        auto new_time = Clock::now();
        std::chrono::duration<time_type> diff = new_time - old_time; 
        old_time = new_time;
        time_type dt = diff.count();
        
        duration_accumulator += dt;
        
        Date current_date = get_current_date(duration_accumulator);
        
        if (current_date != days[cur_day].date)
        {
            ++cur_day;
            ++day_count;
            days[cur_day].records = (Program_Record *)xalloc(sizeof(Program_Record) * MaxDailyRecords);
            days[cur_day].record_count = 0;
            days[cur_day].date = current_date;
        }
        
        Day &today = days[cur_day];
        
        // Assume same day
        bool program_in_today = false;
        u32 ID = 0;
        bool in_table = all_programs.search(active_program.name, &ID);
        
        if (in_table)
        {
            for (u32 i = 0; i < today.record_count; ++i)
            {
                if (ID == today.records[i].ID)
                {
                    program_in_today = true;
                    today.records[i].duration += dt;
                    break;
                }
            }
        }
        else
        {
            ID = all_programs.count;
            all_programs.add_item(active_program.name, ID);
        }
        
        if (!program_in_today)
        {
            today.records[today.record_count] = {ID, dt};
            today.record_count += 1;
        }
        
        if (button_state.clicked)
        {
            rvl_assert(button_state.button != Button_Invalid);
            
            u32 period = 
                (button_state.button == Button_Day) ? 1 :
            (button_state.button == Button_Week) ? 7 : 30; 
            
            rvl_assert(day_count > 0);
            Date earliest_date_not_in_file = days[0].date;
            s32 day_diff = day_difference(current_date, earliest_date_not_in_file);
            if (day_diff+1 >= period)
            {
                // Look for first date that is within period.
                u32 early_date_index = 0;
                if (day_diff+1 == period)
                {
                    u32 early_date_index = 0;
                }
                
                // Linear search for correct starting day
                for (u32 i = 0; i < day_count; ++i)
                {
                    //if (day_difference(days[i].date, 
                }
            }
            else
            {
                // Need days in file
            }
        }
        
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
        
        if (active_program.success) free(active_program.name);
        
        Sleep(60);
        
    } // END LOOP
    
    auto loop_end = Clock::now();
    std::chrono::milliseconds sleeptime(60); // TODO:!
    std::chrono::duration<time_type> loop_time = loop_end - loop_start -  std::chrono::duration<time_type>(sleeptime);
    
    sb.appendf("\nLoop Time: %lf\n", loop_time.count());
    // NOTE: Maybe be slightly different because of instructions between loop finishing and taking the time here, also Sleep may not be exact. Probably fine to ignore.
    sb.appendf("Diff: %lf\n", loop_time.count() - duration_accumulator);
    
    
    //printf(sb.str);
    
    
    
    {
        // NOTE: Always create a new savefile for now
        Header new_header = state.saved_header;
        
        String_Builder sb = create_string_builder(); // We will build strings will add_string to allow null terminators to be interspersed
        
        u32 total_record_count = 0;
        for (u32 i = 0; i < day_count; ++i)
        {
            total_record_count += days[i].record_count;
        }
        
        std::vector<u32> program_ids;
        std::vector<Date> dates;
        std::vector<u32> indexes;
        std::vector<Program_Record> records;
        
        program_ids.reserve(all_programs.count);
        dates.reserve(day_count);
        indexes.reserve(day_count);
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
        new_header.program_record_count = total_record_count;
        
        u32 index = 0;
        for (u32 i = 0; i < day_count; ++i)
        {
            dates.push_back(days[i].date);
            indexes.push_back(index);
            index += days[i].record_count;
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
                        indexes.data(), indexes.size(),
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
