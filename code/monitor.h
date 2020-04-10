#pragma once

#include "date.h"
#include "graphics.h"
#include "helper.h"
#include "monitor_string.h"
#include "stb_truetype.h"

#include <unordered_map>

// TODO: Think of better way than just having error prone max amounts.
static constexpr u32 MaxDailyRecords = 1000;
static constexpr u32 MaxDays = 1000;
static constexpr u32 DefaultDayAllocationCount = 30;
static constexpr i32 MaxKeywordCount = 50;
static constexpr i32 MaxWebsiteCount = 50;

typedef double time_type;

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

typedef u32 Program_Id;

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
    
    template<> struct std::hash<Program_Id>
    {
        std::size_t operator()(Program_Id const& id) const noexcept
        {
            // Probably fine for program ids (start at 0)
            return (size_t)id;
        }
    };
    
}


struct Program_Record
{
    Program_Id id;
    time_type duration;
};

struct Day
{
    Program_Record *records;
    u32 record_count;
    date::sys_days date;
};

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

struct Program_Paths
{
    // TODO: Check that full paths saved to file are valid, and update if possible.
    String full_path;
    String name;
};

struct Keyword
{
    String str;
    Program_Id id;
};

struct Record_Name
{
    // (full url, keyword) or
    // (path, exe name)
    String long_name;
    String short_name;
};

struct Database
{
    std::unordered_map<String, Program_Id> programs;
    
    // Contains websites matching a keyword and programs
    std::unordered_map<Program_Id, Record_Name> record_names;
    
    Keyword keywords[50];
    i32 keyword_count;
    
    u32 next_program_id;      // starts at 0x00000000 zero
    u32 next_website_id;      // starts at 0x80000000 top bit set
    
    // Temporary
    Program_Id firefox_id;
    bool added_firefox;
    
    // Can have:
    // - a path (updated or not) with no corresponding bitmap (either not loaded or unable to be loaded)
    // - a path (updated or not) with a bitmap
    Bitmap icons[200]; // Loaded on demand
    Bitmap website_icons[200]; // Loaded on demand
    
    Bitmap ms_icons[5];
    
    Day days[MaxDays];
    i32 day_count;
};
