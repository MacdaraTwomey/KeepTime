#include <unordered_map>

#include "cian.h"
#include "monitor_string.h"
#include "helper.h"
#include "graphics.h"
#include "icon.h"
#include "helper.h"
#include "helper.cpp"

#include "date.h"

#include "monitor.h"
#include "platform.h"

#include "icon.cpp"    // Deals with win32 icon interface TODO: Move platform dependent parts to win
#include "network.cpp"
#include "ui.cpp"

#define ICON_SIZE 32


bool is_local_program(App_Id id)
{
    return !(id & (1 << 31));
}

bool is_website(App_Id id)
{
    return !is_local_program(id);
}

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

Icon_Asset *
get_icon_asset(Database *database, App_Id id)
{
    if (is_website(id))
    {
        // We should use font id, when no website icon
        //get_favicon_from_website(url);
        
        Assert(0); // no firefox icons for now
        return nullptr;
    }
    
    if (is_local_program(id))
    {
        Assert(database->app_names.count(id) > 0);
        App_Info &program_info = database->app_names[id];
        if (program_info.icon_index == -1)
        {
            Assert(program_info.full_name.length > 0);
            Bitmap bitmap;
            bool success = platform_get_icon_from_executable(program_info.full_name.str, ICON_SIZE, 
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
            return icon;
        }
    }
    
    // Use default OS icon for application
    Icon_Asset *default_icon = database->icons + database->default_icon_index;
    Assert(default_icon->texture_handle != 0);
    return default_icon;
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
    
    return day_view;
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

u32 make_id(Database *database, Id_Type type)
{
    Assert(type != Id_Invalid);
    u32 id = 0;
    
    if (type == Id_LocalProgram)
    {
        // No high bit set
        id = database->next_program_id;
        database->next_program_id += 1;
    }
    else if (type == Id_Website)
    {
        id = database->next_website_id;
        database->next_website_id += 1;
    }
    
    Assert(is_local_program(id) || is_website(id));
    
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
            
            // forward declare
            App_Id get_id_and_add_if_new(Database *database, String key, String full_name, Id_Type id_type);
            
            
            App_Id record_id = get_id_and_add_if_new(database, program_name, full_path, Id_LocalProgram);
            add_or_update_record(day_list, record_id, dt_microsecs);
        }
    }
}

