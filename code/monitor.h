#pragma once

#include "date.h"
#include "graphics.h"
#include "helper.h"
#include "monitor_string.h"
#include "apps.h"
#include "ui.h"

#include <unordered_map>
#include <chrono>

#define PROGRAM_NAME "KeepTime"
#define VERSION_STRING "0.0.1"
#define NAME_STRING "Mac"
#define LICENSE_STRING "MIT Licence"

// TODO: make a malloc failure routine that maybe writes to savefile, and frees stuff and maybe exits gracefully
struct
Monitor_State
{
    UI_State ui;
    
    s64 accumulated_time; // microsecs
    u32 refresh_frame_time; // in milliseconds
    char savefile_path[512]; // TODO: find how big to make, MAX_PATH?
    char temp_savefile_path[512]; // TODO: find how big to make, MAX_PATH? not sure how exactly i plan to make writes safer yet
    
    // Just allocate 100 days + days we already have to stop repeated allocs and potential failure
    // Also say we can have max of 1000 daily records or so, so we can just alloc a 1000 record chunk from arena per day to easily ensure it will be contiguous.
    Day_List day_list;
    App_List apps;
    Settings settings;
    
    bool is_initialised;
};
