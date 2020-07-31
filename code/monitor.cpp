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

#include "resources/source_code_pro.cpp"
#include "resources/world_icon.cpp"

#include "icon.cpp"    // Deals with win32 icon interface TODO: Move platform dependent parts to win
#include "file.cpp"
#include "ui.cpp"

#define GLCheck(x) GLClearError(); x; GLLogCall(#x, __FILE__, __LINE__)

void
GLClearError()
{
    while (glGetError() != GL_NO_ERROR);
}

bool
GLLogCall(const char *function, const char *file, int line)
{
    while (GLenum error = glGetError())
    {
        printf("[OpenGL Error] (%u) %s %s: Line %i\n", error, function, file, line);
        return false;
    }
    
    return true;
}


// TODO: NOt sure where to put this
u32
opengl_create_texture(Bitmap bitmap)
{
    GLuint image_texture;
    GLCheck(glGenTextures(1, &image_texture)); // I think this can fail if out of texture mem
    
    GLCheck(glBindTexture(GL_TEXTURE_2D, image_texture));
    
    GLCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    
    // Can be PACK for glReadPixels or UNPCK GL_UNPACK_ROW_LENGTH
    
    // This sets the number of pixels in the row for the glTexImage2D to expect, good 
    //glPixelStorei(GL_UNPACK_ROW_LENGTH, 24);
    
    // Alignment of the first pixel in each row
    GLCheck(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    
    // You create storage for a Texture and upload pixels to it with glTexImage2D (or similar functions, as appropriate to the type of texture). If your program crashes during the upload, or diagonal lines appear in the resulting image, this is because the alignment of each horizontal line of your pixel array is not multiple of 4. This typically happens to users loading an image that is of the RGB or BGR format (for example, 24 BPP images), depending on the source of your image data. 
    
    GLCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width, bitmap.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, bitmap.pixels));
    
    return image_texture;
}

s32
load_icon_asset(std::vector<Icon_Asset> &icons, Bitmap bitmap)
{
    Icon_Asset icon;
    icon.texture_handle = opengl_create_texture(bitmap); 
    icon.width = bitmap.width;
    icon.height = bitmap.height;
    
    s32 index = icons.size();
    icons.push_back(icon);
    
    return index;
}

void 
invalidate_icon_indexes(App_List *apps)
{
    for (u32 i = 0; i < apps->local_programs.size(); ++i)
    {
        auto &local_program = apps->local_programs[i];
        local_program.icon_index = -1;
    }
}

void
unload_all_icon_assets(std::vector<Icon_Asset> &icons)
{
    for (u32 icon_index = 0; icon_index < icons.size(); ++icon_index)
    {
        GLCheck(glDeleteTextures(1, &icons[icon_index].texture_handle));
    }
    
    // TODO: Free icons memory
    icons.clear();
}

void
load_default_icon_assets(std::vector<Icon_Asset> &icons)
{
    Bitmap program_bitmap;
    if (!platform_get_default_icon(&program_bitmap.width, &program_bitmap.height, &program_bitmap.pitch, &program_bitmap.pixels))
    {
        program_bitmap = make_bitmap(ICON_SIZE, ICON_SIZE, 0x000000); // transparent icon
    }
    
    u32 local_program_icon_index = load_icon_asset(icons, program_bitmap);
    free(program_bitmap.pixels);
    
    Bitmap website_bitmap;
    website_bitmap.pixels = (u32 *)world_icon_data;
    website_bitmap.width  = world_icon_width;
    website_bitmap.height = world_icon_height;
    website_bitmap.pitch  = world_icon_width*4;
    
    u32 website_icon_index = load_icon_asset(icons, website_bitmap);
    
    Assert(local_program_icon_index == DEFAULT_LOCAL_PROGRAM_ICON_INDEX &&
           website_icon_index == DEFAULT_WEBSITE_ICON_INDEX);
}

