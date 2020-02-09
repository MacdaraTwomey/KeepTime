#include "ravel.h"

#include "monitor.h"

#include <chrono>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <windows.h>

#include "date.cpp"
#include "helper.cpp"

#define CONSOLE_ON 0

static char *global_savefile_path;

static char *global_debug_savefile_path;

static bool Global_Running = false;

static HWND global_text_window;


static constexpr char MutexName[] = "RVLmonitor143147";
static constexpr i32 MaxPathLen = 2048;
static constexpr char SaveFileName[] = "monitor_save.pmd";
static constexpr char DebugSaveFileName[] = "debug_monitor_save.txt";
static constexpr int WindowWidth = 400;
static constexpr int WindowHeight = 400;

static constexpr i32 MaxDailyRecords = 1000;
static constexpr i32 MaxDays = 10000;

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
    char *buf = (char *)calloc(MaxPathLen, sizeof(char));
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
esc_key_pressed(HANDLE con_in)
{
    DWORD num_events;
    GetNumberOfConsoleInputEvents(con_in, &num_events); 
    
    if (num_events > 0)
    {
        INPUT_RECORD input;
        for (int i = 0; i < num_events; ++i)
        {
            DWORD num_read_events;
            ReadConsoleInput(con_in, &input, 1, &num_read_events);
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

LRESULT CALLBACK
WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
#if 1
        // case WM_CLOSE: //  If this is ignored DefWindowProc calls DestroyWindow
        
        case WM_DESTROY: 
        {
            // Recieved when window is removed from screen but before destruction occurs 
            // and before child windows are destroyed.
            // Typicall call PostQuitMessage(0) to send WM_QUIT to message loop
            // So typically goes CLOSE -> DESTROY -> QUIT
            PostQuitMessage(0);
            
        } break;
        
        // case WM_QUIT:
        // Handled by message loop
        
        // WM_NCCREATE WM_CREATE send before window becomes visible
#endif
        
#if 0
        case WM_ACTIVATEAPP:
        {
        } break;
        
        case WM_PAINT:
        {
            // Sometimes your program will initiate painting, sometimes will get send WM_PATIN
            // Only responsible for repainting client area
            
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // All painting occurs here, between BeginPaint and EndPaint.
            
            FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));
            
            EndPaint(hwnd, &ps);
        } break;
#endif
        
        // When window created these messages are sent in this order:
        // WM_NCCREATE
        // WM_CREATE
        
        case WM_CREATE:
        {
            SetWindowText(window, "This is the Title Bar");
            
            // Alse creating a static control to allow printing text
            // This needs its own message loop
            HINSTANCE instance = GetModuleHandle(NULL);
            global_text_window = CreateWindow("STATIC",
                                              "TextWindow",
                                              WS_VISIBLE|WS_CHILD|SS_LEFT,
                                              0,0,
                                              400,400,
                                              window, NULL, instance, NULL);
            
            SetWindowText(global_text_window, "This is the static control\n");
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            // we should handle these in pump_messages
            rvl_assert(0);
            
        } break;
        
        
        default:
        result = DefWindowProcA(window, msg, wParam, lParam);
        break;
    }
    
    return result;
}

