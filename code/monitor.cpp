
#include <string.h>
#include <stdio.h>
#include <chrono>
#include <ctime>

#include <windows.h>

#include "ravel.h"

#include "database.h"

static constexpr char MutexName[] = "RVLmonitor143147";
static constexpr i32 MaxPathLen = 1024;

HWND global_text_window;

static char *global_debug_db_filepath;
static char *global_db_filepath;


// TODO: ExtractIconA()

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


LRESULT CALLBACK
WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            if (wParam == VK_ESCAPE)
            {
                SendMessage(window, WM_CLOSE, 0, 0);
            }
            else if (wParam == VK_RIGHT)
            {
                static const char *global_strings[] = {
                    "This is a test\n I can't feel my legs\n",
                    "I want to ride my bicycle!",
                    "Bababa bum.......\n Bum bum bum.......\n zip zing zap\n",
                    "1\n2\n3\n4\n5\n6\n",
                    "To the mooooooon\n Where we can eat cheese and crackers Gromit\n",
                    "Long live the king\n\n\n\n\n\n",
                };
                static int global_cur_string = 0;
                static int global_num_strings = array_count(global_strings);
                

                if (global_cur_string >= global_num_strings) global_cur_string = 0;

                SetWindowTextA(global_text_window, global_strings[global_cur_string]);
                ++global_cur_string;
            }
        } break;

        case WM_CREATE:
        {
            HINSTANCE instance = GetModuleHandle(NULL);
            global_text_window = CreateWindow("STATIC",
                                              "TextWindow",
                                              WS_VISIBLE|WS_CHILD|SS_LEFT,
                                              0,0,
                                              400,400,
                                              window, NULL, instance, NULL);

            SetWindowText(global_text_window, "Line 1\nLine2\n");
        } break;

#if 0
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

        case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam); 
        } break;

        case WM_ACTIVATEAPP:
            break;

        case WM_PAINT:
        {
            // Sometimes your program will initiate painting, sometimes will get send WM_PATIN
            // Only responsible for repainting client area
        } break;

        WM_NCCREATE WM_CREATE send before window becomes visible

#endif

        default:
            result = DefWindowProcA(window, msg, wParam, lParam);
            break;
    }

    return result;
}


bool run_gui(HINSTANCE instance, int cmd_show)
{
    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW|CS_VREDRAW; // Redraw if height or width of win changed
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    // window_class.hIcon;
    // window_class.hbrBackground;
    window_class.lpszClassName = "MonitorWindowClass";

    if (!RegisterClassA(&window_class))
    {
        return false;
    }

    // Sends WM_NCCREATE, WM_NCCALCSIZE, and WM_CREATE to window being created
    // OVERLAPPEDWINDOW is a OR of multiple flags to give title bar, min/maximise, border etc
    // TODO: Maybe don't want border
    HWND window = CreateWindowExA(
        0,
        window_class.lpszClassName,
        "MonitorGUI",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,  // Width (maybe don't want default)
        CW_USEDEFAULT,  // Height
        0,
        0,
        instance,
        0);

    if (!window)
    {
        return false;
    }

    ShowWindow(window, cmd_show);
    // UpdateWindow(window);


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

    // fclose(gui_log);
    return true;
}


int days_in_month(int month, int year)
{
    switch (month)
    {
        case 0: case 2: case 4:
        case 6: case 7: case 9:
        case 11:
            return 31;

        case 3: case 5:
        case 8: case 10:
            return 30;

        case 1:
            if (year % 4 == 0)
                return 29;
            else
                return 28;
    }

    return 0;
}

struct Date
{
    static_assert(std::is_pod<Date>::value, "Isn't POD");
    
    int day; // 1-31
    int month; // 0-11
    int year;  // 0 AD to INT_MAX AD

    void set_to_prev_day()
    {
        if (day == 1 && month == 0)
        {
            rvl_assert(year > 0);
            year -= 1;
        }

        month -= 1;
        if (month == -1) month = 11;
        
        day -= 1;
        if (day == 0)
        {
            day = days_in_month(month, year);
        }
    }

