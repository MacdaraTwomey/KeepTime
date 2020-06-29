
// from monitor.cpp
Icon_Asset * get_icon_asset(Database *database, App_Id id);
void add_keyword(std::vector<Keyword> &keywords, Database *database, char *str, App_Id id);
void set_day_view_range(Day_View *day_view, date::sys_days start_date, date::sys_days end_date);
bool is_exe(u32 id);

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

bool 
is_leap_year(int year)
{
    // if year is a integer multiple of 4 (except for years evenly divisible by 100, which are not leap years unless evenly divisible by 400)
    return ((year % 4 == 0) && !(year % 100 == 0)) || (year % 400 == 0);
}

int 
days_in_month(int month, int year)
{
    switch (month)
    {
        case 1: case 3: case 5:
        case 7: case 8: case 10:
        case 12:
        return 31;
        
        case 4: case 6:
        case 9: case 11:
        return 30;
        
        case 2:
        if (is_leap_year(year))
            return 29;
        else
            return 28;
    }
    
    return 0;
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
    bool finished = false;
    
    float keywords_child_height = ImGui::GetWindowHeight() * 0.6f;
    float keywords_child_width = ImGui::GetWindowWidth() * 0.5f;
    
    // TODO: Make variables for settings window and child windows width height, 
    // and maybe base off users monitor w/h
    // TODO: 800 is probably too big for some screens (make this dynamic)
    ImGui::SetNextWindowSize(ImVec2(850, 600));
    if (ImGui::BeginPopupModal("Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
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
        
        // TODO: Copy windows (look at Everything.exe settings) ui style and white on grey colour scheme
        
        // For coloured text
        //
        //ImGui::TextColored(ImVec4(1,1,0,1), "Sailor");
        //
        
        ImGui::Text("Keyword list:");
        ImGui::SameLine();
        HelpMarker("As with every widgets in dear imgui, we never modify values unless there is a user interaction.\nYou can override the clamping limits by using CTRL+Click to input a value.");
        
        //ImVec2 start_pos = ImGui::GetCursorScreenPos();
        //ImGui::SetCursorScreenPos(ImVec2(start_pos.x, start_pos.y + keywords_child_height));
        
        //ImGui::SetCursorScreenPos(start_pos);
        
        s32 give_focus = -1;
        if (ImGui::Button("Add..."))
        {
            for (int i = 0; i < MAX_KEYWORD_COUNT; ++i)
            {
                if (edit_settings->pending[i][0] == '\0')
                {
                    give_focus = i;
                    break;
                }
            }
        }
        
        {
            //ImGui::BeginChild("List", ImVec2(ImGui::GetWindowWidth()*0.5, ImGui::GetWindowHeight() * 0.6f));
            
            // This just pushes some style vars, calls BeginChild, then pops them
            ImGuiWindowFlags child_flags = 0;
            ImGui::BeginChildFrame(222, ImVec2(keywords_child_width, keywords_child_height), child_flags);
            
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * .9f);
            for (s32 i = 0; i < edit_settings->input_box_count; ++i)
            {
                ImGui::PushID(i);
                
                if (i == give_focus)
                {
                    ImGui::SetKeyboardFocusHere();
                    give_focus = -1;
                }
                bool enter_pressed = ImGui::InputText("", edit_settings->pending[i], array_count(edit_settings->pending[i]));
                
                ImGui::PopID();
            }
            ImGui::PopItemWidth();
            ImGui::EndChildFrame();
        }
        
        //PushStyleColor();
        //PopStyleColor();
        
        {
            ImGui::SameLine();
            //ImGui::BeginChild("Misc Options", ImVec2(keywords_child_width, keywords_child_height));
            
            ImGuiWindowFlags child_flags = 0;
            ImGui::BeginChildFrame(111, ImVec2(keywords_child_width, keywords_child_height), child_flags);
            
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.30f);
            // TODO: Do i need to use bool16 for this (this is workaround)
            bool selected = edit_settings->misc_options.run_at_system_startup;
            ImGui::Checkbox("Run at startup", &selected);
            edit_settings->misc_options.run_at_system_startup = selected;
            
            // ImGuiInputTextFlags_CharsDecimal ?
            int freq = (int)(edit_settings->misc_options.poll_frequency_microseconds / MICROSECS_PER_SEC);
            ImGui::InputInt("Update frequency (s)", &freq); // maybe a slider is better
            
            // maybe put buttons on the slider too
            ImGui::SliderInt("Update frequency (s)##2", &freq, 1, 1000, "%dms");
            
            // Only if valid number
            if (freq < 1 && freq < 1000)
            {
                ImGui::SameLine();
                ImGui::Text("Frequency must be between 1000 and whatever");
            }
            
            // TODO: Save setting, but dont change later maybe? Dont allow close settings?
            edit_settings->misc_options.poll_frequency_microseconds = freq;
            
            char *times[] = {
                "12:00 AM",
                "12:15 AM",
                "12:30 AM",
                "etc..."
            };
            
            // So late night usage still counts towards previous day
            if (ImGui::Combo("New day start time", &edit_settings->day_start_time_item, times, array_count(times)))
            {
                edit_settings->misc_options.day_start_time = edit_settings->day_start_time_item * 15;
            }
            if (ImGui::Combo("Start tracking time", &edit_settings->poll_start_time_item, times, array_count(times)))
            {
                edit_settings->misc_options.poll_start_time = edit_settings->poll_start_time_item * 15;
            }
            if (ImGui::Combo("Finish tracking time", &edit_settings->poll_end_time_item, times, array_count(times)))
            {
                edit_settings->misc_options.poll_end_time = edit_settings->poll_end_time_item * 15;
            }
            
            ImGui::PopItemWidth();
            ImGui::EndChildFrame();
        }
        
        {
            if (ImGui::Button("Ok"))
            {
                edit_settings->update_settings = true;
                ImGui::CloseCurrentPopup();
                finished= true;
                
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
                finished= true;
            }
        }
        
        ImGui::EndPopup(); // only close if BeginPopupModal returns true
    }
    
    // TODO: Revert button?
    return finished;
}