// Do we even need a pump messages or just handle everying in def window proc with GetMessage loop
void
pump_messages()
{
    MSG win32_message;
    while (PeekMessage(&win32_message, nullptr, 0, 0, PM_REMOVE))
    {
        switch (win32_message.message)
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
                // minimuse to tray
                Global_Running = false;
            } break;
            
            case WM_SYSKEYDOWN:
            case WM_KEYDOWN:
            {
                WPARAM wParam = win32_message.wParam;
                if (wParam == VK_ESCAPE)
                {
                    Global_Running = false;
                }
            } break;
            
            default:
            {
                TranslateMessage(&win32_message);
                DispatchMessageA(&win32_message);
            } break;
        }
    }
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
    // Dates of each day                    # days     (u16)
    // Index of end of each day             # days     (u32)
    // Array of all prorgam records clumped by day      # total records (Program_Record)
    
    Header header = {};
    if (fread(&header, sizeof(Header), 1, savefile) != 1)
    {
        fclose(savefile);
        return false;
    }
    
    bool valid = true;
    
    if (header.size_program_names_block > 0 &&
        header.num_total_programs > 0 &&
        header.num_days > 0 &&
        header.num_program_records > 0)
    {
        // All are non-zero
        s64 expected_size = 
            sizeof(Header) + 
            header.size_program_names_block + 
            (sizeof(u32) * header.num_total_programs) + 
            (sizeof(u16) * header.num_days) + 
            (sizeof(u32) * header.num_days) + 
            (sizeof(Program_Record) * header.num_program_records);
        if (expected_size != file_size)
        {
            valid = false;
        }
    }
    else
    {
        if (header.size_program_names_block == 0 &&
            header.num_total_programs == 0 &&
            header.num_days == 0 &&
            header.num_program_records == 0)
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
    // Dates of each day                    # days     (u16)
    // Index of end of each day             # days     (u32)
    // Array of all prorgam records clumped by day      # total records (Program_Record)
    
    Header header = {};
    fread(&header, sizeof(Header), 1, savefile);
    
    char *program_names = (char *)calloc(1, header.size_program_names_block);
    u32 *program_ids = (u32 *)calloc(1, header.num_total_programs * sizeof(u32));
    u16 *dates = (u16 *)calloc(1, header.num_days * sizeof(u16));
    u32 *indexes = (u32 *)calloc(1, header.num_days * sizeof(u32));
    Program_Record *records = (Program_Record *)calloc(1, header.num_program_records * sizeof(Program_Record));
    
    rvl_assert(&header.num_program_records > &header.num_days &&
               &header.num_days > &header.num_total_programs &&
               &(header.num_total_programs) > &(header.size_program_names_block));
    
    String_Builder sb = create_string_builder();
    sb.appendf("Header //-----------------------------------------\n"
               "%-25s %4lu \n"
               "%-25s %4lu \n"
               "%-25s %4lu \n"
               "%-25s %4lu \n",
               "size_program_names_block", header.size_program_names_block,
               "num_total_programs",       header.num_total_programs,
               "num_days",                 header.num_days,
               "num_program_records",      header.num_program_records);
    
    if (header.num_total_programs > 0)
    {
        fread(program_names, 1, header.size_program_names_block, savefile);
        fread(program_ids, sizeof(u32), header.num_total_programs, savefile);
        fread(dates, sizeof(u16), header.num_days, savefile);
        fread(indexes, sizeof(u32), header.num_days, savefile);
        fread(records, sizeof(Program_Record), header.num_program_records, savefile);
        
        sb.appendf("\nPrograms //---------------------------------\n");
        char *p = program_names;
        u32 name_index = 0;
        while (name_index < header.num_total_programs)
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
    for (u32 i = 0; i < header.num_days; ++i)
    {
        Date d = unpack_16_bit_date(dates[i]);
        sb.appendf("%i: [%i/%i/%i]   ", i, d.day, d.month+1, d.year);
    }
    sb.appendf("\n\nIndexes //------------------------------------\n");
    for (u32 i = 0; i < header.num_days; ++i)
    {
        sb.appendf("%lu,  ", indexes[i]);
    }
    
    sb.appendf("\n\nRecords //------------------------------------\n");
    sb.appendf("[ID:  Duration]\n");
    u32 cur_index = 0;
    u32 n = (cur_index == header.num_days-1) ? 
        header.num_program_records - indexes[cur_index] : indexes[cur_index+1] - indexes[cur_index];
    for (u32 i = 0; i < header.num_program_records; ++i)
    {
        sb.appendf("[%lu:  %lf]   Day %lu \n", records[i].ID, records[i].duration, cur_index);
        n -= 1;
        if (n == 0)
        {
            cur_index += 1;
            if (cur_index > header.num_days-1) 
            {
                break;
            }
            if (cur_index == header.num_days-1) 
            {
                n = header.num_program_records - indexes[cur_index];
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
#if 0
    if (header.num_total_programs > 0)
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


bool update_savefile(char *filepath, 
                     Header *header, 
                     char *program_names, u32 size_program_names_block,
                     u32 *program_ids, u32 num_ids,
                     u16 *dates, u32 num_dates,
                     u32 *indexes, u32 num_indexes,
                     Program_Record *records, u32 num_records)
{
    FILE *file = fopen(global_savefile_path, "wb+"); // Creates an empty file
    
    fwrite(header, sizeof(Header), 1, file);
    fwrite(program_names, 1, size_program_names_block, file);
    fwrite(program_ids, sizeof(u32), num_ids, file);
    fwrite(dates, sizeof(u16), num_dates, file);
    fwrite(indexes, sizeof(u32), num_indexes, file);
    fwrite(records, sizeof(Program_Record), num_records, file);
    
    fclose(file);
    return true;
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
    char *exe_path = (char *)calloc(MaxPathLen, sizeof(char));
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
    
    // Create message only window
    WNDCLASS window_class = {};
    window_class.style = CS_HREDRAW|CS_VREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hIcon =  LoadIcon(instance, "Icon");
    // window_class.hbrBackground;
    // window_class.lpszMenuName
    window_class.lpszClassName = "MonitorWindowClassName";
    
    if (!RegisterClassA(&window_class))
    {
        return 1;
    }
    
    HWND window = CreateWindowExA(0, window_class.lpszClassName, "Monitor",
                                  WS_VISIBLE|WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight, 
                                  NULL, NULL, 
                                  instance, NULL);
    
    if (!window)
    {
        return 1;
    }
    
    //ShowWindow(windows, SW_HIDE);
    //
    //
    //
    remove(global_savefile_path);
    remove(global_debug_savefile_path);
    //
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
    u32 num_days = 1;
    u32 cur_day = 0;
    
    days[0].records = (Program_Record *)calloc(1, sizeof(Program_Record) * MaxDailyRecords);
    days[0].num_records = 0;
    days[0].date = get_current_date();
    
    // Can there be no records for a day?
    
    FILE *savefile = fopen(global_savefile_path, "rb");
    
    // Header
    // null terminated names, in a block    # programs (null terminated strings)
    // corresponding ids                    # programs (u32)
    // Dates of each day                    # days     (u16)
    // Index of end of each day             # days     (u32)
    // Array of all prorgam records clumped by day      # total records (Program_Record)
    
    Header header = {};
    fread(&header, sizeof(Header), 1, savefile);
    
    if (header.num_total_programs == 0)
    {
        init_hash_table(&all_programs, 30);
    }
    else
    {
        init_hash_table(&all_programs, header.num_total_programs*2);
        
        char *program_names = (char *)calloc(1, header.size_program_names_block);
        u32 *program_ids = (u32 *)calloc(1, header.num_total_programs * sizeof(u32));
        fread(program_names, 1, header.size_program_names_block, savefile);
        fread(program_ids, sizeof(u32), header.num_total_programs, savefile);
        
        char *p = program_names;
        u32 name_index = 0;
        while (name_index < header.num_total_programs)
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
        long int dates_pos_in_file = sizeof(Header) + header.size_program_names_block + (sizeof(u32) * header.num_total_programs);
        
        long int indexes_pos_in_file = dates_pos_in_file + (sizeof(u16) * header.num_days);
        
        long int records_pos_in_file = indexes_pos_in_file + (sizeof(u32) * header.num_days);
        
        // Get last saved date
        u16 last_date = 0;
        fseek(savefile, dates_pos_in_file + (sizeof(u16) * (header.num_days-1)), SEEK_SET);
        fread(&last_date, sizeof(u16), 1, savefile);
        
        // Copy records if it is still the same day
        u16 current_date = days[0].date;
        if (current_date == last_date)
        {
            // Get index of todays records
            u32 last_day_records_index = 0;
            fseek(savefile, indexes_pos_in_file + (sizeof(u32) * (header.num_days-1)), SEEK_SET);
            fread(&last_day_records_index, sizeof(u32), 1, savefile);
            
            rvl_assert(last_day_records_index < header.num_program_records);
            days[0].num_records = header.num_program_records - last_day_records_index;
            
            // Copy to todays records
            fseek(savefile, records_pos_in_file + (sizeof(Program_Record) * last_day_records_index), SEEK_SET);
            fread(&days[0].records, sizeof(Program_Record), days[0].num_records, savefile);
        }
    }
    
    state.saved_header = header;
    
    fclose(savefile);
    
    String_Builder sb = create_string_builder();
    
    double added_time = 0;
    auto old_time = std::chrono::high_resolution_clock::now();
    
    auto loop_start = std::chrono::high_resolution_clock::now();
    
    Global_Running = true;
    while (Global_Running)
    {
        
#if CONSOLE_ON
        if (esc_key_pressed(con_in))
        {
            break;
        }
#endif
        
        pump_messages();
        
        char buf[2048] = {};
        char *filename = 0;
        
        HWND foreground_win = GetForegroundWindow();
        if (foreground_win)
        {
            DWORD process_id;
            GetWindowThreadProcessId(foreground_win, &process_id); 
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_id);
            if (!process)
            {
                tprint_col(Red, "Could not get process handle");
                break;
            }
            
            // filename_len is overwritten with number of characters written to buf
            DWORD filename_len = array_count(buf);
            BOOL success = QueryFullProcessImageNameA(process, 0, buf, &filename_len); 
            if (!success)
            {
                tprint_col(Red, "Could not get process filepath");
                break;
            }
            
            CloseHandle(process); 
            
            rvl_assert(filename_len > 0);
            
            // Returns a pointer to filename in buf
            filename = get_filename_from_path(buf);
            size_t len = strlen(filename);
            if (len <= 4 ||
                strcmp(&filename[len-4], ".exe") != 0)
            {
                tprint("Error: file is not an .exe");
                break;
            }
            
            rvl_assert(len > 0);
            
            // Remove '.exe' from end
            len -= 4;
            filename[len] = '\0';
            
            rvl_assert(filename[0]);
        }
        else
        {
            strcpy(buf, "No window");
            filename = buf;
        }
        
        auto new_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = new_time - old_time;
        old_time = new_time;
        
        u16 date = get_current_date(added_time);
        
        if (date != days[cur_day].date)
        {
            ++cur_day;
            ++num_days;
            days[cur_day].records = (Program_Record *)calloc(1, sizeof(Program_Record) * MaxDailyRecords);
            days[cur_day].num_records = 0;
            days[cur_day].date = date;
        }
        
        Day &today = days[cur_day];
        
        static double total_duration = 0;
        total_duration += duration.count();
        
        // Assume same day
        bool program_in_today = false;
        u32 ID = 0;
        bool in_table = all_programs.search(filename, &ID);
        
        if (in_table)
        {
            for (u32 i = 0; i < today.num_records; ++i)
            {
                if (ID == today.records[i].ID)
                {
                    program_in_today = true;
                    today.records[i].duration += duration.count();
                    break;
                }
            }
        }
        else
        {
            ID = all_programs.count;
            all_programs.add_item(filename, ID);
        }
        
        if (!program_in_today)
        {
            today.records[today.num_records] = {ID, duration.count()};
            today.num_records += 1;
        }
        
        sb.appendf("Num programs: %lu\n", today.num_records);
        sb.appendf("Accumulated duration.count(): %lf\n\n", total_duration);
        added_time = 0;
        for (u32 i = 0; i < today.num_records; ++i)
        {
            // Can store all names in array to quickly get name associated with ID
            char *name = all_programs.search_by_value(today.records[i].ID);
            rvl_assert(name);
            sb.appendf("%s %lu: %lf\n", name, today.records[i].ID, today.records[i].duration);
            
            added_time += today.records[i].duration;
        }
        
        sb.appendf("\nAdded records duration: %lf\n", added_time);
        SetWindowTextA(global_text_window, sb.str);
        
        if (Global_Running) sb.clear();
        
        Sleep(60);
        
    } // END LOOP
    
    auto loop_end = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds sleeptime(0); // TODO:!
    std::chrono::duration<double> loop_time = loop_end - loop_start -  std::chrono::duration<double>(sleeptime);
    
    sb.appendf("\nLoop Time: %lf\n", loop_time.count());
    sb.appendf("Diff: %lf\n", loop_time.count() - added_time);
    printf(sb.str);
    
    
    
    {
        // NOTE: Always create a new savefile for now
        Header new_header = state.saved_header;
        
        String_Builder sb = create_string_builder(); // We will build strings will add_string to allow null terminators to be interspersed
        
        u32 num_records = 0;
        for (u32 i = 0; i < num_days; ++i)
        {
            num_records += days[i].num_records;
        }
        
        std::vector<u32> program_ids;
        std::vector<u16> dates;
        std::vector<u32> indexes;
        std::vector<Program_Record> records;
        
        program_ids.reserve(all_programs.count);
        dates.reserve(num_days);
        indexes.reserve(num_days);
        records.reserve(num_records);
        
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
        
        new_header.size_program_names_block = sb.len;
        new_header.num_total_programs = all_programs.count;
        new_header.num_days = num_days;
        new_header.num_program_records = num_records;
        
        u32 index = 0;
        for (u32 i = 0; i < num_days; ++i)
        {
            dates.push_back(days[i].date);
            indexes.push_back(index);
            index += days[i].num_records;
            for (u32 j = 0; j < days[i].num_records; ++j)
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
    
#if CONSOLE_ON
    for (;;)
    {
        WaitForSingleObjectEx(con_in, INFINITE, FALSE);
        if (esc_key_pressed(con_in)) break;
    }
#endif
    
    free_table(&all_programs);
    
    
    // Can open new background instance when this is closed
    rvl_assert(mutex);
    CloseHandle(mutex);
    
    //FreeConsole();
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
