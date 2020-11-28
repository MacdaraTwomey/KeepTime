
#include "monitor.h"
#include <stdio.h>

static constexpr u32 MAGIC_NUMBER = 0xDACA571E;
static constexpr u32 CURRENT_VERSION = 0;

// TODO: Prefer to have file read/write in platform layer

// Monitor Binary File
struct MBF_Header
{
    // Not in body of file (just header)
    u32 magic_number;
    u32 version; 
    
    App_Id next_program_id;
    App_Id next_website_id;
    Misc_Options misc_options;      
    
    u32 id_count;
    u32 day_count;
    u32 total_record_count;
    u32 local_program_count;
    u32 website_count;
    u32 keyword_count;
    u32 total_string_count;
    u32 string_block_size;
    
    // NOTE: Strings not null terminated in file
    
    // In file in this order
    u32 ids;             // all local programs, then all websites
    u32 records;
    u32 days;  
    // [short, full] ... for local programs
    // [short] ... for websites
    // [keyword] ... for keywords
    u32 strings;  
    u32 string_lengths; // all local_programs (short_name, full_name), then all website (short_name), then keywords
};

struct MBF_Day
{
    date::sys_days date;
    u32 record_count;
};

struct Loaded_MBF
{
    // Include Header of file?
    
    App_Id next_program_id;
    App_Id next_website_id;
    
    Array<String, MAX_KEYWORD_COUNT> keywords; // null terminated strings
    Misc_Options misc_options;
    App_Id *ids;     
    Day *days;
    Local_Program_Info *local_programs;
    Website_Info *websites;
    
    Record *records; // all records
    char *string_block;
    
    u32 day_count;
    u32 id_count;
    u32 local_program_count;
    u32 website_count;
    u32 total_record_count; 
    u32 string_block_size;
    
};

void 
copy_to_string_block(String s, char *string_block, u32 *cur_offset)
{
    memcpy(string_block + *cur_offset, s.str, s.length);
    *cur_offset += s.length;
}

u32
write_block_aligned(void *block, size_t size, FILE *file, bool *error)
{
    if (size == 0) return 0;
    if (*error) return 0;
    
    long offset = ftell(file);
    Assert(offset % 8 == 0);
    
    if (fwrite(block, size, 1, file) == 1)
    {
        size_t align_amount = (8 - (size & 7)) & 7; // get number of bytes needed to align to 8 byte boundary
        if (align_amount > 0)
        {
            static u8 zero_bytes[8] = {};
            if (fwrite(&zero_bytes, align_amount, 1, file) == 1)
            {
                Assert(ftell(file) % 8 == 0);
                return offset;
            }
        }
        else
        {
            return offset;
        }
    }
    
    *error = true;
    return 0;
}

void
populate_array(u32 *arr, u32 n, u32 start)
{
    for (u32 i = 0; i < n; ++i)
    {
        arr[i] = start++;
    }
}

