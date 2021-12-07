#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdio.h>

#include "base.h"

static constexpr u32 PLATFORM_MAX_PATH_LEN = 2000; // This must not too big, too small for all platforms
static constexpr u32 PLATFORM_MAX_URL_LEN  = 2000;

typedef void * platform_window_handle;


struct platform_file_contents
{
    u8 *Data;
    size_t Size;
};

// Seems that all sizes work except 16 and except larger sizes (64 works though)
constexpr u32 ICON_SIZE = 32;
struct bitmap
{
    // Top Down bitmap with a positive height
    // Don't use outside of this program. Usually top down bitmaps have a negative height I think.
    static constexpr int BYTES_PER_PIXEL = 4;
    i32 Width;
    i32 Height;
    u32 *Pixels;
    i32 Pitch;
};

platform_window_handle PlatformGetActiveWindow();
char *PlatformGetProgramFromWindow(arena *Arena, platform_window_handle WindowHandle);
char *PlatformFirefoxGetUrl(arena *Arena, platform_window_handle WindowHandle);
bool PlatformIsWindowHidden();
bool PlatformGetDefaultIcon(u32 DesiredSize, bitmap * Bitmap);
bool PlatformGetIconFromExecutable(char * Path, u32 DesiredSize, bitmap * Bitmap, u32 * AllocatedBitmapMemory);
char *PlatformGetExecutablePath(arena *Arena);
bool PlatformFileExists(arena *Arena, char * Filename);
s64 PlatformGetFileSize(FILE * file);
platform_file_contents PlatformReadEntireFile(char * FileName);
void *PlatformMemoryReserve(u64 Size);
void *PlatformMemoryCommit(void *Address, u64 size);
void PlatformMemoryDecommit(void *Address, u64 Size);
void PlatformMemoryFree(void *Address);

#endif //PLATFORM_H
