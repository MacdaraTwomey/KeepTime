#pragma once

// TODO: Maybe could use alignas(8) but I think this makes reading difficult as
// size of all structs != size of file. Where the part of file that would have been padding
// in struct becomes dead space in file and not readable with a easy struct slurp.

// NOTE: As long as start of memory you read file into is a multiple of 8, all fields in structs
// should fall on multiple of 8 as well

// unpacked
// 16 bytes unpacked
// avg 10 progams a day
// 5 yeafs of days
// 16 * 10 * 365 = 58400 (58kb) per year

// packed
// 10 * 10 * 365 = 35600 (36kb) per year

static constexpr char BackgroundWindowClassName[] = "BGMonitorWinClass";

typedef u16 bool16;

struct Header
{
    // TODO: String block offset?

    u32 version; // version 0

    // minute 0 to minute 1439 of day (60*24)
    u16 day_start_time;  // Default 0
    u16 poll_start_time; // Default 0 (if start == end, always poll)
    u16 poll_end_time;   // Default 0

    bool16 run_at_system_startup;   // Default 0
    u32 poll_frequency_milliseconds;    // Default 1000

    // TODO: Could hash name of program into ID and use open addressing to resolve collisions
    // Or have sorted list of most used programs 
    u32 num_programs_in_list;
    u32 max_programs;

    // num strings?
    u32 string_block_used;
    u32 string_block_capacity;  // bytes

    // Same as sizeof(PMD_Header) + sizeof(PMD_Program_Record)*max_capacity + string_block_capacity
    u32 num_days;
    u32 days_offset;     // TODO: Does anyone use this
    u32 last_day_offset; // In new files is set to file_size

    u32 _alignmentpaddingbits;
};

// TODO: * Maybe this indirection is bad, could just have each program in a
//         day records point to strings directly
//       * However this would stop a easily accessibly list of all unique programs
struct Program
{
    u32 ID; // Doesn't really need this because its position is it's ID (unless we hash)
    u32 name_offset;    // from string block
};

struct Day_Info
{
    // Day (1-31) Month (0-11) Years after 2000 (0-127)
    // 00000      0000         0000000
    // High                    Low
    u16 date;  
    u16 num_programs;
    u32 _alignmentpaddingbits;
};

struct Program_Time
{
    u32 ID;
    double elapsed_time;
};

struct Memory_Block
{
    u8 *data;
    u32 size;
};

struct Day
{
    // Not in file just used to keep together
    Day_Info info; 
    Program_Time *programs;
};

struct Database
{
    // Maybe add global filepaths as [static] variable
    Memory_Block state;
    u32 file_size;
    u32 file_append_offset;
    bool day_partially_in_file;
};

static_assert(sizeof(Header) % 8 == 0, "Header size must be multiple of 8");
static_assert(sizeof(Program_Time) % 8  == 0, "Record size must be multiple of 8");
static_assert(sizeof(Day_Info) % 8     == 0, "Day size must be multiple of 8");
static_assert(sizeof(Program) % 8 == 0, "Program size must be multiple of 8");

static constexpr u16 DayMask   = 0b1111100000000000;
static constexpr u16 MonthMask = 0b0000011110000000;
static constexpr u16 YearMask  = 0b0000000001111111;

static constexpr char DBFileName[] = "monitor_db.pmd";
static constexpr char DebugDBFileName[] = "debug_monitor_db.txt";

static constexpr u32 DefaultVersion = 0;
static constexpr u32 MaxSupportedVersion = 0;
static constexpr u16 DefaultDayStart = 0;
static constexpr u16 DefaultPollStart = 0;
static constexpr u16 DefaultPollEnd = 0;
static constexpr u16 DefaultRunAtSystemStartup = false;
static constexpr u32 DefaultPollFrequencyMilliseconds = 1000;
static constexpr u32 InitialMaxProgramsInList = 1000;
// avg 15 chars .exe
static constexpr u32 InitialStringBlockCapacity = 15 * InitialMaxProgramsInList;
static constexpr u32 InitialDaysOffset = (sizeof(Header) +
                                          (sizeof(Program) * InitialMaxProgramsInList) +
                                          InitialStringBlockCapacity);
static constexpr u32 MaxProgramsPerDay = 1000;

static constexpr u32 OffsetToProgramList = sizeof(Header);

static_assert(InitialStringBlockCapacity % 8 == 0, "String block size must be multiple of 8");
