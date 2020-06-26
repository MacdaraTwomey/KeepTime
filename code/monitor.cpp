#include <unordered_map>

#include "cian.h"
#include "monitor_string.h"
#include "helper.h"
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

#include "icon.cpp"    // Deals with win32 icon interface TODO: Move platform dependent parts to win
#include "network.cpp"
#include "ui.cpp"

#define ICON_SIZE 32

// TODO: When finished leave asserts and make them write to a log file instead of calling debugbreak()

// Cost of rendering imgui is generally lower than the cost of building the ui elements - "Omar"
// can use array textures maybe, GL_TEXTURE_2D_ARRAY texture target. Textures must be same size

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
    Day_View day_view = {};
    //day_view.start_date = start_date;
    //call  set range...
    //day_view.period = initial_period;
    //day_view.accumulate = (period == 1) ? false : true;
    
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
set_day_view_range(Day_View *day_view, date::sys_days start_date, date::sys_days end_date)
{
    // TODO:
#if 0
    Assert(day_view->days.size() > 0);
    Assert(start_date >= end_date);
    
    // date::sys_days start_date = current_date - date::days{period - 1};
    
    i32 end_range = -1;
    i32 start_range = -1;
    
    bool have_end = false;
    for (i32 day_index = day_view->days.size() - 1; day_index >= 0; --day_index)
    {
        date::sys_days d = day_view->days[day_index].date;
        if (d <= end_date && !have_end)
        {
            end_range = day_index;
            have_end = true;
        }
        if (d >= start_date)
        {
            // Start should be found after end range
            start_range = day_index;
            Assert(have_end);
            break;
        }
    }
    
    
    if (start_range == day_view->days.size() - 1 && start_date != day_view->days[day_index].date;)
    {
        day_view->has_records = false;
    }
#endif
    
    //day_view->start_range = start_range;
    //day_view->range_count = range_count;
    //day_view->accumulate = (start_date == end_date);
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


void init_database(Database *database)
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
    start_new_day(&database->day_list, floor<date::days>(System_Clock::now()));
    
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

const char *
day_suffix(int day)
{
	Assert(day >= 1 && day <= 31);
	static const char *suffixes[] = { "st", "nd", "rd", "th" };
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
	static const char *months[] = { "January","February","March","April","May","June",
		"July","August","September","October","November","December" };
	return months[month];
}

s32
remove_duplicate_and_empty_keyword_strings(char(*keywords)[MAX_KEYWORD_SIZE])
{
	// NOTE: This assumes that keywords that are 'empty' should have no garbage data (dear imgu needs to set
	// empty array slots to '\0';
	s32 total_count = 0;
	for (s32 l = 0, r = MAX_KEYWORD_COUNT - 1; l < r;)
	{
		if (keywords[l][0] == '\0') // is empty string
		{
			if (keywords[r][0] == '\0')
			{
				r--;
			}
			else
			{
				strcpy(keywords[l], keywords[r]);
				total_count += 1;
				l++;
				r--;
			}
		}
		else
		{
			l++;
			total_count += 1;
		}
	}
    
#if MONITOR_DEBUG
	// TODO: THIS IS A TEST
	for (s32 i = 0; i < total_count; ++i)
	{
		Assert(keywords[i][0] != '\0');
	}
#endif
    
	// remove dups
	for (s32 i = 0; i < total_count; ++i)
	{
		for (s32 j = i + 1; j < total_count; ++j)
		{
			if (strcmp(keywords[i], keywords[j]) == 0)
			{
				strcpy(keywords[i], keywords[total_count - 1]);
				total_count -= 1;
                
				// Re-search from same index (with new string)
				i -= 1;
				break;
			}
		}
	}
    
	return total_count;
}

void
update_settings(Edit_Settings *edit_settings, Settings *settings, Database *database)
{
    if (!edit_settings->update_settings) return;
    
    // Empty input boxes are just zeroed array
    i32 pending_count = remove_duplicate_and_empty_keyword_strings(edit_settings->pending);
    
    App_Id pending_keyword_ids[MAX_KEYWORD_COUNT] = {};
    
    // scan all keywords for match
    for (int i = 0; i < pending_count; ++i)
    {
        bool got_an_id = false;
        for (int j = 0; j < settings->keywords.size(); ++j)
        {
            if (string_equals(settings->keywords[j].str, edit_settings->pending[i]))
            {
                // Get match's id
                pending_keyword_ids[i] = settings->keywords[j].id;
                break;
            }
        }
    }
    
    for (int i = 0; i < settings->keywords.size();++i)
    {
        Assert(settings->keywords[i].str.str);
        free(settings->keywords[i].str.str);
    }
    
    settings->keywords.clear();
    settings->keywords.reserve(pending_count);
    for (int i = 0; i < pending_count; ++i)
    {
        // pending keywords without an id (id = 0) will be assigned an id when zero passed to add_keyword
        add_keyword(settings->keywords, database, edit_settings->pending[i], pending_keyword_ids[i]);
    }
    
    settings->misc_options = edit_settings->misc_options;
}

void 
HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool
do_settings_popup(Edit_Settings *edit_settings)
{
    bool open = true;
    
	// TODO: Remove or don't allow
	// - all spaces strings
	// - leading/trailing whitespace
	// - spaces in words
	// - unicode characters (just disallow pasting maybe)
	// NOTE:
	// - Put incorrect format text items in red, and have a red  '* error message...' next to "Keywords"
    
    // IF we get lots of kinds of settings (not ideal), can use tabs
    
	// When enter is pressed the item is not active the same frame
	//bool active = ImGui::IsItemActive();
	// TODO: Does SetKeyboardFocusHere() break if box is clipped?
	//if (give_focus == i) ImGui::SetKeyboardFocusHere();
    
    HelpMarker("As with every widgets in dear imgui, we never modify values unless there is a user interaction.\nYou can override the clamping limits by using CTRL+Click to input a value.");
	ImGui::Text("Keywords");
    
    {
        ImGui::BeginChild("List", ImVec2(ImGui::GetWindowWidth()*0.5, ImGui::GetWindowHeight() * 0.6f));
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.90f);
        for (s32 i = 0; i < edit_settings->input_box_count; ++i)
        {
            ImGui::PushID(i);
            bool enter_pressed = ImGui::InputText("", edit_settings->pending[i], array_count(edit_settings->pending[i]));
            ImGui::PopID();
        }
        ImGui::PopItemWidth();
        ImGui::EndChild();
    }
    
    {
        ImGui::SameLine();
        ImGui::BeginChild("Misc Options", ImVec2(ImGui::GetWindowWidth()*0.5, ImGui::GetWindowHeight() * 0.6));
        
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.30f);
        // TODO: Do i need to use bool16 for this (this is workaround)
        bool selected = edit_settings->misc_options.run_at_system_startup;
        ImGui::Checkbox("Run at startup", &selected);
        edit_settings->misc_options.run_at_system_startup = selected;
        
        // ImGuiInputTextFlags_CharsDecimal ?
        int freq = (int)edit_settings->misc_options.poll_frequency_milliseconds;
        ImGui::InputInt("Update frequency ms", &freq); // maybe a slider is better
        
        ImGui::SliderInt("Update frequency ms##2", &freq, 1000, 10000, "%dms");
        
        // Only if valid number
        if (freq < 1000)
        {
            ImGui::SameLine();
            ImGui::Text("Frequency must be between 1000 and whatever");
        }
        
        // TODO: Save setting, but dont change later maybe? Dont allow close settings?
        edit_settings->misc_options.poll_frequency_milliseconds = freq;
        
        char *times[] = {
            "12:00 AM",
            "12:15 AM",
            "12:30 AM",
            "etc..."
        };
        
        // returns true for frame that an option is selected
        bool ss = ImGui::Combo("Monitor start time", &edit_settings->start_time_item, times, array_count(times));
        bool ss22 = ImGui::Combo("Monitor end time", &edit_settings->end_time_item, times, array_count(times));
        
        // I think this is right
        edit_settings->misc_options.poll_start_time = edit_settings->start_time_item * 15;
        edit_settings->misc_options.poll_end_time = edit_settings->end_time_item * 15;
        
        ImGui::PopItemWidth();
        
        ImGui::EndChild();
    }
    
    {
        ImGui::Separator();
        ImGui::Spacing();
        s32 give_focus = -1;
        if (ImGui::Button("Add keyword..."))
        {
        }
        ImGui::SameLine();
        if (ImGui::Button("Ok"))
        {
            edit_settings->update_settings = true;
            ImGui::CloseCurrentPopup();
            open = false;
            
            // if (invalid settings) make many saying
            // "invalid settings"
            // [cancel][close settings without saving]
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            // Don't update keywords
            edit_settings->update_settings = false;
            ImGui::CloseCurrentPopup();
            open = false;
        }
    }
    
    // TODO: Revert button?
    return open;
}

