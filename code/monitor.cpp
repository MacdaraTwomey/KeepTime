#include <unordered_map>

#include "monitor_string.h"
#include "helper.h"


#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "graphics.h"
#include "icon.h"

#include "helper.h"
#include "helper.cpp"

#include "date.h"

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");

#include "monitor.h"
#include "platform.h"

#include "draw.cpp"
#include "bitmap.cpp"
#include "icon.cpp"    // Deals with win32 icon interface TODO: Move platform dependent parts to win


#include "ui.h"
#include "ui.cpp"

#include "network.cpp"


void
start_new_day(Database *database, date::sys_days date)
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
set_range(Day_View *day_view, i32 period, date::sys_days current_date)
{
    // From start date until today
    if (day_view->day_count > 0)
    {
        date::sys_days start_date = current_date - date::days{period - 1};
        
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
    database->programs = std::unordered_map<String, Program_Id>(30);
    database->program_paths = std::unordered_map<Program_Id, Program_Paths>(30);
    database->websites = std::unordered_map<Program_Id, String>(50);
    
    // TODO: Maybe disallow 0
    database->next_program_id = 0;
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
        id = database->next_program_id;
        database->next_program_id += 1;
    }
    else if (type == Record_Firefox)
    {
        id = database->next_website_id;
        database->next_website_id += 1;
    }
    
    Assert(is_exe(id) || is_firefox(id));
    
    return id;
}

void add_keyword(Database *database, String keyword_str)
{
    Assert(database->keyword_count < MaxKeywordCount);
    if (database->keyword_count < MaxKeywordCount)
    {
        // TODO: Handle deletion of keywords
        Program_Id id = make_id(database, Record_Firefox);
        Keyword *keyword = &database->keywords[database->keyword_count];
        
        Assert(keyword->str.str == 0 && keyword->id == 0);
        keyword->id = id;
        
        // TODO Give database/tables their own allocator
        // For now this string just owns it's own memory
        keyword->str = copy_alloc_string(keyword_str);
        
        database->keyword_count += 1;
    }
}

Keyword *
search_url_for_keywords(String url, Keyword *keywords, i32 keyword_count)
{
    // TODO: Keep looking through keywords for one that fits better (i.e docs.microsoft vs docs.microsoft/buttons).
    // TODO: Sort based on common substrings so we don't have to look further.
    for (i32 i = 0; i < keyword_count; ++i)
    {
        Keyword *keyword = &keywords[i];
        if (search_for_substr(url, 0, keyword->str) != -1)
        {
            return keyword;
        }
    }
    
    return nullptr;
}

Bitmap
get_favicon_from_website(String url)
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
    Size_Mem icon_file = {};
    bool request_success = request_favicon_from_website(url, &icon_file);
    if (request_success)
    {
        tprint("Request success");
        favicon = decode_favicon_file(icon_file);
        free(icon_file.memory);
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
        if (id == day->records[i].id)
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
        if (database->icons[id].pixels)
        {
            Assert(database->icons[id].width > 0 && database->icons[id].pixels > 0);
            return &database->icons[id];
        }
        else
        {
            // Load bitmap on demand
            if  (database->program_paths.count(id) > 0)
            {
                String path = database->program_paths[id].full_path;
                if (path.length > 0)
                {
                    Bitmap *destination_icon = &database->icons[id];
                    bool success = get_icon_from_executable(path, ICON_SIZE, destination_icon, true);
                    if (success)
                    {
                        // TODO: Maybe mark old paths that couldn't get correct icon for deletion.
                        return destination_icon;
                    }
                }
            }
        }
        
        // use default
        u32 index = id % array_count(database->ms_icons);
        Bitmap *ms_icon = &database->ms_icons[index];
        if (ms_icon->pixels) return ms_icon;
    }
    else
    {
        u32 index = id & ((1u << 31) - 1) % array_count(database->ms_icons);
        Bitmap *ms_icon = &database->ms_icons[index];
        if (ms_icon->pixels) return ms_icon;
    }
    
    return nullptr;
}

