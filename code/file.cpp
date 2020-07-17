
#include "monitor.h"
#include <stdio.h>

static constexpr char SaveFileName[] = "monitor_save.pmd";
static constexpr char DebugSaveFileName[] = "debug_monitor_save.txt";
static constexpr u32 MAGIC_NUMBER = 0xDACA571E;
static constexpr u32 CURRENT_VERSION = 0;

// Monitor Binary File
struct MBF_Header
{
    u32 magic_number;
    u32 version; 
    
    App_Id next_program_id;
    App_Id next_website_id;
    Misc_Options misc_options;      
    
    u32 ids;             // all local programs, then all websites
    u32 days;            // record_count for each day
    u32 string_lengths; // all local_programs (short_name, full_name), then all website (short_name), then keywords
    u32 records;
    
    // in this order
    // [short, full] ... for local programs
    // [short] ... for websites
    // [keyword] ... for keywords
    u32 strings;  
    
    u32 id_count;
    u32 day_count;
    u32 record_count;
    u32 local_program_count;
    u32 website_count;
    u32 keyword_count;
    u32 string_block_size;
};

struct MBF_Day
{
    date::sys_days date;
    u32 record_count;
};

struct Read_MBF
{
    App_Id next_program_id;
    App_Id next_website_id;
    Settings *settings; //10k
    
    App_Id *ids;     
    Day *days;            // record_count for each day
    Local_Program_Info *local_programs;
    Website_Info *websites;
    Record *records;
    
    u32 id_count;
    u32 day_count;
    //u32 app_info_count;
    u32 record_count;
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
write_to_MBF(Monitor_State *state, FILE *file)
{
    // Could store string lengths or offsets into the block, not sure which better
    Database *database = &state->database;
    App_List *apps = &state->database.apps;
    Settings *settings = &state->settings;
    
    // assumes no deleted ids
    u32 local_program_count = apps->local_program_ids.size();
    u32 website_count = apps->website_ids.size();
    u32 id_count = local_program_count + website_count;
    
    App_Id *ids = (App_Id *)malloc(sizeof(App_Id) * id_count);
    defer(free(ids));
    if (!ids) return false;
    
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
    
    u32 *string_lengths = (u32 *)malloc(sizeof(u32) * string_count); // upper bound
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
    
    MBF_Header header = {};
    header.magic_number = MAGIC_NUMBER;
    header.version = CURRENT_VERSION;
    header.next_program_id = apps->next_program_id;
    header.next_website_id = apps->next_website_id;
    header.misc_options = settings->misc_options;      
    
    // Doesnt work coz header 0 inited
    u32 id_offset = write_memory_to_file(ids, id_count * sizeof(App_Id), file);
    u32 day_offset = write_memory_to_file(days, day_count * sizeof(MBF_Day), file);     
    u32 string_lengths_offset = write_memory_to_file(string_lengths, string_count * sizeof(u32), file); 
    u32 records_offset = write_memory_to_file(records, total_record_count * sizeof(Record), file);
    u32 string_block_offset = write_memory_to_file(string_block, string_block_size, file);
    
    if (id_offset == -1 || day_offset == -1 || string_lengths_offset == -1 || records_offset == -1 || string_block_offset == -1)
    {
        // write failed
        return false;
    }
    
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


bool
make_empty_savefile(char *filepath)
{
    FILE *file = fopen(filepath, "wb");
    //Header header = {};
    //fwrite(&header, sizeof(header), 1, file);
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


#if 0
bool valid_savefile(char *filepath)
{
}

{
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