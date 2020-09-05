
#ifndef APPS_H
#define APPS_H

#include <unordered_map>
#include <vector>

constexpr s32 MICROSECS_PER_MILLISEC = 1000;
constexpr s32 MILLISECS_PER_SEC = 1000;
constexpr s32 MICROSECS_PER_SEC = 1000000;

constexpr u32 WEBSITE_ID_START = 0x80000000;
constexpr u32 LOCAL_PROGRAM_ID_START = 1;
constexpr u32 APP_TYPE_BIT = WEBSITE_ID_START;

constexpr u32 DEFAULT_POLL_FREQUENCY_MILLISECONDS = 100000; // debug (this is fast)
constexpr float POLL_FREQUENCY_MIN_SECONDS = 0.001f;  // debug
//constexpr float POLL_FREQUENCY_MIN_SECONDS = 0.1f; 
constexpr float POLL_FREQUENCY_MAX_SECONDS = 60.0f; 

// TODO: Make sure that having null terminators in allocation doesn't make these sizes mess up, ui input of keyword must limit length to MAX_KEYWORD_SIZE - 1
constexpr s32 MAX_KEYWORD_COUNT = 100;
constexpr s32 MAX_KEYWORD_SIZE = 101;
constexpr u32 KEYWORD_MEMORY_SIZE = MAX_KEYWORD_SIZE * MAX_KEYWORD_COUNT;

typedef u32 App_Id;

bool is_local_program(App_Id id)
{
    return ((id & APP_TYPE_BIT) == 0) && id != 0;
}

bool is_website(App_Id id)
{
    return id & APP_TYPE_BIT;
}

u32
index_from_id(App_Id id)
{
    Assert(id != 0);
    
    // Get bottom 31 bits of id
    u32 index = id & (WEBSITE_ID_START - 1);
    
    // Local progams start at 1, and we would rather id = 1 to index to 0
    if (is_local_program(id)) index -= 1;
    
    Assert(index < id);
    Assert(index >= 0 && (index <= (WEBSITE_ID_START - 1)));
    
    return index;
}

// custom specialization of std::hash can be injected in namespace std
namespace std {
    template<> struct std::hash<String> {
        std::size_t operator()(String const& s) const noexcept {
            size_t hash = djb2((unsigned char *)s.str, s.length);
            return hash;
        }
    };
    
    template <> struct std::equal_to<String> {
        bool operator()(String const& a, String const& b) const noexcept {
            return string_equals(a, b);
        }
    };
}

struct Record
{
    // Could add a 32-bit date in here without changing size in memory
    App_Id id;
    s64 duration; // microseconds
};
// might want to set with attention paid to arena size, and extra size that is added etc
constexpr u32 MAX_DAILY_RECORDS = 1000;
constexpr u32 MAX_DAILY_RECORDS_MEMORY_SIZE = MAX_DAILY_RECORDS * sizeof(Record); 

struct Day
{
    Record *records;
    date::sys_days date;
    u32 record_count;
};

struct Day_List
{
    Arena record_arena;
    std::vector<Day> days;
};

struct Local_Program_Info
{
    String short_name;
    
    // Only need this when UI is open so could be separate array and only read from disk when ui is open
    String full_name; // this must be null terminated because passed to curl as url or OS as a path
};

struct Website_Info
{
    String short_name;
};

struct App_List
{
    // Need two sets of tables, two ids types etc because a website and app can have the same short_name and they are treated slightly differently
    
    // Contains local programs
    std::unordered_map<String, App_Id> local_program_ids;
    
    // Contains domain names (e.g. developer.mozilla.org)
    std::unordered_map<String, App_Id> website_ids;
    
    // These are just used by UI to get names, and icons
    // Indexed by app id low 31-bits
    std::vector<Local_Program_Info> local_programs;
    std::vector<Website_Info> websites;
    
    App_Id next_program_id;  
    App_Id next_website_id;  
    
    Arena name_arena;
};

enum Settings_Misc_Keyword_Status : s32
{
    Settings_Misc_Keyword_All,
    Settings_Misc_Keyword_Custom,
    Settings_Misc_Keyword_None,
};

struct Misc_Options
{
    u32 poll_frequency_milliseconds;   
    Settings_Misc_Keyword_Status keyword_status;
    
    // In minute of the day 0-1439
    //u32 day_start_time;  // Changing this won't trigger conversion of previously saved records
    //u32 poll_start_time; // Default 0 (12:00AM) (if start == end, always poll)
    //u32 poll_end_time;   // Default 0 (12:00AM)
    //b32 run_at_system_startup;   
    
    static Misc_Options default_misc_options()
    {
        Misc_Options misc;
        misc.poll_frequency_milliseconds = DEFAULT_POLL_FREQUENCY_MILLISECONDS;
        misc.keyword_status = Settings_Misc_Keyword_Custom;
        return misc;
    }
};

struct Settings
{
    Array<String, MAX_KEYWORD_COUNT> keywords; // null terminated strings
    Arena keyword_arena;
    Misc_Options misc;
};


#endif //APPS_H
