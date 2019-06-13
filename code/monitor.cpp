
#include <string.h>
#include <stdio.h>
#include <chrono>

#include <windows.h>

#include "ravel.h"

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

// TODO: This works?
HWND global_text_window;
const char *global_strings[] = {
    "This is a test\n I can't feel my legs\n",
    "I want to ride my bicycle!",
    "Bababa bum.......\n Bum bum bum.......\n zip zing zap\n",
    "1\n2\n3\n4\n5\n6\n",
    "To the mooooooon\n Where we can eat cheese and crackers Gromit\n",
    "Long live the king\n\n\n\n\n\n",
};
int global_cur_string = 0;
int global_num_strings = array_count(global_strings);


LRESULT CALLBACK
WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            OutputDebugString("Got escape\n");
            if (wParam == VK_ESCAPE)
            {
                SendMessage(window, WM_CLOSE, 0, 0);
            }
            else if (wParam == VK_RIGHT)
            {
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

// Should be randomly named to avoid it being already created by other program
#define MUTEX_NAME "ThisApp"

int WINAPI
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance, 
        LPTSTR    CmdLine, 
        int       CmdShow)
{
    printf("test\n");

    HANDLE mutex = CreateMutexA(nullptr, TRUE, MUTEX_NAME);
    if (mutex == nullptr)
    {
        OutputDebugStringA("Error creating mutex\n");
        return 1;
    }
    
    DWORD error = GetLastError();
    if (error == ERROR_ACCESS_DENIED)
    {
        OutputDebugStringA("Error creating mutex: access denied\n");
        return 1;
    }
    else if (error == ERROR_ALREADY_EXISTS)
    {
        // Background exists in separate program, 
        // This could be child process or standalone program (there isn't really a difference)
        OutputDebugStringA("Starting GUI\n");


        WNDCLASSA window_class = {};
        window_class.style = CS_HREDRAW|CS_VREDRAW; // Redraw if height or width of win changed
        window_class.lpfnWndProc = WindowProc;
        window_class.hInstance = Instance;

        // window_class.hIcon;
        // window_class.hbrBackground;
        window_class.lpszClassName = "MonitorWindowClass";

        if(!RegisterClassA(&window_class))
        {
            return 1;
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
            Instance,
            0);

        if(!window)
        {
            return 1;
        }

        ShowWindow(window, CmdShow);
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
                OutputDebugStringA("Error: GetMessage failed\n");
                return 1;
            }

            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
        }


        printf("GUI\n");
        MessageBoxA(nullptr, "This is a GUI", "Title", MB_OKCANCEL); 

        OutputDebugStringA("Closing GUI\n");
        CloseHandle(mutex);
    }
    else
    {
        // Background doesn't exist
        // Start background, and open a GUI process
        OutputDebugStringA("Starting background and child GUI\n");

        STARTUPINFO startup_info = {};
        PROCESS_INFORMATION process_info = {};

        startup_info.cb = sizeof(STARTUPINFO);

        char filepath[2048];
        DWORD len = GetModuleFileNameA(nullptr, filepath, array_count(filepath));
        if (len == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            OutputDebugStringA("Could not get executable path");
            return 1;
        }

        // Need to get exe path, because could be run from anywhere
        BOOL success = CreateProcessA(
            filepath,
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
            OutputDebugStringA("Could not create child GUI\n");
            return 1;
        }

        printf("test 2\n");

        WaitForSingleObject(process_info.hProcess, INFINITE);

        // have to specify these?

        // waits until new process initialisaiton is finished and is waiting for input
        // May only need this if have to find window (or maybe process handle?) of child
        // WaitForInputIdle();

        // Preferred way of closing process is ExitProcess()



        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);


        OutputDebugStringA("Closing background\n");

        // Can open new background instance when this is closed
        // Must be valid
        rvl_assert(mutex);
        CloseHandle(mutex);
    }

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

    // NOTE: Possible option
    // SetWinEventHook() with EVENT_SYSTEM_FOREGROUND to get notified on foreground window change 
    Program programs[1000];
    i32 num_programs = 0;

    HANDLE con_in = GetStdHandle(STD_INPUT_HANDLE); 
    auto start = std::chrono::high_resolution_clock::now();

    bool running = true;
    while (running)
    {
        if (esc_key_pressed(con_in))
        {
            running = false;
        }

        HWND foreground_win = GetForegroundWindow();
        if (foreground_win)
        {
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


            // printf("QPC: %f\n", QPC);
            // tprint("Chrono: %", Chrono);


            if (window_text_len == 0) strcpy(window_text, "no window text");

            printf("Process ID: %u\n", process_id);
            printf("Filename: %s\n", filename);
            printf("Elapsed time: %lf\n", programs[prog_i].seconds);
            printf("Window text: %s\n", window_text);
            printf("\n");

        }
        else
        {
            printf("Losing activation\n");
        }
    }   // END LOOP


    printf("REPORT\n");
    for (i32 i = 0; i < num_programs; ++i)
    {
        printf("Filename: %s\n", programs[i].name);
        printf("Elapsed time: %lf\n", programs[i].seconds);
        printf("\n");
    }

    CloseHandle(con_in);
}
