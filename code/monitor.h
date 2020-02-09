#pragma once

// unpacked
// 16 bytes unpacked
// avg 10 progams a day
// 5 yeafs of days
// 16 * 10 * 365 = 58400 (58kb) per year

// packed
// 10 * 10 * 365 = 35600 (36kb) per year


// static constexpr u32 DefaultVersion = 0;
// static constexpr u32 MaxSupportedVersion = 0;
// static constexpr u16 DefaultDayStart = 0;
// static constexpr u16 DefaultPollStart = 0;
// static constexpr u16 DefaultPollEnd = 0;
// static constexpr u16 DefaultRunAtSystemStartup = false;
// static constexpr u32 DefaultPollFrequencyMilliseconds = 1000;



typedef u16 bool16;

struct Hash_Node
{
    char *key;
    u32 value;
};

struct Hash_Table
{
    static constexpr u64 DEFAULT_TABLE_SIZE = 128;
    static constexpr u8 EMPTY = 0;
    static constexpr u8 DELETED = 1;
    static constexpr u8 OCCUPIED = 2;
    
    s64 count;
    s64 size;
    Hash_Node *buckets;
    u8 *occupancy;
    
    s64 add_item(char *key, u32 value);
    bool search(char *key, u32 *found_value);
    void remove(char *key);
    char *search_by_value(u32 value);
    private:
    void grow_table();
};

struct String_Builder
{
    char *str;
    size_t capacity;
    size_t len; // Length not including null terminator
    
    void grow(size_t min_amount);
    void append(char *new_str);
    void appendf(char *new_str, ...);
    void add_bytes(char *new_str, size_t len); // Will add all bytes of string of size len (including extra null terminators you insert)
    void clear();
};

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
    
    u32 size_program_names_block;
    u32 num_total_programs;
    u32 num_days;
    u32 num_program_records;    // Should be fine as 32-bit as 4 billion * X seconds each is > 30 years
    
    // Header
    // null terminated names, in a block    # programs (null terminated strings)
    // corresponding ids                    # programs (u32)
    // Dates of each day                    # days     (u16)
    // Index of end of each day             # days     (u32)
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

struct Monitor_State
{
    // All below becomes old data once loop starts
    // Header and hash table are continualy updated though
    
    
    Header saved_header;
    //u32 *saved_day_offsets;           // will contain index of start of each day - e.g. [0, 5, 7]
    //u16 *saved_dates;                 // Contains dates for each day
    //Program_Record *saved_records;       // Contains array of all days records
};


struct Day
{
    Program_Record *records;
    u32 num_records;
    u16 date;
};