void init_database(Database *database, date::sys_days current_date)
{
    *database = {};
    database->local_programs = std::unordered_map<String, App_Id>(30);
    database->domain_names = std::unordered_map<String, App_Id>(30);
    database->app_names = std::unordered_map<App_Id, App_Info>(80);
    
    size_t size = Kilobytes(50);
    char *buffer = (char *)malloc(size);
    Assert(buffer);
    init_arena(&database->names_arena, buffer, size);
    
    database->next_program_id = 1;
    database->next_website_id = 1 << 31;
    
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


void 
add_keyword(Arena *arena, Array<String, MAX_KEYWORD_COUNT> &keywords, char *str)
{
    Assert(strlen(str) < MAX_KEYWORD_SIZE);
    String k = push_string(arena, str);
    keywords.add_item(k);
}

bool
string_matches_keyword(String string, std::vector<String> &keywords)
{
    // TODO: Sort based on common substrings, or alphabetically, so we don't have to look further.
    for (i32 i = 0; i < keywords.size(); ++i)
    {
        if (search_for_substr(string, 0, keywords[i]) != -1)
        {
			// TODO: When we match keyword move it to the front of array, 
			// maybe shuffle others down to avoid first and last being swapped and re-swapped repeatedly.
            return true;
        }
    }
    
    return false;
}

size_t
scheme_length(String url)
{
    // "A URL-scheme string must be one ASCII alpha, followed by zero or more of ASCII alphanumeric, U+002B (+), U+002D (-), and U+002E (.)" - https://url.spec.whatwg.org/#url-scheme-string
    // such as http, file, dict
    // can have file:///c:/ in which case host will still have /c:/ in front of it, but this is fine we only handle http(s) type urls
    
    size_t len = 0;
    for (size_t i = 0; i < url.length - 2; ++i)
    {
        char c = url.str[i];
        if (c == ':' && url.str[i+1] == '/' && url.str[i+2] == '/')
        {
            len = i + 3;
            break;
        }
        
        if (!(is_upper(c) || is_lower(c) || c == '+' || c == '-' || c == '.'))
        {
            // non_valid scheme string character
            break;
        }
    }
    
    return len;
}

String
extract_domain_name(String url)
{
    // Other good reference: https://github.com/curl/curl/blob/master/lib/urlapi.c
    
    // From wikipedia: (where [component] means optional)
    
    // URI       = scheme:[//authority]path[?query][#fragment]
    // authority = [userinfo@]host[:port]
    
    // host must be a hostname or a IP address
    // IPv4 addresses must be in dot-decimal notation, and IPv6 addresses must be enclosed in brackets ([]).
    
    // A path may be empty consisting of two slashes (//)
    // If an authority component is absent, then the path cannot begin with an empty segment, that is with two slashes (//), because it could be confused with an authority.
    
    // UTF8 URLs are possible
    // Web and Internet software automatically convert the __domain name__ into punycode usable by the Domain Name System
    // http://'Chinese symbols' becomes http://xn--fsqu00a.xn--3lr804guic/. 
    // The xn-- indicates that the character was not originally ASCII
    
    // The __path name__ can also be encoded using percent encoding, but we don't care about path component here
    
    // NOTE: URL could just be gibberish (page wan't loaded and user was just typing in url bar)
    
    // path slash after host can also be omitted and instead end in query '?' like  http://www.url.com?id=2380
    // This is not valid but may occur and seems to be accepted by broswer
    
    // Browsers also accept multiple slashes where there should be one in path
    
    // If this finds a port number or username password or something, when there is no http://, then it just wont satisfy other double-slash condition.
    
    // We don't handle weird urls very well, or file:// 
    size_t start = scheme_length(url);
    
    // Look for end of host
    s32 slash_end = search_for_char(url, start, '/');
    s32 question_mark_end = search_for_char(url, start, '?');
    if (slash_end == -1) slash_end = url.length;
    if (question_mark_end == -1) question_mark_end = url.length;
    
    s32 end = std::min(slash_end, question_mark_end) - 1; // -1 to get last char of url or one before / or ?
    
    String authority = substr_range(url, start, end);
    if (authority.length > 0)
    {
        // If authority has a '@' (or ':') it has a userinfo (or port) before (or after) it
        
        // userinfo followed by at symbol
        s32 host_start = 0;
        s32 at_symbol = search_for_char(authority, host_start, '@');
        if (at_symbol != -1) host_start = at_symbol + 1;
        
        // port preceded by colon
        s32 host_end = authority.length - 1;
        s32 port_colon = search_for_char(authority, host_start, ':');
        if (port_colon != -1) host_end = port_colon - 1;
        
        if (host_end - host_start >= 1)
        {
            // Characters of host not checked (doesn't matter because if doesn't match keyword it won't be recorded anyway)
            return substr_range(authority, host_start, host_end);
        }
    }
    
    String result;
    result.str = url.str;
    result.capacity = 0;
    result.length = 0;
    
    return result;
}

App_Id
get_id_and_add_if_new(Database *database, String key, String full_name, Id_Type id_type)
{
    Assert(id_type == Id_LocalProgram || id_type == Id_Website);
    
    std::unordered_map<String, App_Id> &table = 
        (id_type == Id_LocalProgram) ? database->local_programs : database->domain_names;
    
    App_Id result = Id_Invalid;
    
    if (table.count(key) == 0)
    {
        App_Id new_id = make_id(database, id_type);
        
        App_Info app_info;
        app_info.short_name = copy_alloc_string(key);
        app_info.full_name = copy_alloc_string(full_name);
        app_info.icon_index = -1;
        
        database->app_names.insert({new_id, app_info});
        
        String key_copy = copy_alloc_string(key);
        table.insert({key_copy, new_id});
        
        result = new_id;
    }
    else
    {
        result = table[key];
        Assert(database->app_names.count(result) > 0);
    }
    
    return result;
}

#if 0
App_Id
get_id_and_add_if_new(Database *database, String key, String full_name, Id_Type id_type)
{
    // TODO: Make hash tables have their own arena for strings maybe, and make short_name point into full_name for locals and for website just alloc short_name. because imgui can take length string, not null terminated
    
    // TODO: Could just alloc 1 megabyte + sizeof strings in file at startup, and do a realloc + fixing all the pointers if we need a rare resize
    
    // or could store indexes (only difference is that the mov instruction just has to add offset into its index, after loading offset into a register), so no real difference.
    
    // TODO: String of shortname can point into string of full_name for paths
    
    // if using custom hash table impl can use open addressing, and since nothing is ever deleted don't need a occupancy flag or whatever can just use id == 0 or string == null to denote empty.
    
    Assert(id_type == Id_LocalProgram || id_type == Id_Website);
    
    std::unordered_map<String, App_Id> &table = 
        (id_type == Id_LocalProgram) ? database->local_programs : database->domain_names;
    
    App_Id result = Id_Invalid;
    
    if (table.count(key) == 0)
    {
        App_Id new_id = make_id(database, id_type);
        
        String key_copy = push_string(&database->names_arena, key);
        
        App_Info app_info;
        app_info.short_name = key_copy;
        app_info.icon_index = -1;
        
#if MONITOR_DEBUG
        app_info.full_name = push_string(&database->names_arena, full_name); 
#else
        if (id_type == Id_LocalProgram)
        {
            app_info.full_name = push_string(&database->names_arena, full_name); 
        }
        else
        {
            app_info.full_name = String{" ", 0, 0};
        }
#endif
        
        database->app_names.insert({new_id, app_info});
        
        // Shares string with app_info struct
        table.insert({key_copy, new_id});
        
        result = new_id;
    }
    else
    {
        result = table[key];
        Assert(database->app_names.count(result) > 0);
    }
    
    return result;
}
#endif
#if 0
// Bad with unordered map, and quite complex too...
bool
resize_database_names_arena(Database *database)
{
    size_t new_size = database->names_arena.size + Kilobytes(50);
    char *new_buffer = (char *)malloc(new_size);
    if (new_buffer == nullptr) return false;
    
    Arena new_arena;
    init_arena(&new_arena, new_buffer, new_size);
    
    char *old_arena_base_ptr = database->names_arena.buffer;
    char *new_arena_base_ptr = new_arena.buffer;
    
    String ab = {};
    String c = ab;
    {
        //std::unordered_map<String, App_Id>::iterator it = database->local_programs.begin();
        //while (it != database->local_programs.end())
        //for(std::unordered_map<String, App_Id>::iterator iter = database->local_programs.begin(); iter != database->local_programs.end(); ++iter)
        
        for (std::pair<String, App_Id> &pair : database->local_programs)
        {
            String s = pair.first;
            
            ptrdiff_t offset = s.str - old_arena_base_ptr;
            assert(offset >= 0);
            
            s.str = new_arena_base_ptr + offset;
            pair.first = s;
        }
    }
    {
        auto it = database->domain_names.begin();
        while (it != database->domain_names.end())
        {
            ptrdiff_t offset = it->first.str - old_arena_base_ptr;
            assert(offset >= 0);
            
            it->first.str = new_arena_base_ptr + offset;
            ++it;
        }
    }
    {
        auto it = database->local_programs.begin();
        while (it != database->app_names.end())
        {
            ptrdiff_t offset = it->second.short_name.str - old_arena_base_ptr;
            assert(offset >= 0);
            
            it->second.short_name.str = new_arena_base_ptr + offset;
            if (is_local_program(it->first))
            {
                offset = it->second.full_name.str - old_arena_base_ptr;
                assert(offset >= 0);
                
                it->second.full_name.str = new_arena_base_ptr + offset;
            }
            
            ++it;
        }
    }
}
#endif

App_Id
poll_windows(Database *database, Settings *settings)
{
    
    ZoneScoped;
    
    char buf[PLATFORM_MAX_PATH_LEN];
    size_t len = 0;
    
    Platform_Window window = platform_get_active_window();
    if (!window.is_valid) 
        return Id_Invalid;
    
    // If this failed it might be a desktop or other things like alt-tab screen or something
    bool got_path = platform_get_program_from_window(window, buf, &len);
    if (!got_path) 
        return Id_Invalid;
    
    String full_path = make_string_size_cap(buf, len, array_count(buf));
    String program_name = get_filename_from_path(full_path);
    if (program_name.length == 0) 
        return Id_Invalid;
    
    remove_extension(&program_name);
    if (program_name.length == 0) 
        return Id_Invalid;
    
    // string_to_lower(&program_name);
    
    bool program_is_firefox = string_equals(program_name, "firefox");
    if (program_is_firefox)
    {
        // TODO: Maybe cache last url to quickly get record without comparing with keywords, retrieving record etc
        // We wan't to detect, incomplete urls and people just typing garbage into url bar, but
        char url_buf[PLATFORM_MAX_URL_LEN];
        size_t url_len = 0;
        if (platform_get_firefox_url(window, url_buf, &url_len))
        {
            String url = make_string(url_buf, url_len);
            if (url.length > 0)
            {
                String domain_name = extract_domain_name(url);
                if (domain_name.length > 0)
                {
#if MONITOR_DEBUG
                    // match will all urls we can get a domain from
                    return get_id_and_add_if_new(database, domain_name, url, Id_Website);
#else
                    if (string_matches_keyword(domain_name, settings->keywords))
                    {
                        return get_id_and_add_if_new(database, domain_name, url, Id_Website);
                    }
#endif
                }
            }
        }
    }
    
    // If program wasn't a browser, or it was a browser but the url didn't match any keywords, get browser's program id
    return get_id_and_add_if_new(database, program_name, full_path, Id_LocalProgram);
}

void
update(Monitor_State *state, SDL_Window *window, time_type dt_microseconds, u32 window_status)
{
    ZoneScoped;
    
    date::sys_days current_date = get_localtime();
    if (!state->is_initialised)
    {
        //remove(global_savefile_path);
        //remove(global_debug_savefile_path);
        
        *state = {};
        
        init_database(&state->database, current_date);
        
        Misc_Options options = {};
        options.day_start_time = 0;
        options.poll_start_time = 0;
        options.poll_end_time = 0;
        options.run_at_system_startup = true;
        options.poll_frequency_microseconds = 10000;
        
        state->settings.misc_options = options;
        
        char *buffer = (char *)malloc(MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE);
        Assert(buffer);
        init_arena(&state->keyword_arena, buffer, MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE);
        
        // Keywords must be null terminated when given to platform gui
        state->settings.keywords;
        add_keyword(&state->keyword_arena, state->settings.keywords, "CITS3003");
        add_keyword(&state->keyword_arena, state->settings.keywords, "youtube");
        add_keyword(&state->keyword_arena, state->settings.keywords, "docs.microsoft");
        add_keyword(&state->keyword_arena, state->settings.keywords, "google");
        add_keyword(&state->keyword_arena, state->settings.keywords, "github");
        
        
        state->accumulated_time = 0;
        state->total_runtime = 0;
        state->startup_time = win32_get_time();
        
        // UI initialisation
        
        // This will set once per program execution
        Date_Picker &picker = state->date_picker;
        
#if 0
        picker.range_type = Range_Type_Daily;
        picker.start = current_date;
        picker.end = current_date;
#else
        // DEBUG: Default to monthly
        auto ymd = date::year_month_day{current_date};
        picker.range_type = Range_Type_Monthly;
        picker.start = date::sys_days{ymd.year()/ymd.month()/1};
        picker.end   = date::sys_days{ymd.year()/ymd.month()/date::last};
#endif
        
        date::sys_days oldest_date = state->database.day_list.days.front().date;
        date::sys_days newest_date = state->database.day_list.days.back().date;
        
        // sets label and if buttons are disabled 
        date_picker_clip_and_update(&picker, oldest_date, newest_date);
        
        init_calendar(&picker.first_calendar, current_date, oldest_date, newest_date);
        init_calendar(&picker.second_calendar, current_date, oldest_date, newest_date);
        
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
    
    // Also true if on a browser and the tab changed
    // platform_get_changed_window();
    
    state->accumulated_time += dt_microseconds;
    state->total_runtime += dt_microseconds;
    time_type poll_window_freq = 100000;
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
    
    if (window_status & Window_Just_Visible)
    {
    }
    
    Day_View day_view = get_day_view(&database->day_list);
    
    if (window_status & Window_Visible)
    {
        draw_ui_and_update(window, state, database, &day_view);
    }
    
    free_day_view(&day_view);
    
}

