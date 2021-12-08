
#include "platform.h"
#include "base.h"
#include "date.h"
#include "monitor_string.h"
#include "helper.h"

typedef u32 app_id;
enum app_type
{
    NONE = 0,
    PROGRAM = 1, // Program IDs do not have the most significant bit set
    WEBSITE = 2, // Website IDs have the most significant bit set
};

struct app_info
{
    app_type Type;
    union
    {
        string ProgramPath; 
        string WebsiteHost;
        string Name; // For referencing app's path or host in a non-specific way
    };
};

constexpr u32 APP_TYPE_BIT_INDEX = 31;

struct record
{
    app_id ID;
    date::sys_days Date; // days since 2000 or something...
    u64 DurationMicroseconds; 
};

static_assert(sizeof(date::sys_days) == 4, ""); // check size

struct monitor_state
{
    bool IsInitialised;
    
    arena PermanentArena;
    arena TemporaryArena;
    
    hash_table AppTable;
    u32 CurrentID;
    
    record *Records;
    u64 RecordCount;
};

string UrlParseScheme(string *Url)
{
    // "A URL-scheme string must be one ASCII alpha, followed by zero or more of ASCII alphanumeric, U+002B (+), U+002D (-), and U+002E (.)" - https://url.spec.whatwg.org/#url-scheme-string
    // such as http, file, dict
    
    u64 SchemeLength = 0;
    
    u64 ColonPos = StringFindChar(*Url, ':');
    if (ColonPos < Url->Length)
    {
        for (u64 i = 0; i < ColonPos; ++i)
        {
            char c = StringGetChar(*Url, i);
            if (!(IsAlphaNumeric(c) || c == '+' || c == '-' || c == '.'))
            {
                // non_valid scheme string character
                SchemeLength = 0;
                break;
            }
        }
        
        if (!IsAlpha(StringGetChar(*Url, 0)))
        {
            SchemeLength = 0;
        }
    }
    
    string Scheme = StringPrefix(*Url, SchemeLength);
    StringSkip(Url, SchemeLength + 1); // skip scheme and ':'
    
    return Scheme;
}

string UrlParseAuthority(string *Url)
{
    // If Authority present then path must start with a slash
    u64 NextSlashPos = StringFindChar(*Url, '/');
    string Authority = StringPrefix(*Url, NextSlashPos);
    return Authority;
}

string UrlParseHostFromAuthority(string Authority)
{
    // authority = [userinfo "@"] host [":" port]
    
    // Enclosed by square brackets
    if (StringGetChar(Authority, 0) == '[')
    {
        StringSkip(&Authority, 1);
        if (StringGetChar(Authority, Authority.Length - 1) == ']')
        {
            StringChop(&Authority, 1);
        }
        else
        {
            // Error missing ']'
            // But we try and parse the rest anyway
        }
    }
    
    // Skip optional userinfo before Host
    u64 AtPos = StringFindChar(Authority, '@');
    if (AtPos < Authority.Length)
    {
        StringSkip(&Authority, AtPos + 1); 
    }
    
    // Remove optional port after host
    u64 ColonPos = StringFindChar(Authority, ':');
    string Host = StringPrefix(Authority, ColonPos); 
    
    return Host;
}

string UrlParseHost(string Url)
{
    // Other good reference: https://github.com/curl/curl/blob/master/lib/urlapi.c
    // From wikipedia: (where [component] means optional)
    
    // Browsers are must more permissive than spec 
    
    // host must be a hostname or a IP address
    // IPv4 addresses must be in dot-decimal notation, and IPv6 addresses must be enclosed in brackets ([]).
    
    // UTF8 URLs are possible
    // Web and Internet software automatically convert the __domain name__ into punycode usable by the Domain Name System
    // http://'Chinese symbols' becomes http://xn--fsqu00a.xn--3lr804guic/. 
    // The xn-- indicates that the character was not originally ASCII
    
    // The __path name__ can also be encoded using percent encoding, but we don't care about path component here
    
    // URL could just be gibberish (page wan't loaded and user was just typing in url bar)
    
    // Browsers also accept multiple slashes where there should be one in path
    
    string Host = {};
    
    // Ensure no invalid spaces in URL
    if (!StringFindChar(Url, ' '))
    {
        string Scheme = UrlParseScheme(&Url);
        if (StringEquals(Scheme, StringLiteral("https")) || 
            StringEquals(Scheme, StringLiteral("http")))
        {
            // URI = scheme ":" ["//" authority] path ["?" query] ["#" fragment]
            // Path may or may not start with slash, or can be empty resulting in two slashes "//"
            // If the authority component is absent then the path cannot begin with an empty segment "//"
            bool ExpectAuthority = (StringGetChar(Url, 0) == '/') && (StringGetChar(Url, 1) == '/');
            if (ExpectAuthority)
            {
                StringSkip(&Url, 2);
                string Authority = UrlParseAuthority(&Url);
                //StringSkip(*Url, Authority.Length);
                
                Host = UrlParseHostFromAuthority(Authority);
                
                // Expect 1 or 2 slashes after host
            }
            else
            {
                // Expect 0 or 1 slashes after
            }
        }
    }
    
    return Host;
}

