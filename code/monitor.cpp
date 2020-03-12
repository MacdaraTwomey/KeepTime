#include "monitor.h"


void 
start_new_day(Database *database, sys_days date)
{
    rvl_assert(database);
    rvl_assert(database->days);
    
    Day *days = database->days;
    
    Day new_day = {};
    new_day.records = (Program_Record *)xalloc(sizeof(Program_Record) * MaxDailyRecords);
    new_day.record_count = 0;
    new_day.date = date;
    
    database->day_count += 1;
    database->days[database->day_count-1] = new_day;
}


void
init_day_view(Database *database, Day_View *day_view)
{
    rvl_assert(database);
    rvl_assert(day_view);
    
    // Don't view the last day (it may still change)
    rvl_assert(database->day_count > 0);
    i32 immutable_day_count = database->day_count-1;
    for (i32 i = 0; i < immutable_day_count; ++i)
    {
        day_view->days[i] = &database->days[i];
    }
    
    Day &current_mutable_day = database->days[database->day_count-1];
    
    day_view->last_day_ = current_mutable_day;
    
    // Copy the state of the current day
    if (current_mutable_day.record_count > 0)
    {
        Program_Record *records = (Program_Record *)xalloc(sizeof(Program_Record) * current_mutable_day.record_count);
        memcpy(records, 
               current_mutable_day.records, 
               sizeof(Program_Record) * current_mutable_day.record_count);
        day_view->last_day_.records = records;
    }
    else
    {
        day_view->last_day_.records = nullptr;
    }
    
    day_view->day_count = database->day_count;
    
    // Point last day at copied day owned by view
    // View must be passed by reference for this to be used
    day_view->days[database->day_count-1] = &day_view->last_day_;
    
    day_view->start_range = 0;
    day_view->range_count = day_view->day_count;
    day_view->accumulate = false;
}

// TODO: Consider reworking this
void
set_range(Day_View *day_view, i32 period, sys_days current_date)
{
    // From start date until today
    if (day_view->day_count > 0)
    {
        sys_days start_date = current_date - date::days{period - 1};
        
        i32 end_range = day_view->day_count-1;
        i32 start_range = end_range;
        i32 start_search = std::max(0, end_range - (period-1));
        
        i32 range_count = 1;
        for (i32 day_index = start_search; day_index <= end_range; ++day_index)
        {
            if (day_view->days[day_index]->date >= start_date)
            {
                start_range = day_index;
                break;
            }
        }
        
        day_view->start_range = start_range;
        day_view->range_count = range_count;
        day_view->accumulate = (period == 1) ? false : true;
    }
    else
    {
        rvl_assert(0);
    }
}

void 
destroy_day_view(Day_View *day_view)
{
    rvl_assert(day_view);
    if (day_view->last_day_.records)
    {
        free(day_view->last_day_.records);
    }
    *day_view = {};
}


void init_database(Database *database)
{
    *database = {};
    init_hash_table(&database->all_programs, 30);
    
    database->next_exe_id = 0;
    database->next_website_id = 1 << 31;
}


bool is_exe(u32 id)
{
    return !(id & (1 << 31));
} 

bool is_firefox(u32 id)
{
    return !is_exe(id);
}

u32 make_id(Database *database, Record_Type type)
{
    rvl_assert(type != Record_Invalid);
    u32 id = 0;
    
    if (type == Record_Exe)
    {
        // No high bit set
        id = database->next_exe_id;
        database->next_exe_id += 1;
    }
    else if (type == Record_Firefox)
    {
        id = database->next_website_id;
        database->next_website_id += 1;
    }
    
    rvl_assert(is_exe(id) || is_firefox(id));
    
    return id;
}

void add_keyword(Database *database, char *str)
{
    rvl_assert(database->keyword_count < MaxKeywordCount);
    if (database->keyword_count < MaxKeywordCount)
    {
        // TODO: Handle deletion of keywords
        u32 id = make_id(database, Record_Firefox);
        Keyword *keyword = &database->keywords[database->keyword_count];
        
        rvl_assert(keyword->str == 0 && keyword->id == 0);
        keyword->id = id;
        keyword->str = clone_string(str);
        
        database->keyword_count += 1;
    }
}

Keyword *
find_keywords(char *url, Keyword *keywords, i32 keyword_count)
{
    // TODO: Keep looking through keywords for one that fits better (i.e docs.microsoft vs docs.microsoft/buttons).
    // TODO: Sort based on common substrings so we don't have to look further.
    for (i32 i = 0; i < keyword_count; ++i)
    {
        Keyword *keyword = &keywords[i];
        char *sub_str = strstr(url, keyword->str);
        if (sub_str) 
        {
            return keyword;
        }
    }
    
    return nullptr;
}

void
update_days_records(Day *day, u32 id, double dt)
{
    for (u32 i = 0; i < day->record_count; ++i)
    {
        if (id == day->records[i].ID)
        {
            day->records[i].duration += dt;
            return;
        }
    }
    
    rvl_assert(day->record_count < MaxDailyRecords);
    day->records[day->record_count] = {id, dt};
    day->record_count += 1;
}


Simple_Bitmap *
get_icon_from_database(Database *database, u32 id)
{
    rvl_assert(database);
    if (is_exe(id))
    {
        // TODO: Change this assert
        rvl_assert(id < 200);
        
        if (database->icons[id].pixels)
        {
            rvl_assert(database->icons[id].width > 0 && database->icons[id].pixels > 0);
            return &database->icons[id];
        }
        else
        {
            // Load bitmap on demand
            if (database->paths[id].path)
            {
                Simple_Bitmap *destination_icon = &database->icons[id];
                bool success = get_icon_from_executable(database->paths[id].path, ICON_SIZE, destination_icon, true);
                if (success)
                {
                    // TODO: Maybe mark old paths that couldn't get correct icon for deletion.
                    return destination_icon;
                }
            }
        }
    }
    else
    {
#if 1
        
        u32 index = id & ((1 << 31) - 1);
        if (index >= array_count(global_ms_icons))
        {
            return &global_ms_icons[0];
        }
        else
        {
            Simple_Bitmap *ms_icon = &global_ms_icons[index];
            return ms_icon;
        }
#else
        if (database->firefox_icon.pixels)
        {
            return &database->firefox_icon;
        }
        else
        {
            // TODO: @Cleanup: This is debug
            u32 temp;
            bool has_firefox = database->all_programs.search("firefox", &temp); 
            
            // should not have gotten firefox, else we would have saved its icon
            rvl_assert(!has_firefox);
            rvl_assert(database->website_count == 0);
            temp = 1; // too see which assert triggers more clearly.
            rvl_assert(0);
            return nullptr;
        }
#endif
    }
    
    return nullptr;
}
