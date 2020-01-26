#include "ravel.h"

static HWND global_text_window;

static char *global_records_str;

bool
send_message_to_background(HWND sender_window)
{
    // Find message only window
    HWND background_window = FindWindowExA(HWND_MESSAGE, nullptr,
                                           BackgroundWindowClassName, nullptr);
    if (!background_window)
    {
        return false;
    }

    Message message = {};
    message.GUID = MessageGUID;
    message.type = MSG_WRITE_TO_FILE;

    // Dont trust sender of WM_COPYDATA, it could be any application
    COPYDATASTRUCT copy_data = {};
    copy_data.dwData = 1;
    copy_data.cbData = sizeof(Message);
    copy_data.lpData = &message;

    // Don't modify data being sent, or modify on recieving end, just copy it on receive

    // This will block until message is processed (we might want this because need to wait to
    // read the file anyway).
    SendMessage(background_window, WM_COPYDATA, (WPARAM)sender_window, (LPARAM)&copy_data);

    return true;
}

LRESULT CALLBACK
GUIWindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
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
                static const char *strings[] = {
                    "This is a test\n I can't feel my legs\n",
                    "I want to ride my bicycle!",
                    "Bababa bum.......\n Bum bum bum.......\n zip zing zap\n",
                    "1\n2\n3\n4\n5\n6\n",
                    "To the mooooooon\n Where we can eat cheese and crackers Gromit\n",
                    "Long live the king\n\n\n\n\n\n",
                };
                static int cur_string = 0;
                static int num_strings = array_count(strings);
                

                if (cur_string >= num_strings) cur_string = 0;

                SetWindowTextA(global_text_window, strings[cur_string]);
                ++cur_string;
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

            // Check if exists
            u32 cycles_burned = 0;
            File_Data file = {};
            do 
            {
                file = read_entire_file(global_db_filepath);
                if (cycles_burned == UINT32_MAX)
                {
                    tprint("Cycles burned: %", cycles_burned);
                    return false;
                }

                ++cycles_burned;
            }
            while (file.data == nullptr);

            tprint("Cycles burned: %", cycles_burned);
            
            // Check that database is in valid state
            Memory_Block state = {file.data, (u32)file.size};
            Header *header = get_header(state);

            global_records_str = (char *)calloc(10000, sizeof(char));
            strcpy(global_records_str, "Todays records\n");

            Program *program_list = get_program_list(state);
            char *string_block = get_string_block(state, header);

            Day_Info today = *(Day_Info *)(state.data + sizeof_top_block(header));
            Program_Time *record = (Program_Time *)(state.data + header->last_day_offset);

            for (u32 i = 0; i < today.num_programs; ++i)
            {
                for (u32 j = 0; j < header->num_programs_in_list; ++j)
                {
                    if (record[i].ID == program_list[j].ID)
                    {
                        strcpy(global_records_str, string_block + program_list[j].name_offset);
                        char buf[50];
                        snprintf(buf, 50, "%lf\n", record[i].elapsed_time);
                        strcpy(global_records_str, buf);
                        break;
                    }
                }
            }
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
    // Could use WNDCLASSEXA for hIconSm (but leaving it null allows windows to search hIcon
    // resource for necessary size)
    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW|CS_VREDRAW; // Redraw if height or width of win changed
    window_class.lpfnWndProc = GUIWindowProc;
    window_class.hInstance = instance;

    // Disabling Alloc console shows green icon in taskbar
    window_class.hIcon = LoadIcon(instance, "Icon");
    // window_class.hbrBackground;
    window_class.lpszClassName = "GUIMonitorWindowClass";

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


    // Could make this asynchronous then do other initialisation stuff,
    // then wait on file to be available
    bool success = send_message_to_background(window);

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
