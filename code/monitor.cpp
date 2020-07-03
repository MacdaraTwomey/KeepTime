#include <unordered_map>

#include "cian.h"
#include "monitor_string.h"
#include "helper.h"
#include "graphics.h"
#include "icon.h"
#include "helper.h"
#include "helper.cpp"

#include "date.h"

static_assert(sizeof(date::sys_days) == sizeof(u32), "");
static_assert(sizeof(date::year_month_day) == sizeof(u32), "");

#include "monitor.h"
#include "platform.h"

#include "icon.cpp"    // Deals with win32 icon interface TODO: Move platform dependent parts to win
#include "network.cpp"
#include "ui.cpp"

#define ICON_SIZE 32

i32
load_icon_and_add_to_database(Database *database, Bitmap bitmap)
{
    // TODO: Either delete unused icons, or make more room
    Assert(database->icon_count < 200);
    
    i32 icon_index = database->icon_count;
    database->icon_count += 1;
    
    Icon_Asset *icon = &database->icons[icon_index];
    icon->texture_handle = opengl_create_texture(database, bitmap); // This binds
    icon->bitmap = bitmap;
    
    return icon_index;
}

Block *
new_block(Block *head)
{
    Block *block = (Block *)calloc(1, BLOCK_SIZE);
    block->next = head;
    block->count = 0;
    block->full = false;
    return block;
}

void
start_new_day(Day_List *day_list, date::sys_days date)
{
    // day points to next free space
    Day new_day = {};
    new_day.record_count = 0;
    new_day.date = date;
    
    if (day_list->blocks->count == BLOCK_MAX_RECORDS)
    {
        day_list->blocks = new_block(day_list->blocks);
        new_day.records = day_list->blocks->records;
    }
    else
    {
        new_day.records = &day_list->blocks->records[day_list->blocks->count];
    }
    
    day_list->days.push_back(new_day);
}

void
add_or_update_record(Day_List *day_list, App_Id id, time_type dt)
{
    // Assumes a day's records are sequential in memory
    // DEBUG: Record id can be 0, used to denote 'no program'
    Assert(day_list->days.size() > 0);
    
    Day *cur_day = &day_list->days.back();
    for (u32 i = 0; i < cur_day->record_count; ++i)
	{
		if (id == cur_day->records[i].id)
		{
			cur_day->records[i].duration += dt;
			return;
		}
	}
    
    // Add a new record 
    // If no room in current block and move days from one block to a new block (keeping them sequential)
    if (day_list->blocks->count + 1 > BLOCK_MAX_RECORDS)
    {
        Record *day_start_in_old_block = cur_day->records;
        
        // Update old block
        day_list->blocks->count -= cur_day->record_count;
        day_list->blocks->full = true;
        
        day_list->blocks = new_block(day_list->blocks);
        day_list->blocks->count += cur_day->record_count;
        
        // Copy old and a new records to new block
        cur_day->records = day_list->blocks->records;
        memcpy(cur_day->records, day_start_in_old_block, cur_day->record_count * sizeof(Record));
        cur_day->records += cur_day->record_count;
    }
    
    // Add new record to block
    cur_day->records[cur_day->record_count] = Record{ id, dt };
    cur_day->record_count += 1;
    day_list->blocks->count += 1;
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
    
    day_view.range_type = Range_Type_Daily;
    day_view.start_date = cur_day->date;
    day_view.end_date = cur_day->date;
    day_view.left_disabled = (day_view.days.size() == 1); // we are on the first and only day
    day_view.right_disabled = true;
    snprintf(day_view.date_label, array_count(day_view.date_label), "Today");
    
    return day_view;
}

