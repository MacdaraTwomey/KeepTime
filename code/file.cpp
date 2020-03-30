
#include "monitor.h"
#include <stdio.h>

static constexpr i32 MaxPathLen = 2048;
static constexpr char SaveFileName[] = "monitor_save.pmd";
static constexpr char DebugSaveFileName[] = "debug_monitor_save.txt";

u64 file_program_names_block_offset(Header header) {
    return sizeof(Header);
}
u64 file_program_ids_offset(Header header) {
    return sizeof(Header) + header.program_names_block_size;
}
u64 file_dates_offset(Header header) {
    return file_program_ids_offset(header) + (sizeof(u32) * header.total_program_count);
}
u64 file_indexes_offset(Header header) {
    return file_dates_offset(header) + (sizeof(sys_days) * header.day_count);
}
u64 file_records_offset(Header header) {
    return file_indexes_offset(header) + (sizeof(u32) * header.day_count);
}

bool
make_empty_savefile(char *filepath)
{
    FILE *file = fopen(filepath, "wb");
    Header header = {};
    fwrite(&header, sizeof(header), 1, file);
    fclose(file);
    return true;
}

bool file_exists(char *path)
{
    FILE *file = fopen(path, "rb");
    if (file)
    {
        fclose(file);
        return true;
    }
    
    return false;
}

s64 get_file_size(char *filepath)
{
    // TODO: Pretty sure it is bad to seek to end of file
    FILE *file = fopen(filepath, "rb");
    if (file)
    {
        fseek(file, 0, SEEK_END); // Not portable
        long int size = ftell(file);
        fclose(file);
        return (s64) size;
    }
    
    return -1;
}


void
read_programs_from_savefile(FILE *savefile, Header header, Hash_Table *programs)
{
    if (header.total_program_count > 0)
    {
        char *program_names = (char *)xalloc(header.program_names_block_size);
        u32 *program_ids = (u32 *)xalloc(header.total_program_count * sizeof(u32));
        
        fseek(savefile, sizeof(Header), SEEK_SET);
        fread(program_names, 1, header.program_names_block_size, savefile);
        fread(program_ids, sizeof(u32), header.total_program_count, savefile);
        
        char *p = program_names;
        u32 name_index = 0;
        while (name_index < header.total_program_count)
        {
            if (*p == '\0')
            {
                programs->add_item(program_names, program_ids[name_index]); // TODO: read full path from file
                ++name_index;
                program_names = p+1;
            }
            
            ++p;
        }
        
        free(program_names);
        free(program_ids);
    }
}

void
read_all_days_from_savefile(FILE *savefile, Header header, Day *days)
{
    Assert(savefile);
    Assert(days);
    
    if (header.day_count > 0)
    {
        sys_days         *dates = (sys_days *)xalloc(header.day_count * sizeof(sys_days));
        u32             *counts = (u32 *)xalloc(header.day_count * sizeof(u32));
        Program_Record *records = (Program_Record *)xalloc(header.total_record_count * sizeof(Program_Record));
        
        {
            Header test_header = {};
            fread(&test_header, sizeof(Header), 1, savefile);
            fseek(savefile, 0, SEEK_SET);
            Assert(test_header.program_names_block_size == header.program_names_block_size &&
                   test_header.total_program_count == header.total_program_count &&
                   test_header.day_count == header.day_count &&
                   test_header.total_record_count == header.total_record_count);
        }
        
        fseek(savefile, file_dates_offset(header), SEEK_SET);
        fread(dates, sizeof(sys_days), header.day_count, savefile);
        fread(counts, sizeof(u32), header.day_count, savefile);
        fread(records, sizeof(Program_Record), header.total_record_count, savefile);
        
        Program_Record *src = records;
        for (u32 i = 0; i < header.day_count; ++i)
        {
            Day *day = &days[i];
            day->date = dates[i];
            day->record_count = counts[i];
            
            // TODO: In future just pass a big block and will copy all records in and point their pointers to correct records.
            day->records = (Program_Record *)xalloc(sizeof(Program_Record) * day->record_count);
            memcpy(day->records, src, sizeof(Program_Record) * day->record_count);
            src += day->record_count;
        }
        
        free(dates);
        free(counts);
        free(records);
    }
}

