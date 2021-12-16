
#include "base.h"
#include "platform.h"
#include "monitor_string.h"
#include "date.h"
#include "helper.h"
#include "monitor.h"

bool UrlSchemeIsValid(string Scheme)
{
    bool IsValid = IsAlpha(StringGetChar(Scheme, 0));
    if (IsValid)
    {
        for (u64 i = 0; i < Scheme.Length; ++i)
        {
            char c = Scheme[i];
            bool ValidChar = IsAlphaNumeric(c) || c == '+' || c == '-' || c == '.';
            if (!ValidChar)
            {
                IsValid = false;
                break;
            }
        }
    }
    
    return IsValid;
}

string UrlParseScheme(string *Url)
{
    // "A URL-scheme string must be one ASCII alpha, followed by zero or more of ASCII alphanumeric, U+002B (+), U+002D (-), and U+002E (.)" - https://url.spec.whatwg.org/#url-scheme-string
    // such as http, file, dict
    
    string Scheme = {};
    
    u64 ColonPos = StringFindChar(*Url, ':');
    if (ColonPos < Url->Length)
    {
        u64 SchemeLength = ColonPos;
        string TestScheme = StringPrefix(*Url, SchemeLength);
        if (UrlSchemeIsValid(TestScheme))
        {
            Scheme = TestScheme;
            StringSkip(Url, SchemeLength + 1); // skip scheme and ':'
        }
    }
    
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
    // Here square brackets mean optional 
    // authority = [userinfo "@"] host [":" port] 
    
    // IPv6 addresses are enclosed by square brackets
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
    if (!StringContainsChar(Url, ' '))
    {
        string Scheme = UrlParseScheme(&Url);
        if (StringsAreEqual(Scheme, StringLit("https")) || 
            StringsAreEqual(Scheme, StringLit("http")))
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

bool StringMatchesKeyword(string String, settings *Settings)
{
    // O(KeywordCount * KeywordLength * StringLength) 
    for (u32 i = 0; i < Settings->KeywordCount; ++i)
    {
        if (StringContainsSubstr(String, Settings->Keywords[i]))
        {
            // TODO: Maybe cache last few keywords, if it doesn't match cache?
            // maybe shuffle others down to avoid first and last being swapped and re-swapped repeatedly.
            // However, don't really want to change order of keywords in the settings window. So maybe settings should be able to change it's order but also has a array on index values that it maintains so it can copy into edit_settings in original order.
            
            return true;
        }
    }
    
    return false;
}

app_info GetActiveAppInfo(arena *Arena, settings *Settings)
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
                
                keyword_options KeywordOptions = Settings->MiscOptions.KeywordOptions;
                
                if (KeywordOptions != KeywordOptions_None && 
                    StringsAreEqualCaseInsensitive(Filename, StringLit("firefox")))
                {
                    char *UrlCString = PlatformFirefoxGetUrl(Arena, Window);
                    string Url = MakeString(UrlCString);
                    string Host = UrlParseHost(Url);
                    if (Host.Length > 0)
                    {
                        if ((KeywordOptions == KeywordOptions_All) ||
                            ((KeywordOptions == KeywordOptions_Custom) && StringMatchesKeyword(Host, Settings)))
                        {
                            // // TODO(mac): URLs are case sensitive but the domain name part is not, not sure if we should store the host non-capitalised, and if we should or shouldn't case insensitive match keywords.
                            StringToLower(&Host);
                            AppInfo.Type = app_type::WEBSITE;
                            AppInfo.WebsiteHost = Host;
                        }
                    }
                }
            }
        }
    }
    
    return AppInfo;
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
        State->PermanentArena = CreateArena(MB(100), GB(1));
        State->TemporaryArena = CreateArena(KB(64), GB(1));
        
        temp_memory TempMemory = BeginTempMemory(&State->TemporaryArena);
        
        char *ExecutablePath = PlatformGetExecutablePath(&State->TemporaryArena );
        string ExeDirectory = MakeString(ExecutablePath);
        if (ExeDirectory.Length == 0)
        {
            Assert(0);
        }
        
        ExeDirectory = StringPrefix(ExeDirectory, StringFindLastSlash(ExeDirectory) + 1);
        
        string SaveFileName = StringLit("savefile.mbf");
        string TempSaveFileName = StringLit("temp_savefile.mbf");
        
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
            
        } // NO SAVEFILE DEFAULTS
        
        // We won't have many programs but may have lots of websites (so is this enough?)
        // 24 byte entries * 10000 Entries = 240 KB
        u64 AppItemCount = 10000;
        State->AppNameMap = CreateStringMap(&State->PermanentArena, AppItemCount); 
        
        State->CurrentID = 1; // Leave ID 0 unused
        
        State->ProgramPaths = Allocate(&State->PermanentArena, AppItemCount, c_string);
        State->ProgramCount = 0;
        
        u64 MaxRecordCount = 100000;
        State->Records = Allocate(&State->PermanentArena, MaxRecordCount, record);
        State->RecordCount = 0;
        
        settings *Settings = &State->Settings;
        *Settings = {};
        Settings->Arena = &State->PermanentArena;
        Settings->MiscOptions = DefaultMiscOptions();
        Settings->Keywords = Allocate(Settings->Arena, MAX_KEYWORD_COUNT, string);
        Settings->KeywordCount = 0;
        
        AddKeyword(Settings, StringLit("google"));
        AddKeyword(Settings, StringLit("github"));
        
        // TODO: Arena is not sorted out correctly
        gui *GUI = &State->GUI;
        *GUI = CreateGUI();
        
        State->IsInitialised = true;
        
        EndTempMemory(TempMemory);
    }
    
    temp_memory TempMemory = BeginTempMemory(&State->TemporaryArena);
    
    app_info AppInfo = GetActiveAppInfo(&State->TemporaryArena, &State->Settings);
    if (AppInfo.Type != NONE)
    {
        app_id ID = GetAppID(&State->PermanentArena, &State->AppNameMap, 
                             State->ProgramPaths, &State->ProgramCount, 
                             &State->CurrentID, AppInfo);
        if (ID)
        {
            u64 MaxRecordCount = 100000;
            Assert(State->RecordCount < MaxRecordCount);
            UpdateRecords(State->Records, &State->RecordCount, ID, Date, dtMicroseconds);
        }
    }
    
    EndTempMemory(TempMemory);
    
    // TODO: Make general purpose allocator, give it a block of memory (expanding?)
    // and alloc and free from it.
    // Is the best solution to some memory allocation patterns
    
    // Could all this inside an UpdateGUI() call
    // Could pass in windows status that is updated by events rather than querying OS for status every update
    gui *GUI = &State->GUI;
    if (PlatformIsWindowHidden())
    {
        if (GUI->IsLoaded)
        {
            UnloadGUI(GUI);
        }
    }
    else
    {
        if (!GUI->IsLoaded)
        {
            record *Records = State->Records;
            
            // TODO: Make the NULL record with duration = 0, date = 0?, ID = 0
            date::sys_days OldestDate = date::sys_days{date::year{2012}/date::month{12}/date::day{1}};
            date::sys_days NewestDate = date::sys_days{date::year{2012}/date::month{12}/date::day{5}};
            if (State->RecordCount > 0)
            {
                OldestDate = Records[0].Date;
                NewestDate = Records[State->RecordCount - 1].Date;
            }
            
            LoadGUI(GUI, Date, OldestDate, NewestDate);
        }
        
        DrawGUI(State, &State->GUI, &State->Settings);
    }
    
    CheckArena(&State->PermanentArena);
    CheckArena(&State->TemporaryArena);
}

#if 0

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
