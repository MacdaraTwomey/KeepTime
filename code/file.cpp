
#include "monitor.h"
#include <stdio.h>

static constexpr char SaveFileName[] = "monitor_save.pmd";
static constexpr char DebugSaveFileName[] = "debug_monitor_save.txt";
static constexpr u32 MAGIC_NUMBER = 0xDACA571E;
static constexpr u32 CURRENT_VERSION = 0;

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
write_memory_to_file(void *memory, size_t size, FILE *file)
{
    // Returns -1 on error
    // Returns offset in file of start of memory, or 0 if memory size was 0.
    long offset = ftell(file);
    size_t num_written = fwrite(memory, size, 1, file);
    if (num_written == 1)
    {
        return offset;
    }
    else
    {
        return 0;
    }
}

bool
write_to_MBF(App_List *apps, Day_List *days, Settings *settings, FILE *file)
{
    // Could store string lengths or offsets into the block, not sure which better
    
    // assumes no deleted ids
    u32 local_program_count = apps->local_program_ids.size();
    u32 website_count = apps->website_ids.size();
    u32 id_count = local_program_count + website_count;
    
    App_Id *ids = (App_Id *)malloc(sizeof(App_Id) * id_count);
    defer(free(ids));
    if (!ids) return false;
    
    // Generate id's because they are not explicitly stored (except in hash table)
    u32 id_idx = 0;
    for (App_Id gen_id = 1; id_idx < apps->local_program_ids.size(); ++id_idx, ++gen_id)
    {
        Assert(is_local_program(gen_id));
        ids[id_idx] = gen_id; // starts at 1
    }
    for (App_Id gen_id = 0; id_idx < apps->website_ids.size(); ++id_idx, ++gen_id)
    {
        Assert(is_website(gen_id));
        ids[id_idx] = gen_id & (1 << 31);
    }
    
    u32 keyword_count = settings->keywords.count;
    u32 string_count = (local_program_count * 2) + website_count + keyword_count;
    
    u32 *string_lengths = (u32 *)malloc(sizeof(u32) * string_count);
    defer(free(string_lengths));
    if (!string_lengths) return false;
    
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
    char *string_block = (char *)malloc(string_block_size);
    defer(free(string_block));
    if (!string_block) return false;
    
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
    
    u32 day_count = database->day_list.days.size();
    MBF_Day *days = (MBF_Day *)malloc(sizeof(MBF_Day) * day_count);
    defer(free(days));
    if (!days) return false;
    
    u32 total_record_count = 0;
    for (u32 i = 0; i < day_count; ++i)
    {
        Day *day = &database->day_list.days[i];
        MBF_Day *dest_day = &days[i];
        
        dest_day->date = day->date;
        dest_day->record_count = day->record_count;
        
        total_record_count += day->record_count;
    }
    
    Record *records = (Record *)malloc(sizeof(Record) * total_record_count);
    defer(free(records));
    if (!records) return false;
    
    u32 record_offset = 0;
    for (u32 i = 0; i < day_count; ++i)
    {
        Day *day = &database->day_list.days[i];
        memcpy(records + record_offset, day->records, day->record_count * sizeof(Record));
        record_offset += day->record_count;
    }
    
    // seek past header at start of file
    fseek(file, sizeof(MBF_Header), SEEK_SET);
    
    u32 id_offset = write_memory_to_file(ids, id_count * sizeof(App_Id), file);
    u32 records_offset = write_memory_to_file(records, total_record_count * sizeof(Record), file);
    u32 day_offset = write_memory_to_file(days, day_count * sizeof(MBF_Day), file);     
    u32 string_block_offset = write_memory_to_file(string_block, string_block_size, file);
    u32 string_lengths_offset = write_memory_to_file(string_lengths, string_count * sizeof(u32), file); 
    
    if (id_offset == -1 || day_offset == -1 || string_lengths_offset == -1 || records_offset == -1 || string_block_offset == -1)
    {
        // write failed
        return false;
    }
    
    
    MBF_Header header = {};
    header.magic_number = MAGIC_NUMBER;
    header.version = CURRENT_VERSION;
    header.next_program_id = apps->next_program_id;
    header.next_website_id = apps->next_website_id;
    header.misc_options = settings->misc_options;      
    
    // 0 if the corresponding count is also 0 (i.e. no elements are written to file)
    header.ids = id_offset;
    header.days = day_offset;
    header.string_lengths = string_lengths_offset; 
    header.records = records_offset;
    header.strings = string_block_offset;
    
    header.id_count = id_count;
    header.day_count = day_count;
    header.record_count = total_record_count;
    header.local_program_count = local_program_count;
    header.website_count = website_count;
    header.keyword_count = settings->keywords.count;
    header.total_string_count = string_count;
    header.string_block_size = string_block_size;
    
    // go back to start of file and write header
    fseek(file, 0, SEEK_SET);
    size_t num_written = fwrite(&header, sizeof(MBF_Header), 1, file);
    if (num_written != 1)
    {
        return false;
    }
    
    return true;
}

String
get_string_from_block(char *string_block, u32 string_length, u32 *string_block_offset)
{
    String s = {};
    s.str = string_block + string_block_offset;
    s.length = string_length;
    *string_block_offset += s.length;
    return s;
}

