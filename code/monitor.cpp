#include <unordered_map>

#include "cian.h"
#include "monitor_string.h"
#include "graphics.h"
#include "icon.h"
#include "helper.h"

#include "date.h"

#include "monitor.h"
#include "platform.h"

#include "resources/source_code_pro.cpp"
#include "resources/world_icon.cpp"

#include "helper.cpp"
#include "bitmap.cpp"
#include "apps.cpp"
#include "file.cpp"
#include "date_picker.cpp"
#include "icon.cpp" 
#include "ui.cpp"


Icon_Asset *
get_app_icon_asset(App_List *apps, UI_State *ui, App_Id id)
{
    if (is_website(id))
    {
        //get_favicon_from_website(url);
        
        Icon_Asset *default_icon = &ui->icons[DEFAULT_WEBSITE_ICON_INDEX];
        Assert(default_icon->texture_handle != 0);
        return default_icon;
    }
    
    if (is_local_program(id))
    {
        u32 id_index = index_from_id(id);
        if (id_index >= ui->icon_indexes.size())
        {
            ui->icon_indexes.resize(id_index + 1, -1); // insert -1 at the end of array to n=id_index+1 slots
        }
        
        s32 *icon_index = &ui->icon_indexes[id_index];
        if (*icon_index == -1)
        {
            String path = apps->local_programs[id_index].full_name;
            Assert(path.length > 0);
            
            Bitmap bitmap = {};
            bool success = platform_get_icon_from_executable(path.str, ICON_SIZE, &bitmap, ui->icon_bitmap_storage);
            if (success)
            {
                // TODO: Maybe mark old paths that couldn't get correct icon for deletion or to stop trying to get icon, or assign a default invisible icon
                *icon_index = load_icon_asset(ui->icons, bitmap);
                Assert(*icon_index > 1);
                return &ui->icons[*icon_index];
            }
        }
        else
        {
            // Icon already loaded
            Assert(*icon_index < ui->icons.size());
            Icon_Asset *icon = &ui->icons[*icon_index];
            Assert(icon->texture_handle != 0);
            return icon;
        }
    }
    
    Icon_Asset *default_icon = &ui->icons[DEFAULT_LOCAL_PROGRAM_ICON_INDEX];
    Assert(default_icon->texture_handle != 0);
    return default_icon;
}


void
debug_add_records(App_List *apps, Day_List *day_list, date::sys_days cur_date)
{
    static char *website_names[] = {
        "teachyourselfcs.com",
        "trello.com",
        "github.com",
        "ukulelehunt.com",
        "forum.ukuleleunderground.com",
        "soundcloud.com",
        "www.a1k0n.net",
        "ocw.mit.edu",
        "artsandculture.google.com",
        "abcdefghijklmnopqrstuvwxyz1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*[]-+_=();',./",
        "QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQQA",
    };
    
    static char *exes[] = {
#include "..\\build\\exes_with_icons.txt"
    };
    static int count = array_count(exes);
    
#if 0
    // Write file
    static char *base  = SDL_GetBasePath();
    snprintf(filename, array_count(filename), "%sexes_with_icons.txt", base);
    
    FILE *file = fopen(filename, "w");
    Assert(file);
    
    SDL_free(base);
    
    for (int i = 0; i < count; ++i)
    {
        Bitmap bmp = {};
        u32 *mem = (u32 *)malloc(ICON_SIZE*ICON_SIZE*4);;
        if (platform_get_icon_from_executable(exes[i], 32, &bmp, mem))
        {
            char buf[2000];
            snprintf(buf, 2000, "\"%s\",\n", exes[i]);
            fputs(buf, file);
        }
        free(mem);
    }
    
    fclose(file);
#endif
    
    
    char **names = exes;
    int num_names = count;
    //char **names = some_names;
    //int num_names = array_count(some_names);
    int num_local_programs = rand_between(1, 6);
    int num_websites = rand_between(1, 3);
    
    // Should be no days in day list
    Assert(day_list->days.size() == 0);
    
    date::sys_days start  = cur_date - date::days{400};
    
    for (date::sys_days d = start; d < cur_date; d += date::days{1})
    {
        start_new_day(day_list, d);
        
        int n = num_local_programs;
        for (int j = 0; j < n; ++j)
        {
            int name_idx = rand_between(0, num_names - 1);
            int dt_microsecs = rand_between(0, MICROSECS_PER_SEC * 3);
            
            String full_path = make_string_size_cap(names[name_idx], strlen(names[name_idx]), strlen(names[name_idx]));
            String program_name = get_filename_from_path(full_path);
            Assert(program_name.length != 0);
            
            remove_extension(&program_name);
            Assert(program_name.length != 0);
            
            App_Id record_id = get_local_program_app_id(apps, program_name, full_path);
            add_or_update_record(day_list, record_id, dt_microsecs);
        }
        
        n = num_websites;
        for (int j = 0; j < n; ++j)
        {
            int name_idx = rand_between(0, array_count(website_names) - 1);
            int dt_microsecs = rand_between(0, MICROSECS_PER_SEC * 2);
            
            String domain_name = make_string_size_cap(website_names[name_idx], strlen(website_names[name_idx]), strlen(website_names[name_idx]));
            
            App_Id record_id = get_website_app_id(apps, domain_name);
            add_or_update_record(day_list, record_id, dt_microsecs);
        }
    }
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
    
    if (search_for_char(url, 0, ' ') != -1)
    {
        // Found an invalid char in url (space)
        String result;
        result.str = url.str;
        result.capacity = 0;
        result.length = 0;
        return result;
    }
    
    // We don't handle weird urls very well, or file:// 
    size_t start = scheme_length(url);
    
    if (start >= url.length)
    {
        // URl is only a scheme
        String result;
        result.str = url.str;
        result.capacity = 0;
        result.length = 0;
        return result;
    }
    
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
            // Check against valid characters so if user was just typing in garbage into URL bar, then clicked away, we won't think its a domain.
            String domain = substr_range(authority, host_start, host_end);
            
            bool good_domain = true;
            for (u32 i = 0; i < domain.length; ++i)
            {
                char c = domain[i];
                if (!(isalnum(c) || c == '-' || c == '.'))
                {
                    good_domain = false;
                    break;
                }
            }
            
            if (domain[0] == '.' || domain[domain.length-1] == '.')
            {
                good_domain = false;
            }
            
            s32 period_offset = search_for_char(domain, 0, '.');
            if (period_offset == -1)
            {
                // If no sub domains stuff then probably bad
                good_domain = false;
                
            }
            
            
            if (good_domain) return domain;
        }
    }
    
    String result;
    result.str = url.str;
    result.capacity = 0;
    result.length = 0;
    
    return result;
}

