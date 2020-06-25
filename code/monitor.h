#pragma once

#include "date.h"
#include "graphics.h"
#include "helper.h"
#include "monitor_string.h"

#include <unordered_map>

// TODO: Think of better way than just having error prone max amounts.
static constexpr u32 MaxDailyRecords = 1000;
static constexpr u32 MaxDays = 1000; // temporary
static constexpr u32 DefaultDayAllocationCount = 30;
static constexpr i32 MaxWebsiteCount = 50; // this used?
static constexpr i32 MAX_KEYWORD_COUNT = 100;
static constexpr i32 MAX_KEYWORD_SIZE = 101;

// u32 can overflows after 50 days when usning milliseconds, this might be ok
// as we only get this when summing multiple days, but for now KISS.
typedef u64 time_type;

// Steady clock typically uses system startup time as epoch, and system clock uses systems epoch like 1970-1-1 00:00
// Clocks have a starting point (epoch) and tick rate (e.g. 1 tick per second)
// A Time Point is a duration of time that has passed since a clocks epoch
// A Duration consists of a span of time, defined as a number of ticks of some time unit (e.g. 12 ticks in millisecond unit)
// On windows Steady clock is based on QueryPerformanceCounter
using Steady_Clock = std::chrono::steady_clock;
using System_Clock = std::chrono::system_clock;

struct Header
{
    // u32 version; // version 0
    // minute 0 to minute 1439 of day (60*24)
    // u16 day_start_time;  // Default 0        // Changing this won't affect previously saved records
    // u16 poll_start_time; // Default 0 (if start == end, always poll)
    // u16 poll_end_time;   // Default 0
    // bool16 run_at_system_startup;   // Default 0
    // u32 poll_frequency_milliseconds;    // Default 1000
    u32 program_names_block_size;
    u32 total_program_count;
    u32 day_count;
    u32 total_record_count;    // Should be fine as 32-bit as 4 billion * X seconds each is > 30 years
    
    // Header
    // null terminated names, in a block    # programs (null terminated strings)
    // corresponding ids                    # programs (u32)
    // Dates of each day                    # days     (u32)
    // Counts       of each day             # days     (u32)
    // Array of all prorgam records clumped by day      # total records (Program_Records)
    // Indexes work like this
    // |0        |5  |7     <- indexes (3 indexes for 10 total records)
    // [][][][][][][][][][] <- records
};

typedef u32 Assigned_Id;

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
    
    template<> struct std::hash<Assigned_Id>
    {
        std::size_t operator()(Assigned_Id const& id) const noexcept
        {
            // Probably fine for program ids (start at 0)
            return (size_t)id;
        }
    };
    
}


struct Record
{
    Assigned_Id id;
    time_type duration;
};

// Days (and records) are:
//  - append only
//  - only last day modified
//  - saved to file
//  - any day may be checked up on
//  - contiguous set is iterated over
//  - new set of days is merged in

// could use linked list of large blocks to add records to and dyn array of day to point at records
// each block can have a header with info about its records (if needed)

// to make a day view just copy days array (that can be used read only)
// and make last day of original days dyn array point to a new block, which can later be merged

// Biggest difficulties may be:
// - serialising, where each days pointer will have to be relatived to its corresponding
//   block to create an overall record index (this is made easier by the fact that days are sequential)
// - Merging and day view creation

struct Day
{
    Record *records;
    u32 record_count;
    date::sys_days date;
};

// TODO: Make day view look at original array(s) of days (and records)
// and just copy current day to new array and append new data to that.
// Currently we do the opposite, by copying last day and appending new data to original.
// So when UI opened be just instantly copy current day and while UI is open append to it,
// allowing as many read only views as needed to be made.
struct Day_View
{
    // Must pass by reference because of pointer to last day
    Day *days[MaxDays];
    Day last_day_;
    i32 day_count;
    
    i32 start_range;
    i32 range_count;
    bool accumulate;
};
// Might want a linked list of fixed size blocks for the records, and a dynamic array for days


enum Record_Type
{
    Record_Invalid,
    Record_Exe,     // Start at 0
    Record_Firefox, // Start at 0x800000
};

struct Keyword
{
    // Null terminated
    String str;
    Assigned_Id id;
};

struct Program_Info
{
    // TODO: Check that full paths saved to file are valid, and update if possible.
    // (full url, keyword) or
    // (path, exe name)
    
    String long_name; // this must be null terminated because passed to curl as url or OS as a path
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
    // This is used to quickly Assigned_Idable paths -> ID
    // Don't need a corresponding one for websites as we have to test agains all keywords anyway
    std::unordered_map<String, Assigned_Id> program_id_table;
    
    // Contains websites and programs
    // We use long_name as a path to load icons from executables
    // We use long_nameAssigned_Idto download favicon from website
    // We use shortname when we iterate records and want to display names
    std::unordered_map<Assigned_Id, Program_Info> names;
    
    // dont like passing database to give this to the settings code
    // 0 is illegal
    Assigned_Id next_program_id;      // starts at 0x00000000 1
    Assigned_Id next_website_id;      // starts at 0x80000000 top bit set
    
    // Temporary
    Assigned_Id firefox_id;
    bool added_firefox;
    
    // Can have:
    // - a path (updated or not) with no corresponding bitmap (either not loaded or unable to be loaded)
    // - a path (updated or not) with a bitmap
    //Bitmap icons[200]; // Loaded on demand
    //Bitmap website_icons[200]; // Loaded on demand
    
    // loaded at startup
    i32 default_icon_index;
    
    u32 icon_count;
    Icon_Asset icons[200];
    
    Day days[MaxDays];
    i32 day_count;
};


struct Misc_Options
{
    // These may want to be different datatypes (tradeoff file IO ease vs imgui datatypes conversion)
    
    // u16 day_start_time;  // Default 0        // Changing this won't affect previously saved records
    u16 poll_start_time; // Default 0 (if start == end, always poll)
    u16 poll_end_time;   // Default 0
    bool16 run_at_system_startup;   // Default 0
    u32 poll_frequency_milliseconds;    // Default 1000
};

struct Edit_Settings
{
    // This array can have blank strings representing empty input boxes in between valid strings
    char pending[MAX_KEYWORD_COUNT][MAX_KEYWORD_SIZE];
    s32 input_box_count;
    Misc_Options misc_options;
    int start_time_item;
    int end_time_item;
    bool update_settings;
};

struct Settings
{
    std::vector<Keyword> keywords;
    Misc_Options misc_options;
};

struct
Monitor_State
{
    bool is_initialised;
    Header header;
    Database database;
    
    Settings settings;
    Edit_Settings *edit_settings; // allocated when needed
    
    Day_View day_view;
    time_type accumulated_time;
};