void
set_day_view_range(Day_View *day_view, date::sys_days start_date, date::sys_days end_date)
{
#if 0
    // TODO:
    Assert(day_view->days.size() > 0);
    Assert(start_date >= end_date);
    
    i32 start_range = -1;
    i32 end_range = -1;
    
    if (start_date > day_view->days.back().date ||
        end_date < day_view->days[0].date)
    {
        day_view->has_days = false;
    }
    else
    {
        bool have_end = false;
        if (start_date < day_view->days[0].date)
        {
            start_range = 0;
        }
        if (end_date > day_view->days.back().date)
        {
            end_range = day_view->days.size() - 1;
            have_end = true;
        }
        
        for (i32 day_index = day_view->days.size() - 1; day_index >= 0; --day_index)
        {
            date::sys_days d = day_view->days[day_index].date;
            if (d <= end_date && !have_end)
            {
                end_range = day_index;
                have_end = true;
            }
            if (d <= start_date)
            {
                // We are guarenteed to have a smaller date, or have already set start_range to 0
                if (d < start_date) start_range = day_index + 1;
                else if (d == start_date) start_range = day_index;
                break;
            }
        }
        
        
        Assert(have_end);
        Assert(start_range != -1);
        Assert(end_range != -1);
        
        day_view->has_days = true;
    }
    
    day_view->start_date = start_date;
    day_view->end_date = end_date;
    day_view->start_range = start_range;
    day_view->end_range = end_range;
    //range_count = range_count;
#endif
}


