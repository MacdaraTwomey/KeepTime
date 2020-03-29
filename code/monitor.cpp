#include "monitor.h"

void
start_new_day(Database *database, sys_days date)
{
    Assert(database);
    Assert(database->days);
    
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
    Assert(database);
    Assert(day_view);
    
    // Don't view the last day (it may still change)
    Assert(database->day_count > 0);
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
        Assert(0);
    }
}

void
destroy_day_view(Day_View *day_view)
{
    Assert(day_view);
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
    Assert(type != Record_Invalid);
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
    
    Assert(is_exe(id) || is_firefox(id));
    
    return id;
}

void add_keyword(Database *database, char *str)
{
    Assert(database->keyword_count < MaxKeywordCount);
    if (database->keyword_count < MaxKeywordCount)
    {
        // TODO: Handle deletion of keywords
        u32 id = make_id(database, Record_Firefox);
        Keyword *keyword = &database->keywords[database->keyword_count];
        
        Assert(keyword->str == 0 && keyword->id == 0);
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

Bitmap
get_icon_from_website(String url)
{
    // Url must be null terminated
    
    // mit very screwed
    // stack overflow, google has extra row on top?
    // onesearch.library.uwa.edu.au is 8 bit
    // teaching.csse.uwa.edu.au hangs forever, maybe because not SSL?
    // https://www.scotthyoung.com/ 8 bit bitmap, seems fully transparent, maybe aren't using AND mask right? FAIL
    // http://ukulelehunt.com/ not SSL, getting Success read 0 bytes
    // forum.ukuleleunderground.com 15x16 icon, seemed to render fine though...
    // onesearch has a biSizeImage = 0, and bitCount = 8, and did set the biClrUsed field to 256, when icons shouldn't
    // voidtools.com
    // getmusicbee.com
    
    // This is the site, must have /en-US on end
    // https://www.mozilla.org/en-US/ 8bpp seems the and mask is empty the way i'm trying to read it
    
    // lots of smaller websites dont have an icon under /favicon.ico, e.g. ukulele sites
    
    // maths doesn't seem to work out calcing and_mask size ourmachinery.com
    
    // https://craftinginterpreters.com/ is in BGR format just like youtube but stb image doesn't detect
    
    
    // TODO: Validate url and differentiate from user just typing in address bar
    
    
    //String url = make_string_from_literal("https://craftinginterpreters.com/");
    
    Bitmap favicon = {};
    Size_Data icon_file = {};
    bool request_success = request_icon_from_url(url, &icon_file);
    if (request_success)
    {
        tprint("Request success");
        favicon = decode_favicon_file(icon_file);
        free(icon_file.data);
    }
    else
    {
        tprint("Request failure");
    }
    
    if (!favicon.pixels)
    {
        favicon = make_bitmap(10, 10, RGBA(190, 25, 255, 255));
    }
    
    return favicon;
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
    
    Assert(day->record_count < MaxDailyRecords);
    day->records[day->record_count] = {id, dt};
    day->record_count += 1;
}


Bitmap *
get_icon_from_database(Database *database, u32 id)
{
    Assert(database);
    if (is_exe(id))
    {
        // TODO: Change this assert
        Assert(id < 200);
        
        if (database->icons[id].pixels)
        {
            Assert(database->icons[id].width > 0 && database->icons[id].pixels > 0);
            return &database->icons[id];
        }
        else
        {
            // Load bitmap on demand
            if (database->paths[id].path)
            {
                Bitmap *destination_icon = &database->icons[id];
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
        
        u32 index = id & ((1u << 31) - 1);
        if (index >= array_count(global_ms_icons))
        {
            return &global_ms_icons[0];
        }
        else
        {
            Bitmap *ms_icon = &global_ms_icons[index];
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
            Assert(!has_firefox);
            Assert(database->website_count == 0);
            temp = 1; // too see which assert triggers more clearly.
            Assert(0);
            return nullptr;
        }
#endif
    }
    
    return nullptr;
}



// TODO: Do we need/want dt here
void
poll_windows(Database *database, double dt)
{
    Assert(database);
    
    char buf[2000];
    size_t len = 0;
    Platform_Window window = platform_get_active_window();
    if (!window.is_valid) return;
    
    bool success = platform_get_active_program(window, buf, &len);
    if (!success) return;
    
    char *name_start         = get_filename_from_path(info->full_path);
    size_t  program_name_len = strlen(name_start);
    
    for (int i = 0; i < full_path_len; ++i)
    {
        info->full_path[i] = tolower(info->full_path[i]);
    }
    
    Assert(program_name_len > 4 && strcmp(name_start + (program_name_len - 4), ".exe") == 0);
    
    program_name_len -= 4;
    memcpy(info->program_name, name_start, program_name_len); // Don't copy '.exe' from end
    info->program_name[program_name_len] = '\0';
    
    
    // If this is a good feature then just pass in created HWND and test
    // if (strcmp(winndow_info->full_path, "c:\\dev\\projects\\monitor\\build\\monitor.exe") == 0) return;
    
    u32 record_id = 0;
    
    bool program_is_firefox = (strcmp(window_info.program_name, "firefox") == 0);
    bool add_to_executables = !program_is_firefox;
    
    if (program_is_firefox)
    {
        // TODO: Maybe cache last url to quickly get record without comparing with keywords, retrieving record etc
        
        // TODO: If we get a URL but it has no url features like www. or https:// etc
        // it is probably someone just writing into the url bar, and we don't want to save these
        // as urls.
        char url[2100];
        size_t url_len = 0;
        
        bool keyword_match = false;
        Keyword *keyword = find_keywords(url, database->keywords, database->keyword_count);
        if (keyword)
        {
            bool website_is_stored = false;
            for (i32 website_index = 0; website_index < database->website_count; ++website_index)
            {
                if (database->websites[website_index].id == keyword->id)
                {
                    website_is_stored = true;
                    break;
                }
            }
            
            if (!website_is_stored)
            {
                Assert(database->website_count < MaxWebsiteCount);
                
                Website *website = &database->websites[database->website_count];
                website->id = keyword->id;
                website->website_name = clone_string(keyword->str);
                
                database->website_count += 1;
            }
            
            record_id = keyword->id;
            
            // HACK: We would like to treat websites and programs the same and just index their icons
            // in a smarter way maybe.
            if (!database->firefox_path.updated_recently)
            {
                if (database->firefox_path.path) free(database->firefox_path.path);
                
                database->firefox_path.path = clone_string(window_info.full_path, window_info.full_path_len);
                database->firefox_path.updated_recently = true;
                database->firefox_id = keyword->id;
                
            }
        }
        else
        {
            add_to_executables = true;
        }
        
        if (!database->firefox_icon.pixels)
        {
            char *firefox_path = window_info.full_path;
            database->firefox_icon = {};
            bool success = get_icon_from_executable(firefox_path, ICON_SIZE, &database->firefox_icon, true);
            Assert(success);
        }
    }
    
    if (add_to_executables)
    {
        u32 temp_id = 0;
        bool in_table = database->all_programs.search(window_info.program_name, &temp_id);
        if (!in_table)
        {
            temp_id = make_id(database, Record_Exe);
            database->all_programs.add_item(window_info.program_name, temp_id);
        }
        
        record_id = temp_id;
        
        // TODO: This is similar to what happens to firefox path and icon, needs collapsing.
        Assert(!(temp_id & (1 << 31)));
        if (!in_table || (in_table && !database->paths[temp_id].updated_recently))
        {
            
            if (in_table && database->paths[temp_id].path)
            {
                free(database->paths[temp_id].path);
            }
            database->paths[temp_id].path = clone_string(window_info.full_path);
            database->paths[temp_id].updated_recently = true;
        }
    }
    
    Assert(database->day_count > 0);
    Day *current_day = &database->days[database->day_count-1];
    
    update_days_records(current_day, record_id, dt);
}


struct
Monitor_State
{
    bool is_initialised;
    Header header;
    Database database;
    Day_View day_view;
    Font font;
    Favicon favicon;
}


void
update_and_render(Monitor_State *state, Bitmap *screen_buffer, float dt)
{
    if (!state->is_initialised)
    {
        //remove(global_savefile_path);
        //remove(global_debug_savefile_path);
        
        state->header = {};
        
        state->database = {};
        init_database(&state->database);
        add_keyword(&state->database, "CITS3003");
        add_keyword(&state->database, "youtube");
        add_keyword(&state->database, "docs.microsoft");
        add_keyword(&state->database, "eso-community");
        
        start_new_day(&state->database, floor<date::days>(System_Clock::now()));
        
        state->day_view = {};
        
        state->font = create_font("c:\\windows\\fonts\\times.ttf", 28);
        //state->favicon = get_icon_from_website();
        
        state->is_initialised = true;
    }
    
    
    // speed up time!!!
    // int seconds_per_day = 5;
    //sys_days current_date = floor<date::days>(System_Clock::now()) + date::days{(int)duration_accumulator / seconds_per_day};
    
    sys_days current_date = floor<date::days>(System_Clock::now());
    if (current_date != state->database.days[state->database.day_count-1].date)
    {
        start_new_day(&state->database, current_date);
    }
    
    if (ui_timer_elapsed())
    {
        poll_windows(&state->database, dt);
    }
    
#if 0
    if (gui_opened)
    {
        // Save a freeze frame of the currently saved days.
        init_day_view(&state->database, &day_view);
    }
    if (gui_closed)
    {
        destroy_day_view(&state->day_view);
    }
#endif
    init_day_view(&database, &state->day_view);
    
    if (button_clicked)
    {
        Button button_clicked = Button_Day;
        
        i32 period =
            (button_clicked == Button_Day) ? 1 :
        (button_clicked == Button_Week) ? 7 : 30;
        
        set_range(&state->day_view, period, current_date);
    }
    
    
    if (gui_visible)
    {
        // TODO: Move to system where we more tell renderer what to draw
        // this would also stop needing to get_icon__from_database in render
        
        render_gui(global_screen_buffer, &state->database, &state->day_view, &font);
    }
    
    destroy_day_view(&state->day_view);
    
    if (state->favicon.pixels)
    {
        draw_simple_bitmap(global_screen_buffer, state->favicon, 200, 200);
    }
}