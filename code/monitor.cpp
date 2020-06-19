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
#include "icon.cpp"    // Deals with win32 icon interface TODO: Move platform dependent parts to win

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
    database->names = std::unordered_map<Program_Id, Program_Name>(80);
    
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

void add_keyword_with_id(Database *database, char *str, Program_Id id)
{
    Assert(database->keyword_count < MAX_KEYWORD_COUNT);
    if (database->keyword_count < MAX_KEYWORD_COUNT)
    {
        Keyword k;
        k.id = id;
        k.str = make_string_size_cap(database->keyword_buf[database->keyword_count],
                                     strlen(str),
                                     array_count(database->keyword_buf[database->keyword_count]));
        strcpy(k.str.str, str);
        
        database->keywords[database->keyword_count] = k;
        database->keyword_count += 1;
    }
}

void add_keyword(Database *database, char *str)
{
    Assert(database->keyword_count < MAX_KEYWORD_COUNT);
    if (database->keyword_count < MAX_KEYWORD_COUNT)
    {
        Program_Id id = make_id(database, Record_Firefox);
        add_keyword_with_id(database, str, id);
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
update_days_records(Day *day, u32 id, time_type dt)
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
get_icon_from_database(Database *database, Program_Id id)
{
    if (is_firefox(id))
    {
        // HACK: We just use firefox icon
        id = database->firefox_id;
        //get_favicon_from_website(url);
    }
    
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
            if  (database->names.count(id) > 0)
            {
                String path = database->names[id].long_name;
                if (path.length > 0)
                {
                    Bitmap *destination_icon = &database->icons[id];
                    bool success = platform_get_icon_from_executable(path.str, 32, destination_icon, true);
                    if (success)
                    {
                        // TODO: Maybe mark old paths that couldn't get correct icon for deletion.
                        return destination_icon;
                    }
                }
            }
            
            // DEBUG use default
            u32 index = id % array_count(database->ms_icons);
            Bitmap *ms_icon = &database->ms_icons[index];
            if (ms_icon->pixels) return ms_icon;
        }
    }
    
    return nullptr;
}

// TODO: Do we need/want dt here,
// maybe want to split up update_days
void
poll_windows(Database *database, time_type dt)
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
    
    // string_to_lower(&program_name);
    
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
        bool got_url = platform_get_firefox_url(window, url_buf, array_count(url_buf), &url_len);
        if (got_url)
        {
            String url = make_string_size_cap(url_buf, url_len, array_count(url_buf));
            if (url.length > 0)
            {
                Keyword *keyword = search_url_for_keywords(url, database->keywords, database->keyword_count);
                if (keyword)
                {
                    if (database->names.count(keyword->id) == 0)
                    {
                        Program_Name names;
                        names.long_name = copy_alloc_string(url);
                        names.short_name = copy_alloc_string(keyword->str);
                        
                        database->names.insert({keyword->id, names});
                    }
                    
                    record_id = keyword->id;
                    add_to_executables = false;
                }
            }
        }
    }
    
    // Temporary, for when we get firefox website, before we have added firefox program
    if (program_is_firefox && !add_to_executables)
    {
        if (!database->added_firefox)
        {
            Program_Id new_id = make_id(database, Record_Exe);
            
            Program_Name names;
            names.long_name = copy_alloc_string(full_path);
            names.short_name = copy_alloc_string(program_name);
            
            database->names.insert({new_id, names});
            
            // These don't share the same strings for now
            String name_2 = copy_alloc_string(program_name);
            database->programs.insert({name_2, new_id});
            
            database->firefox_id = new_id;
            database->added_firefox = true;
        }
    }
    
    if (add_to_executables)
    {
        if (database->programs.count(program_name) == 0)
        {
            Program_Id new_id = make_id(database, Record_Exe);
            
            Program_Name names;
            names.long_name = copy_alloc_string(full_path);
            names.short_name = copy_alloc_string(program_name);
            
            database->names.insert({new_id, names});
            
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



const char *
day_suffix(int day)
{
    Assert(day >= 1 && day <= 31);
    static const char *suffixes[] = {"st", "nd", "rd", "th"};
    switch (day)
    {
        case 1:
        case 21:
        case 31:
        return suffixes[0];
        break;
        
        case 2:
        case 22:
        return suffixes[1];
        break;
        
        case 3:
        case 23:
        return suffixes[2];
        break;
        
        default:
        return suffixes[3];
        break;
    }
}

const char *
month_string(int month)
{
    Assert(month >= 1 && month <= 12);
    month -= 1;
    static const char *months[] = {"January","February","March","April","May","June",
        "July","August","September","October","November","December"};
    return months[month];
}

void render(SDL_Window *window, Database *database, date::sys_days current_date, Day_View *day_view)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    
    bool show_demo_window = true;
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);
    
    {
        //ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(550, 580), true);
        
        ImGui::Begin("Main window");
        
        // date:: overrides operator int() etc ...
        date::year_month_day ymd_date {current_date};
        int y = int(ymd_date.year());
        int m = unsigned(ymd_date.month());
        int d = unsigned(ymd_date.day());
        
        ImGui::Text("%i%s %s, %i", d, day_suffix(d), month_string(m), y);
        
        static i32 period = 1;
        if (ImGui::Button("Day", ImVec2(ImGui::GetWindowSize().x*0.3f, 0.0f))) 
        {
            period = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Week", ImVec2(ImGui::GetWindowSize().x*0.3f, 0.0f)))
        {
            period = 7;
        }
        ImGui::SameLine();
        if (ImGui::Button("Month", ImVec2(ImGui::GetWindowSize().x*0.3f, 0.0f)))
        {
            period = 30;
        }
        
        set_range(day_view, period, current_date);
        
        ImGui::BeginChild("Graph");
        
        // need child window?
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        //float values[] ={ 62,75,75,29,19,57,24,87,89,30,48,81,33,85,87,45,48,70,58,70,29,43,49,53,59,21,81,32,72,100};
        
        
        Assert(day_view->day_count > 0);
        Day *today = day_view->days[day_view->day_count-1];
        
        if (today->record_count >= 0)
        {
            i32 record_count = today->record_count;
            
            Program_Record sorted_records[MaxDailyRecords];
            memcpy(sorted_records, today->records, sizeof(Program_Record) * record_count);
            
            std::sort(sorted_records, sorted_records + record_count, [](Program_Record &a, Program_Record &b){ return a.duration > b.duration; });
            
            time_type max_duration = sorted_records[0].duration;
            
            ImVec2 max_size = ImVec2(ImGui::CalcItemWidth() * 0.85, ImGui::GetFrameHeight());
            
            for (i32 i = 0; i < record_count; ++i)
            {
                Program_Record &record = sorted_records[i];
                
                // TODO: Dont recreate textures...
                Bitmap *icon = get_icon_from_database(database, record.id);
                if (icon)
                {
                    GLuint image_texture;
                    glGenTextures(1, &image_texture); // I think this can faiil if out of texture mem
                    glBindTexture(GL_TEXTURE_2D, image_texture);
                    
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, icon->width, icon->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, icon->pixels);
                    
                    ImGui::Image((ImTextureID)(intptr_t)image_texture, ImVec2((float)icon->width, (float)icon->height));
                }
                
                ImGui::SameLine();
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                
                ImVec2 bar_size = max_size;
                bar_size.x *= ((float)record.duration / (float)max_duration);
                
                if (bar_size.x > 0)
                {
                    ImVec2 p1 = ImVec2(p0.x + bar_size.x, p0.y + bar_size.y);
                    ImU32 col_a = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                    ImU32 col_b = ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                    draw_list->AddRectFilledMultiColor(p0, p1, col_a, col_b, col_b, col_a);
                }
                
                char *name = database->names[record.id].short_name.str;
                double seconds = (double)record.duration / 1000;
                
                // Get cursor pos before writing text
                ImVec2 pos = ImGui::GetCursorScreenPos();
                
                // TODO: Limit name length
                ImGui::Text("%s", name); // TODO: Ensure bars unique id
                
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowSize().x * 0.90);
                ImGui::Text("%.2fs", seconds); // TODO: Ensure bars unique id
                
                ImGui::SameLine();
                ImGui::InvisibleButton("##gradient1", ImVec2(bar_size.x + 10, bar_size.y));
            }
        }
        
        
        ImGui::EndChild();
        
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }
    
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


