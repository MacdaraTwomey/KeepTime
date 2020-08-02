#pragma once

#include "date.h"
#include "graphics.h"
#include "helper.h"
#include "monitor_string.h"

#include <unordered_map>
#include <chrono>

// Seems that all sizes work except 16 and except larger sizes (64 works though)
constexpr u32 ICON_SIZE = 32;

//constexpr s32 MAX_KEYWORD_COUNT = 6;
constexpr s32 MAX_KEYWORD_COUNT = 100;
constexpr s32 MAX_KEYWORD_SIZE = 101;

constexpr s32 MICROSECS_PER_MILLISEC = 1000;
constexpr s32 MILLISECS_PER_SEC = 1000;
constexpr s32 MICROSECS_PER_SEC = 1000000;

constexpr float POLL_FREQUENCY_MIN_SECONDS = 0.001f;  // debug
//constexpr float POLL_FREQUENCY_MIN_SECONDS = 0.1f; 
constexpr float POLL_FREQUENCY_MAX_SECONDS = 60.0f; 

constexpr u32 DEFAULT_LOCAL_PROGRAM_ICON_INDEX = 0;
constexpr u32 DEFAULT_WEBSITE_ICON_INDEX = 1;

#define PROGRAM_NAME "MonitorXXXX"
#define VERSION_STRING "0.0.0"
#define NAME_STRING "Mac"
#define LICENSE_STRING "TODO: Licence"

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");

// u32 can overflows after 50 days when usning milliseconds, this might be ok
// as we only get this when summing multiple days, but for now KISS.
typedef s64 time_type;
typedef u32 App_Id;

// TODO: should i be using local_days instead of sys_days (does it actually matter, using sys days and the get_localtime function I did fix the "day not changing after midnight because of utc time" issue)

constexpr u32 WEBSITE_ID_START = 0x80000000;
constexpr u32 LOCAL_PROGRAM_ID_START = 1;
constexpr u32 APP_TYPE_BIT = WEBSITE_ID_START;

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
namespace std
{
    template<> struct std::hash<String>
    {
        std::size_t operator()(String const& s) const noexcept
        {
            size_t hash = djb2((unsigned char *)s.str, s.length);
            return hash;
        }
    };
    
    template <> struct std::equal_to<String>
    {
        bool operator()(String const& a, String const& b) const noexcept
        {
            return string_equals(a, b);
        }
    };
    
    template<> struct std::hash<App_Id>
    {
        std::size_t operator()(App_Id const& id) const noexcept
        {
            // Probably fine for program ids (start at 1)
            Assert(id != 0);
            return (size_t)id;
        }
    };
    
}

enum Id_Type
{
	Id_Invalid, 
	Id_LocalProgram,    
	Id_Website,         
};
struct Record
{
    // Could add a 32-bit date in here without changing size in memory
    App_Id id;
    time_type duration; // microseconds
};
// might want to set with attention paid to arena size, and extra size that is added etc
constexpr u32 MAX_DAILY_RECORDS = 1000;
constexpr u32 DEFAULT_DAILY_RECORDS_ARENA_SIZE = MAX_DAILY_RECORDS * sizeof(Record); 

struct Day
{
    Record *records;
    date::sys_days date;
    u32 record_count;
};

struct Day_List
{
    Arena arena;
    std::vector<Day> days;
};

enum Range_Type : int
{
    Range_Type_Daily = 0, // code relies on these number values/order
    Range_Type_Weekly,
    Range_Type_Monthly,
    Range_Type_Custom,
};

struct Day_View
{
    // feels somewhat unneeded, but the concept is good
    std::vector<Day> days;
    Record *copy_of_current_days_records;
};

struct Local_Program_Info
{
    String short_name;
    s32 icon_index;   // -1 means not loaded, (TODO: Can this be somehow stored in record array we create during ui barplot display)
    //bool icon_retreival_failure;
    String full_name; // this must be null terminated because passed to curl as url or OS as a path
};

struct Website_Info
{
    String short_name;
    //s32 icon_index;   
};

struct Icon_Asset
{
    u32 texture_handle;
    int width;
    int height;
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
    
    Arena names_arena;
    
    // TODO: Allocated bitmap for loading icons
};

enum Settings_Misc_Keyword_Status : s32
{
    Settings_Misc_Keyword_All,
    Settings_Misc_Keyword_Custom,
    Settings_Misc_Keyword_None,
};

struct Misc_Options
{
    // solution just make them u32 for gods sake
    
    // In minute of the day 0-1439
    //u32 day_start_time;  // Changing this won't trigger conversion of previously saved records
    //u32 poll_start_time; // Default 0 (12:00AM) (if start == end, always poll)
    //u32 poll_end_time;   // Default 0 (12:00AM)
    
    //b32 run_at_system_startup;   
    u32 poll_frequency_milliseconds;   
    Settings_Misc_Keyword_Status keyword_status;
};

struct Edit_Settings
{
    // This array can have blank strings representing empty input boxes in between valid strings
    char pending[MAX_KEYWORD_COUNT][MAX_KEYWORD_SIZE];
    Misc_Options misc;
    int day_start_time_item;
    int poll_start_time_item;
    int poll_end_time_item;
    bool keyword_limit_error;
    bool keyword_disabled_character_error;
};

struct Settings
{
    // Keywords act as a filter list to only allow some domain names to be entered into database and to be added as records
    
    // contains at most MAX_KEYWORD_COUNT 
    // each of length MAX_KEYWORD_SIZE
    Array<String, MAX_KEYWORD_COUNT> keywords; // null terminated strings
    Misc_Options misc;
    Arena keyword_arena;
};

struct Calendar
{
    date::year_month_weekday first_day_of_month;
    date::sys_days selected_date;
    
    char button_label[64]; // set on ui initialisation
    bool is_backwards_disabled; // set when popup opened
    bool is_forwards_disabled; // set when popup opened
};

struct Date_Picker
{
    // TODO: When new days are added do we update end_date
    // TODO: Get rid of day suffix probably...
    Range_Type range_type;
    
    date::sys_days start;
    date::sys_days end;
    
    // Longest string "30st September 2020 - 31st September 2020"
    // 41 chars
    char date_label[64];
    bool is_backwards_disabled;
    bool is_forwards_disabled;
    Calendar first_calendar;
    Calendar second_calendar;
};

struct UI_State
{
    std::vector<Icon_Asset> icons;
    Date_Picker date_picker;
    Day_View day_view;
    Edit_Settings *edit_settings; // allocated when needed
    bool open;
};

// TODO: make a malloc failure routine that maybe writes to savefile, and frees stuff and maybe exits gracefully
struct
Monitor_State
{
    UI_State ui;
    
    time_type accumulated_time; // microsecs
    u32 refresh_frame_time; // in milliseconds
    
    
    // NOTE: Should this be here?
    // Just allocate 100 days + days we already have to stop repeated allocs and potential failure
    // Also say we can have max of 1000 daily records or so, so we can just alloc a 1000 record chunk from arena per day to easily ensure it will be contiguous.
    Day_List day_list;
    App_List apps;
    Settings settings;
    
    bool is_initialised;
};