void
free_day_view(Day_View *day_view)
{
    if (day_view->copy_of_current_days_records)
    {
        free(day_view->copy_of_current_days_records);
    }
    
    // TODO: Should I deallocate day_view->days memory? (probably)
    *day_view = {};
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

void
debug_add_records(Database *database, Day_List *day_list, date::sys_days cur_date)
{
    static char *names[] = {
        "C:\\Program Files\\Krita (x64)\\bin\\krita.exe",
        "C:\\Program Files\\Mozilla Firefox\\firefox.exe",
        "C:\\dev\\projects\\monitor\\build\\monitor.exe",
        "C:\\Program Files\\7-Zip\\7zFM.exe",  // normal 7z.exe had no icon
        "C:\\Qt\\5.12.2\\msvc2017_64\\bin\\designer.exe",
        "C:\\Qt\\5.12.2\\msvc2017_64\\bin\\assistant.exe",
        "C:\\Program Files (x86)\\Dropbox\\Client\\Dropbox.exe",
        "C:\\Program Files (x86)\\Vim\\vim80\\gvim.exe",
        "C:\\Windows\\notepad.exe",
        "C:\\Program Files\\CMake\\bin\\cmake-gui.exe",
        "C:\\Program Files\\Git\\git-bash.exe",
        "C:\\Program Files\\Malwarebytes\\Anti-Malware\\mbam.exe",
        "C:\\Program Files\\Sublime Text 3\\sublime_text.exe",
        "C:\\Program Files\\Typora\\bin\\typora.exe",
        "C:\\files\\applications\\cmder\\cmder.exe",
        "C:\\files\\applications\\4coder\\4ed.exe",
        "C:\\files\\applications\\Aseprite\\aseprite.exe",
        "C:\\dev\\projects\\shell\\shell.exe",  // No icon in executable just default windows icon showed in explorer, so can't load anything
        "C:\\Program Files\\Realtek\\Audio\\HDA\\RtkNGUI64.exe",
        "C:\\Program Files\\VideoLAN\\VLC\\vlc.exe",
        "C:\\Program Files\\Windows Defender\\MpCmdRun.exe",
        "C:\\Program Files\\Java\\jdk-11.0.2\\bin\\java.exe",
        "C:\\Program Files (x86)\\Internet Explorer\\iexplore.exe",
        "C:\\Program Files (x86)\\Meld\\Meld.exe",
        "C:\\Program Files (x86)\\MSI Afterburner\\MSIAfterburner.exe"
    };
    
    // Should be no days in day list
    Assert(day_list->days.size() == 0);
    
    
#include <stdlib.h> // rand (debug)
    
    auto rand_between = [](int low, int high) -> int {
        static bool first = true;
        if (first)
        {
            time_t time_now;
            srand((unsigned) time(&time_now));
            first = false;
        }
        
        float t = (float)rand() / (float)RAND_MAX;
        float result = round((1.0f - t) * low + t*high);
        return (int)result;
    };
    
    date::sys_days start  = cur_date - date::days{70};
    
    for (date::sys_days d = start; d < cur_date; d += date::days{1})
    {
        start_new_day(day_list, d);
        
        int n = rand_between(1, 6);
        for (int j = 0; j < n; ++j)
        {
            int name_idx = rand_between(0, array_count(names) - 1);
            int dt_microsecs = rand_between(0, MICROSECS_PER_SEC * 3);
            
            String full_path = make_string_size_cap(names[name_idx], strlen(names[name_idx]), strlen(names[name_idx]));
            String program_name = get_filename_from_path(full_path);
            Assert(program_name.length != 0);
            
            remove_extension(&program_name);
            Assert(program_name.length != 0);
            
            // string_to_lower(&program_name);
            
            App_Id record_id = 0;
            if (database->id_table.count(program_name) == 0)
            {
                App_Id new_id = make_id(database, Record_Exe);
                
                App_Info names;
                names.long_name = copy_alloc_string(full_path);
                names.short_name = copy_alloc_string(program_name);
                names.icon_index = -1;
                
                database->app_names.insert({new_id, names});
                
                // These don't share the same strings for now
                String name_2 = copy_alloc_string(program_name);
                database->id_table.insert({name_2, new_id});
                
                record_id = new_id;
            }
            else
            {
                // get record id
                record_id = database->id_table[program_name];
            }
            
            add_or_update_record(day_list, record_id, dt_microsecs);
        }
    }
}


void init_database(Database *database, date::sys_days current_date)
{
    *database = {};
    database->id_table = std::unordered_map<String, App_Id>(30);
    database->app_names = std::unordered_map<App_Id, App_Info>(80);
    
    database->next_program_id = 1;
    database->next_website_id = 1 << 31;
    
    database->firefox_id = 0;
    database->added_firefox = false;
    
    database->day_list = {};
    database->day_list.blocks = new_block(nullptr);
    
    // add fake days before current_date
    debug_add_records(database, &database->day_list, current_date);
    
    start_new_day(&database->day_list, current_date);
    
#if 0
    for (int i = 0; i < array_count(state->database.ms_icons); ++i)
    {
        HICON ico = LoadIconA(NULL, MAKEINTRESOURCE(32513 + i));
        Bitmap *bitmap = &state->database.ms_icons[i];
        win32_get_bitmap_data_from_HICON(ico, &bitmap->width, &bitmap->height, &bitmap->pitch, &bitmap->pixels);
    }
#endif
    
    // Load default icon, to use if we can't get an apps program
    Bitmap bitmap;
    // Dont need to DestroyIcon this
    HICON hicon = LoadIconA(NULL, IDI_ERROR); // TODO: Replace with IDI_APPLICATION
    if (hicon)
    {
        if (!win32_get_bitmap_data_from_HICON(hicon, &bitmap.width, &bitmap.height, &bitmap.pitch, &bitmap.pixels))
        {
            // If can't load OS icon make an background coloured icon to use as default
            init_bitmap(&bitmap, ICON_SIZE, ICON_SIZE);
        }
    }
    
    database->default_icon_index = load_icon_and_add_to_database(database, bitmap);
}


void add_keyword(std::vector<Keyword> &keywords, Database *database, char *str, App_Id id = 0)
{
    Assert(keywords.size() < MAX_KEYWORD_COUNT);
    Assert(strlen(str) < MAX_KEYWORD_SIZE);
    Assert(id == 0 || is_firefox(id));
    
    if (keywords.size() < MAX_KEYWORD_COUNT)
    {
        App_Id assigned_id = id != 0 ? id : make_id(database, Record_Firefox);
        
        Keyword k;
        k.str = copy_alloc_string(str);
        k.id = assigned_id; 
        keywords.push_back(k);
    }
}

Keyword *
search_url_for_keywords(String url, std::vector<Keyword> &keywords)
{
    // TODO: Keep looking through keywords for one that fits better (i.e docs.microsoft vs docs.microsoft/buttons).
    // TODO: Sort based on common substrings, or alphabetically, so we don't have to look further.
    for (i32 i = 0; i < keywords.size(); ++i)
    {
        Keyword *keyword = &keywords[i];
        if (search_for_substr(url, 0, keyword->str) != -1)
        {
			// TODO: When we match keyword move it to the front of array, 
			// maybe shuffle others down to avoid first and last being swapped and re-swapped repeatedly.
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

Icon_Asset *
get_icon_asset(Database *database, App_Id id)
{
    if (is_firefox(id))
    {
        // HACK: We just use firefox icon
        id = database->firefox_id;
        //get_favicon_from_website(url);
    }
    
    if (is_exe(id))
    {
        Assert(database->app_names.count(id) > 0);
        App_Info &program_info = database->app_names[id];
        if (program_info.icon_index == -1)
        {
            Assert(program_info.long_name.length > 0);
            Bitmap bitmap;
            bool success = platform_get_icon_from_executable(program_info.long_name.str, ICON_SIZE, 
                                                             &bitmap.width, &bitmap.height, &bitmap.pitch, &bitmap.pixels);
            if (success)
            {
                // TODO: Maybe mark old paths that couldn't get correct icon for deletion.
                program_info.icon_index = load_icon_and_add_to_database(database, bitmap);
                return database->icons + program_info.icon_index;
            }
        }
        else
        {
            Icon_Asset *icon = database->icons + program_info.icon_index;
            Assert(icon->texture_handle != 0);
            glBindTexture(GL_TEXTURE_2D, icon->texture_handle);
            return icon;
        }
    }
    
    // DEBUG a use default
    Icon_Asset *default_icon = database->icons + database->default_icon_index;
    Assert(default_icon->texture_handle != 0);
    glBindTexture(GL_TEXTURE_2D, default_icon->texture_handle);
    return default_icon;
}

// TODO: Do we need/want dt here, (but on error we would prefer to do nothing, and not return anything though)
// maybe want to split up update_days
App_Id
poll_windows(Database *database, Settings *settings)
{
    ZoneScoped;
    
    // TODO: Probably should just make everything a null terminated String as imgui, windows, curl all want null terminated
	App_Id record_id = 0;
    
    char buf[2000];
    size_t len = 0;
    
    Platform_Window window = platform_get_active_window();
    if (!window.is_valid) return 0;
    
    // If this failed it might be a desktop or other things like alt-tab screen or something
    bool got_path = platform_get_program_from_window(window, buf, &len);
    if (!got_path) return 0;
    
    String full_path = make_string_size_cap(buf, len, array_count(buf));
    String program_name = get_filename_from_path(full_path);
    if (program_name.length == 0) return 0;
    
    remove_extension(&program_name);
    if (program_name.length == 0) return 0;
    
    // string_to_lower(&program_name);
    
    
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
        bool got_url = platform_get_firefox_url(window, url_buf, array_count(url_buf), &url_len);
        if (got_url)
        {
            String url = make_string_size_cap(url_buf, url_len, array_count(url_buf));
            if (url.length > 0)
            {
                Keyword *keyword = search_url_for_keywords(url, settings->keywords);
                if (keyword)
                {
                    if (database->app_names.count(keyword->id) == 0)
                    {
                        App_Info names;
                        names.long_name = copy_alloc_string(url);
                        names.short_name = copy_alloc_string(keyword->str);
                        names.icon_index = -1;
                        
                        database->app_names.insert({keyword->id, names});
                    }
                    
					// If this url had a keyword then was removed, may be listed in day's records twice with different ids
                    record_id = keyword->id;
                    add_to_executables = false;
                }
            }
        }
    }
    
    // @Temporary, for when we get firefox website, before we have added firefox program
    if (program_is_firefox && !add_to_executables)
    {
        if (!database->added_firefox)
        {
            App_Id new_id = make_id(database, Record_Exe);
            
            App_Info names;
            names.long_name = copy_alloc_string(full_path);
            names.short_name = copy_alloc_string(program_name);
            names.icon_index = -1;
            
            database->app_names.insert({new_id, names});
            
            // These don't share the same strings for now
            String name_2 = copy_alloc_string(program_name);
            database->id_table.insert({name_2, new_id});
            
            database->firefox_id = new_id;
            database->added_firefox = true;
        }
    }
    
    if (add_to_executables)
    {
        if (database->id_table.count(program_name) == 0)
        {
            App_Id new_id = make_id(database, Record_Exe);
            
            App_Info names;
            names.long_name = copy_alloc_string(full_path);
            names.short_name = copy_alloc_string(program_name);
            names.icon_index = -1;
            
            database->app_names.insert({new_id, names});
            
            // These don't share the same strings for now
            String name_2 = copy_alloc_string(program_name);
            database->id_table.insert({name_2, new_id});
            
            record_id = new_id;
        }
        else
        {
            // get record id
            record_id = database->id_table[program_name];
        }
    }
    
	return record_id;
}

void
update(Monitor_State *state, SDL_Window *window, time_type dt, u32 window_status)
{
    //OPTICK_EVENT();
    ZoneScoped;
    
    date::sys_days current_date = get_localtime(); // + date::days{state->extra_days};;
    if (!state->is_initialised)
    {
        //remove(global_savefile_path);
        //remove(global_debug_savefile_path);
        
        *state = {};
        state->header = {};
        
        init_database(&state->database, current_date);
        
        Misc_Options options = {};
        options.day_start_time = 0;
        options.poll_start_time = 0;
        options.poll_end_time = 0;
        options.run_at_system_startup = true;
        options.poll_frequency_microseconds = 10000;
        
        state->settings.misc_options = options;
        
        // Keywords must be null terminated when given to platform gui
        add_keyword(state->settings.keywords, &state->database, "CITS3003");
        add_keyword(state->settings.keywords, &state->database, "youtube");
        add_keyword(state->settings.keywords, &state->database, "docs.microsoft");
        add_keyword(state->settings.keywords, &state->database, "google");
        add_keyword(state->settings.keywords, &state->database, "github");
        
        // maybe allow comma seperated keywords
        // https://www.hero.com/specials/hi&=yes
        // hero, specials
        
        
        
        state->accumulated_time = 0;
        state->total_runtime = 0;
        state->startup_time = win32_get_time();
        
        state->calendar_date_range_start.selected_date = current_date;
        state->calendar_date_range_start.first_day_of_month = current_date;
        state->calendar_date_range_end.selected_date = current_date;
        state->calendar_date_range_end.first_day_of_month = current_date;
        
        state->is_initialised = true;
    }
    
    Database *database = &state->database;
    
    // TODO: Just use google or duckduckgo service for now (look up how duckduckgo browser does it, ever since they switched from using their service...)
    
    if (window_status & Window_Just_Hidden)
    {
        // delete day view (if exists)
    }
    
    if (current_date != database->day_list.days.back().date)
    {
        start_new_day(&database->day_list, current_date);
    }
    
    // maybe start_new_day equivalent for day view (without adding records) just to keep track of current day for ui
    
    state->accumulated_time += dt;
    state->total_runtime += dt;
    time_type poll_window_freq = 10000;
    if (state->accumulated_time >= poll_window_freq)
    {
        App_Id id = poll_windows(database, &state->settings);
		if (id != 0)
		{
			add_or_update_record(&database->day_list, id, state->accumulated_time);
		}
        else
        {
            // debug to account for no time records
            add_or_update_record(&database->day_list, 0, state->accumulated_time);
        }
		
        state->accumulated_time = 0;
    }
    
    static Day_View day_view = {};
    if (window_status & Window_Just_Visible)
    {
        // Save a freeze frame of the currently saved days.
        
        // setup day view and 
        
        
        // sets start and end date and sets to daily currently
        // TODO: This should not default to daily, but keep the alst used range, even if the records are updated
        day_view = get_day_view(&database->day_list);
    }
    
    
    //date::sys_days start_date = current_date;
    //set_day_view_range(&day_view, start_date, current_date);
    
    if (window_status & Window_Visible)
    {
        // I think we are drawing more frames than monitor is eating (hence high cpu usage)?
        // But doesn't vsync sleep application on swap buffers, when it cant buffer any more frames
        draw_ui_and_update(window, state, database, current_date, &day_view);
    }
    
    //free_day_view(&day_view);
    
}

