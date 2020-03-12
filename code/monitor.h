#pragma once

#include "date.h"
#include "helper.h"

#include "stb_truetype.h"

// TODO: Think of better way than just having error prone max amounts.
static constexpr u32 MaxDailyRecords = 1000;
static constexpr u32 MaxDays = 1000;
static constexpr u32 DefaultDayAllocationCount = 30;

static constexpr i32 MaxKeywordCount = 50;
static constexpr i32 MaxWebsiteCount = 50;

typedef double time_type;
// using namespace date;

// static constexpr u32 DefaultVersion = 0;
// static constexpr u32 MaxSupportedVersion = 0;
// static constexpr u16 DefaultDayStart = 0;
// static constexpr u16 DefaultPollStart = 0;
// static constexpr u16 DefaultPollEnd = 0;
// static constexpr u16 DefaultRunAtSystemStartup = false;
// static constexpr u32 DefaultPollFrequencyMilliseconds = 1000;


// Steady clock typically uses system startup time as epoch, and system clock uses systems epoch like 1970-1-1 00:00 
// Clocks have a starting point (epoch) and tick rate (e.g. 1 tick per second)
// A Time Point is a duration of time that has passed since a clocks epoch
// A Duration consists of a span of time, defined as a number of ticks of some time unit (e.g. 12 ticks in millisecond unit)
// On windows Steady clock is based on QueryPerformanceCounter
using Steady_Clock = std::chrono::steady_clock;
using System_Clock = std::chrono::system_clock;

// Holds settings and info about file contents
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


struct Program_Record
{
    u32 ID;
    double duration;
};

struct Memory_Block
{
    u8 *data;
    u32 size;
};

struct Day
{
    Program_Record *records;
    u32 record_count;
    date::sys_days date;
};

struct NOT_SURE
{
    Day *days;
    Program_Record *records; // all records, each days records points in here
    u32 day_count;
};

enum Button : u8 
{
    Button_Invalid,
    Button_Day,
    Button_Week,
    Button_Month,
};

enum Event_Type 
{
    Event_Invalid,
    Event_Button_Click,
    Event_GUI_Open,
    Event_GUI_Close,
    Event_Poll_Programs,
};

struct Event
{
    union {
        Button button;
    };
    Event_Type type;
};

struct Simple_Bitmap
{
    // Top Down bitmap with a positive height
    // Don't use outside of this program. Usually top down bitmaps have a negative height I think.
    static constexpr int BYTES_PER_PIXEL = 4;
    i32 width;
    i32 height;
    u32 *pixels;
    // No pitch for now, pitch == width
};

struct Font
{
    i32 atlas_width;
    i32 atlas_height;
    u8 *atlas;  // Not sure if should be 32-bit
    stbtt_bakedchar *glyphs;
    i32 glyphs_count;
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

struct website
{
    char *url;
    char *name;
};

struct Exe_Path
{
    char *path;
    bool updated_recently;
};

struct Keyword
{
    char *str;
    u32 id;
};

struct Website
{
    u32 id;
    char *website_name;
};

struct Database
{
    Hash_Table all_programs; // name -> ID
    Website websites[50];    // ID -> name TODO: Replace with a hash table?
    i32 website_count;
    
    Keyword keywords[50];
    i32 keyword_count;
    
    u32 next_exe_id;      // starts at 0x00000000 zero
    u32 next_website_id;  // starts at 0x80000000 top bit set
    
    // Set these when possible
    Exe_Path firefox_path;
    Simple_Bitmap firefox_icon;
    u32 firefox_id;
    
    // Can have:
    // - a path (updated or not) with no corresponding bitmap (either not loaded or unable to be loaded)
    // - a path (updated or not) with a bitmap
    Simple_Bitmap icons[200]; // Loaded on demand
    Exe_Path paths[200];      // Updated when possible at most once a session.
    
    Day days[MaxDays];
    i32 day_count;
};

enum Record_Type
{
    Record_Invalid,
    Record_Exe,
    Record_Firefox,
};
