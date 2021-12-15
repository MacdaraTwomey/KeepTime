#pragma once

#include "base.h"
#include "monitor_string.h"
#include "date.h"
#include "helper.h"
#include "gui.h"

#define PROGRAM_NAME "KeepTime"
#define VERSION_STRING "0.0.1"
#define NAME_STRING "Mac"
#define LICENSE_STRING "MIT Licence"

typedef u32 app_id;

enum app_type
{
    NONE = 0,
    PROGRAM = 1, // Program IDs do not have the most significant bit set
    WEBSITE = 2, // Website IDs have the most significant bit set
};

constexpr u32 APP_TYPE_BIT_INDEX = 31;
constexpr u32 APP_TYPE_BIT_MASK = 1 << 31;

struct app_info
{
    app_type Type;
    union
    {
        string ProgramPath; 
        string WebsiteHost;
        string Name; // For referencing app's path or host in a non-specific way
    };
};

struct record
{
    app_id ID;
    date::sys_days Date; // days since 2000 or something...
    u64 DurationMicroseconds; 
};

static_assert(sizeof(record) == 16, "");

static_assert(sizeof(date::sys_days) == 4, ""); // check size

struct monitor_state
{
    bool IsInitialised;
    
    arena PermanentArena;
    arena TemporaryArena;
    
    string_map AppNameMap;
    u32 CurrentID;
    
    record *Records;
    u64 RecordCount;
    
    settings Settings;
    
    gui GUI;
};