void
update(Monitor_State *state, SDL_Window *window, time_type dt, u32 window_status)
{
    if (!state->is_initialised)
    {
        //remove(global_savefile_path);
        //remove(global_debug_savefile_path);
        
        *state = {};
        state->header = {};
        
        state->database = {};
        init_database(&state->database);
        
        // Keywords must be null terminated when given to platform gui
        add_keyword(&state->database, "CITS3003");
        add_keyword(&state->database, "youtube");
        add_keyword(&state->database, "docs.microsoft");
        add_keyword(&state->database, "eso-community");
        
        start_new_day(&state->database, floor<date::days>(System_Clock::now()));
        
        state->day_view = {};
        
        state->accumulated_time = dt;
        
        for (int i = 0; i < array_count(state->database.ms_icons); ++i)
        {
            HICON ico = LoadIconA(NULL, MAKEINTRESOURCE(32513 + i));
            win32_get_bitmap_from_HICON(ico, &state->database.ms_icons[i]);
        }
        
        state->is_initialised = true;
    }
    
    Database *database = &state->database;
    
    // TODO: Separate platform and icon code in icon.cpp
    
    // TODO: Just use google or duckduckgo service for now
    
    date::sys_days current_date = floor<date::days>(System_Clock::now());
    if (current_date != database->days[state->database.day_count-1].date)
    {
        start_new_day(database, current_date);
    }
    
    // Maybe better if this is all integer arithmetic
    // 1000ms per 1s
    state->accumulated_time += dt;
    time_type poll_window_freq = 100;
    if (state->accumulated_time >= poll_window_freq)
    {
        poll_windows(database, state->accumulated_time);
        state->accumulated_time -= poll_window_freq;
    }
    
    if (window_status & Window_Just_Visible)
    {
        // Save a freeze frame of the currently saved days.
    }
    
    init_day_view(database, &state->day_view);
    
    if (window_status & Window_Visible)
    {
        render(window, &state->database, current_date, &state->day_view);
    }
}

#if 0
if (duration_seconds < 60)
{
    // Seconds
}
else if(duration_seconds < 3600)
{
    // Minutes
    snprintf(text, array_count(text), "%llum", duration_seconds/60);
}
else
{
    // Hours
    snprintf(text, array_count(text), "%lluh", duration_seconds/3600);
}
#endif