date::sys_days
GetLocalTime()
{
    time_t rawtime;
    time( &rawtime );
    
    // Looks in TZ database, not threadsafe
    struct tm *info;
    info = localtime( &rawtime );
    auto now = date::sys_days{
        date::month{(unsigned)(info->tm_mon + 1)}/date::day{(unsigned)info->tm_mday}/date::year{info->tm_year + 1900}};
    
    return now;
}

void SetBit(u32 *A, u32 BitIndex)
{
    Assert(BitIndex < 32);
    *A |= (1 << BitIndex);
}

u32 GetBit(u32 A, u32 BitIndex)
{
    Assert(BitIndex < 32);
    return A & (1 << BitIndex);
}

app_id GenerateAppID(u32 *CurrentID, app_type AppType)
{
    Assert(*CurrentID != 0);
    Assert(*CurrentID < (1 << APP_TYPE_BIT_INDEX));
    
    app_id ID = *CurrentID++;
    if (AppType == app_type::WEBSITE) SetBit(&ID, APP_TYPE_BIT_INDEX); 
    
    return ID;
}

app_info GetActiveAppInfo(arena *Arena)
{
    app_info AppInfo = {app_type::NONE};
    
    platform_window_handle Window = PlatformGetActiveWindow();
    if (Window)
    {
        char *ProgramPathCString = PlatformGetProgramFromWindow(Arena, Window);
        if (ProgramPathCString)
        {
            string ProgramPath = MakeString(ProgramPathCString);
            string Filename = StringFilenameFromPath(ProgramPath);
            StringRemoveExtension(&Filename);
            if (Filename.Length)
            {
                AppInfo.Type = app_type::PROGRAM;
                AppInfo.ProgramPath = ProgramPath;
                
                if (StringEquals(Filename, StringLiteral("firefox")))
                {
                    char *UrlCString = PlatformFirefoxGetUrl(Arena, Window);
                    string Url = MakeString(UrlCString);
                    string Host = UrlParseHost(Url);
                    if (Host.Length > 0)
                    {
                        AppInfo.Type = app_type::WEBSITE;
                        AppInfo.WebsiteHost = Host;
                    }
                }
            }
        }
    }
    
    return AppInfo;
}

app_id GetAppID(arena *Arena, hash_table *AppTable, u32 *CurrentID, app_info Info)
{
    app_id ID = 0;
    
    // Share hash table
    hash_node *Slot = AppTable->GetItemSlot(Info.Name);
    if (!Slot)
    {
        // Error no space in hash table (should never realistically happen)
        // Return ID of NULL App?
        Assert(0);
    }
    else if (AppTable->ItemExists(Slot))
    {
        ID = Slot->Value;
    }
    else 
    {
        ID = GenerateAppID(CurrentID, Info.Type);
        
        // Copy string
        string AppName = PushString(Arena, Info.Name);
        AppTable->AddItem(Slot, AppName, ID);
    }
    
    Assert((Info.Type == app_type::PROGRAM && ((ID & (1 << APP_TYPE_BIT_INDEX)) == 0)) ||
           (Info.Type == app_type::WEBSITE && ((ID & (1 << APP_TYPE_BIT_INDEX)) != 0)));
    
    return ID;
}


void AddOrUpdateRecord(record *Records, u64 *RecordCount, app_id ID, date::sys_days Date, s64 DurationMicroseconds)
{
    // Loop backwards searching through Records with passed Date for record marching our ID
    for (u64 RecordIndex = *RecordCount - 1; 
         RecordIndex != static_cast<u64>(-1); 
         --RecordIndex)
    {
        record *ExistingRecord = Records + RecordIndex;
        if (ExistingRecord->Date != Date)
        {
            // No matching ID in the Records in this Date
            record *NewRecord = Records + *RecordCount;
            NewRecord->ID = ID;
            NewRecord->Date = Date;
            NewRecord->DurationMicroseconds = DurationMicroseconds;
            
            *RecordCount += 1;
            break;
        }
        
        if (ExistingRecord->ID == ID)
        {
            ExistingRecord->DurationMicroseconds += DurationMicroseconds;
            break;
        }
    }
}