// TODO: Do we need/want dt here,
// maybe want to split up update_days
void
poll_windows(Database *database, double dt)
{
    char buf[2000];
    size_t len = 0;
    
    Platform_Window window = platform_get_active_window();
    if (!window.is_valid) return;
    
    // If this failed it might be a desktop or other things like alt-tab screen or something
    bool got_path = platform_get_program_from_window(window, buf, &len);
    if (!got_path) return;
    
    String full_path = make_string_size_cap(buf, len, array_count(buf));
    String program_name   = get_filename_from_path(full_path);
    if (program_name.length == 0) return;
    
    remove_extension(&program_name);
    if (program_name.length == 0) return;
    
    string_to_lower(&program_name);
    
    // Maybe disallow 0 as a valid id so this initialisation makes more sense
    Program_Id record_id = 0;
    
    bool program_is_firefox = string_equals(program_name, "firefox");
    bool add_to_executables = true;
    
    if (program_is_firefox)
    {
        // Get url
        // match against keywords
        // add to website if match, else add to programs as 'firefox' program
        
        // TODO: Maybe cache last url to quickly get record without comparing with keywords, retrieving record etc
        // We wan't to detect, incomplete urls and people just typing garbage into url bar, but
        // urls can validly have no www. or https:// and still be valid.
        char url_buf[2000];
        size_t url_len = 0;
        //bool got_url = platform_get_firefox_url(window, url_buf, &url_len);
        bool got_url = platform_get_firefox_url2(window, url_buf, array_count(url_buf), &url_len);
        if (got_url)
        {
            String url = make_string_size_cap(buf, url_len, array_count(url_buf));
            if (url.length > 0)
            {
                Keyword *keyword = search_url_for_keywords(url, database->keywords, database->keyword_count);
                if (keyword)
                {
                    if (database->websites.count(keyword->id) == 0)
                    {
                        Assert(database->websites.size() < MaxWebsiteCount);
                        String s = copy_alloc_string(url);
                        database->websites.insert({keyword->id, s});
                    }
                    else
                    {
                        // get record id
                        record_id = keyword->id;
                    }
                    
                    
                    record_id = keyword->id;
                    add_to_executables = false;
                }
            }
        }
    }
    
    if (add_to_executables)
    {
        if (database->programs.count(program_name) == 0)
        {
            // These are null terminated
            String name = copy_alloc_string(program_name);
            String path = copy_alloc_string(full_path);
            Program_Id new_id = make_id(database, Record_Exe);
            
            Program_Paths paths = {};
            paths.full_path = full_path;
            paths.name = name;
            
            database->program_paths.insert({new_id, paths});
            
            // These don't share the same strings for now
            String name_2 = copy_alloc_string(program_name);
            database->programs.insert({name_2, new_id});
            
            record_id = new_id;
        }
        else
        {
            // get record id
            record_id = database->programs[program_name];
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
    Bitmap favicon;
    time_type accumulated_time;
};


void
update(Monitor_State *state, Bitmap *screen_buffer, time_type dt)
{
    if (!state->is_initialised)
    {
        //remove(global_savefile_path);
        //remove(global_debug_savefile_path);
        
        state->header = {};
        
        state->database = {};
        init_database(&state->database);
        add_keyword(&state->database, make_string_from_literal("CITS3003"));
        add_keyword(&state->database, make_string_from_literal("youtube"));
        add_keyword(&state->database, make_string_from_literal("docs.microsoft"));
        add_keyword(&state->database, make_string_from_literal("eso-community"));
        
        start_new_day(&state->database, floor<date::days>(System_Clock::now()));
        
        state->day_view = {};
        
        //state->favicon = get_icon_from_website();
        
        state->accumulated_time = dt;
        
        // TODO: ui gets font instead of state
        state->font = create_font("c:\\windows\\fonts\\times.ttf", 28);
        ui_init(screen_buffer, &state->font);
        
        for (int i = 0; i < array_count(state->database.ms_icons); ++i)
        {
            HICON ico = LoadIconA(NULL, MAKEINTRESOURCE(32513 + i));
            state->database.ms_icons[i] = get_icon_bitmap(ico);
        }
        
        
        state->is_initialised = true;
    }
    // TODO: Separate platform and icon code in icon.cpp
    
    date::sys_days current_date = floor<date::days>(System_Clock::now());
    if (current_date != state->database.days[state->database.day_count-1].date)
    {
        start_new_day(&state->database, current_date);
    }
    
    // Maybe better if this is all integer arithmetic
    state->accumulated_time = dt;
    float poll_window_freq = 100; // ms
    if (state->accumulated_time >= poll_window_freq)
    {
        state->accumulated_time -= poll_window_freq;
    }
    
    poll_windows(&state->database, dt);
    
    if (ui_was_shown())
    {
        // Save a freeze frame of the currently saved days.
    }
    
    init_day_view(&state->database, &state->day_view);
    
    draw_rectangle(screen_buffer, 0, 0, screen_buffer->width, screen_buffer->height, GREY(255));
    
    ui_begin();
    
    {
        static i32 period = 1;
        ui_button_row_begin(270, 30, 100);
        
        if (ui_button("Day"))
        {
            period = 1;
        }
        if (ui_button("Week"))
        {
            period = 7;
        }
        if (ui_button("Month"))
        {
            period = 30;
        }
        
        set_range(&state->day_view, period, current_date);
        
        ui_button_row_end();
    }
    
    {
        Assert(state->day_view.day_count > 0);
        Day *today = state->day_view.days[state->day_view.day_count-1];
        
        if (today->record_count >= 0)
        {
            i32 record_count = today->record_count;
            
            Program_Record sorted_records[MaxDailyRecords];
            memcpy(sorted_records, today->records, sizeof(Program_Record) * record_count);
            
            std::sort(sorted_records, sorted_records + record_count, [](Program_Record &a, Program_Record &b){ return a.duration > b.duration; });
            
            double max_duration = sorted_records[0].duration;
            
            ui_graph_begin(270, 80, 650, 400, gen_id());
            for (i32 i = 0; i < record_count; ++i)
            {
                Program_Record &record = sorted_records[i];
                float length = (record.duration / max_duration);
                length += 0.1f; // bump factor
                if (length > 1.0f) length = 1.0f;
                
                char text[512];
                snprintf(text, array_count(text), "%.2lfs", record.duration);
#if 0
                if (record.duration < 60.0f)
                {
                    // Seconds
                }
                else if(record.duration < 3600.0f)
                {
                    // Minutes
                    snprintf(text, array_count(text), "%.0lfm", record.duration/60);
                }
                else
                {
                    // Hours
                    snprintf(text, array_count(text), "%.0lfh", record.duration/3600);
                }
#endif
                
                Bitmap *icon = get_icon_from_database(&state->database, record.id);;
                ui_graph_bar(length, text, icon); // pass bitmap and text too?
            }
            ui_graph_end();
        }
    }
    
    ui_end();
    
    destroy_day_view(&state->day_view);
    
    if (ui_was_hidden())
    {
    }
}