App_Id
poll_windows(App_List *apps, Settings *settings)
{
    // Returns 0 if we return early
    
    ZoneScoped;
    
    char buf[PLATFORM_MAX_PATH_LEN];
    size_t len = 0;
    
    Platform_Window window = platform_get_active_window();
    if (!window.is_valid) 
        return 0;
    
    // If this failed it might be a desktop or other things like alt-tab screen or something
    bool got_path = platform_get_program_from_window(window, buf, &len);
    if (!got_path) 
        return 0;
    
    String full_path = make_string_size_cap(buf, len, array_count(buf));
    String program_name = get_filename_from_path(full_path);
    if (program_name.length == 0) 
        return 0;
    
    remove_extension(&program_name);
    if (program_name.length == 0) 
        return 0;
    
    // string_to_lower(&program_name);
    
    bool program_is_firefox = string_equals(program_name, "firefox");
    if (program_is_firefox && settings->misc.keyword_status != Settings_Misc_Keyword_None)
    {
        // TODO: Maybe cache last url to quickly get record without comparing with keywords
        char url_buf[PLATFORM_MAX_URL_LEN];
        size_t url_len = 0;
        
        if (platform_firefox_get_url(window, url_buf, &url_len))
        {
            String url = make_string(url_buf, url_len);
            if (url.length > 0)
            {
                String domain_name = extract_domain_name(url);
                if (domain_name.length > 0)
                {
                    Assert(settings->misc.keyword_status >= Settings_Misc_Keyword_All && settings->misc.keyword_status <= Settings_Misc_Keyword_None);
                    if (settings->misc.keyword_status == Settings_Misc_Keyword_All)
                    {
                        return get_website_app_id(apps, domain_name);
                    }
                    else
                    {
                        // Custom
                        if (string_matches_keyword(domain_name, settings->keywords))
                        {
                            return get_website_app_id(apps, domain_name);
                        }
                    }
                }
            }
        }
    }
    
    // If program wasn't a browser, or it was a browser but the url didn't match any keywords, get browser's program id
    return get_local_program_app_id(apps, program_name, full_path);
}