void Update(monitor_state *State, s64 dtMicroseconds)
{
    date::sys_days Date = GetLocalTime();
    
    if (!State->IsInitialised)
    {
        //create_world_icon_source_file("c:\\dev\\projects\\monitor\\build\\world.png", "c:\\dev\\projects\\monitor\\code\\world_icon.cpp", ICON_SIZE);
        
        
        // Do i need this to construct all vectors etc, as opposed to just zeroed memory?
        *State = {};
        
        u32 Alignment = 8;
        State->PermanentArena = MakeArena(MB(100), Alignment);
        State->TemporaryArena = MakeArena(KB(64), Alignment);
        
        temp_memory TempMemory = BeginTempArena(&State->TemporaryArena);
        
        char *ExecutablePath = PlatformGetExecutablePath(&State->TemporaryArena );
        string ExeDirectory = MakeString(ExecutablePath);
        if (ExeDirectory.Length == 0)
        {
            Assert(0);
        }
        
        ExeDirectory = StringPrefix(ExeDirectory, StringFindLastSlash(ExeDirectory) + 1);
        
        string SaveFileName = StringLiteral("savefile.mbf");
        string TempSaveFileName = StringLiteral("temp_savefile.mbf");
        
        // or just pass allocator?
        string_builder FileBuilder = StringBuilder(&State->PermanentArena, ExeDirectory.Length + SaveFileName.Length + 1); 
        FileBuilder.Append(ExeDirectory);
        FileBuilder.Append(SaveFileName);
        FileBuilder.NullTerminate();
        string SaveFilePath = FileBuilder.GetString();
        
        string_builder TempFileBuilder = StringBuilder(&State->PermanentArena, ExeDirectory.Length + TempSaveFileName.Length + 1); 
        TempFileBuilder.Append(ExeDirectory);
        TempFileBuilder.Append(TempSaveFileName);
        TempFileBuilder.NullTerminate();
        string TempSaveFilePath = TempFileBuilder.GetString();
        
        if (PlatformFileExists(&State->TemporaryArena, SaveFilePath.Str))
        {
            //if (read_from_MBF(apps, day_list, settings, State->savefile_path))
        }
        else
        {
            //apps->local_program_ids = std::unordered_map<String, App_Id>(50);
            //apps->website_ids       = std::unordered_map<String, App_Id>(50);
            
            // Can be used with temp_arena or arena
            
            //apps->local_programs.reserve(50);
            //apps->websites.reserve(50);
            //apps->next_program_id = LOCAL_PROGRAM_ID_START;
            //apps->next_website_id = WEBSITE_ID_START;
            
            //init_arena(&apps->name_arena, Kilobytes(10), Kilobytes(10));
            //init_arena(&day_list->record_arena, MAX_DAILY_RECORDS_MEMORY_SIZE, MAX_DAILY_RECORDS_MEMORY_SIZE);
            //init_arena(&settings->keyword_arena, MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE);
            
            // add fake days before current_date
#if FAKE_RECORDS
            debug_add_records(apps, day_list, current_date);
#endif
            //start_new_day(day_list, current_date);
            
            //settings->misc = Misc_Options::default_misc_options();
            
            // Keywords must be null terminated when given to platform gui
            //add_keyword(settings, "CITS3003");
            //add_keyword(settings, "youtube");
            //add_keyword(settings, "docs.microsoft");
            //add_keyword(settings, "google");
            //add_keyword(settings, "github");
        }
        
        // We won't have many programs but may have many websites
        // 24 byte entries * 10000 Entries = 240 KB
        u64 AppTableItemCount = 10000;
        State->AppTable = MakeHashTable(&State->PermanentArena, AppTableItemCount); 
        
        State->CurrentID = 1; // Leave ID 0 unused
        
        u64 MaxRecordCount = 100000;
        State->Records = Allocate(&State->PermanentArena, MaxRecordCount, record);
        State->RecordCount = 0;
        
        State->IsInitialised = true;
        
        EndTempMemory(TempMemory);
    }
    
    temp_memory TempMemory = BeginTempArena(&State->TemporaryArena);
    
    app_info AppInfo = GetActiveAppInfo(&State->TemporaryArena);
    if (AppInfo.Type != NONE)
    {
        app_id ID = GetAppID(&State->PermanentArena, &State->AppTable, &State->CurrentID, AppInfo);
        if (ID)
        {
            u64 MaxRecordCount = 100000;
            Assert(State->RecordCount < MaxRecordCount);
            AddOrUpdateRecord(State->Records, &State->RecordCount, ID, Date, dtMicroseconds);
        }
    }
    
    EndTempMemory(TempMemory);
    
    CheckArena(&State->PermanentArena);
    CheckArena(&State->TemporaryArena);
}


#if 0

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
update2(Monitor_State *state, SDL_Window *window, s64 dt_microseconds, Window_Event window_event)
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


#endif