void
do_calendar(Calendar_State *calendar)
{
    // TODO: Decide best date types to use, to minimise conversions and make cleaner
    
    // maybe only allow selection between days we actually have, or at least limit to current date
    
    ImVec2 calendar_size = ImVec2(300,300);
    ImGui::SetNextWindowSize(calendar_size);
    if (ImGui::BeginPopup("Calendar"))
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.SelectableTextAlign = ImVec2(1.0f, 0.0f); // make calendar numbers right aligned
        
        static char *weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        static char *labels[] = {
            "1","2","3","4","5","6","7","8","9","10",
            "11","12","13","14","15","16","17","18","19","20",
            "21","22","23","24","25","26","27","28","29","30",
            "31"};
        
        //ImGui::ArrowButton("##left", ImGuiDir_Left); // looks worse but not too bad
        if (ImGui::Button(ICON_MD_ARROW_BACK))
        {
            auto ymd = date::year_month_day{calendar->first_day_of_month};
            Assert(ymd.ok());
            
            int year = int(ymd.year());
            unsigned month = unsigned (ymd.month());
            if (month == 1) 
            {
                month = 12;
                year -= 1;
            }
            else
            {
                month -= 1;
            }
            
            auto new_date = date::month{month}/ymd.day()/year;
            Assert(new_date.ok());
            
            calendar->first_day_of_month = date::year_month_weekday{new_date};
            Assert(calendar->first_day_of_month.ok());
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 40); 
        if (ImGui::Button(ICON_MD_ARROW_FORWARD))
        {
            auto ymd = date::year_month_day{calendar->first_day_of_month};
            Assert(ymd.ok());
            
            int year = int(ymd.year());
            unsigned month = unsigned(ymd.month());
            if (month == 12) 
            {
                month = 1;
                year += 1;
            }
            else
            {
                month += 1;
            }
            
            auto new_date = date::month{month}/ymd.day()/year;
            Assert(new_date.ok());
            
            calendar->first_day_of_month = date::year_month_weekday{new_date};
            Assert(calendar->first_day_of_month.ok());
        }
        
        int year = int(calendar->first_day_of_month.year());
        int month = unsigned(calendar->first_day_of_month.month());
        
        char page_month_year[64];
        snprintf(page_month_year, 64, "%s %i", month_string(month), year);
        ImVec2 text_size = ImGui::CalcTextSize(page_month_year);
        
        // Centre text
        ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2.0f - (text_size.x / 2.0f)); 
        ImGui::Text(page_month_year); 
        ImGui::Spacing();
        
        // NOTE: TODO: Not getting the correct time it is 12:53 28/6/2020, ui says it is 27/6/2020
        
        // year, month (1-12), day of the week (0 thru 6 in c_encoding, 1 - 7 in iso_encoding), index in the range [1, 5]
        // indicating if this is the first, second, etc. weekday of the indicated month.
        // Saturday is wd = 6, Sunday is 0, monday is 1
        
        s32 skipped_spots = calendar->first_day_of_month.weekday().iso_encoding() - 1;
        s32 days_in_month_count = days_in_month(month, year);
        
        ImGui::Columns(7, "mycolumns", false);  // no border
        for (int i = 0; i < array_count(weekdays); i++)
        {
            ImGui::Text(weekdays[i]);
            ImGui::NextColumn();
        }
        ImGui::Separator();
        
        for (int i = 0; i < skipped_spots; i++)
        {
            ImGui::NextColumn();
        }
        
        // can also just pass a bool to Selactable
        bool selected[31] = {};
        {
            auto ymd = date::year_month_day{calendar->selected_date};
            int sel_year = int(ymd.year());
            int sel_month = unsigned(ymd.month());
            int sel_day = unsigned(ymd.day());
            if (year == sel_year && month == sel_month)
            {
                selected[sel_day - 1] = true;
            }
        }
        
        ImGuiSelectableFlags flags = 0;
        for (int i = 0; i < days_in_month_count; i++)
        {
            //if (i + 1 > day && calendar->selected_index != -1) flags |= ImGuiSelectableFlags_Disabled;
            
            if (ImGui::Selectable(labels[i], &selected[i], flags)) 
            {
                // close calendar and say selected this one
                calendar->selected_date = date::sys_days{calendar->first_day_of_month} + date::days{i};
                break;
            }
            ImGui::NextColumn();
        }
        
        
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
        ImGui::EndPopup();
    }
}

