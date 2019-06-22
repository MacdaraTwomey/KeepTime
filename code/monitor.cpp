
#include <string.h>
#include <stdio.h>
#include <chrono>
#include <ctime>

#include <windows.h>

#include "ravel.h"

#include "database.h"

static constexpr char MutexName[] = "RVLmonitorprogram147";

// TODO: ExtractIconA()

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

struct Program
{
    char *name;
    double seconds;
};

HWND global_text_window;

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

    if(!RegisterClassA(&window_class))
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

    if(!window)
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

    AllocConsole();

    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONIN", "r", stdin);

    HANDLE con_out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD out_mode;
    GetConsoleMode(con_out, &out_mode); 
    SetConsoleMode(con_out, out_mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING); 

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
        //
        //
        remove(DBFileName);
        remove(DebugDBFileName);


        // NOTE: Need to keep one copy of current file state in memory

        // Check if file exists (can exist but be opened by GUI)
        WIN32_FIND_DATAA find_file;
        HANDLE file_handle = FindFirstFileA(DBFileName, &find_file); 
        if (file_handle != INVALID_HANDLE_VALUE)
        {
            // File exists
            FindClose(file_handle);

            // TODO: Truncate file to 0 on error and create fresh
            if (find_file.nFileSizeHigh > 0)
            {
                // Should never get this big
                tprint("Error: Database size > 4GB");
                return 1;
            }

            if (find_file.nFileSizeLow < sizeof(PMD_Header))
            {
                tprint("Error: Corrupted database");
                return 1;
            }

            // Reads with FILE_SHARE_READ, and opens existing
            Read_Entire_File_Result database_file = read_entire_file_and_null_terminate(DBFileName);
            if (!database_file.data)
            {
                tprint("Error: Could not read database");
                return 1;
            }

            PMD_Header *header = (PMD_Header *)database_file.data;
            if (header->version > MaxSupportedVersion ||
                header->records_offset >= database_file.size ||
                (header->num_records == 0 && header->records_offset != database_file.size))
            {
                tprint("Error: Corrupted database");
                return 1;
            }

            free(database_file.data);
        }
        else
        {
            // File doesn't exist

            // Should be nothing else trying to access file

            HANDLE database_file = CreateFileA(DBFileName,
                                               GENERIC_WRITE,
                                               0, nullptr, // Exclusive access
                                               CREATE_NEW,
                                               0, nullptr); 
            if (database_file != INVALID_HANDLE_VALUE)
            {
                PMD_Header header = {};
                header.version                     = DefaultVersion;
                header.day_start_time              = DefaultDayStart;
                header.poll_start_time             = DefaultPollStart;
                header.poll_end_time               = DefaultPollEnd;
                header.poll_frequency_milliseconds = DefaultPollFrequencyMilliseconds;
                header.run_at_system_startup       = DefaultRunAtSystemStartup;
                header.num_programs                = 0;
                header.max_programs                = InitialProgramListMax;
                header.string_block_used           = 0;
                header.string_block_capacity       = InitialStringBlockCapacity;
                header.num_records                 = 0;
                header.records_offset              = InitialRecordOffset;


                // Write header and zeroed out program index and string block
                u8 *file_data = (u8 *)calloc(1, InitialRecordOffset);
                if (!file_data)
                {
                    tprint("Error: Could not allocate memory");
                    return 1;
                }

                *((PMD_Header *)file_data) = header;

                DWORD bytes_written;
                BOOL success = WriteFile(database_file, file_data,
                                         InitialRecordOffset, &bytes_written,
                                         nullptr);
                if (!success || bytes_written != InitialRecordOffset)
                {
                    tprint("Error: Could not write to database");
                    return 1;
                }

                // If background is opened on any one day, even if it itsn't poll start time 
                // it still creates an Day record (possibly with no programs)


                // TODO: change num days ++

                PMD_Day today = {};
                time_t result = time(NULL);
                tprint("TIME: %", result);


                CloseHandle(database_file);






                // Write debug file
                // TODO: How to sync writes to debug file between two processes
                //       (when GUI updates settings)
                FILE *debug_database_file = fopen(DebugDBFileName, "w");
                if (!debug_database_file)
                {
                    tprint("Error: could not open debug database file");
                    return 1;
                }

                auto now = std::chrono::system_clock::now();
                std::time_t datetime = std::chrono::system_clock::to_time_t(now);
                char *datetime_str = std::ctime(&datetime); // overwritten by other calls
                size_t len = strlen(datetime_str);

                fwrite(datetime_str, 1, len, debug_database_file);

                char buf[2048];
                rvl_snprintf(buf, 2048,
                         "\n"
                         "PMD_HEADER \n"
                         "version: % \n"
                         "day start time: % \n"
                         "poll start time: % \n"
                         "poll end time: % \n"
                         "run at system startup: % (0 or 1) \n"
                         "poll frequency milliseconds: % \n"
                         "num programs: % \n"
                         "max programs: % \n"
                         "string block used: % \n"
                         "string block capacity: % \n"
                         "records offst: % \n"
                         "num records: % \n"
                         "\n"
                         "PROGRAM LIST \n"
                             "\n"
                             "STRING BLOCK \n"
                             "\n"
                             "RECORDS \n"
                             ""
                             ,
                         header.version,
                         header.day_start_time,
                         header.poll_start_time,
                         header.poll_end_time,
                         header.run_at_system_startup,
                         header.poll_frequency_milliseconds,
                         header.num_programs,
                         header.max_programs,
                         header.string_block_used,
                         header.string_block_capacity,
                         header.records_offset,
                         header.num_records);

                len = strlen(buf);
                fwrite(buf, 1, len, debug_database_file);
                fclose(debug_database_file);

                tprint(buf);

                // Create current day









                free(file_data);
            }
        }
            
        // FlushFileBuffers()?
        // SetEndOfFile()
        // SetFilePointer();    // overwrite not insert
        // WriteFile();







        STARTUPINFO startup_info = {};
        startup_info.cb = sizeof(STARTUPINFO);
        PROCESS_INFORMATION process_info = {};

        char filepath[2048];
        DWORD len = GetModuleFileNameA(nullptr, filepath, array_count(filepath));
        if (len == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            tprint("Error: Could not get executable path");
            return 1;
        }

        // Need to get exe path, because could be run from anywhere
        BOOL success =
            CreateProcessA(filepath,
                           nullptr,
                           nullptr,
                           nullptr,
                           FALSE,
                           0,
                           nullptr,
                           nullptr,
                           &startup_info,
                           &process_info);
        if (!success)
        {
            tprint("Error: Could not create child GUI");
            return 1;
        }


        // have to specify these?

        // waits until new process initialisaiton is finished and is waiting for input
        // May only need this if have to find window (or maybe process handle?) of child
        // WaitForInputIdle();

        // Preferred way of closing process is ExitProcess()

        


        DWORD sleep_milliseconds = 1000;

        Program programs[1000];
        i32 num_programs = 0;

        auto start = std::chrono::high_resolution_clock::now();
        int runs = 1000;
        
        bool running = true;
        while (running)
        {
            HWND foreground_win = GetForegroundWindow();
            if (!foreground_win)
            {
                // tprint("Losing activation\n");
                Sleep(sleep_milliseconds);
                continue;
            }

            char window_text[2048];
            int window_text_len = GetWindowTextA(foreground_win, window_text,
                                                 array_count(window_text));

            DWORD process_id;
            GetWindowThreadProcessId(foreground_win, &process_id); 

            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process_id);
            if (!process)
            {
                tprint_col(Red, "Could not get process handle");
                break;
            }

            // filename_len is overwritten with number of characters written to buf
            char buf[2048];
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
            filename[len - 4] = '\0';

            // Can make faster be recording each programs process_id and check those first
            // then check filenames (however proc id can be reused quickly?)
            bool filename_recorded = false;
            int prog_i = 0;
            for (i32 i = 0; i < num_programs; ++i)
            {
                if (strcmp(filename, programs[i].name) == 0)
                {
                    filename_recorded = true;
                    programs[i].seconds += duration.count();
                    prog_i = i;
                }
            }

            if (!filename_recorded)
            {
                rvl_assert(num_programs < array_count(programs));

                size_t new_size = len - 4 + 1; // minus '.exe' plus null terminator
                char *new_program_name = (char *)malloc(new_size);
                memcpy(new_program_name, filename, new_size);

                programs[num_programs].name = new_program_name;
                programs[num_programs].seconds = duration.count();
                ++num_programs;
            }


            if (window_text_len == 0) strcpy(window_text, "no window text");

            // tprint("Process ID: %", process_id);
            // tprint("Filename: %", filename);
            // tprint("Elapsed time: %", programs[prog_i].seconds);
            // tprint("Window text: %", window_text);
            // tprint("\n");
            
            Sleep(sleep_milliseconds);
            ++runs;
            if (runs >= 3) break;
        }

        tprint("REPORT");
        for (i32 i = 0; i < num_programs; ++i)
        {
            tprint("Filename: %", programs[i].name);
            tprint("Elapsed time: %", programs[i].seconds);
            tprint("\n");
        }


        // Cleanup
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);

        Sleep(5000);

        // Can open new background instance when this is closed
        // Must be valid
        CloseHandle(mutex);
    }

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
