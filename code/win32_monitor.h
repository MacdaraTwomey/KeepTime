#pragma once


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

// TODO: Do we want to make this the same as a Bitmap to simplify, but how do we handle 
// need for BITMAPINFO?
struct Screen_Buffer
{
    static constexpr int BYTES_PER_PIXEL = 4;
    void *data;
    BITMAPINFO bitmap_info;
    int width;
    int height;
    int pitch;
};