bool
write_to_MBF(App_List *apps, Day_List *day_list, Settings *settings, char *filepath)
{
    FILE *file = fopen(filepath, "wb");
    if (!file) return false;
    
    u32 local_program_count = apps->local_program_ids.size();
    u32 website_count = apps->website_ids.size();
    u32 id_count = local_program_count + website_count;
    u32 keyword_count = settings->keywords.count;
    u32 string_count = (local_program_count * 2) + website_count + keyword_count;
    u32 day_count = day_list->days.size();
    
    App_Id *ids = (App_Id *)xalloc(sizeof(App_Id) * id_count);
    u32 *string_lengths = (u32 *)xalloc(sizeof(u32) * string_count);
    MBF_Day *days = (MBF_Day *)xalloc(sizeof(MBF_Day) * day_count);
    
    // Generate id's because they are not explicitly stored (except in hash table)
    populate_array(ids, local_program_count, LOCAL_PROGRAM_ID_START);
    populate_array(ids + local_program_count, website_count, WEBSITE_ID_START);
    
    size_t all_strings_length = 0;
    
    u32 string_idx = 0;
    for (u32 i = 0; i < apps->local_programs.size(); ++i)
    {
        String &short_name = apps->local_programs[i].short_name;
        String &full_name = apps->local_programs[i].full_name;
        
        string_lengths[string_idx++] = short_name.length;
        all_strings_length += short_name.length;
        
        string_lengths[string_idx++] = full_name.length;
        all_strings_length += full_name.length;
    }
    for (u32 i = 0; i < apps->websites.size(); ++i)
    {
        String &short_name = apps->websites[i].short_name;
        
        string_lengths[string_idx++] = short_name.length;
        all_strings_length += short_name.length;
    }
    for (u32 i = 0; i < settings->keywords.count; ++i)
    {
        string_lengths[string_idx++] = settings->keywords[i].length;
        all_strings_length += settings->keywords[i].length;
    }
    
    size_t string_block_size = all_strings_length;
    char *string_block = (char *)xalloc(string_block_size);
    
    u32 string_block_cur_offset = 0;
    for (auto &info : apps->local_programs)
    {
        copy_to_string_block(info.short_name, string_block, &string_block_cur_offset);
        copy_to_string_block(info.full_name, string_block, &string_block_cur_offset);
    }
    for (auto &info : apps->websites)
    {
        copy_to_string_block(info.short_name, string_block, &string_block_cur_offset);
    }
    for (int i = 0; i < settings->keywords.count; ++i)
    {
        copy_to_string_block(settings->keywords[i], string_block, &string_block_cur_offset);
    }
    
    Assert(string_block_cur_offset == string_block_size);
    
    u32 total_record_count = 0;
    for (u32 i = 0; i < day_count; ++i)
    {
        Day *day = &day_list->days[i];
        MBF_Day *dest_day = &days[i];
        
        dest_day->date = day->date;
        dest_day->record_count = day->record_count;
        
        total_record_count += day->record_count;
    }
    
    Record *records = (Record *)xalloc(sizeof(Record) * total_record_count);
    
    u32 record_offset = 0;
    for (u32 i = 0; i < day_count; ++i)
    {
        Day *day = &day_list->days[i];
        memcpy(records + record_offset, day->records, day->record_count * sizeof(Record));
        record_offset += day->record_count;
    }
    
    // Write empty header at start of file
    bool write_error = false;
    MBF_Header header = {};
    write_block_aligned(&header, sizeof(MBF_Header), file, &write_error);
    
    header.magic_number = MAGIC_NUMBER;
    header.version = CURRENT_VERSION;
    header.next_program_id = apps->next_program_id;
    header.next_website_id = apps->next_website_id;
    header.misc_options = settings->misc;      
    
    // 0 if the corresponding count is also 0 (i.e. no elements are written to file)
    header.ids = write_block_aligned(ids, id_count * sizeof(App_Id), file, &write_error);
    header.records = write_block_aligned(records, total_record_count * sizeof(Record), file, &write_error);
    header.days = write_block_aligned(days, day_count * sizeof(MBF_Day), file, &write_error);     
    header.strings = write_block_aligned(string_block, string_block_size, file, &write_error);
    header.string_lengths = write_block_aligned(string_lengths, string_count * sizeof(u32), file, &write_error); 
    
    header.id_count = id_count;
    header.day_count = day_count;
    header.total_record_count = total_record_count;
    header.local_program_count = local_program_count;
    header.website_count = website_count;
    header.keyword_count = settings->keywords.count;
    header.total_string_count = string_count;
    header.string_block_size = string_block_size;
    
    // go back to start of file and write header
    fseek(file, 0, SEEK_SET);
    write_block_aligned(&header, sizeof(MBF_Header), file, &write_error);
    if (write_error)
    {
        fclose(file);
        return false;
    }
    
    if (fclose(file) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

String
get_string_from_block(char *string_block, u32 string_length, u32 *string_block_offset)
{
    String s = {};
    s.str = string_block + *string_block_offset;
    s.length = string_length;
    *string_block_offset += string_length;
    return s;
}

bool
read_from_MBF(App_List *apps, Day_List *day_list, Settings *settings, char *file_name)
{
    Platform_Entire_File entire_file = platform_read_entire_file(file_name);
    defer(free(entire_file.data));
    
    if (entire_file.size < sizeof(MBF_Header)) 
    {
        return false;
    }
    
    MBF_Header *header = (MBF_Header *)entire_file.data;
    
    Assert(header->total_string_count == 
           header->local_program_count*2 + header->website_count + header->keyword_count);
    
    if (header->magic_number != MAGIC_NUMBER) return false;
    if (header->version > CURRENT_VERSION) return false;
    if (header->id_count != header->local_program_count + header->website_count) return false;
    
    // last section of file
    if (entire_file.size < header->string_lengths + (header->total_string_count * sizeof(u32)))
    {
        return false;
    }
    
    u8 *start = entire_file.data;
    
    // Strings in string block are not null terminated, but we want to have null terminated strings
    App_Id *ids           = (App_Id *)((header->ids) ? start + header->ids : nullptr);     
    MBF_Day *days         = (MBF_Day *)((header->days) ? start + header->days : nullptr);
    Record *records       = (Record *)((header->records) ? start + header->records : nullptr); 
    char *string_block    = (char *)((header->strings) ? start + header->strings : nullptr);
    u32 *string_lengths  = (u32 *)((header->string_lengths) ? start + header->string_lengths : nullptr);
    
    apps->local_program_ids = std::unordered_map<String, App_Id>(header->local_program_count + 50);
    apps->website_ids       = std::unordered_map<String, App_Id>(header->website_count + 50);
    apps->local_programs.reserve(header->local_program_count + 50);
    apps->websites.reserve(header->website_count + 50);
    
    apps->next_program_id = header->next_program_id;
    apps->next_website_id = header->next_website_id;
    
    settings->misc = header->misc_options;
    
    init_arena(&apps->name_arena, header->string_block_size + Kilobytes(10), Kilobytes(10));
    init_arena(&day_list->record_arena, (header->total_record_count * sizeof(Record)) + MAX_DAILY_RECORDS_MEMORY_SIZE, MAX_DAILY_RECORDS_MEMORY_SIZE);
    init_arena(&settings->keyword_arena, MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE);
    
    Assert(header->id_count == header->local_program_count + header->website_count);
    
    u32 current_string = 0;
    u32 string_block_offset = 0;
    u32 id_offset = 0;
    if (ids && string_lengths && string_block)
    {
        for (u32 i = 0; i < header->local_program_count; ++i)
        {
            String short_name = get_string_from_block(string_block, string_lengths[current_string++], &string_block_offset);
            String full_name = get_string_from_block(string_block, string_lengths[current_string++], &string_block_offset);
            
            // Copies to arena
            add_local_program(apps, ids[id_offset++], short_name, full_name);
        }
        if (header->website_count > 0)
        {
            for (u32 i = 0; i < header->website_count; ++i)
            {
                String short_name = get_string_from_block(string_block, string_lengths[current_string++], &string_block_offset);
                add_website(apps, ids[id_offset++], short_name);
            }
        }
    }
    
    for (u32 i = 0; i < header->keyword_count; ++i)
    {
        String keyword = get_string_from_block(string_block, string_lengths[current_string++], &string_block_offset);
        add_keyword(settings, keyword);
    }
    
    if (days)
    {
        // Just copy all records at once, then fix pointers into arena, rather than repeatedly copying each day
        Record *new_records = (Record *)push_size(&day_list->record_arena, header->total_record_count * sizeof(Record));
        memcpy(new_records, records, header->total_record_count * sizeof(Record));
        u32 record_offset = 0;
        for (u32 i = 0; i < header->day_count; ++i)
        {
            MBF_Day *src_day = days + i;
            
            Day new_day;
            new_day.date = src_day->date;
            new_day.record_count = src_day->record_count;
            new_day.records = (new_day.record_count > 0) ? new_records + record_offset : nullptr;
            
            day_list->days.push_back(new_day);
            
            record_offset += new_day.record_count;
        }
    }
    
    date::sys_days current_date = get_local_time_day();
    if (day_list->days.size() == 0 || current_date != day_list->days.back().date)
    {
        // TODO:  Dont like this here
        start_new_day(day_list, current_date);
    }
    
    return true;
}

bool
make_empty_savefile(char *filepath)
{
    // Called when opening program (so no state in memory) and there is no file
    
    bool ok = false;
    FILE *file = fopen(filepath, "wb");
    if (file)
    {
        MBF_Header header = {};
        header.magic_number = MAGIC_NUMBER;
        header.version = CURRENT_VERSION;
        header.next_program_id = LOCAL_PROGRAM_ID_START;
        header.next_website_id = WEBSITE_ID_START;
        header.misc_options = Misc_Options::default_misc_options();      
        
        // 0 if the corresponding count is also 0 (i.e. no elements are written to file)
        header.ids = 0;
        header.days = 0;
        header.string_lengths = 0;
        header.records = 0;
        header.strings = 0;
        
        header.id_count = 0;
        header.day_count = 0;
        header.total_record_count = 0;
        header.local_program_count = 0;
        header.website_count = 0;
        header.keyword_count = 0;
        header.total_string_count = 0;
        header.string_block_size = 0;
        
        ok = (fwrite(&header, sizeof(header), 1, file) == 1);
        
        fclose(file);
    }
    
    return ok;
}