void draw_ui_and_update_state(SDL_Window *window, Monitor_State *state, Database *database, date::sys_days current_date, Day_View *day_view)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();
    
	bool show_demo_window = true;
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);
    
	ImGuiStyle& style = ImGui::GetStyle();
	//style.FrameRounding = 0.0f; // 0 to 12 ? 
	style.WindowBorderSize = 1.0f; // or 1.0
	// style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
	style.FrameBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	//style.WindowRounding = 0.0f;
    
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_MenuBar
		| ImGuiWindowFlags_NoResize;
    
	ImGui::SetNextWindowPos(ImVec2(0, 0), true);
	ImGui::SetNextWindowSize(ImVec2(850, 690), true);
	ImGui::Begin("Main window", nullptr, flags);
    
	bool open_settings = false;
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Settings##menu"))
		{
			// NOTE: Id might be different because nexted in menu stack, so might be label + menu
			open_settings = true;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	if (open_settings)
		ImGui::OpenPopup("Settings");
    
    // TODO: Make variables for settings window and child windows width height, 
    // and maybe base off users monitor w/h
    // TODO: 800 is probably too big for some screens (make this dynamic)
	ImGui::SetNextWindowSize(ImVec2(850, 600));
	if (ImGui::BeginPopupModal("Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
        if (ImGui::IsWindowAppearing())
        {
            state->edit_settings = (Edit_Settings *)calloc(1, sizeof(Edit_Settings));
            
            // Not sure if should try to leave only one extra or all possible input boxes
            state->edit_settings->input_box_count = MAX_KEYWORD_COUNT;
            state->edit_settings->misc_options = state->settings.misc_options;
            
            // TODO: Should be a better way than this
            state->edit_settings->start_time_item = state->settings.misc_options.poll_start_time / 15;
            state->edit_settings->end_time_item = state->settings.misc_options.poll_end_time / 15;
            
            Assert(state->settings.keywords.size() <= MAX_KEYWORD_COUNT);
            for (s32 i = 0; i < state->settings.keywords.size(); ++i)
            {
                Assert(state->settings.keywords[i].str.length + 1 <= MAX_KEYWORD_SIZE);
                strcpy(state->edit_settings->pending[i], state->settings.keywords[i].str.str);
            }
        }
        
        bool open = do_settings_popup(state->edit_settings);
        if (!open)
        {
            update_settings(state->edit_settings, &state->settings, database);
            free(state->edit_settings);
        }
        
        ImGui::EndPopup(); // only close if BeginPopupModal returns true
    }
    
    // date:: overrides operator int() etc ...
    date::year_month_day ymd_date{ current_date };
    int y = int(ymd_date.year());
    int m = unsigned(ymd_date.month());
    int d = unsigned(ymd_date.day());
    
    ImGui::Text("%i%s %s, %i", d, day_suffix(d), month_string(m), y);
    
    static i32 period = 1;
    if (ImGui::Button("Day", ImVec2(ImGui::GetWindowSize().x*0.2f, 0.0f)))
    {
        period = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Week", ImVec2(ImGui::GetWindowSize().x*0.2f, 0.0f)))
    {
        period = 7;
    }
    ImGui::SameLine();
    if (ImGui::Button("Month", ImVec2(ImGui::GetWindowSize().x*0.2f, 0.0f)))
    {
        period = 30;
    }
    
    //set_range(day_view, period, current_date);
    
    // To allow frame border
    ImGuiWindowFlags child_flags = 0;
    ImGui::BeginChildFrame(55, ImVec2(ImGui::GetWindowSize().x * 0.9, ImGui::GetWindowSize().y * 0.7), child_flags);
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    Day *today = &day_view->days.back();
    
    time_type sum_duration = 0;
    
    if (today->record_count > 0)
    {
        i32 record_count = today->record_count;
        
        Record sorted_records[MaxDailyRecords];
        memcpy(sorted_records, today->records, sizeof(Record) * record_count);
        
        std::sort(sorted_records, sorted_records + record_count, [](Record &a, Record &b) { return a.duration > b.duration; });
        
        time_type max_duration = sorted_records[0].duration;
        
        ImVec2 max_size = ImVec2(ImGui::CalcItemWidth() * 0.85, ImGui::GetFrameHeight());
        
        for (i32 i = 0; i < record_count; ++i)
        {
            Record &record = sorted_records[i];
            
            // DEBUG:
            Icon_Asset *icon = 0;
            if (record.id == 0)
            {
                icon = database->icons + database->default_icon_index;
            }
            else
            {
                // TODO: Check if pos of bar or image will be clipped entirely, and maybe skip rest of loop
                icon = get_icon_asset(database, record.id);
            }
            
            // Don't need to bind texture before this because imgui binds it before draw call
            ImGui::Image((ImTextureID)(intptr_t)icon->texture_handle, ImVec2((float)icon->bitmap.width, (float)icon->bitmap.height));
            
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
            
            // try with and without id == 0 record
            sum_duration += record.duration;
            
            // This is a hash table search
            // DEBUG
            char *name = "sdfsdf";
            if (record.id == 0)
            {
                name = "Not polled time";
            }
            else
            {
                name = database->app_names[record.id].short_name.str;
            }
            
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
    
    
    float red[] = {255.0f, 0, 0};
    float black[] = {0, 0, 0};
    
    double total_runtime_seconds = (double)state->total_runtime / 1000;
    double sum_duration_seconds = (double)sum_duration / 1000;
    auto now = Steady_Clock::now();
    time_type start_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - state->startup_time).count();
    double time_since_startup_seconds = (double)start_time / 1000;
    
    double diff_seconds2 = (double)(start_time - sum_duration) / 1000;
    
    
    ImGui::Text("Total runtime:        %.5fs", total_runtime_seconds);
    ImGui::Text("Sum duration:        %.5fs", sum_duration_seconds);
    
    
    ImGui::SameLine();
    memcpy((void *)style.Colors, red, 3 * sizeof(float));
    ImGui::Text("diff: %llu", state->total_runtime - sum_duration);
    memcpy((void *)style.Colors, black, 3 * sizeof(float));
    
    
    ImGui::Text("time since startup: %.5fs", time_since_startup_seconds);
    
    ImGui::SameLine();
    memcpy((void *)style.Colors, red, 3 * sizeof(float));
    ImGui::Text("diff between time since startup and sum duration: %.5fs", diff_seconds2);
    memcpy((void *)style.Colors, black, 3 * sizeof(float));
    
    
    
    
    
    
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
    
    
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
#if 0
    if (duration_seconds < 60)
        // Seconds
        else if (duration_seconds < 3600)
        // Minutes
        snprintf(text, array_count(text), "%llum", duration_seconds / 60);
    else
        // Hours
        snprintf(text, array_count(text), "%lluh", duration_seconds / 3600);
#endif
    
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
        
        init_database(&state->database);
        
        Misc_Options options = {};
        options.poll_start_time = 11;
        options.poll_end_time = 22;
        options.run_at_system_startup = true;
        options.poll_frequency_milliseconds = 100;
        
        state->settings.misc_options = options;
        
        // Keywords must be null terminated when given to platform gui
        add_keyword(state->settings.keywords, &state->database, "CITS3003");
        add_keyword(state->settings.keywords, &state->database, "youtube");
        add_keyword(state->settings.keywords, &state->database, "docs.microsoft");
        add_keyword(state->settings.keywords, &state->database, "eso-community");
        
        state->accumulated_time = 0;
        state->total_runtime = 0;
        state->startup_time = Steady_Clock::now();
        
        state->is_initialised = true;
    }
    
    Database *database = &state->database;
    
    // TODO: Just use google or duckduckgo service for now
    
    if (window_status & Window_Just_Hidden)
    {
        // delete day view (if exists)
    }
    
    date::sys_days current_date = floor<date::days>(System_Clock::now());
    
    if (current_date != database->day_list.days.back().date)
    {
        start_new_day(&database->day_list, current_date);
    }
    
    // TODO: Probably want to use wall clock time not cpu time
    // or maybe store everything in ticks, and only convert to time when it needs to be displayed
    //    - works fine as long user is on the same comp 9else you have to convert saved ticks to account for           
    //      different tick/sec of new computer (this is doable, but can also ignore this conversion and just say 
    //      use one computer for now).
    
    // TODO: I think i am losing time, when adding durations (which is what the dts are), or
    // maybe its just a problem of adding milliseconds, and should instead use nanoseconds
    
    // nanoseconds per year = 3.1556952E+16
    // u64 =  1.8446744e+19 (which is hundreds of years in nanos)
    
    // problem std::nanoseconds etc is a signed integral type of at least 64 bits
    // and i am using u64 to store duration
    
    // TODO: Also compare steady clock time since startup to system clock in ui
    
    state->accumulated_time += dt;
    state->total_runtime += dt;
    time_type poll_window_freq = 100;
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
        // Save a freeze frame of the currently saved days.
        
        // setup day view and 
    }
    
    Day_View day_view = get_day_view(&database->day_list);
    
    if (window_status & Window_Visible)
    {
        draw_ui_and_update_state(window, state, database, current_date, &day_view);
    }
    
	free_day_view(&day_view);
    
}

