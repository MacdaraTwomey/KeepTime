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

typedef u16 bool16;

struct PMD_Header
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
    u32 records_offset; 
    u32 offset_to_last_day;

    u32 _alignmentpaddingbits;
};

// TODO: Maybe this indirection is bad, could just have each program in a
// day records point to strings directly
struct PMD_Program
{
    u32 ID;
    u32 name_offset;    // from string block
};

struct PMD_Day
{
    // Use 16 bit date to allow possible future rework of file format
    // Day (1-31) Month (0-11) Years after 2000 (0-127)
    // 00000      0000         0000000
    // High bits               Low bits
    u16 date;  
    u16 num_records;
    u32 _alignmentpaddingbits;
};

struct PMD_Record
{
    u32 ID;
    double elapsed_time;
};

struct File_Block
{
    u8 *data;
    u32 size;
};

static_assert(sizeof(PMD_Header) % 8  == 0, "PMD_Header size must be multiple of 8");
static_assert(sizeof(PMD_Record) % 8  == 0, "PMD_Record size must be multiple of 8");
static_assert(sizeof(PMD_Day) % 8     == 0, "PMD_Day size must be multiple of 8");
static_assert(sizeof(PMD_Program) % 8 == 0, "PMD_Program size must be multiple of 8");

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
static constexpr u32 InitialProgramListMax = 1000;
static constexpr u32 InitialStringBlockCapacity = 15 * InitialProgramListMax;  // avg 15 chars .exe
static constexpr u32 InitialRecordOffset = (sizeof(PMD_Header) +
                                             (sizeof(PMD_Program) * InitialProgramListMax) +
                                             InitialStringBlockCapacity);

static constexpr u32 OffsetToProgramList = sizeof(PMD_Header);

static_assert(InitialStringBlockCapacity % 8 == 0, "String block size must be multiple of 8");