// Database only needed for app_names, and generating new settings ids
// current_date not really needed
// State needed for calendar and settings structs
void draw_ui_and_update(SDL_Window *window, Monitor_State *state, Database *database, date::sys_days current_date, Day_View *day_view)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    ImGui::NewFrame();
    
    bool show_demo_window = true;
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 0.0f; // 0 to 12 ? 
    style.WindowBorderSize = 1.0f; // or 1.0
    style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.WindowRounding = 0.0f;
    
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
    
    {
        // TODO: I would perfer a settings button with cog icon, if nothing else is in menu bar
        bool open_settings = false; // workaround to nexted id stack
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Settings##menu"))
            {
                // NOTE: Id might be different because nexted in menu stack, so might be label + menu
                open_settings = true;
                // init edit_settings
                state->edit_settings = (Edit_Settings *)calloc(1, sizeof(Edit_Settings));
                
                // Not sure if should try to leave only one extra or all possible input boxes
                state->edit_settings->input_box_count = MAX_KEYWORD_COUNT;
                state->edit_settings->misc_options = state->settings.misc_options;
                
                // TODO: Should be a better way than this
                state->edit_settings->poll_start_time_item = state->settings.misc_options.poll_start_time / 15;
                state->edit_settings->poll_end_time_item = state->settings.misc_options.poll_end_time / 15;
                
                Assert(state->settings.keywords.size() <= MAX_KEYWORD_COUNT);
                for (s32 i = 0; i < state->settings.keywords.size(); ++i)
                {
                    Assert(state->settings.keywords[i].str.length + 1 <= MAX_KEYWORD_SIZE);
                    strcpy(state->edit_settings->pending[i], state->settings.keywords[i].str.str);
                }
                
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        
        if (open_settings) ImGui::OpenPopup("Settings");
        
        bool finished = do_settings_popup(state->edit_settings);
        if (finished)
        {
            update_settings(state->edit_settings, &state->settings, database);
            free(state->edit_settings);
        }
    }
    
    auto do_calendar_button = [](Calendar_State *calendar) {
        date::year_month_day ymd{ calendar->selected_date };
        int year = int(ymd.year());
        int month = unsigned(ymd.month());
        int day = unsigned(ymd.day());
        
        // renders old selected date for one frame (the frame when a new one is selected)
        char buf[64];
        snprintf(buf, 64, ICON_MD_DATE_RANGE " %i/%02i/%i", day, month, year);
        
        ImGui::PushID((void *)&calendar->selected_date);
        if (ImGui::Button(buf, ImVec2(120, 30))) // TODO: Better size estimate (maybe based of font)?
        {
            ImGui::OpenPopup("Calendar");
            
            // Init calendar
            
            // Want to show the calendar month with our current selected date
            auto ymd = date::year_month_day{calendar->selected_date};
            
            auto new_date = ymd.month()/date::day{1}/ymd.year();
            Assert(new_date.ok());
            
            auto first_day_of_month = date::year_month_weekday{new_date};
            Assert(first_day_of_month.ok());
            
            // Selected date should already be set
            calendar->first_day_of_month = first_day_of_month;
        }
        
        do_calendar(calendar);
        
        ImGui::PopID();
    };
    
    // can put in above func to use pushed id
    do_calendar_button(&state->calendar_date_range_start);
    do_calendar_button(&state->calendar_date_range_end);
    
    
    {
        date::year_month_day ymd_date{ current_date };
        int year = int(ymd_date.year());
        int month = unsigned(ymd_date.month());
        int day = unsigned(ymd_date.day());
        
        ImGui::Text("%i%s %s, %i", day, day_suffix(day), month_string(month), year);
        if (ImGui::Button("Day", ImVec2(ImGui::GetWindowSize().x*0.2f, 0.0f)))
        {
            s32 period = 1;
            
            date::sys_days start_date = current_date - date::days{period - 1};
            set_day_view_range(day_view, start_date, current_date);
        }
        ImGui::SameLine();
        if (ImGui::Button("Week", ImVec2(ImGui::GetWindowSize().x*0.2f, 0.0f)))
        {
            s32 period = 7;
            
            date::sys_days start_date = current_date - date::days{period - 1};
            set_day_view_range(day_view, start_date, current_date);
        }
        ImGui::SameLine();
        if (ImGui::Button("Month", ImVec2(ImGui::GetWindowSize().x*0.2f, 0.0f)))
        {
            s32 period = 30;
            
            date::sys_days start_date = current_date - date::days{period - 1};
            set_day_view_range(day_view, start_date, current_date);
        }
#if 0
        ImGui::SameLine();
        if (ImGui::Button("Next day", ImVec2(ImGui::GetWindowSize().x*0.2f, 0.0f)))
        {
            state->extra_days += 1;
            
            // because it hasn't been updated yet
            //current_date += date::days{state->extra_days};
            //set_day_view_range(day_view, start_date, current_date);
        }
#endif
    }
    
    // To allow frame border
    ImGuiWindowFlags child_flags = 0;
    ImGui::BeginChildFrame(55, ImVec2(ImGui::GetWindowSize().x * 0.9, ImGui::GetWindowSize().y * 0.7), child_flags);
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    Day *today = &day_view->days.back();
    
    time_type sum_duration = 0;
    
    if (day_view->has_days && today->record_count > 0)
    {
        i32 record_count = today->record_count;
        
        Record sorted_records[MaxDailyRecords];
        memcpy(sorted_records, today->records, sizeof(Record) * record_count);
        
        std::sort(sorted_records, sorted_records + record_count, [](Record &a, Record &b) { return a.duration > b.duration; });
        
        time_type max_duration = sorted_records[0].duration;
        
        // calc item width does what...?
        ImVec2 max_size = ImVec2(ImGui::CalcItemWidth() * 0.85, ImGui::GetFrameHeight());
        
        for (i32 i = 0; i < record_count; ++i)
        {
            Record &record = sorted_records[i];
            
            if (is_exe(record.id))
            {
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
            }
            else
            {
                ImGui::Text(ICON_MD_PUBLIC);
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
            
            double seconds = (double)record.duration / MICROSECS_PER_SEC;
            
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
    
    double total_runtime_seconds = (double)state->total_runtime / 1000000;
    double sum_duration_seconds = (double)sum_duration / 1000000;
    
    auto now = win32_get_time();
    auto start_time = win32_get_microseconds_elapsed(state->startup_time, now, global_performance_frequency);
    
    double time_since_startup_seconds = (double)start_time / 1000000;
    
    double diff_seconds2 = (double)(start_time - sum_duration) / 1000000;
    
    
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