Icon_Asset *
get_app_icon_asset(App_List *apps, std::vector<Icon_Asset> &icons, App_Id id)
{
    if (is_website(id))
    {
        //get_favicon_from_website(url);
        
        Icon_Asset *default_icon = &icons[DEFAULT_WEBSITE_ICON_INDEX];
        Assert(default_icon->texture_handle != 0);
        return default_icon;
    }
    
    if (is_local_program(id))
    {
        u32 name_index = index_from_id(id);
        
        Assert(name_index < apps->local_programs.size());
        Assert(apps->local_programs.size() > 0);
        
        Local_Program_Info &info = apps->local_programs[name_index];
        if (info.icon_index == -1)
        {
            Assert(info.full_name.length > 0);
            Bitmap bitmap;
            bool success = platform_get_icon_from_executable(info.full_name.str, ICON_SIZE, 
                                                             &bitmap.width, &bitmap.height, &bitmap.pitch, &bitmap.pixels);
            if (success)
            {
                // TODO: Maybe mark old paths that couldn't get correct icon for deletion, or
                info.icon_index = load_icon_asset(icons, bitmap);
                free(bitmap.pixels);
                
                Assert(info.icon_index > 1);
                
                return &icons[info.icon_index];
            }
        }
        else
        {
            // Icon already loaded
            Assert(info.icon_index < icons.size());
            Icon_Asset *icon = &icons[info.icon_index];
            Assert(icon->texture_handle != 0);
            return icon;
        }
    }
    
    Icon_Asset *default_icon = &icons[DEFAULT_LOCAL_PROGRAM_ICON_INDEX];
    Assert(default_icon->texture_handle != 0);
    return default_icon;
}


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
        info.icon_index = -1;
        
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

void
debug_add_records(App_List *apps, Day_List *day_list, date::sys_days cur_date)
{
    
    static char *some_names[] = {
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
    
    static char **exes = nullptr;
    static int count = 0;
    static bool first = true;
    
    if (first)
    {
        
        exes = (char **)calloc(1, sizeof(char *) * 10000);
        Assert(exes);
        
        char filename[500];
        char *base = SDL_GetBasePath();
        //snprintf(filename, array_count(filename), "%sexes.txt", base);
        snprintf(filename, array_count(filename), "%sexes_with_icons.txt", base);
        
        FILE *exes_file = fopen(filename, "r");
        Assert(exes_file);
        
        SDL_free(base);
        
        char buf[2000] = {};
        while (fgets(buf, 2000, exes_file) != nullptr)
        {
            size_t len = strlen(buf);
            exes[count] = (char *)calloc(1, len + 1);
            Assert(exes[count]);
            
            strcpy(exes[count], buf);
            
            // remove newline
            exes[count][len-1] = 0;
            count += 1;
        }
        
        fclose(exes_file);
        first = false;
        
#if 0
        char filename[500];
        char *base = SDL_GetBasePath();
        snprintf(filename, array_count(filename), "%sexes_with_icons.txt", base);
        
        FILE *file = fopen(filename, "w");
        Assert(file);
        
        SDL_free(base);
        
        for (int i = 0; i < count; ++i)
        {
            int  w;
            int h; 
            int pitch;
            u32 *p;
            if (platform_get_icon_from_executable(exes[i], 32, &w, &h, &pitch, &p))
            {
                char buf[2000];
                snprintf(buf, 2000, "%s\n", exes[i]);
                fputs(buf, file);
                
                free(p);
            }
        }
        
        fclose(file);
#endif
    }
    
    char **names = exes;
    int num_names = count;
    //char **names = some_names;
    //int num_names = array_count(some_names);
    int num_local_programs = rand_between(1, 6);
    int num_websites = rand_between(1, 3);
    
    // Should be no days in day list
    Assert(day_list->days.size() == 0);
    
    date::sys_days start  = cur_date - date::days{70};
    
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


bool
resize_bitmap(Bitmap *in, Bitmap *out, u32 dimension)
{
    u32 *out_pixels = (u32 *)malloc(dimension * dimension * 4);
    if (out_pixels)
    {
        if (stbir_resize_uint8((u8 *)in->pixels,in->width , in->height, 0, // packed in memory
                               (u8 *)out_pixels, dimension, dimension, 0, // packed in memory
                               4))
        {
            out->width = dimension;
            out->height = dimension;
            out->pitch = dimension*4;
            out->pixels = out_pixels;
            return true;
        }
        else
        {
            free(out_pixels);
        }
    }
    
    return false;
}

void 
add_keyword(Settings *settings, char *str)
{
    Assert(strlen(str) < MAX_KEYWORD_SIZE);
    
    String keyword = push_string(&settings->keyword_arena, str);
    settings->keywords.add_item(keyword);
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
    ZoneScoped;
    
    // Returns 0 if we return early
    
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
        // TODO: Maybe cache last url to quickly get record without comparing with keywords
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
                    return get_website_app_id(apps, domain_name);
#else
                    if (string_matches_keyword(domain_name, settings->keywords))
                    {
                        return get_website_app_id(apps, domain_name);
                    }
#endif
                }
            }
        }
    }
    
    // If program wasn't a browser, or it was a browser but the url didn't match any keywords, get browser's program id
    return get_local_program_app_id(apps, program_name, full_path);
}