void convert_savefile_to_text_file(char *savefile_path, char *text_file_path)
{
    FILE *savefile = fopen(savefile_path, "rb");
    defer(fclose(savefile));
    
    // Header
    // null terminated names, in a block    # programs (null terminated strings)
    // corresponding ids                    # programs (u32)
    // Dates of each day                    # days     (u32)
    // Counts of days records               # days     (u32)
    // Array of all prorgam records clumped by day      # total records (Program_Record)
    
    String_Builder sb = create_string_builder();
    
    auto datetime = System_Clock::now();
    time_t time = System_Clock::to_time_t(datetime);
    sb.append(ctime(&time));
    
    Header header = {};
    fread(&header, sizeof(Header), 1, savefile);
    
    sb.appendf("\nHeader //-----------------------------------------\n"
               "%-25s %4lu \n"
               "%-25s %4lu \n"
               "%-25s %4lu \n"
               "%-25s %4lu \n",
               "program_names_block_size",  header.program_names_block_size,
               "total_program_count",       header.total_program_count,
               "day_count",                 header.day_count,
               "total_record_count",      header.total_record_count);
    
    if (header.total_program_count > 0)
    {
        char     *program_names = (char *)xalloc(header.program_names_block_size);
        u32        *program_ids = (u32 *)xalloc(header.total_program_count * sizeof(u32));
        sys_days         *dates = (sys_days *)xalloc(header.day_count * sizeof(sys_days));
        u32             *counts = (u32 *)xalloc(header.day_count * sizeof(u32));
        Program_Record *records = (Program_Record *)xalloc(header.total_record_count * sizeof(Program_Record));
        
        fread(program_names, 1, header.program_names_block_size, savefile);
        fread(program_ids, sizeof(u32), header.total_program_count, savefile);
        fread(dates, sizeof(sys_days), header.day_count, savefile);
        fread(counts, sizeof(u32), header.day_count, savefile);
        fread(records, sizeof(Program_Record), header.total_record_count, savefile);
        
        sb.appendf("\nPrograms //---------------------------------\n");
        char *p = program_names;
        char *name = program_names;
        u32 name_index = 0;
        while (name_index < header.total_program_count)
        {
            if (*p == '\0')
            {
                // Weird symbols to align to columns
                sb.appendf("%-15s %lu \n", name, program_ids[name_index]);
                ++name_index;
                name = p+1;
            }
            
            ++p;
        }
        
        sb.appendf("\nDates //--------------------------------------\n");
        for (u32 i = 0; i < header.day_count; ++i)
        {
            // day is 1-31, month is 1-12 for this type
            auto d = year_month_day(dates[i]);
            sb.appendf("%u: [%u/%u/%i]   ", i, (unsigned int)d.day(), (unsigned int)d.month(), (int)d.year());
        }
        sb.appendf("\n\nCounts //------------------------------------\n");
        for (u32 i = 0; i < header.day_count; ++i)
        {
            sb.appendf("%lu,  ", counts[i]);
        }
        
        sb.appendf("\n\nRecords //------------------------------------\n");
        sb.appendf("[ID:  Duration]\n");
        u32 day = 0;
        u32 sum = 0;
        for (u32 i = 0; i < header.total_record_count; ++i)
        {
            sb.appendf("[%lu:  %lf]   Day %lu \n", records[i].id, records[i].duration, day);
            ++sum;
            if (sum == counts[day])
            {
                sum = 0;
                ++day;
            }
        }
        
        free(program_names);
        free(program_ids);
        free(dates);
        free(counts);
        free(records);
    }
    
    FILE *text_file = fopen(text_file_path, "wb");
    fwrite(sb.str, 1, sb.len, text_file);
    fclose(text_file);
    
    free_string_builder(&sb);
}



