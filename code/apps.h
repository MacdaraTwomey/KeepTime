
#ifndef APPS_H
#define APPS_H

#include <unordered_map>
#include <vector>

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
    Misc.KeywordOptions= KeywordOptions_Custom;
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