void
update(Monitor_State *state, SDL_Window *window, time_type dt_microseconds, Window_Event window_event)
{
    ZoneScoped;
    
    date::sys_days current_date = get_local_time_day();
    
    if (!state->is_initialised)
    {
        //remove(global_savefile_path);
        //remove(global_debug_savefile_path);
        //create_world_icon_source_file("c:\\dev\\projects\\monitor\\build\\world.png",  "c:\\dev\\projects\\monitor\\code\\world_icon.cpp", ICON_SIZE);
        
        // Do i need this to construct all vectors etc, as opposed to just zeroed memory?
        *state = {};
        
        state->ui_visible = false;
        // ui is uninitialised until window shown
        
        // will be 16ms for 60fps monitor
        state->accumulated_time = 0;
        state->refresh_frame_time = (u32)(MILLISECS_PER_SEC / (float)platform_SDL_get_monitor_refresh_rate());
        
        App_List *apps = &state->apps;
        apps->local_program_ids = std::unordered_map<String, App_Id>(30);
        apps->website_ids       = std::unordered_map<String, App_Id>(30);
        apps->local_programs.reserve(30);
        apps->websites.reserve(30);
        apps->next_program_id = LOCAL_PROGRAM_ID_START;
        apps->next_website_id = WEBSITE_ID_START;
        
        size_t size = Kilobytes(30);
        init_arena(&apps->names_arena, size);
        
        
        Day_List *day_list = &state->day_list;
        // not needed just does the allocation here rather than during firsts push_size
        init_arena(&day_list->arena, Kilobytes(10));
        // add fake days before current_date
        debug_add_records(apps, day_list, current_date);
        start_new_day(day_list, current_date);
        
        
        Misc_Options options = {};
        options.poll_frequency_milliseconds = 16;  // or want 17?
        
        state->settings.misc_options = options;
        
        Settings *settings = &state->settings;
        // minimum block size of this should be MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE, then should only need 1 block
        init_arena(&settings.keyword_arena, MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE);
        
        // Keywords must be null terminated when given to platform gui
        add_keyword(settings, "CITS3003");
        add_keyword(settings, "youtube");
        add_keyword(settings, "docs.microsoft");
        add_keyword(settings, "google");
        add_keyword(settings, "github");
        
        // will be poll_frequency_milliseconds in release
        platform_set_sleep_time(16);
        
        state->is_initialised = true;
    }
    
    App_List *apps = &state->apps;
    
    if (window_event == Window_Closed)
    {
        // save file
        // no need to free stuff or delete textures (as glcontext is deleted)
    }
    
    if (window_event == Window_Hidden && state->ui_visible)
    {
        invalidate_icon_indexes(apps);
        
        date_picker = {};
        unload_all_icon_assets(state->icons);
        if (state->edit_settings)
        {
            free(state->edit_settings);
        }
        
        //platform_set_sleep_time(settings->poll_frequency_milliseconds);
        
        state->ui_visible = false;
    }
    
    if (window_event == Window_Shown && !state->ui_visible)
    {
        init_imgui_fonts_and_style(22.0f);
        
        UI_State *ui = &state->ui;
        ui->icons.reserve(100);
        load_default_icon_assets(ui->icons);
        
        date::sys_days oldest_date = database->day_list.days.front().date;
        date::sys_days newest_date = database->day_list.days.back().date;
        
        init_date_picker(&state->date_picker, current_date, oldest_date, newest_date);
        
        // will be poll_frequency_milliseconds in release
        platform_set_sleep_time(16);
        
        state->ui_visible = true;
    }
    
    if (current_date != database->day_list.days.back().date)
    {
        start_new_day(&database->day_list, current_date);
        
        // Not really needed as we just unload all icons on UI close, and GPU can a lot of icons
        //unload_all_icon_assets(database, state->icons);
        //load_default_icon_assets(database, state->icons);
    }
    
    // TODO: Maybe just do a poll when ever this update function is called (except when the ui is open, then we limit the rate). 
    // Else may have a degenerate case where the function is called slightly before when it should have and the accumulated time is slightly < then poll freq, then we have to wait another full poll freq to do one poll, essentailly halving the experienced poll frequency.
    
    state->accumulated_time += dt_microseconds;
    time_type poll_window_freq = 100000;
    //s64 poll_window_freq = state->settings.misc_options.poll_frequency_milliseconds * MICROSECS_PER_MILLISEC;
    if (state->accumulated_time >= poll_window_freq)
    {
        App_Id id = poll_windows(apps, &state->settings);
        if (id != Id_Invalid)
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
    
    Day_View day_view = get_day_view(&database->day_list);
    
    if (state->ui_visible)
    {
        draw_ui_and_update(window, state, database, &day_view);
    }
    
    free_day_view(&day_view);
}