bool valid_savefile(char *filepath)
{
    // Assumes file exists
    s64 file_size = get_file_size(filepath);
    if (file_size < sizeof(Header))
    {
        return false;
    }
    
    FILE *savefile = fopen(filepath, "rb");
    if (!savefile)
    {
        return false;
    }
    
    Header header = {};
    if (fread(&header, sizeof(Header), 1, savefile) != 1)
    {
        fclose(savefile);
        return false;
    }
    
    bool valid = true;
    
    if (header.program_names_block_size > 0 &&
        header.total_program_count > 0 &&
        header.day_count > 0 &&
        header.total_record_count > 0)
    {
        // All are non-zero
        s64 expected_size = file_records_offset(header) + (sizeof(Program_Record) * header.total_record_count);
        if (expected_size != file_size)
        {
            valid = false;
        }
    }
    else
    {
        if (header.program_names_block_size == 0 &&
            header.total_program_count == 0 &&
            header.day_count == 0 &&
            header.total_record_count == 0)
        {
            // All are zero
            if (file_size > sizeof(Header))
            {
                valid = false;
            }
        }
        else
        {
            // Only some are zero
            valid = false;
        }
    }
    
    // TODO: check values of saved data (e.g. id must be smaller than program count)
    
    fclose(savefile);
    
    return valid;
}


static char *
get_filename_from_path(const char *filepath)
{
    // Returns pointer to filename part of filepath
    char *char_after_last_slash = (char *)filepath;
    for (char *at = char_after_last_slash; at[0]; ++at)
    {
        if (*at == '\\')
        {
            char_after_last_slash = at + 1;
        }
    }
    
    return char_after_last_slash;
}

char *
make_filepath(char *exe_path, const char *filename)
{
    char *buf = (char *)xalloc(MaxPathLen);
    if (!buf)
    {
        return nullptr;
    }
    
    char *exe_name = get_filename_from_path(exe_path);
    ptrdiff_t dir_len = exe_name - exe_path;
    if (dir_len + strlen(filename) + 1 > MaxPathLen)
    {
        free(buf);
        return nullptr;
    }
    
    concat_strings(buf, MaxPathLen,
                   exe_path, dir_len,
                   filename, strlen(filename));
    
    realloc(buf, dir_len + strlen(filename) + 1);
    if (!buf)
    {
        return nullptr;
    }
    
    return buf;
}

void update_savefile(char *filepath,
                     Header *header,
                     char *program_names_block, u32 program_names_block_size,
                     u32 *program_ids, u32 program_ids_count,
                     sys_days *dates, u32 dates_count,
                     u32 *daily_record_counts, u32 count_of_daily_record_counts,
                     Program_Record *records, u32 record_count)
{
    FILE *savefile = fopen(filepath, "wb");
    fwrite(header, 1, sizeof(Header), savefile);
    fwrite(program_names_block, 1, program_names_block_size, savefile);
    fwrite(program_ids, sizeof(u32), program_ids_count, savefile);
    fwrite(dates, sizeof(sys_days), dates_count, savefile);
    fwrite(daily_record_counts, sizeof(u32), count_of_daily_record_counts, savefile);
    fwrite(records, sizeof(Program_Record), record_count, savefile);
    fclose(savefile);
}

#if 0
// Save all to file