    bool is_same_date(Date date)
    {
        return (day == date.day && month == date.month && year == date.year);
    }

    bool is_later_date(Date date)
    {
        return ((year > date.year) ||
                (year == date.year && month > date.month) ||
                (year == date.year && month == date.month && day > date.day));
    }
};


bool
valid_date(Date date)
{
    if (date.day > 0 ||
        date.month >= 0 || date.month <= 11 ||
        date.year >= 2019 || date.year <= 2200)
    {
        if (date.day <= days_in_month(date.month, date.year))
        {
            return true;
        }
    }
    
    return false;
}

Date
unpack_16_bit_date(u16 date)
{
    Date d;
    d.day   = (date & DayMask) >> 11;
    d.month = (date & MonthMask) >> 7;
    d.year  = (date & YearMask) + 2000;

    return d;
}

u16
pack_16_bit_date(Date date)
{
    u16 day_bits   = (u16)date.day << 11;
    u16 month_bits = (u16)date.month << 7;
    u16 year_bits  = (u16)date.year - 2000;

    return day_bits|month_bits|year_bits;
}

bool
valid_16_bit_date(u16 date)
{
    Date d = unpack_16_bit_date(date);
    return valid_date(d);
}

Date
get_current_canonical_date(u16 day_start_time)
{
    time_t time_since_epoch = time(NULL);
    tm *local_time = localtime(&time_since_epoch);
    local_time->tm_year += 1900;
                
    // Tues night -------------------->|Mon morning (DayStart == 2*60)
    // Tues night ---->|Mon morning ------------>   (DayStart == 0*60)
    // | 22:00 | 23:00 | 00:00 | 01:00 | 02:00 |
    Date date {local_time->tm_mday, local_time->tm_mon, local_time->tm_year};

    i32 current_minute = local_time->tm_hour*60 + local_time->tm_min;
    if (current_minute < day_start_time)
    {
        date.set_to_prev_day();
    }

    return date;
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

u8 *get_program_list(u8 *database)
{
    rvl_assert(database);
    return database + sizeof(PMD_Header);
}

u8 *get_string_block(u8 *database, u32 max_programs)
{
    rvl_assert(database);
    return database + sizeof(PMD_Header) + max_programs*sizeof(PMD_Program);
}

u32 program_list_size(u32 max_programs)
{
    return max_programs*sizeof(PMD_Program);
}

File_Block
grow_string_block_and_program_list(File_Block database)
{
    // Could just realloc but doesn't matter this is called so rarely
    rvl_assert(database.data);


    // Double size of string block and program list
    PMD_Header *header = (PMD_Header *)database.data;
    size_t grow_amount = header->string_block_capacity + program_list_size(header->max_programs);

    File_Block new_database = {};
    new_database.data = (u8 *)calloc(1, database.size + grow_amount);
    if (!new_database.data)
    {
        return new_database;
    }

    new_database.size = database.size + grow_amount;


    PMD_Header *new_header = (PMD_Header *)new_database.data;
    new_header->max_programs *= 2;
    new_header->string_block_capacity *= 2;
    new_header->records_offset += grow_amount;
    new_header->offset_to_last_day += grow_amount;


    // Copy header and filled part of old program list
    memcpy(new_database.data,
           database.data,
           sizeof(PMD_Header) + program_list_size(header->max_programs));

    // Copy filled part of old string block
    memcpy(get_string_block(new_database.data, new_header->max_program),
           get_string_block(database.data, header->max_program),
           header->string_block_capacity);

    // Copy records
    memcpy(new_database.data + new_header->records_offset,
           database.data + header->record_offset,
           cur_size - header->record_offset);

    free(database.data);

    return new_database;
}

u32 sizeof_day(u16 num_records)
{
    return sizeof(PMD_Day) + num_records*sizeof(PMD_Record);
}

void
new_debug_database(u8 *database, PMD_Day today)
{
    rvl_assert(database);

    // Write debug file
    // TODO: How to sync writes to debug file between two processes
    //       (when GUI updates settings)
    PMD_Header *header = (PMD_Header *)database;
    FILE *debug_database_file = fopen(global_debug_db_filepath, "w");
    if (!debug_database_file)
    {
        tprint("Error: could not open debug database file");
        return;
    }

    time_t datetime = time(NULL);
    char *datetime_str = ctime(&datetime); // overwritten by other calls

    Date debug_today = unpack_16_bit_date(today.date);

    char buf[2048];
    rvl_snprintf(buf, 2048,
                 "\n"
                 "Created: % \n"
                 "PMD_HEADER \n"
                 "version: % \n"
                 "day start time: % \n"
                 "poll start time: % \n"
                 "poll end time: % \n"
                 "run at system startup: % (0 or 1) \n"
                 "poll frequency milliseconds: % \n"
                 "num programs in list: % \n"
                 "max programs: % \n"
                 "string block used: % \n"
                 "string block capacity: % \n"
                 "records offst: % \n"
                 "num records: % \n"
                 "offset to last day: % \n"
                 "\n"
                 "PROGRAM LIST \n"
                 "\n"
                 "STRING BLOCK \n"
                 "\n"
                 "RECORDS \n"
                 "Day: %, Month: %, Year: % \n"
                 "num programs: % \n",
                 datetime_str,
                 header->version,
                 header->day_start_time,
                 header->poll_start_time,
                 header->poll_end_time,
                 header->run_at_system_startup,
                 header->poll_frequency_milliseconds,
                 header->num_programs_in_list,
                 header->max_programs,
                 header->string_block_used,
                 header->string_block_capacity,
                 header->records_offset,
                 header->num_records,
                 header->offset_to_last_day,
                 debug_today.day, debug_today.month, debug_today.year,
                 today.num_programs);

    len = strlen(buf);
    fwrite(buf, 1, len, debug_database_file);
    fclose(debug_database_file);
    // tprint(buf);
}

void
debug_write_day(PMD_Day day)
{
    FILE *debug_database_file = fopen(global_debug_db_filepath, "a");
    if (!debug_database_file)
    {
        tprint("Error: could not open debug database file");
        return;
    }

    Date date = unpack_16_bit_date(day.date);

    char buf[2048];
    rvl_snprintf(buf, 2048,
                 "Day \n"
                 "date: d:% m:% y:% \n"
                 "num records \n",
                 date.day, date.month, date.year,
                 today.num_records);

    len = strlen(buf);
    fwrite(buf, 1, len, debug_database_file);
    fclose(debug_database_file);
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

    char *exe_path           = (char *)calloc(MaxPathLen, sizeof(char));
    global_db_filepath       = (char *)calloc(MaxPathLen, sizeof(char));
    global_debug_db_filepath = (char *)calloc(MaxPathLen, sizeof(char));
    if (!exe_path || !global_db_filepath || !global_debug_db_filepath)
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

    char *exe_name = get_file_from_path(exe_path);
    ptrdiff_t dir_len = exe_name - exe_path;
    if ((dir_len + array_count(DebugDBFileName) > MaxPathLen) ||
        (dir_len + array_count(DBFileName) > MaxPathLen))
    {
        tprint("Error: Directory path too long");
        return 1;
    }

    concat_strings(global_db_filepath, MaxPathLen,
                   exe_path, dir_len,
                   DBFileName, strlen(DBFileName));
    concat_strings(global_debug_db_filepath, MaxPathLen,
                   exe_path, dir_len,
                   DebugDBFileName, strlen(DebugDBFileName));

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
        tprint("GUI");
        // TODO: If only GUI is open, another run of program will only open another GUI
        bool success = run_gui(instance, cmd_show);
        if (!success) return 1;
        CloseHandle(mutex);
    }
    else
    {

        //
        //
        //
        remove(global_db_filepath);
        remove(global_debug_db_filepath);
        //
        //
        //

        File_Block database = {};
        database.data = nullptr;
        database.size = 0;

        u32 file_size = 0;

        PMD_Day today = {};
        PMD_Record *todays_records = (PMD_Record *)calloc(DayRecordMax, sizeof(PMD_Record));
        if (!todays_records)
        {
            tprint("Error: Could not allocate for todays records");
            return 1;
        }


        // Check if file exists (can exist but be opened by GUI)
        WIN32_FIND_DATAA find_file;
        HANDLE file_handle = FindFirstFileA(global_db_filepath, &find_file); 
        if (file_handle == INVALID_HANDLE_VALUE)
        {
            // File doesn't exist
            // Should be nothing else trying to access file, but ahve exclusive access

            // This all feels like two things
            // 1. write header to file and put in memory
            // 2. make a today in memory and write to file
            HANDLE file = CreateFileA(global_db_filepath, GENERIC_WRITE, 0,
                                      nullptr, CREATE_NEW, 0, nullptr); 
            if (file == INVALID_HANDLE_VALUE)
            {
                tprint("Error: Could not create database file");
                return 1;
            }

            u8 *database.data = (u8 *)calloc(1, InitialRecordOffset);
            if (!database.data)
            {
                tprint("Error: Could not allocate memory");
                return 1;
            }
                
            PMD_Header *header = (PMD_Header *)database.data;
            header->version                     = DefaultVersion;
            header->day_start_time              = DefaultDayStart;
            header->poll_start_time             = DefaultPollStart;
            header->poll_end_time               = DefaultPollEnd;
            header->poll_frequency_milliseconds = DefaultPollFrequencyMilliseconds;
            header->run_at_system_startup       = DefaultRunAtSystemStartup;
            header->num_programs_in_list        = 0;
            header->max_programs                = InitialProgramListMax;
            header->string_block_used           = 0;
            header->string_block_capacity       = InitialStringBlockCapacity;
            header->num_records                 = 1; // Because we add one for today
            header->records_offset              = InitialRecordOffset;
            header->offset_to_last_day          = InitialRecordOffset;

            database.size = InitialRecordOffset; 

            DWORD bytes_written;
            BOOL success = WriteFile(database_file, database.data,
                                     InitialRecordOffset, &bytes_written, nullptr);
            if (!success || bytes_written != InitialRecordOffset)
            {
                tprint("Error: Could not write to database");
                return 1;
            }

            // It still creates an Day record even if not poll start time (possibly with no programs)
            Date date = get_current_cannonical_date(header->day_start_time);

            today.num_programs = 0;
            today.date = pack_16_bit_date(date);

            bytes_written;
            success = WriteFile(file, &today,
                                sizeof(PMD_Day), &bytes_written, nullptr);
            if (!success || bytes_written != sizeof(PMD_Day))
            {
                tprint("Error: Could not write to database");
                return 1;
            }

            file_size = InitialRecordOffset + sizeof(today);

            FlushFileBuffers(file);            
            CloseHandle(file);

            new_debug_file(file_data, today);
            debug_write_day(today);
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

            // Reads with FILE_SHARE_READ, and opens existing
            File_Data file = read_entire_file(global_db_filepath);
            if (!file.data)
            {
                tprint("Error: Could not read database");
                return 1;
            }

            // TODO: Better checks for validity of whole file (because this is background startup)
            PMD_Header *header = (PMD_Header *)file.data;
            if (database.size % 8 != 0 ||
                header->version > MaxSupportedVersion ||
                header->day_start_time > 1439 ||
                header->poll_start_time > 1439 ||
                header->poll_end_time > 1439 ||
                header->poll_start_time > header->poll_end_time ||
                !(header->run_at_system_startup == 0 || header->run_at_system_startup == 1) ||
                header->num_programs_in_list > header->max_programs ||
                header->string_block_used > header->string_block_capacity ||
                header->records_offset >= file.size ||   // Maybe want this to be >=
                header->offset_to_last_day < header->records_offset ||
                header->offset_to_last_day >= file.size ||
                header->offset_to_last_day % 8 != 0 ||
                header->records_offset     % 8 != 0)
            {
                tprint("Error: Invalid header data");
                return 1;
            }

            if (header->num_records == 0)
            {
                // TODO: Not sure if I should allow this
                tprint("Error: Zero records");
                return 1;
            }

            PMD_Day *last_day = (PMD_Day *)(file.data + header->offset_to_last_day);
            Date last_date = unpack_16_bit_date(last_day->date);
            rvl_assert(valid_date(last_date));

            Date current_date = get_current_cannonical_date(header->day_start_time);
            if (current_date.is_later_date(last_date))
            {
                // Add a new day
                header->num_days += 1;
                header->offset_to_last_day += sizeof_day(last_day->num_records);

                today.date = pack_16_bit_date(current_date);
                today.num_records = 0;

                // Update header and make new day in records
                HANDLE f = CreateFileA(global_db_filepath, GENERIC_WRITE, 0,
                                       nullptr, OPEN_EXISTING, 0, nullptr); 
                if (f == INVALID_HANDLE_VALUE)
                {
                    tprint("Error: Could not create database file");
                    return 1;
                }

                // Overwrite header
                DWORD bytes_written;
                success = WriteFile(f, header, sizeof(PMD_Header), &bytes_written, nullptr);
                if (!success || bytes_written != sizeof(PMD_Header))
                {
                    tprint("Error: Could not write to database");
                    return 1;
                }
                
                DWORd fp = SetFilePointer(f, file.size, nullptr, FILE_BEGIN);
                if (fp == INVALID_SET_FILE_POINTER)
                {
                    tprint("Error: could not append to database");
                    return 1;
                }

                // Append today
                bytes_written;
                success = WriteFile(f, &today, sizeof(PMD_Day), &bytes_written, nullptr);
                if (!success || bytes_written != sizeof(PMD_Day))
                {
                    tprint("Error: Could not write to database");
                    return 1;
                }

                CloseHandle(f);

                debug_write_day(today);

                database.data = file.data;
                database.size = file.size;
                file_size = file.size + sizeof(today);
            }
            else if (current_date.is_same_date(last_date))
            {
                if (last_day->num_records > DayMaxRecords)
                {
                    tprint("Error: too many records for a day");
                    return 1;
                }

                today = *last_day;
                PMD_Record *last_day_records = last_day + sizeof(PMD_Day);
                today_records = memcpy(today_records,
                                       last_day_records,
                                       last_day->num_records * sizeof(PMD_Record));

                debug_write_day(today);

                // To be able to assume current day not in memory
                database.data = file.data;
                database.size = file.size - sizeof_day(last_day->num_records);
                file_size = file.size;
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

    // Need to get exe path, because could be run from anywhere
    // Pass command line?
    BOOL success = CreateProcessA(exe_path, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                                  &startup_info, &process_info);
    if (!success)
    {
        tprint("Error: Could not create child GUI");
        return 1;
    }

    free(exe_path);
    exe_path = nullptr;


    // TODO: Cleanup or functionise all the scary pointer arithmetic

    // Update these when they change
    PMD_Header  *header       = (PMD_Header *)database;
    PMD_Program *program_list = get_program_list(database);
    char        *string_block = get_string_block(database, header->max_programs);

    // TODO: Cache the days program strings and IDs, can also cache HWND (recycled) for a quick
    // initial lookup (with a double check against cached filename), then fallback to checking
    // all filenames.

    auto start = std::chrono::high_resolution_clock::now();
        
    bool running = true;
    while (running)
    {
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

        // char window_text[2048];
        // int window_text_len = GetWindowTextA(foreground_win, window_text, array_count(window_text));

        DWORD process_id;
        GetWindowThreadProcessId(foreground_win, &process_id); 

        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_id);
        if (!process)
        {
            tprint_col(Red, "Could not get process handle");
            break;
        }

        // filename_len is overwritten with number of characters written to buf
        char buf[2048]; // Do all our string operations in this
        DWORD filename_len = array_count(buf);
        BOOL success = QueryFullProcessImageNameA(process, 0, buf, &filename_len); 
        if (!success)
        {
            tprint_col(Red, "Could not get process filepath");
            break;
        }
            
        CloseHandle(process); 

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        start = end;

        // Returns a pointer to filename in buf
        char *filename = get_file_from_path(buf);
        size_t len = strlen(filename);
        if (len <= 4)
        {
            tprint("Error: filename too short");
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

        bool program_in_list = false;
        for (u32 i = 0; i < header->num_programs_in_list; ++i)
        {
            char *program_name = string_block + programs_list[i].name_offset;
            if (strcmp(filename, program_name) == 0)
            {
                bool program_in_list = true;
                u32 ID = i;
                break;
            }
        }

        Date now = get_current_cannonical_date(header->day_start_time);
        Date current = unpack_16_bit_date(today.date);
        if (now.is_same_date(last_date))
        {
        }
        else if (now.is_later_date(last_date))
        {
            // Add a new day

            // Put the last day and records into memory block
            realloc(database.data, database.size + sizeof_day(last_day->num_records));
            database.size += sizeof_day(last_day->num_records);
            header       = (PMD_Header *)database;
            program_list = get_program_list(database);
            string_block = get_string_block(database, header->max_programs);

            *(PMD_Day *)(database.data + header->offset_to_last_day) = today;
            memcpy(database.data + header->offset_to_last_day + sizeof(PMD_Day),
                   todays_records,
                   last_day->num_records*sizeof(PMD_Record));

            HANDLE file = CreateFileA(global_db_filepath, GENERIC_WRITE, 0,
                                      nullptr, OPEN_EXISTING, 0, nullptr); 
            if (file == INVALID_HANDLE_VALUE)
            {
                tprint("Error: Could not create database file");
                return 1;
            }

            DWORD bytes_written;
            success = WriteFile(file, database.data, database.size, &bytes_written, nullptr);
            if (!success || bytes_written != database.size)
            {
                tprint("Error: Could not write to database");
                return 1;
            }

            today.date = pack_16_bit_date(now);
            today.num_records = 0;

            header->num_days += 1;
            header->offset_to_last_day += sizeof_day(last_day->num_records);
                
            // Append today
            bytes_written;
            success = WriteFile(file, &today, sizeof(PMD_Day), &bytes_written, nullptr);
            if (!success || bytes_written != sizeof(PMD_Day))
            {
                tprint("Error: Could not write to database");
                return 1;
            }

            CloseHandle(f);
        }
        else
        {
            tprint("Error: current date is earlier than last recorded date");
            return 1;
        }

        if (!program_in_list)
        {
            bool resize_program_list = (header->num_programs_in_list + 1 > header->max_programs);
            bool resize_string_block = (len + 1 > header->string_block_capacity - header->string_block_used);
            if (resize_program_list || resize_string_block)
            {
                // invalidates all pointers to data in database
                database = grow_string_block_and_program_list(database);
                if (!database.data)
                {
                    tprint("Error: could not grow in memory database");
                    return 1;
                }

                header = (PMD_Header *)database.data;
                program_list = (PMD_Program*)get_program_list(database.data);
                string_block = (char *)get_string_block(database.data, header->max_programs);
            }

            // Add new program to list and string block
            PMD_Program program = {};
            program.ID = ID;
            program.name_offset = header->string_block_used;
            program_list[header->num_programs_in_list] = program;

            strcpy(string_block + header->string_block_used, filename);

            header->num_programs_in_list += 1;
            header->string_block_used += len + 1;
        }

        // Update record
        PMD_Record record = {};
        record.ID = ID;
        record.elapsed_time = duration.count();
        todays_records[today->num_records] = record;
        today->num_records += 1;

        // if (window_text_len == 0) strcpy(window_text, "no window text");
        // tprint("Process ID: %", process_id);
        // tprint("Filename: %", filename);
        // tprint("Elapsed time: %", programs[prog_i].seconds);
        // tprint("Window text: %", window_text);
        // tprint("\n");
            
        Sleep(sleep_milliseconds);
    } // END LOOP

    tprint("REPORT");
    for (i32 i = 0; i < num_programs; ++i)
    {
        tprint("Filename: %", programs[i].name);
        tprint("Elapsed time: %", programs[i].seconds);
        tprint("\n");
    }

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

}
