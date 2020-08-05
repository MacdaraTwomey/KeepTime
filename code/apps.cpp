
date::sys_days
get_local_time_day()
{
    time_t rawtime;
    time( &rawtime );
    
    struct tm *info;
    // tm_mday;        /* day of the month, range 1 to 31  */
    // tm_mon;         /* month, range 0 to 11             */
    // tm_year;        /* The number of years since 1900   */
    // tm_hour; hours since midnight    0-23
    // tm_min; minutes after the hour   0-59
    // tm_sec; seconds after the minute 0-61* (*generally 0-59 extra range to accomodate leap secs in some systems)
    // tm_isdst; Daylight Saving Time flag 
    /* is greater than zero if Daylight Saving Time is in effect, zero if Daylight Saving Time is not in effect, and less than zero if the information is not available. */
    
    // Looks if TZ database
    info = localtime( &rawtime );
    auto now = date::sys_days{
        date::month{(unsigned)(info->tm_mon + 1)}/date::day{(unsigned)info->tm_mday}/date::year{info->tm_year + 1900}};
    
    //auto test_date = std::floor<date::local_days>()};
    //Assert(using local_days    = local_time<days>;
    return now;
}

void
start_new_day(Day_List *day_list, date::sys_days date)
{
    // day points to next free space
    Day new_day = {};
    new_day.record_count = 0;
    new_day.date = date;
    new_day.records = (Record *)push_size(&day_list->arena, MAX_DAILY_RECORDS * sizeof(Record));
    
    day_list->days.push_back(new_day);
}

void
add_or_update_record(Day_List *day_list, App_Id id, s64 dt)
{
    // Assumes a day's records are sequential in memory
    
    // DEBUG: Record id can be 0, used to denote 'no program'
    Assert(day_list->days.size() > 0);
    
    Day *cur_day = &day_list->days.back();
    for (u32 i = 0; i < cur_day->record_count; ++i)
    {
        if (id == cur_day->records[i].id)
        {
            // Existing id today
            cur_day->records[i].duration += dt;
            return;
        }
    }
    
    if (cur_day->record_count < MAX_DAILY_RECORDS)
    {
        
        // Add new record to block
        cur_day->records[cur_day->record_count] = Record{ id, dt };
        cur_day->record_count += 1;
    }
    else
    {
        // TODO: in release we will just not add record if above daily limit (which is extremely high)
        Assert(0);
    }
}

Day_View
get_day_view(Day_List *day_list)
{
    // TODO: Handle zero day in day_list
    // (are we handling zero records in list?)
    
    // TODO: This should not default to daily, but keep the alst used range, even if the records are updated
    
    Day_View day_view = {};
    
    // If no records this might allocate 0 bytes (probably fine)
    Day *cur_day = &day_list->days.back();
    day_view.copy_of_current_days_records = (Record *)calloc(1, cur_day->record_count * sizeof(Record));
    memcpy(day_view.copy_of_current_days_records, cur_day->records, cur_day->record_count * sizeof(Record));
    
    // copy vector
    day_view.days = day_list->days;
    day_view.days.back().records = day_view.copy_of_current_days_records;
    
    return day_view;
}

void
free_day_view(Day_View *day_view)
{
    if (day_view->copy_of_current_days_records)
    {
        free(day_view->copy_of_current_days_records);
    }
    
    // TODO: Should I deallocate day_view->days vector memory? (probably)
    *day_view = {};
}

App_Id 
make_local_program_id(App_List *apps)
{
    App_Id id = apps->next_program_id;
    apps->next_program_id += 1;
    Assert(is_local_program(id));
    return id;
}

App_Id 
make_website_id(App_List *apps)
{
    App_Id id = apps->next_website_id;
    apps->next_website_id += 1;
    Assert(is_website(id));
    return id;
}