{
    // NOTE: Always create a new savefile for now
    Header new_header = header;
    
    String_Builder sb = create_string_builder(); // We will build strings will add_string to allow null terminators to be interspersed
    
    u32 total_record_count = 0;
    for (u32 i = 0; i < day_count; ++i)
    {
        total_record_count += days[i].record_count;
    }
    
    std::vector<u32> program_ids;
    std::vector<sys_days> dates;
    std::vector<u32> counts;
    std::vector<Program_Record> records;
    
    program_ids.reserve(all_programs.count);
    dates.reserve(day_count);
    counts.reserve(day_count);
    records.reserve(total_record_count);
    
    u32 test = 0; // TODO: remove this
    for (s64 i = 0; i < all_programs.size; ++i)
    {
        if (all_programs.occupancy[i] == Hash_Table::OCCUPIED)
        {
            // add null terminator
            sb.add_bytes(all_programs.buckets[i].key, strlen(all_programs.buckets[i].key) + 1);
            
            program_ids.push_back(all_programs.buckets[i].value);
            
            test += strlen(all_programs.buckets[i].key) + 1;
        }
    }
    
    Assert(test == sb.len);
    Assert(program_ids.size() == all_programs.count);
    
    // TODO: Not sure whether to keep days as AOS or SOA
    // SOA wouldn't need to do this kind of thing when saving to file
    
    new_header.program_names_block_size = sb.len;
    new_header.total_program_count = all_programs.count;
    new_header.day_count = day_count;
    new_header.total_record_count = total_record_count;
    
    u32 index = 0;
    for (u32 i = 0; i < day_count; ++i)
    {
        dates.push_back(days[i].date);
        counts.push_back(days[i].record_count);
        for (u32 j = 0; j < days[i].record_count; ++j)
        {
            records.push_back(days[i].records[j]);
        }
    }
    
    update_savefile(global_savefile_path,
                    &new_header,
                    sb.str, sb.len,
                    program_ids.data(), program_ids.size(),
                    dates.data(), dates.size(),
                    counts.data(), counts.size(),
                    records.data(), records.size());
    
    convert_savefile_to_text_file(global_savefile_path, global_debug_savefile_path);
}

read_from_file()

// TODO: This is bad and error prone
// Consider a separate array, or linked list of chunks to hold records, when the days then point into.
// Easy to just allocate a big chunk add all the days, then easily add more days, with no resizing/reallocating
{
    // Can there be no records for a day in the file, or there be days with no programs and vice versa?
    FILE *savefile = fopen(global_savefile_path, "rb");
    fread(&header, sizeof(Header), 1, savefile);
    
    // If there are zero days or progams it sets size to 30
    init_hash_table(&all_programs, header.total_program_count*2 + 30);
    
    u32 max_days = std::max(MaxDays, (header.day_count*2) + DefaultDayAllocationCount);
    days = (Day *)xalloc(sizeof(Day) * max_days);
    
    read_programs_from_savefile(savefile, header, &all_programs);
    read_all_days_from_savefile(savefile, header, days);
    
    day_count = header.day_count;
    
    sys_days now = floor<date::days>(System_Clock::now());
    
    // Leave space for 30 days
    if (day_count > 0)
    {
        if (now == days[day_count-1].date)
        {
            cur_day = day_count-1;
            days[day_count-1].records = (Program_Record *)realloc(days[day_count-1].records, sizeof(Program_Record) * MaxDailyRecords);
        }
        else
        {
            cur_day = day_count;
            day_count += 1;
            days[cur_day].records = (Program_Record *)xalloc(sizeof(Day) * MaxDailyRecords);
            days[cur_day].date = now;
            days[cur_day].record_count = 0;
        }
    }
    else
    {
        cur_day = 0;
        day_count = 1;
        days[cur_day].records = (Program_Record *)xalloc(sizeof(Program_Record) * MaxDailyRecords);
        days[cur_day].date = now;
        days[cur_day].record_count = 0;
    }
    
    fclose(savefile);
}


if (!file_exists(global_savefile_path))
{
    // Create database
    make_empty_savefile(global_savefile_path);
    Assert(valid_savefile(global_savefile_path));
}
else
{
    // check database in valid state
    if (!valid_savefile(global_savefile_path))
    {
        tprint("Error: savefile corrupted");
        return 1;
    }
}



#endif