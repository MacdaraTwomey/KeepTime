
#ifndef APPS_H
#define APPS_H

#include "base.h"
#include "monitor_string.h"

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
    date::sys_days Date; // Could use days since 2000 or something...
    u64 DurationMicroseconds; 
};

static_assert(sizeof(record) == 16, "");
static_assert(sizeof(date::sys_days) == 4, ""); // check size

struct record_slice
{
    record *Records;
    u64 Count;
};


constexpr s32 MAX_KEYWORD_COUNT = 100;
constexpr s32 MAX_KEYWORD_LENGTH = 100; // Not including null byte

enum keyword_options : s32
{
    KeywordOptions_All,
    KeywordOptions_Custom,
    KeywordOptions_None,
};

struct misc_options
{
    keyword_options KeywordOptions;
    
    // In minute of the day 0-1439
    //u32 day_start_time;  // Changing this won't trigger conversion of previously saved records
    //u32 poll_start_time; // Default 0 (12:00AM) (if start == end, always poll)
    //u32 poll_end_time;   // Default 0 (12:00AM)
    //b32 run_at_system_startup;   
};

misc_options DefaultMiscOptions()
{
    misc_options Misc;
    Misc.KeywordOptions = KeywordOptions_Custom;
    return Misc;
}
// Free list allocator just for strings because these are the most transient
struct settings
{
    arena *Arena;
    misc_options MiscOptions;
    string *Keywords; // Null terminated, NULL byte not counted in Length (because GUI wants them that way)
    u32 KeywordCount;
};

#endif //APPS_H
