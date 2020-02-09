#include <string.h>
#include <stdio.h>
#include <chrono>
#include <ctime>

#include <windows.h>

#include "ravel.h"

#include "database.h"
#include "msg.h"

static char *global_db_filepath;
static char *global_debug_filepath;

#include "date.cpp"
#include "database.cpp"
#include "gui.cpp"

static constexpr char MutexName[] = "RVLmonitor143147";
static constexpr i32 MaxPathLen = 2048;

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
get_file_from_path(const char *filepath)
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
BackgroundWindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
#if 0
        case WM_CREATE:
        {
        } break;
        
        case WM_CLOSE:
        // If this is ignored DefWindowProc calls DestroyWindow
        break;
        
        case WM_DESTROY:
        // Recieved when window is removed from screen but before destruction occurs 
        // and before child windows are destroyed.
        // Typicall call PostQuitMessage(0) to send WM_QUIT to message loop
        // So typically goes CLOSE -> DESTROY -> QUIT
        break;
        
        // case WM_QUIT:
        // Handled by message loop
        
        WM_NCCREATE WM_CREATE send before window becomes visible
        
#endif
        
            default:
        result = DefWindowProcA(window, msg, wParam, lParam);
        break;
    }
    
    return result;
}

void
pump_messages(Database *database, Day today)
{
    MSG win32_message;
    while (PeekMessage(&win32_message, nullptr, 0, 0, PM_REMOVE))
    {
        switch (win32_message.message)
        {
            case WM_COPYDATA:
            {
                COPYDATASTRUCT *copy_data = (COPYDATASTRUCT *)win32_message.lParam;
                if (copy_data->cbData == sizeof(Message) &&
                    copy_data->dwData == 1)
                {
                    // Check that message is most likely from our GUI
                    Message m = *(Message *)copy_data->lpData;
                    if (m.GUID == MessageGUID)
                    {
                        switch (m.type)
                        {
                            case MSG_WRITE_TO_FILE:
                            {
                                Header *header = get_header(database->state);
                                HANDLE file = CreateFileA(global_db_filepath, GENERIC_WRITE, 0,
                                                          nullptr, OPEN_EXISTING, 0, nullptr); 
                                if (file == INVALID_HANDLE_VALUE)
                                {
                                }
                                
                                DWORD bytes_written;
                                BOOL success = WriteFile(file, database->state.data, sizeof_top_block(header), &bytes_written, nullptr);
                                if (!success)
                                {
                                }
                                
                                DWORD fp = SetFilePointer(file, database->file_append_offset, nullptr, FILE_BEGIN);
                                if (fp == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
                                {
                                }
                                
                                bytes_written;
                                success = WriteFile(file, &today.info, sizeof(Day_Info), &bytes_written, nullptr);
                                if (!success)
                                {
                                }
                                
                                bytes_written;
                                success = WriteFile(file, today.programs, today.info.num_programs*sizeof(Program_Time),
                                                    &bytes_written, nullptr);
                                if (!success)
                                {
                                }
                                
                                CloseHandle(file);
                                
                                database->file_size = database->file_append_offset + sizeof_day(today.info);
                                // file_append_offset stays the same
                                
                                if (!database->day_partially_in_file)
                                {
                                    header->num_days += 1;
                                    header->last_day_offset = database->file_append_offset; 
                                }
                                
                                database->day_partially_in_file = true;
                                
                                debug_update_file(*database);
                            } break;
                            
                            default:
                            {
                                rvl_assert(0);
                            } break;
                        }
                    }
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

char *
init_filepath(char *exe_path, const char *filename)
{
    char *buf = (char *)calloc(MaxPathLen, sizeof(char));
    if (!buf)
    {
        return nullptr;
    }
    
    char *exe_name = get_file_from_path(exe_path);
    ptrdiff_t dir_len = exe_name - exe_path;
    if (dir_len + strlen(filename) + 1 > MaxPathLen)
    {
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



int WINAPI
WinMain(HINSTANCE instance,
        HINSTANCE prev_instance, 
        LPTSTR    cmd_line, 
        int       cmd_show)
{
    // Use FileLock to synchronise read write
    // Or maybe just use exclusive file open so only one process can get at it at once
    // Could use mutex to signal when done I suppose
    
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
    HANDLE con_in = create_console();
    
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
    
    global_db_filepath = init_filepath(exe_path, DBFileName);
    if (!global_db_filepath)
    {
        tprint("Error: Could not initialise filepath");
        return 1;
    }
    
    global_debug_filepath = init_filepath(exe_path, DebugDBFileName);
    if (!global_debug_filepath)
    {
        tprint("Error: Could not initialise debug filepath");
        return 1;
    }
    
    size_t arg_len = strlen(cmd_line);
    if (strcmp(cmd_line, "-background") == 0)
    {
        // Run B if not running
    }
    else if (strcmp(cmd_line, "-gui") == 0)
    {
        // Run G
    }
    else if (strcmp(cmd_line, "") == 0)
    {
        // Run G, and B if not running
    }
    else if (strcmp(cmd_line, "--help") == 0)
    {
    }
    else if (strcmp(cmd_line, "/?") == 0)
    {
    }
    else
    {
        // TODO: Pretty sure this wont print is a normal terminal because window app
        printf("Monitor unknown option: %s\n", cmd_line);
        printf("Try '--help' for options\n");
        return 1;
    }
    
    HANDLE mutex = CreateMutexA(nullptr, TRUE, MutexName);
    DWORD error = GetLastError();
    if (mutex == nullptr ||
        error == ERROR_ACCESS_DENIED ||
        error == ERROR_INVALID_HANDLE)
    {
        tprint("Error creating mutex\n");
        return 1;
    }
    
    bool background_exists = (error == ERROR_ALREADY_EXISTS);
    
    
    if (background_exists)
    {
        CloseHandle(mutex); // this right?
        tprint("GUI");
        
        bool success = run_gui(instance, cmd_show);
    }
    else
    {
        // Create message only window
        WNDCLASSA window_class = {};
        window_class.style = 0;
        window_class.lpfnWndProc = BackgroundWindowProc;
        window_class.hInstance = instance;
        // window_class.hIcon;
        // window_class.hbrBackground;
        window_class.lpszClassName = BackgroundWindowClassName;
        
        if (!RegisterClassA(&window_class))
        {
            return 1;
        }
        // Message only window
        HWND window = CreateWindowExA(0, window_class.lpszClassName, "MonitorBackground",
                                      0, 0, 0, 0, 0, HWND_MESSAGE, 0, instance, 0);
        if (!window)
        {
            return 1;
        }
        
        //
        //
        //
        remove(global_db_filepath);
        remove(global_debug_filepath);
        //
        //
        //
        
        Database database = {};
        
        Day today = {};
        today.programs = (Program_Time *)calloc(MaxProgramsPerDay, sizeof(Program_Time));
        if (!today.programs)
        {
            tprint("Error: Could not allocate for todays records");
            return 1;
        }
        
        tprint(global_db_filepath);
        
        // Check if file exists (can exist but be already opened by GUI)
        if (strlen(global_db_filepath) > MAX_PATH)
        {
            tprint("Error: Find first file ANSI version limited to MATH_PATH");
            return 1;
        }
        
        WIN32_FIND_DATAA find_file;
        HANDLE file_handle = FindFirstFileA(global_db_filepath, &find_file); 
        if (file_handle == INVALID_HANDLE_VALUE)
        {
            if (GetLastError() != ERROR_FILE_NOT_FOUND)
            {
                tprint("Error: FindFile failed");
                return 1;
            }
            
            // File doesn't exist
            Memory_Block *block = &database.state;
            block->size = InitialDaysOffset; 
            block->data = (u8 *)calloc(1, InitialDaysOffset);
            if (!block->data)
            {
                tprint("Error: Could not allocate memory");
                return 1;
            }
            
            Header *header = (Header *)block->data;
            header->version                     = DefaultVersion;
            header->day_start_time              = DefaultDayStart;
            header->poll_start_time             = DefaultPollStart;
            header->poll_end_time               = DefaultPollEnd;
            header->poll_frequency_milliseconds = DefaultPollFrequencyMilliseconds;
            header->run_at_system_startup       = DefaultRunAtSystemStartup;
            header->num_programs_in_list        = 0;
            header->max_programs                = InitialMaxProgramsInList;
            header->string_block_used           = 0;
            header->string_block_capacity       = InitialStringBlockCapacity;
            header->num_days                    = 0;
            header->days_offset                 = InitialDaysOffset;  // Set to this for safety
            header->last_day_offset             = InitialDaysOffset;  // Set to this for safety
            
            HANDLE file = CreateFileA(global_db_filepath, GENERIC_WRITE, 0,
                                      nullptr, CREATE_NEW, 0, nullptr); 
            if (file == INVALID_HANDLE_VALUE)
            {
                tprint("Error: Could not create database file");
                return 1;
            }
            
            DWORD bytes_written;
            BOOL success = WriteFile(file, block->data,
                                     block->size, &bytes_written, nullptr);
            if (!success)
            {
                tprint("Error: Could not write to database");
                return 1;
            }
            
            rvl_assert(bytes_written == block->size);
            
            CloseHandle(file);
            
            Date current_date = get_current_canonical_date(header->day_start_time);
            today.info.num_programs = 0;
            today.info.date = pack_16_bit_date(current_date);
            
            database.file_size = InitialDaysOffset;
            database.file_append_offset = InitialDaysOffset;
            
            debug_update_file(database);
        }
        else
        {
            // File exists
            FindClose(file_handle);
            
            if (find_file.nFileSizeHigh == 0 && find_file.nFileSizeLow == 0)
            {
                // Have to check here because read returns nullptr on file size 0
                tprint("Error: Database size of 0");
                return 1;
            }
            
            if (find_file.nFileSizeHigh > 0)
            {
                // Have to check here because read returns nullptr on file size 0
                tprint("Error: Database size too large > 4gb");
                return 1;
            }
            
            File_Data file = read_entire_file(global_db_filepath);
            if (!file.data)
            {
                tprint("Error: Could not read entire database");
                return 1;
            }
            
            // Block *block = &database.state;
            
            // Try this
            Memory_Block& block = database.state;
            block.data = file.data;
            block.size = (u32)file.size;
            
            database.file_size = (u32)file.size;
            database.file_append_offset = (u32)file.size;
            
            // Can have file with zero days and records, as well as file with offsets set to 0
            
            // TODO: Better checks for validity of whole file (because this is background startup)
            Header *header = (Header *)block.data;
            if (database.file_size % 8 != 0 ||
                header->version > MaxSupportedVersion ||
                header->day_start_time > 1439 ||
                header->poll_start_time > 1439 ||
                header->poll_end_time > 1439 ||
                header->poll_start_time > header->poll_end_time ||
                !(header->run_at_system_startup == 0 || header->run_at_system_startup == 1) ||
                header->num_programs_in_list > header->max_programs ||
                header->string_block_used > header->string_block_capacity ||
                (header->num_days > 0 && header->days_offset >= file.size) ||
                (header->num_days > 0 && header->last_day_offset >= file.size) ||
                (header->num_days > 0 && header->days_offset < sizeof_top_block(header)) ||
                (header->num_days > 0 && header->last_day_offset < sizeof_top_block(header)) ||
                header->last_day_offset < header->days_offset ||
                header->last_day_offset % 8 != 0 ||
                header->days_offset     % 8 != 0)
            {
                tprint("Error: Invalid header data");
                return 1;
            }
            
            // Reduce size so we can ignore records (could also realloc)
            block.size = sizeof_top_block(header);
            
            Day_Info& day_info = today.info;
            
            Date current_date = get_current_canonical_date(header->day_start_time);
            if (header->num_days == 0)
            {
                // Has no 'last day'
                day_info.date = pack_16_bit_date(current_date);
                day_info.num_programs = 0;
            }
            else
            {
                Day_Info *last_day = (Day_Info *)(block.data + header->last_day_offset);
                Date last_date = unpack_16_bit_date(last_day->date);
                rvl_assert(valid_date(last_date));
                
                if (current_date.is_later_than(last_date))
                {
                    day_info.date = pack_16_bit_date(current_date);
                    day_info.num_programs = 0;
                }
                else if (current_date.is_same(last_date))
                {
                    rvl_assert(last_day->num_programs <= MaxProgramsPerDay);
                    
                    database.day_partially_in_file = true;
                    database.file_append_offset -= sizeof_day(*last_day);
                    
                    // Copy what we have of day so far into memory and remvove from file
                    day_info.date          = last_day->date;
                    day_info.num_programs  = last_day->num_programs;
                    
                    memcpy(today.programs,
                           block.data + header->last_day_offset + sizeof(Day_Info),
                           last_day->num_programs * sizeof(Program_Time));
                }
                else
                {
                    // Error should not be earlier than saved date
                    tprint("Error: Corrupted record, (invalid date)");
                    return 1;
                }
            }
        }
        
        STARTUPINFO startup_info = {};
        startup_info.cb = sizeof(STARTUPINFO);
        PROCESS_INFORMATION process_info = {};
        // Pass command line?
        BOOL success = CreateProcessA(exe_path, nullptr, nullptr, nullptr,
                                      FALSE, 0, nullptr, nullptr,
                                      &startup_info, &process_info);
        if (!success)
        {
            tprint("Error: Could not create child GUI");
            return 1;
        }
        
        // This is not freed in GUI process
        free(exe_path);
        exe_path = nullptr;
        
        Header  *header       = get_header(database.state);
        Program *program_list = get_program_list(database.state);
        char    *string_block = get_string_block(database.state, header);
        
        // TODO: Cache the days program strings and IDs, can also cache HWND (recycled) for a quick
        // initial lookup (with a double check against cached filename), then fallback to checking
        // all filenames.
        
        // TODO: make sure everything works when there are zero records
        
        DWORD sleep_milliseconds = 2000;
        
        auto old_time = std::chrono::high_resolution_clock::now();
        
        bool running = true;
        while (running)
        {
            // When MsgWaitForMultipleObjects tells you there is message, you have to process
            // all messages
            // Returns when there is a message no one knows about
            DWORD wait_result = MsgWaitForMultipleObjects(0, nullptr, false, sleep_milliseconds,
                                                          QS_SENDMESSAGE);
            
            debug_update_file(database);
            
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
            
            pump_messages(&database, today);
            
            if (esc_key_pressed(con_in))
            {
                break;
            }
            
            HWND foreground_win = GetForegroundWindow();
            if (!foreground_win)
            {
                // tprint("Losing activation\n");
                Sleep(sleep_milliseconds);
                continue;
            }
            
            // Get date and time just after we get foreground window to be accurate as possible
            Date current_date = get_current_canonical_date(header->day_start_time);
            
            auto new_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = new_time - old_time;
            old_time = new_time;
            
            DWORD process_id;
            GetWindowThreadProcessId(foreground_win, &process_id); 
            
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_id);
            if (!process)
            {
                tprint_col(Red, "Could not get process handle");
                break;
            }
            
            char buf[2048];
            // filename_len is overwritten with number of characters written to buf
            DWORD filename_len = array_count(buf);
            BOOL success = QueryFullProcessImageNameA(process, 0, buf, &filename_len); 
            if (!success)
            {
                tprint_col(Red, "Could not get process filepath");
                break;
            }
            
            CloseHandle(process); 
            
            // Returns a pointer to filename in buf
            char *filename = get_file_from_path(buf);
            size_t len = strlen(filename);
            if (len <= 4)
            {
                tprint("Error: filename too short to be an executable");
                break;
            }
            
            if (filename[len - 4] != '.' &&
                filename[len - 1] != 'e' &&
                filename[len - 2] != 'x' &&
                filename[len - 3] != 'e')
            {
                tprint("Error: file is not an .exe");
                break;
            }
            
            // Remove '.exe' from end
            len -= 4;
            filename[len] = '\0';
            
            Date last_date = unpack_16_bit_date(today.info.date);
            if (current_date.is_later_than(last_date))
            {
                // Add a new day (possibly overwrites part of the last day in file)
                // Updates header but doesn't write changes to file.
                HANDLE file = CreateFileA(global_db_filepath, GENERIC_WRITE, 0,
                                          nullptr, OPEN_EXISTING, 0, nullptr); 
                if (file == INVALID_HANDLE_VALUE)
                {
                    tprint("Error: Could not create database file");
                    return 1;
                }
                
                DWORD fp = SetFilePointer(file, database.file_append_offset, nullptr, FILE_BEGIN);
                if (fp == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
                {
                    tprint("Error: Could not seek in file");
                    return 1;
                }
                
                DWORD bytes_written;
                success = WriteFile(file, &today.info, sizeof(Day_Info), &bytes_written, nullptr);
                if (!success)
                {
                    tprint("Error: Could not write to database");
                    return 1;
                }
                
                rvl_assert(bytes_written == sizeof(Day_Info));
                
                bytes_written;
                success = WriteFile(file, today.programs,
                                    today.info.num_programs*sizeof(Program_Time),
                                    &bytes_written, nullptr);
                if (!success)
                {
                    tprint("Error: Could not write to database");
                    return 1;
                }
                
                rvl_assert(bytes_written == today.info.num_programs*sizeof(Program_Time));
                
                CloseHandle(file);
                
                // Size is where we were writing from + how much we wrote
                database.file_size = database.file_append_offset + sizeof_day(today.info);
                
                if (database.day_partially_in_file)
                {
                    // Day now over, so write whole day to file
                    database.day_partially_in_file = false;
                    database.file_append_offset = database.file_size;
                }
                else
                {
                    header->num_days += 1;
                    header->last_day_offset = database.file_append_offset; 
                    database.file_append_offset = database.file_size;
                }
                
                // Change date of today
                today.info.date = pack_16_bit_date(current_date);
                today.info.num_programs = 0;
                
                debug_update_file(database);
            }
            
            bool program_in_list = false;
            u32 ID = 0;
            for (u32 i = 0; i < header->num_programs_in_list; ++i)
            {
                char *program_name = string_block + program_list[i].name_offset;
                if (strcmp(filename, program_name) == 0)
                {
                    bool program_in_list = true;
                    ID = i;
                    break;
                }
            }
            
            if (!program_in_list)
            {
                u32 string_block_unused = header->string_block_capacity - header->string_block_used;
                bool resize_program_list = (header->num_programs_in_list + 1 > header->max_programs);
                bool resize_string_block = (len + 1 > string_block_unused);
                
                if (resize_program_list || resize_string_block)
                {
                    size_t grow_amount = header->string_block_capacity +
                        sizeof_program_list(header);
                    
                    // This updates file
                    database = grow_string_block_and_program_list(database);
                    if (!database.state.data)
                    {
                        tprint("Error: Could not grow database");
                        return 1;
                    }
                    
                    // Update to possible new memory location
                    header       = get_header(database.state);
                    program_list = get_program_list(database.state);
                    string_block = get_string_block(database.state, header);
                    
                }
                
                ID = add_program_to_database(filename, database.state);
            }
            
            
            // Add to todays records
            bool program_in_today = false;
            for (u32 i = 0; i < today.info.num_programs; ++i)
            {
                if (ID == today.programs[i].ID)
                {
                    bool program_in_today = true;//bug
                    today.programs[i].elapsed_time += duration.count();
                    break;
                }
            }
            
            if (!program_in_today)
            {
                // This will get executed if new program was added to program_list
                Program_Time program_time = {};
                program_time.ID = ID;
                program_time.elapsed_time = duration.count();
                today.programs[today.info.num_programs] = program_time;
                today.info.num_programs += 1;
            }
            
            rvl_assert(today.info.num_programs < MaxProgramsPerDay);
            
        } // END LOOP
        
        for (;;)
        {
            WaitForSingleObjectEx(con_in, INFINITE, FALSE);
            if (esc_key_pressed(con_in)) break;
        }
        
        // Cleanup
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
        
        // Can open new background instance when this is closed
        rvl_assert(mutex);
        CloseHandle(mutex);
    } // END BACKGROUND
    
    FreeConsole();
    return 0;
}

void
tray_icon()
{
    // https://bobobobo.wordpress.com/2009/03/30/adding-an-icon-system-tray-win32-c/
    
    // Makes window and taskbar icon disappear (while still running)
    // ShowWindow(window, SW_HIDE);
    
    // Make a Message-Only Windows (not visible, and can send and recieve messages)
    // Or could also do window without WS_VISIBLE
    
    // WM_QUERYENDSESSION and WM_ENDSESSION for system shutdown
    
    // add the icon to the system tray
    // NOTIFYICONDATAA 
    // Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
    // Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData); // on app close
    
    // WM_TASKBARCREATED if explorer.exe (and taskbar) crashes
    
    // LoadIcon 
    
    // can respond to use defined TRAY_ICON message which is defined in NOTIFYICONDATA
    // allowing to detect clicks etc
    
    // Can also just hide window and create tray icon on close (therefore only using one process total)
    
    
}



void get_info()
{
    // TODO: Handle program closing, into an untracked program (or into desktop or other non program)
    
    // TODO: ExtractIconA()
}
