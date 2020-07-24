#pragma once

#include "date.h"
#include "graphics.h"
#include "helper.h"
#include "monitor_string.h"

#include <unordered_map>
#include <chrono>

// Doing non PoT texture sizes seems to have artifacts and may even cause crash for some reason
// must be 32 to match default windows icon size from LoadIcon too
// NOTE: Problem was probably a non-4 byte pitch when submitting texture to GPU (https://www.khronos.org/opengl/wiki/Common_Mistakes#Texture_upload_and_pixel_reads)
constexpr u32 ICON_SIZE = 32;

constexpr s32 MAX_KEYWORD_COUNT = 6;
//constexpr s32 MAX_KEYWORD_COUNT = 100;
constexpr s32 MAX_KEYWORD_SIZE = 101;
constexpr s32 MICROSECS_PER_SEC = 1000000;

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");

// u32 can overflows after 50 days when usning milliseconds, this might be ok
// as we only get this when summing multiple days, but for now KISS.
typedef s64 time_type;
typedef u32 App_Id;

// TODO: should i be using local_days instead of sys_days (does it actually matter, using sys days and the get_localtime function I did fix the "day not changing after midnight because of utc time" issue)


bool is_local_program(App_Id id)
{
    return !(id & (1 << 31)) && id != 0;
}

bool is_website(App_Id id)
{
    return id & (1 << 31);
}

u32
index_from_id(App_Id id)
{
    Assert(id != 0);
    
    
    u32 index = id & (0x8000 - 1);
    if (is_local_program(id)) index -= 1;
    
    Assert(index < id);
    Assert(index >= 0 && (index <= (0x8000 - 1)));
    
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
	Id_LocalProgram,     // Start at 1
	Id_Website,          // Start at 0x800000
};
struct Record
{
    // Id could be made 64 bit and record would be the same size
    App_Id id;
    time_type duration; // microseconds
};

// might want to set with attention paid to arena size, and extra size that is added etc
static constexpr u32 MAX_DAILY_RECORDS = 1000;
static constexpr u32 DEFAULT_DAILY_RECORDS_ARENA_SIZE = MAX_DAILY_RECORDS * sizeof(Record);

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

struct Local_Program_Info
{
    String short_name;
    i32 icon_index;   // -1 means not loaded
    
    String full_name; // this must be null terminated because passed to curl as url or OS as a path
};

struct Website_Info
{
    String short_name;
    i32 icon_index;   // -1 means not loaded
};

struct Icon_Asset
{
    // how to get back to icon_index if this is deleted
    
    // Do I even need to keep CPU side textures around after giving to GPU
    Bitmap bitmap;
    u32 texture_handle;
};

struct App_List
{
    // Need two sets of tables, two ids types etc because a website and app can have the same short_name and they are treated slightly differently
    
    // Contains local programs
    std::unordered_map<String, App_Id> local_program_ids;
    
    // Contains domain names (e.g. developer.mozilla.org)
    std::unordered_map<String, App_Id> website_ids;
    
    // Indexed by app id low 31-bits
    std::vector<Local_Program_Info> local_programs;
    std::vector<Website_Info> websites;
    
    App_Id next_program_id;      // starts at 0x00000001
    App_Id next_website_id;      // starts at 0x80000000 top bit set
    
    Arena names_arena;
};

struct Database
{
    App_List apps;
    
    // Just allocate 100 days + days we already have to stop repeated allocs and potential failure
    // Also say we can have max of 1000 daily records or so, so we can just alloc a 1000 record chunk from arena per day to easily ensure it will be contiguous.
    Day_List day_list;
    
    // Also make a malloc failure routine that maybe writes to savefile, and frees stuff and maybe exits gracefully
    
    u32 default_website_icon_index;
    u32 default_local_program_icon_index;
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
    
    b32 run_at_system_startup;   
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
    bool keyword_limit_error;
    bool keyword_disabled_character_error;
};

struct Settings
{
    // Keywords act as a filter list to only allow some domain names to be entered into database and to be added as records
    
    // contains at most MAX_KEYWORD_COUNT 
    // each of length MAX_KEYWORD_SIZE
    Array<String, MAX_KEYWORD_COUNT> keywords; // null terminated strings
    Misc_Options misc_options;
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

struct
Monitor_State
{
    bool is_initialised;
    
    // Persists on disk
    Database database; // mostly
    Settings settings;
    
    // persists ui open/closes
    Date_Picker date_picker;
    
    Edit_Settings *edit_settings; // allocated when needed
    
    // microsecs
    time_type accumulated_time;
    time_type microseconds_until_next_day;
    
    date::sys_days current_date;
    
    // debug temporary
    time_type total_runtime;
    LARGE_INTEGER startup_time;
};