bool
read_from_MBF(Loaded_MBF *loaded_mbf, FILE *file)
{
    // NOTE: THis is not even using the section indexes in the header, i could do a read entire file then use those, but not sure what to do about unaligned memory accesses
    // TODO: Check success of reads, i would much prefer a read entire file for easy check reads 
    
    BMF_Header header = {};
    s64 file_size = get_file_size(file);
    if (file_size < sizeof(BMF_Header)) return false;
    
    fread(&header, sizeof(BMF_Header), 1, file);
    if (header.magic_number != MAGIC_NUMBER) return false;
    if (header.version > CURRENT_VERSION) return false;
    if (header.id_count != header.local_program_count + header.website_count) return false;
    
    App_Id *ids = nullptr;     
    Day *days = nullptr;
    Local_Program_Info *local_programs = nullptr;
    Website_Info *websites = nullptr;
    Record *records = nullptr; 
    char *string_block = nullptr;
    Array<String, MAX_KEYWORD_COUNT> keywords = {};
    
    if (header.id_count > 0)
    {
        ids = xalloc(sizeof(App_Id) * header.id_count);
        fread(ids, sizeof(App_Id), header.id_count, file);
    }
    
    if (header.total_record_count > 0)
    {
        records = xalloc(sizeof(Record) * header.total_record_count);
        fread(records, sizeof(Record), header.total_record_count, file);
    }
    
    if (header.day_count > 0)
    {
        days = xalloc(sizeof(Day) * header.day_count);
        u32 record_offset = 0;
        for (u32 i = 0; i < header.day_count; ++i)
        {
            MBF_Day d;
            fread(&d, sizeof(MBF_Day), 1, file);
            
            days[i].date = d.date;
            days[i].record_count = d.record_count;
            days[i].records = (days[i].record_count > 0) ? records + record_offset : nullptr;
            
            record_offset += days[i].record_count;
        }
    }
    
    if (header.string_block_size > 0)
    {
        string_block = (char *)xalloc(header.string_block_size);
        fread(string_block, 1, string_block_size, file);
    }
    if (header.total_string_count > 0)
    {
        Assert(header.string_block_size > 0);
        
        u32 *string_lengths = xalloc(sizeof(u32) * header.total_string_count);
        fread(string_lengths, sizeof(u32), header.total_string_count, file);
        
        u32 current_string = 0;
        u32 string_block_offset = 0;
        if (header.local_program_count > 0)
        {
            local_programs = xalloc(sizeof(Local_Program_Info) * header.local_program_count);
            for (u32 i = 0; i < header.local_program_count; ++i)
            {
                local_programs[i].short_name = get_string_from_block(string_block, string_lengths[current_string++], &string_block_offset);
                local_programs[i].full_name = get_string_from_block(string_block, string_lengths[current_string++], &string_block_offset);
            }
        }
        if (header.website_count > 0)
        {
            websites = xalloc(sizeof(Website_Info) * header.website_count);
            for (u32 i = 0; i < header.website_count; ++i)
            {
                websites[i].short_name = get_string_from_block(string_block, string_lengths[current_string++], &string_block_offset);
            }
        }
        if (header.keyword_count > 0)
        {
            for (u32 i = 0; i < header.keyword_count; ++i)
            {
                String keyword = get_string_from_block(string_block, string_lengths[current_string++], &string_block_offset);
                keywords.push_back(keyword);
            }
        }
    }
    
    loaded_mbf->next_website_id = header.next_website_id;
    loaded_mbf->next_program_id = header.next_program_id;
    loaded_mbf->keywords = keywords;
    loaded_mbf->misc_options = header.misc_options;
    loaded_mbf->ids = ids;
    loaded_mbf->days = days;
    loaded_mbf->local_programs = local_programs;
    loaded_mbf->websites = websites;
    loaded_mbf->records = records;
    loaded_mbf->string_block = string_block;
    
    loaded_mbf->day_count = header.day_count;
    loaded_mbf->id_count = header.id_count;
    loaded_mbf->local_program_count = header.local_program_count;
    loaded_mbf->website_count = header.website_count;
    loaded_mbf->total_record_count = header.record_count;
    loaded_mbf->string_block_size = header.string_block_size;
    
    return true;
}

void 
free_loaded_MBF(Loaded_MBF *loaded_mbf)
{
    free_array(loaded_mbf->keywords);
    free(loaded_mbf->ids);
    free(loaded_mbf->days);
    free(loaded_mbf->local_programs);
    free(loaded_mbf->websites);
    free(loaded_mbf->records);
    free(loaded_mbf->string_block);
    *loaded_mbf = {};
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
        header.misc_options = Misc_Options.default_misc_options();      
        
        // 0 if the corresponding count is also 0 (i.e. no elements are written to file)
        header.ids = 0;
        header.days = 0;
        header.string_lengths = 0;
        header.records = 0;
        header.strings = 0;
        
        header.id_count = 0;
        header.day_count = 0;
        header.record_count = 0;
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

// TODO: Debug doesn't check if file exists etc
s64 
get_file_size(FILE *file)
{
    // TODO: Pretty sure it is bad to seek to end of file
    fseek(file, 0, SEEK_END); // Not portable. TODO: Use os specific calls
    long int size = ftell(file);
    fclose(file);
    return (s64) size;
}
