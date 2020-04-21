#pragma once

#include <windows.h>
#undef min
#undef max

#define ID_DAY_BUTTON 200
#define ID_WEEK_BUTTON 201
#define ID_MONTH_BUTTON 202

#define ID_TRAY_APP_ICON 1001
#define CUSTOM_WM_TRAY_ICON (WM_USER + 1)

struct Process_Ids
{
    DWORD parent;
    DWORD child;
};


struct Options_Window
{
    bool is_open;
    char edit[100][50];
    HWND list_view;
    HWND dialog;
    int count;
    int capacity;
};