void
update(Monitor_State *state, SDL_Window *window, s64 dt_microseconds, Window_Event window_event)
{
    ZoneScoped;
    
    date::sys_days current_date = get_local_time_day();
    
    if (!state->is_initialised)
    {
        //create_world_icon_source_file("c:\\dev\\projects\\monitor\\build\\world.png", "c:\\dev\\projects\\monitor\\code\\world_icon.cpp", ICON_SIZE);
        
        
        // Do i need this to construct all vectors etc, as opposed to just zeroed memory?
        *state = {};
        
        // TODO: how to handle clipped path, because path was too long
        // TODO: Pretty sure strncpy does not guarantee null termination, FIX this
        platform_get_base_directory(state->savefile_path, array_count(state->savefile_path));
        size_t remaining_space = array_count(state->savefile_path) - strlen(state->savefile_path);
        strncat(state->savefile_path, "savefile.mbf", remaining_space);
        strncat(state->temp_savefile_path, "temp_savefile.mbf", remaining_space);
        
        //remove(state->savefile_path);
        //exit(3);
        
        // will be 16ms for 60fps monitor
        state->accumulated_time = 0;
        state->refresh_frame_time = (u32)(MILLISECS_PER_SEC / (float)platform_get_monitor_refresh_rate());
        
        App_List *apps = &state->apps;
        Settings *settings = &state->settings;
        Day_List *day_list = &state->day_list;
        
        bool init_state_from_defaults = true;
        if (platform_file_exists(state->savefile_path))
        {
            if (read_from_MBF(apps, day_list, settings, state->savefile_path))
            {
                init_state_from_defaults = false;
            }
            else 
            {
                Assert(0);
            }
        }
        
        if (init_state_from_defaults)
        {
            apps->local_program_ids = std::unordered_map<String, App_Id>(50);
            apps->website_ids       = std::unordered_map<String, App_Id>(50);
            apps->local_programs.reserve(50);
            apps->websites.reserve(50);
            apps->next_program_id = LOCAL_PROGRAM_ID_START;
            apps->next_website_id = WEBSITE_ID_START;
            
            init_arena(&apps->name_arena, Kilobytes(10), Kilobytes(10));
            init_arena(&day_list->record_arena, MAX_DAILY_RECORDS_MEMORY_SIZE, MAX_DAILY_RECORDS_MEMORY_SIZE);
            init_arena(&settings->keyword_arena, MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE);
            
            // add fake days before current_date
#if FAKE_RECORDS
            debug_add_records(apps, day_list, current_date);
#endif
            start_new_day(day_list, current_date);
            
            settings->misc = Misc_Options::default_misc_options();
            
            // Keywords must be null terminated when given to platform gui
            add_keyword(settings, "CITS3003");
            add_keyword(settings, "youtube");
            add_keyword(settings, "docs.microsoft");
            add_keyword(settings, "google");
            add_keyword(settings, "github");
        }
        
        // will be poll_frequency_milliseconds in release
        platform_set_sleep_time(16);
        
        state->is_initialised = true;
    }
    
    App_List *apps = &state->apps;
    UI_State *ui = &state->ui;
    Day_List *day_list = &state->day_list;
    
    if (window_event == Window_Closed)
    {
        // no need to free stuff or delete textures (as glcontext is deleted)
        
        // Creates or overwrites old file
        if (!write_to_MBF(apps, day_list, &state->settings, state->savefile_path))
        {
            Assert(0);
        }
    }
    
    if (window_event == Window_Hidden)
    {
        if (ui->open)
        {
            unload_ui(ui);
            //platform_set_sleep_time(settings->poll_frequency_milliseconds);
        }
    }
    
    
    if (current_date != day_list->days.back().date)
    {
        start_new_day(day_list, current_date);
        // Don't really need to unload and reload all icons from GPU as it can easily hold many thousands
    }
    
    if (window_event == Window_Shown)
    {
        if (!ui->open)
        {
            // These dates used to determine selected and limits to date navigation in picker and calendars
            date::sys_days oldest_date = day_list->days.front().date;
            date::sys_days newest_date = day_list->days.back().date;
            
            load_ui(ui, apps->local_programs.size(), current_date, oldest_date, newest_date);
            //platform_set_sleep_time(state->refresh_frame_time);
        }
    }
    
    state->accumulated_time += dt_microseconds;
    if (ui->open)
    {
        // TODO: Can set the rhs compared variable to 0 when ui closed and we want to poll everytime woken up
        // state->settings.misc_options.poll_frequency_milliseconds * MICROSECS_PER_MILLISEC;
        if (state->accumulated_time >= DEFAULT_POLL_FREQUENCY_MILLISECONDS)
        {
            App_Id id = poll_windows(apps, &state->settings);
            if (id) 
                add_or_update_record(&state->day_list, id, state->accumulated_time);
            state->accumulated_time = 0;
        }
        
        ui->day_view = get_day_view(day_list); // debug
        draw_ui_and_update(window, ui, &state->settings, apps, state); // state is just for debug
        free_day_view(&ui->day_view);
    }
    else
    {
        // If ui is not open just poll whenever we enter the update function (should usually by the poll frequency after we wakeup from sleep)
        // This avoids the degenerate case where we repeatedly exit sleep just before our poll frequency mark, and have to do another sleep cycle before polling - effectively halving the experienced poll frequency.
        
        App_Id id = poll_windows(apps, &state->settings);
        if (id) 
            add_or_update_record(&state->day_list, id, state->accumulated_time);
        state->accumulated_time = 0;
    }
    
}

