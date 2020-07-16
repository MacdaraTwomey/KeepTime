#pragma once

#include "date.h"
#include "graphics.h"
#include "helper.h"
#include "monitor_string.h"

#include <unordered_map>
#include <chrono>

static constexpr i32 MAX_KEYWORD_COUNT = 100;
static constexpr i32 MAX_KEYWORD_SIZE = 101;
static constexpr i32 MICROSECS_PER_SEC = 1000000;

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");

// u32 can overflows after 50 days when usning milliseconds, this might be ok
// as we only get this when summing multiple days, but for now KISS.
typedef s64 time_type;
typedef u32 App_Id;

// TODO: should i be using local_days instead of sys_days (does it actually matter, using sys days and the get_localtime function I did fix the "day not changing after midnight because of utc time" issue)


date::sys_days
get_localtime()
{
    time_t rawtime;
    time( &rawtime );
    
    struct tm *info;
    //int tm_mday;        /* day of the month, range 1 to 31  */
    //int tm_mon;         /* month, range 0 to 11             */
    //int tm_year;        /* The number of years since 1900   */
    info = localtime( &rawtime );
    auto now = date::sys_days{date::month{(unsigned)(info->tm_mon + 1)}/date::day{(unsigned)info->tm_mday}/date::year{info->tm_year + 1900}};
    
    //auto test_date = std::floor<date::local_days>()};
    //Assert(using local_days    = local_time<days>;
    return now;
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
	Id_LocalProgram,     // Start at 1
	Id_Website, // Start at 0x800000
};
struct Record
{
    // Id could be made 64 bit and record would be the same size
    App_Id id;
    time_type duration; // microseconds
};

// Days (and records) are:
//  - append only
//  - only last day modified
//  - saved to file
//  - any day may be checked up on
//  - contiguous set is iterated over
//  - new set of days is merged in

// Biggest difficulties may be:
// - serialising, where each days pointer will have to be relatived to its corresponding
//   block to create an overall record index (this is made easier by the fact that days are sequential)
//      - Could maybe add blocks to the start of the list, so it goes from newest to oldest (and same for file)

// NOTE: When deserialising may just want to put all records into one big block
// and then just append normal sized blocks during runtime.
// This probably means blocks must have a block_size field, because they can be variable size

static constexpr u32 BLOCK_SIZE = 1024 * sizeof(Record); // 16384
static constexpr u32 BLOCK_MAX_RECORDS = 1023;

struct Day
{
    Record *records;
    date::sys_days date;
    u32 record_count;
};
struct Block
{
    Record records[BLOCK_MAX_RECORDS];
    Block *next;
    u32 count;
    bool32 full; // needed? because we imediately start a new block when this would be set to true
    //u32 padding;
};
struct Day_List
{
    Block *blocks;
    std::vector<Day> days;
};

enum Range_Type : int
{
    Range_Type_Daily = 0, // code relies on this
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

static_assert(BLOCK_SIZE == sizeof(Block), "");

struct App_Info
{
    // TODO: Check that full paths saved to file are valid, and update if possible.
    // (full url, keyword) or
    // (path, exe name)
    
    String full_name; // this must be null terminated because passed to curl as url or OS as a path
    String short_name;
    
    // I don't really want this here
    i32 icon_index;   // -1 means not loaded
};

struct Icon_Asset
{
    // how to get back to icon_index if this is deleted
    
    // Do I even need to keep CPU side textures around after giving to GPU
    Bitmap bitmap;
    u32 texture_handle;
};


struct Database
{
    // TODO: make class that treats these three tables as one two way table
    
    // Contains local programs
    std::unordered_map<String, App_Id> local_programs;
    // Contains domain names (e.g. developer.mozilla.org)
    std::unordered_map<String, App_Id> domain_names;
    
    // Contains websites and local programs
    // We use full_name as a path to load icons from executables
    // full_name for websites isn't used
    std::unordered_map<App_Id, App_Info> app_names;
    
    Arena names_arena;
    
    App_Id next_program_id;      // starts at 0x00000000 1
    App_Id next_website_id;      // starts at 0x80000000 top bit set
    
    // Just allocate 100 days + days we already have to stop repeated allocs and potential failure
    // Also say we can have max of 1000 daily records or so, so we can just alloc a 1000 record chunk from arena per day to easily ensure it will be contiguous.
    Day_List day_list;
    
    // Also make a malloc failure routine that writes to savefile and maybe exits gracefully
    // except imgui and SDL wont use it i suppose
    
    
    // TODO: Maybe don't want this stuff in database. It is needed when other database stuff is needed (by UI) but it doesn't have the same lifetime, nor the same relation as others
    // loaded at startup
    i32 default_icon_index;
    u32 icon_count;
    Icon_Asset icons[200]; // 200 icons isn't really that much, should make like 1000
};


struct Misc_Options
{
    // solution just make them u32 for gods sake
    
    // In minute of the day 0-1439
    u32 day_start_time;  // Changing this won't trigger conversion of previously saved records
    u32 poll_start_time; // Default 0 (12:00AM) (if start == end, always poll)
    u32 poll_end_time;   // Default 0 (12:00AM)
    
    bool32 run_at_system_startup;   
    u32 poll_frequency_microseconds;   
};

struct Edit_Settings
{
    // This array can have blank strings representing empty input boxes in between valid strings
    char pending[MAX_KEYWORD_COUNT][MAX_KEYWORD_SIZE];
    Misc_Options misc_options;
    int day_start_time_item;
    int poll_start_time_item;
    int poll_end_time_item;
    bool update_settings;
};

struct Settings
{
    // Keywords act as a filter list to only allow some domain names to be entered into database and to be added as records
    
    // contains at most MAX_KEYWORD_COUNT 
    // each of length MAX_KEYWORD_SIZE
    Array<String, MAX_KEYWORD_COUNT> keywords; // null terminated strings
    Misc_Options misc_options;
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

struct
Monitor_State
{
    bool is_initialised;
    
    // Persists on disk
    Database database; // mostly
    Arena keyword_arena;
    Settings settings;
    
    // persists ui open/closes
    Date_Picker date_picker;
    
    Edit_Settings *edit_settings; // allocated when needed
    
    // microsecs
    time_type accumulated_time;
    
    // debug temporary
    time_type total_runtime;
    LARGE_INTEGER startup_time;
};