App_Id
get_local_program_app_id(App_List *apps, String short_name, String full_name)
{
    // TODO: String of shortname can point into string of full_name for paths
    // imgui can use non null terminated strings so interning a string is fine
    
    // if using custom hash table impl can use open addressing, and since nothing is ever deleted don't need a occupancy flag or whatever can just use id == 0 or string == null to denote empty.
    
    App_Id result = Id_Invalid;
    
    if (apps->local_program_ids.count(short_name) == 0)
    {
        App_Id new_id = make_local_program_id(apps);
        
        Local_Program_Info info;
        info.full_name = push_string(&apps->names_arena, full_name); 
        info.short_name = push_string(&apps->names_arena, short_name); // string intern this from fullname
        
        apps->local_programs.push_back(info);
        
        // TODO: Make this share short_name given to above functions
        String key_copy = push_string(&apps->names_arena, short_name);
        apps->local_program_ids.insert({key_copy, new_id});
        
        result = new_id;
    }
    else
    {
        // if for some reason this is not here this should return 0 (default for an integer)
        result = apps->local_program_ids[short_name];
    }
    
    return result;
}
App_Id
get_website_app_id(App_List *apps, String short_name)
{
    App_Id result = Id_Invalid;
    
    if (apps->website_ids.count(short_name) == 0)
    {
        App_Id new_id = make_website_id(apps);
        
        Website_Info info;
        info.short_name = push_string(&apps->names_arena, short_name); 
        //info.icon_index = -1;
        
        apps->websites.push_back(info);
        
        String key_copy = push_string(&apps->names_arena, short_name);
        apps->website_ids.insert({key_copy, new_id});
        
        result = new_id;
    }
    else
    {
        result = apps->website_ids[short_name];
    }
    
    return result;
}

String 
get_app_name(App_List *apps, App_Id id)
{
    String result;
    
    u32 index = index_from_id(id);
    if (is_local_program(id))
    {
        result = apps->local_programs[index].short_name;
    }
    else if (is_website(id))
    {
        result = apps->websites[index].short_name;
    }
    else
    {
        result = make_string_from_literal(" ");
    }
    
    return result;
}

s32 
get_app_count(App_List *apps)
{
    s32 count = apps->local_programs.size() + apps->websites.size();
    Assert(count == (index_from_id(apps->next_website_id) + index_from_id(apps->next_program_id)));
    
    return count;
}

Record *
get_records_in_date_range(Day_View *day_view, u32 *record_count, 
                          date::sys_days start_range, date::sys_days end_range)
{
    Record *result = nullptr;
    
    // Dates should be in our days range
    int start_idx = -1;
    int end_idx = -1;
    for (int i = 0; i < day_view->days.size(); ++i)
    {
        if (start_range == day_view->days[i].date)
        {
            start_idx = i;
        }
        if (end_range == day_view->days[i].date)
        {
            end_idx = i;
            Assert(start_idx != -1);
            break;
        }
    }
    
    Assert(start_idx != -1 && end_idx != -1);
    
    s32 total_record_count = 0;
    for (int i = start_idx; i <= end_idx; ++i)
    {
        total_record_count += day_view->days[i].record_count;
    }
    
    if (total_record_count > 0)
    {
        Record *range_records = (Record *)xalloc(total_record_count * sizeof(Record));
        Record *deduplicated_records = (Record *)xalloc(total_record_count * sizeof(Record));
        
        Record *at = range_records;
        for (int i = start_idx; i <= end_idx; ++i)
        {
            memcpy(at, day_view->days[i].records, day_view->days[i].record_count * sizeof(Record));
            at += day_view->days[i].record_count;
        }
        
        // Sort by id
        std::sort(range_records, range_records + total_record_count, 
                  [](Record &a, Record &b) { return a.id < b.id; });
        
        // Merge same id records 
        deduplicated_records[0] = range_records[0];
        int unique_id_count = 1; 
        
        for (int i = 1; i < total_record_count; ++i)
        {
            if (range_records[i].id == deduplicated_records[unique_id_count-1].id)
            {
                deduplicated_records[unique_id_count-1].duration += range_records[i].duration;
            }
            else
            {
                deduplicated_records[unique_id_count] = range_records[i];
                unique_id_count += 1;
            }
        }
        
        // Sort by duration
        std::sort(deduplicated_records, deduplicated_records + unique_id_count, 
                  [](Record &a, Record &b) { return a.duration > b.duration; });
        
        free(range_records);
        
        *record_count = unique_id_count;
        return deduplicated_records;
    }
    
    return result;
}
