#pragma once

#include <windows.h>
#undef min
#undef max

#define ID_DAY_BUTTON 200
#define ID_WEEK_BUTTON 201
#define ID_MONTH_BUTTON 202

#define ID_TRAY_APP_ICON 1001
#define CUSTOM_WM_TRAY_ICON (WM_USER + 1)

// https://docs.microsoft.com/en-us/cpp/build/reference/manifestdependency-specify-manifest-dependencies?view=vs-2019
// #pragma comment(linker, "\"/manifestdependency:type='Win32' name='Test.Research.SampleAssembly' version='6.0.0.0' processorArchitecture='X86' publicKeyToken='0000000000000000' language='*'\"")

struct Process_Ids
{
    DWORD parent;
    DWORD child;
};

struct Options_Window
{
    bool is_open;
    int capacity;
    HWND list_view;
    HWND dialog;
};
