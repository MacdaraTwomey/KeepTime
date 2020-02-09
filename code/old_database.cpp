
#include "ravel.h"
#include "database.h"

void
debug_update_file(Database database)
{
    File_Data binary_file = read_entire_file(global_db_filepath);
    rvl_assert(binary_file.data);
    
    
    // Discards old file contents
    FILE *file = fopen(global_debug_filepath, "w");
    rvl_assert(file);
    
    if (binary_file.size == 0)
    {
        fprintf(file, "EMPTY");
        free(binary_file.data);
        return;
    }
    
    Header *header = (Header *)binary_file.data;
    
    time_t datetime = time(NULL);
    char *datetime_str = ctime(&datetime); // overwritten by other calls
    
    char buf[4096];
    rvl_snprintf(buf, 4096,
                 "\n"
                 "Created: % \n"
                 "HEADER \n"
                 "version: % \n"
                 "day start time: % \n"
                 "poll start time: % \n"
                 "poll end time: % \n"
                 "run at system startup: % (0 or 1) \n"
                 "poll frequency milliseconds: % \n"
                 "num programs in list: % \n"
                 "max programs: % \n"
                 "string block used: % \n"
                 "string block capacity: % \n"
                 "days offset: % \n"
                 "num days: % \n"
                 "offset to last day: % \n\n",
                 datetime_str,
                 header->version,
                 header->day_start_time,
                 header->poll_start_time,
                 header->poll_end_time,
                 header->run_at_system_startup,
                 header->poll_frequency_milliseconds,
                 header->num_programs_in_list,
                 header->max_programs,
                 header->string_block_used,
                 header->string_block_capacity,
                 header->days_offset,
                 header->num_days,
                 header->last_day_offset);
    
    // Assumes no writing errors (fprintf can return negative)
    int written = fprintf(file, buf);
    
    written += fprintf(file, "PROGRAM LIST \n");
    
    if (header->num_programs_in_list > 0)
    {
        Program *program_list = get_program_list(database.state);
        char *string_block = get_string_block(database.state, header);
        rvl_assert(header->string_block_used > 0);
        
        // Assumes a valid file state
        for (u32 i = 0; i < header->num_programs_in_list; ++i)
        {
            char *name = string_block + program_list[i].name_offset;
            rvl_snprintf(buf, 4096, "ID: %, name: % \n", program_list[i].ID, name);
            written += fprintf(file, buf);
        }
        
        written += fprintf(file, "\nSTRING_BLOCK \n");
        written += fwrite(string_block, sizeof(char), header->string_block_used, file);
    }
    
    written += fprintf(file, "\nDAYS \n");
    if (header->num_days > 0)
    {
        Day_Info *day_position = (Day_Info *)(database.state.data + sizeof_top_block(header));
        Program_Time *first_program_times =
            (Program_Time *)(database.state.data + sizeof_top_block(header) + sizeof(Day_Info));
        
        Day day = {*day_position, first_program_times};
        
        auto next_day = [&]() -> Day {
            day_position = (Day_Info *)((u8 *)day_position + sizeof_day(*day_position));
            Program_Time *programs = (Program_Time *)((u8 *)day_position + sizeof(Day_Info));
            Day new_day = {*day_position, programs};
            return new_day;
        };
        
        for (u32 i = 0; i < header->num_days; ++i)
        {
            Date date = unpack_16_bit_date(day.info.date);
            rvl_snprintf(buf, 4096,
                         "\nDate: d% m% y% \n"
                         "num programs: % \n",
                         date.day, date.month, date.year,
                         day.info.num_programs);
            written += fprintf(file, buf);
            
            for (u32 j = 0; j < day.info.num_programs; ++j)
            {
                rvl_snprintf(buf, 4096, "ID: %, elapsed time: % \n",
                             day.programs[j].ID, day.programs[j].elapsed_time);
                written += fprintf(file, buf);
            }
            
            // If not last day
            if (i + 1 < header->num_days) day = next_day();
        }
    }
    
    rvl_assert(written >= 0);
    fclose(file);
    free(binary_file.data);
}

#if 0
void
debug_new_file(Header *header)
{
    // Write debug file
    // TODO: How to sync writes to debug file between two processes
    //       (when GUI updates settings)
    global_debug_file.file = fopen(global_debug_filepath, "w");
    if (!global_debug_file.file)
    {
        tprint("Error: could not open debug database file");
        return;
    }
    
    time_t datetime = time(NULL);
    char *datetime_str = ctime(&datetime); // overwritten by other calls
    
    char buf[2048];
    rvl_snprintf(buf, 2048,
                 "\n"
                 "Created: % \n"
                 "HEADER \n"
                 "version: % \n"
                 "day start time: % \n"
                 "poll start time: % \n"
                 "poll end time: % \n"
                 "run at system startup: % (0 or 1) \n"
                 "poll frequency milliseconds: % \n"
                 "num programs in list: % \n"
                 "max programs: % \n"
                 "string block used: % \n"
                 "string block capacity: % \n"
                 "days offset: % \n"
                 "num days: % \n"
                 "offset to last day: % \n"
                 "\n"
                 "PROGRAM LIST \n"
                 "\n"
                 "STRING BLOCK \n"
                 "\n"
                 "RECORDS \n",
                 datetime_str,
                 header->version,
                 header->day_start_time,
                 header->poll_start_time,
                 header->poll_end_time,
                 header->run_at_system_startup,
                 header->poll_frequency_milliseconds,
                 header->num_programs_in_list,
                 header->max_programs,
                 header->string_block_used,
                 header->string_block_capacity,
                 header->days_offset,
                 header->num_days,
                 header->last_day_offset);
    
    char *program_list = strstr(buf, "PROGRAM LIST");
    char *string_block = strstr(buf, "STRING BLOCK");
    char *records = strstr(buf, "RECORDS");
    rvl_assert(program_list && string_block && records);
    
    // Writes buf not including null terminator
    int written = fprintf(global_debug_file.file, buf);
    
    global_debug_file.size = strlen(buf);
    global_debug_file.program_list_offset = program_list - buf;
    global_debug_file.string_block_offset = string_block - buf;
    global_debug_file.records_offset = records - buf;
    
    // fclose(debug_database_file);
}

void
debug_write_day(Day_Info day)
{
    FILE *debug_database_file = fopen(global_debug_filepath, "a");
    if (!debug_database_file)
    {
        tprint("Error: could not open debug database file");
        return;
    }
    
    Date date = unpack_16_bit_date(day.date);
    
    char buf[2048];
    rvl_snprintf(buf, 2048,
                 "Day \n"
                 "date: d:% m:% y:% \n"
                 "num records \n",
                 date.day, date.month, date.year,
                 day.num_programs);
    
    fwrite(buf, 1, strlen(buf), debug_database_file);
    fclose(debug_database_file);
}
#endif
