
// from monitor.cpp
Icon_Asset * get_icon_asset(Database *database, App_Id id);
void add_keyword(Arena *arena, Array<String, MAX_KEYWORD_COUNT> &keywords, char *str);
bool is_local_program(App_Id id);
bool is_website(App_Id id);


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
	Assert(month >= 1 && month <= 12);
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
	static const char *months[] = { "January","February","March","April","May","June",
		"July","August","September","October","November","December" };
	return months[month-1];
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
    if (ImGui::BeginPopupModal("Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove))
    {
        // TODO: Remove or don't allow
        // - all spaces strings
        // - leading/trailing whitespace
        // - spaces in words
        // - unicode characters (just disallow pasting maybe)
        // NOTE:
        // - Put incorrect format text items in red, and have a red  '* error message...' next to "Keywords"
        // TODO: Allow pasting but maybe normalise unicode to ascii, UTF8 is allowed in urls I think
        
        // Domain names are allowed normal alphabet and numbers and hyphen '-', and the period '.' separator
        
        // IF we get lots of kinds of settings (not ideal), can use tabs
        
        // When enter is pressed the item is not active the same frame
        //bool active = ImGui::IsItemActive();
        // TODO: Does SetKeyboardFocusHere() break if box is clipped?
        //if (give_focus == i) ImGui::SetKeyboardFocusHere();
        
        // TODO: Copy windows (look at Everything.exe settings) ui style and white on grey colour scheme
        
        
        ImGui::Text("Keyword list:");
        
        s32 give_focus = -1;
        bool error_max_keywords_reached = false;
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
            
            // TODO: How to make this persist for the right amount of time (what length is this? until they click?)
            if (give_focus == -1)
            {
                // All boxes full
                error_max_keywords_reached = true;
            }
            
        }
        
        ImGui::SameLine();
        HelpMarker("As with every widgets in dear imgui, we never modify values unless there is a user interaction.\nYou can override the clamping limits by using CTRL+Click to input a value.");
        
        
        error_max_keywords_reached = true;
        if (error_max_keywords_reached) 
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(200,0,0,255), "   *maximum number of keywords reached");
        }
        
        s32 index_of_last_keyword = 0;
        for (int i = 0; i < MAX_KEYWORD_COUNT; ++i)
        {
            if (edit_settings->pending[i][0] != '\0')
            {
                index_of_last_keyword = i;
            }
        }
        
        s32 number_of_boxes = std::min(index_of_last_keyword + 8, MAX_KEYWORD_COUNT);
        
        {
            ImGui::BeginChild(222, ImVec2(keywords_child_width, keywords_child_height), true);
            
            // If I use WindowDrawList must subtract padding or else lines are obscured by below text input box
            // can use ForegroundDrawList but this is not clipped to keyword child window
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            //ImDrawList* draw_list = ImGui::GetForeGroundDrawList();
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * .9f);
            for (s32 i = 0; i < number_of_boxes; ++i)
            {
                if (i == give_focus)
                {
                    ImGui::SetKeyboardFocusHere();
                    give_focus = -1;
                }
                
                auto filter = [](ImGuiTextEditCallbackData* data) -> int {
                    ImWchar c = data->EventChar;
                    if ((c >= 'a' && c <= 'z') || 
                        (c >= 'A' && c <= 'Z') || 
                        (c >= '0' && c <= '9') ||
                        c == '-' || 
                        c == '.')
                    {
                        return 0;
                    }
                    else
                    {
                        return 1;
                    }
                };
                
                ImGui::PushID(i);
                bool enter_pressed = ImGui::InputText("", edit_settings->pending[i], array_count(edit_settings->pending[i]), ImGuiInputTextFlags_CallbackCharFilter, filter);
                ImGui::PopID();
                
                float padding = 3;
                u32 grey = 150;
                ImVec2 pos = ImGui::GetCursorScreenPos();
                pos.y -= padding;
                draw_list->AddLine(pos, ImVec2(pos.x + keywords_child_width, pos.y), IM_COL32(grey,grey,grey,255), 1.0f);
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleVar();
            
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

bool
ButtonSpecial(const char* label, bool is_disabled, const ImVec2& size = ImVec2(0, 0))
{
    if (is_disabled)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // imgui_internal.h
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    bool result = ImGui::Button(label, size);
    if (is_disabled)
    {
        ImGui::PopItemFlag(); // imgui_internal.h
        ImGui::PopStyleVar();
    }
    return result;
}

bool 
calendar_is_backwards_disabled(Calendar *calendar, date::sys_days oldest_date)
{
    auto oldest_ymd = date::year_month_day{oldest_date};
    auto prev_month = date::year_month{
        calendar->first_day_of_month.year(),calendar->first_day_of_month.month()} - date::months{1};
    return (prev_month < date::year_month{oldest_ymd.year(), oldest_ymd.month()}); 
}

bool 
calendar_is_forwards_disabled(Calendar *calendar, date::sys_days newest_date)
{
    auto newest_ymd = date::year_month_day{newest_date};
    auto next_month = date::year_month{
        calendar->first_day_of_month.year(), calendar->first_day_of_month.month()} + date::months{1};
    return (next_month > date::year_month{newest_ymd.year(), newest_ymd.month()}); 
}

bool
do_calendar(Calendar *calendar, date::sys_days oldest_date, date::sys_days newest_date)
{
    bool selection_made = false;
    
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
        
        if(ButtonSpecial(ICON_MD_ARROW_BACK, calendar->is_backwards_disabled))
        {
            // Have to convert to ymd because subtracting month from ymw may change the day of month
            auto d = date::year_month_day{calendar->first_day_of_month} - date::months{1};
            calendar->first_day_of_month = date::year_month_weekday{d};
            calendar->is_backwards_disabled = calendar_is_backwards_disabled(calendar, oldest_date);
            calendar->is_forwards_disabled = false;
        }
        
        ImGui::SameLine(ImGui::GetWindowWidth() - 40); 
        
        if(ButtonSpecial(ICON_MD_ARROW_FORWARD, calendar->is_forwards_disabled))
        {
            auto d = date::year_month_day{calendar->first_day_of_month} + date::months{1};
            calendar->first_day_of_month = date::year_month_weekday{d};
            calendar->is_backwards_disabled = false;
            calendar->is_forwards_disabled = calendar_is_forwards_disabled(calendar, newest_date);
        }
        
        // Set Month Year text
        char page_month_year[64];
        snprintf(page_month_year, 64, "%s %i", 
                 month_string(unsigned(calendar->first_day_of_month.month())), int(calendar->first_day_of_month.year()));
        ImVec2 text_size = ImGui::CalcTextSize(page_month_year);
        
        // Centre text
        ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2.0f - (text_size.x / 2.0f)); 
        ImGui::Text(page_month_year); 
        ImGui::Spacing();
        
        s32 skipped_spots = calendar->first_day_of_month.weekday().iso_encoding() - 1;
        s32 days_in_month_count = days_in_month(unsigned(calendar->first_day_of_month.month()), int(calendar->first_day_of_month.year()));
        
        s32 days_before_oldest = 0;
        if (calendar->is_backwards_disabled) 
        {
            auto oldest_ymd = date::year_month_day{oldest_date};
            days_before_oldest = unsigned(oldest_ymd.day()) - 1; 
        }
        
        s32 days_after_newest = 0;
        if (calendar->is_forwards_disabled) 
        {
            auto newest_ymd = date::year_month_day{newest_date};
            days_after_newest = days_in_month_count - unsigned(newest_ymd.day()); 
        }
        
        s32 selected_index = -1;
        auto selected_ymd = date::year_month_day{calendar->selected_date};
        if (calendar->first_day_of_month.year() == selected_ymd.year() && calendar->first_day_of_month.month() == selected_ymd.month())
        {
            selected_index = unsigned(selected_ymd.day()) - 1;
        }
        
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
        
        int label_index = 0;
        for (int i = 0; i < days_before_oldest; i++, label_index++)
        {
            ImGui::Selectable(labels[label_index], false, ImGuiSelectableFlags_Disabled);
            ImGui::NextColumn();
        }
        
        int enabled_days = days_in_month_count - (days_before_oldest + days_after_newest);
        for (int i = 0; i < enabled_days; i++, label_index++)
        {
            bool selected = (selected_index == label_index);
            if (ImGui::Selectable(labels[label_index], selected)) 
            {
                calendar->selected_date = date::sys_days{calendar->first_day_of_month} + date::days{label_index};
                selection_made = true;
            }
            ImGui::NextColumn();
        }
        
        for (int i = 0; i < days_after_newest; i++, label_index++)
        {
            ImGui::Selectable(labels[label_index], false, ImGuiSelectableFlags_Disabled);
            ImGui::NextColumn();
        }
        
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
        ImGui::EndPopup();
    }
    
    return selection_made;
}

void
init_calendar(Calendar *calendar, date::sys_days selected_date, 
              date::sys_days oldest_date, date::sys_days newest_date)
{
    calendar->selected_date = selected_date;
    
    // Want to show the calendar month with our current selected date
    auto ymd = date::year_month_day{selected_date};
    auto new_date = ymd.year()/ymd.month()/date::day{1};
    calendar->first_day_of_month = date::year_month_weekday{new_date};
    
    calendar->is_backwards_disabled = calendar_is_backwards_disabled(calendar, oldest_date);
    calendar->is_forwards_disabled  = calendar_is_forwards_disabled(calendar, newest_date);
    
    auto d = date::year_month_day{selected_date};
    snprintf(calendar->button_label, array_count(calendar->button_label), 
             ICON_MD_DATE_RANGE " %u/%02u/%i", unsigned(d.day()), unsigned(d.month()), int(d.year()));
}

bool
do_calendar_button(Calendar *calendar, date::sys_days oldest_date, date::sys_days newest_date)
{
    ImGui::PushID((void *)&calendar->selected_date);
    if (ImGui::Button(calendar->button_label, ImVec2(120, 30))) // TODO: Better size estimate (maybe based of font)?
    {
        // Calendar should be initialised already
        ImGui::OpenPopup("Calendar");
        
        // Show the calendar month with our current selected date
        auto ymd = date::year_month_day{calendar->selected_date};
        auto new_date = ymd.year()/ymd.month()/date::day{1};
        calendar->first_day_of_month = date::year_month_weekday{new_date};
    }
    
    // @WaitForInput date label
    bool changed_selection = do_calendar(calendar, oldest_date, newest_date);
    if (changed_selection)
    {
        auto d = date::year_month_day{calendar->selected_date};
        snprintf(calendar->button_label, array_count(calendar->button_label), ICON_MD_DATE_RANGE " %u/%02u/%i", 
                 unsigned(d.day()), unsigned(d.month()), int(d.year()));
    }
    
    ImGui::PopID();
    
    return changed_selection;
}


void 
date_picker_update_label(Date_Picker *date_picker, date::sys_days oldest_date, date::sys_days newest_date)
{
    if (date_picker->range_type == Range_Type_Daily)
    {
        if (date_picker->start == oldest_date)
            snprintf(date_picker->date_label, array_count(date_picker->date_label), "Today");
        else if (date_picker->start == oldest_date - date::days{1})
            snprintf(date_picker->date_label, array_count(date_picker->date_label), "Yesterday");
        else
        {
            auto d1 = date::year_month_day{date_picker->start};
            snprintf(date_picker->date_label, array_count(date_picker->date_label), 
                     "%u %s %i", unsigned(d1.day()), month_string(unsigned(d1.month())), int(d1.year()));
        }
    }
    else if (date_picker->range_type == Range_Type_Weekly)
    {
        auto d1 = date::year_month_day{date_picker->start};
        auto d2 = date::year_month_day{date_picker->end};
        snprintf(date_picker->date_label, array_count(date_picker->date_label), 
                 "%u %s %i - %u %s %i", 
                 unsigned(d1.day()), month_string(unsigned(d1.month())), int(d1.year()),
                 unsigned(d2.day()), month_string(unsigned(d2.month())), int(d2.year()));
    }
    else if (date_picker->range_type == Range_Type_Monthly)
    {
        auto d1 = date::year_month_day{date_picker->start};
        snprintf(date_picker->date_label, array_count(date_picker->date_label), 
                 "%s %i", month_string(unsigned(d1.month())), int(d1.year()));
    }
    else if (date_picker->range_type == Range_Type_Custom)
    {
        auto d1 = date::year_month_day{date_picker->start};
        auto d2 = date::year_month_day{date_picker->end};
        snprintf(date_picker->date_label, array_count(date_picker->date_label), 
                 "%u %s %i - %u %s %i", 
                 unsigned(d1.day()), month_string(unsigned(d1.month())), int(d1.year()),
                 unsigned(d2.day()), month_string(unsigned(d2.month())), int(d2.year()));
    }
}

void 
date_picker_clip_and_update(Date_Picker *date_picker, date::sys_days oldest_date, date::sys_days newest_date)
{
    // TODO: Maybe this should take range and if Range_Type is Custom then disable both buttons
    
    date_picker->start = clamp(date_picker->start, oldest_date, newest_date);
    date_picker->end = clamp(date_picker->end, oldest_date, newest_date);
    
    Assert(date_picker->start <= date_picker->end);
    Assert(date_picker->start >= oldest_date);
    Assert(date_picker->end <= newest_date);
    
    // If start is clipped then there is always no earlier day/week/month (can't go backwards)
    // Or start is not clipped but is on the oldest date (can't go backwards)
    // If start is not clipped then must be at the beginning of a week or month, so any earlier date (i.e. oldest) would have to be in another week or month (can go backwards)
    
    date_picker->is_backwards_disabled = (date_picker->start == oldest_date);
    date_picker->is_forwards_disabled = (date_picker->end == newest_date);
    
    date_picker_update_label(date_picker, oldest_date, newest_date);
}

void
date_picker_backwards(Date_Picker *date_picker, date::sys_days oldest_date, date::sys_days newest_date) 
{
    if (date_picker->range_type == Range_Type_Daily)
    {
        date_picker->start -= date::days{1};
        date_picker->end -= date::days{1};
    }
    else if (date_picker->range_type == Range_Type_Weekly)
    {
        // start end stay on monday and sunday of week unless clipped
        date_picker->start -= date::days{7};
        date_picker->end -= date::days{7};
        
        // If end is currently clipped to the newest date then it may not be on sunday, so subtracting 7 days will reduce the date by too much, so need to add days to get back to the weeks sunday
        // This is not a problem for the start date if we are going backwards
        auto day_of_week = date::weekday{date_picker->end};
        if (day_of_week != date::Sunday)
        {
            auto days_to_next_sunday = date::Sunday - day_of_week;
            date_picker->end +=  days_to_next_sunday;
        }
    }
    else if (date_picker->range_type == Range_Type_Monthly)
    {
        auto d = date::year_month_day{date_picker->start} - date::months{1};
        date_picker->start = date::sys_days{d.year()/d.month()/1};
        date_picker->end = date::sys_days{d.year()/d.month()/date::last};
    }
    
    date_picker_clip_and_update(date_picker, oldest_date, newest_date);
}

void
date_picker_forwards(Date_Picker *date_picker, date::sys_days oldest_date, date::sys_days newest_date) 
{
    if (date_picker->range_type == Range_Type_Daily)
    {
        date_picker->start += date::days{1};
        date_picker->end += date::days{1};
    }
    else if (date_picker->range_type == Range_Type_Weekly)
    {
        // start end stay on monday and sunday of week unless clipped
        date_picker->start += date::days{7};
        date_picker->end += date::days{7};
        
        // If we overshot our monday because start was clipped to a non-monday day, we need to move back
        auto day_of_week = date::weekday{date_picker->start};
        if (day_of_week != date::Monday)
        {
            auto days_back_to_monday = day_of_week - date::Monday;
            date_picker->start -= days_back_to_monday;
        }
    }
    else if (date_picker->range_type == Range_Type_Monthly)
    {
        auto d = date::year_month_day{date_picker->end} + date::months{1};
        date_picker->start = date::sys_days{d.year()/d.month()/1};
        date_picker->end = date::sys_days{d.year()/d.month()/date::last};
    }
    
    date_picker_clip_and_update(date_picker, oldest_date, newest_date);
}

void
do_date_select_popup(Date_Picker *date_picker, date::sys_days oldest_date, date::sys_days newest_date)
{
    ImVec2 pos = ImVec2(ImGui::GetWindowWidth() / 2.0f, ImGui::GetCursorPosY());
    ImGui::SetNextWindowPos(pos, true, ImVec2(0.5f, 0)); // centre on x
    
    //ImGui::SetNextWindowSize(ImVec2(300,300));
    
    if (ImGui::BeginPopup("Date Select"))
    {
        // @WaitOnInput: this may change the date displayed on button
        
        char *items[] = {"Daily", "Weekly", "Monthly", "Custom range"};
        
        int prev_range_type = date_picker->range_type;
        if (ImGui::Combo("##picker", (int *)&date_picker->range_type, items, array_count(items)))
        {
            // TODO: Collapse this to avoid rechecking range_type and coverting to ymd more than once (though this may need to happen if the day is clipped)
            
            // TODO: May want to use end_date as basis of range instead of start_date because we tend to care more about more recent dates, so when switching from month to daily it goes to last day of month, etc.
            if (date_picker->range_type != prev_range_type)
            {
                if (date_picker->range_type == Range_Type_Daily)
                {
                    // start_date stays the same
                    date_picker->end = date_picker->start;
                }
                else if  (date_picker->range_type == Range_Type_Weekly)
                {
                    // Select the week that start_date was in
                    date::days days_back_to_monday_of_week = date::weekday{date_picker->start} - date::Monday;
                    
                    date_picker->start = date_picker->start - days_back_to_monday_of_week;
                    date_picker->end   = date_picker->start + date::days{6};
                }
                else if  (date_picker->range_type == Range_Type_Monthly)
                {
                    // Select the month that start_date was in
                    auto ymd = date::year_month_day{date_picker->start};
                    date_picker->start = date::sys_days{ymd.year()/ymd.month()/1};
                    date_picker->end   = date::sys_days{ymd.year()/ymd.month()/date::last};
                }
                else if (date_picker->range_type == Range_Type_Custom)
                {
                    // Either calendar can be before other
                    date_picker->start = std::min(date_picker->first_calendar.selected_date, date_picker->second_calendar.selected_date);
                    date_picker->end = std::max(date_picker->first_calendar.selected_date, date_picker->second_calendar.selected_date);
                }
                else
                {
                    Assert(0);
                }
            }
            
            date_picker_clip_and_update(date_picker, oldest_date, newest_date);
        }
        
        // @WaitForInput
        if (do_calendar_button(&date_picker->first_calendar, oldest_date, newest_date))
        {
            if (date_picker->range_type == Range_Type_Custom)
            {
                // NOTE: Maybe we allow start > end for calendars and we always choose oldest one to be start when assigning to date_picker and newest one to be end
                date_picker->start = std::min(date_picker->first_calendar.selected_date, date_picker->second_calendar.selected_date);
                date_picker->end = std::max(date_picker->first_calendar.selected_date, date_picker->second_calendar.selected_date);
                date_picker_clip_and_update(date_picker, oldest_date, newest_date);
            }
        } 
        ImGui::SameLine();
        if (do_calendar_button(&date_picker->second_calendar, oldest_date, newest_date))
        {
            if (date_picker->range_type == Range_Type_Custom)
            {
                date_picker->start = std::min(date_picker->first_calendar.selected_date, date_picker->second_calendar.selected_date);
                date_picker->end = std::max(date_picker->first_calendar.selected_date, date_picker->second_calendar.selected_date);
                date_picker_clip_and_update(date_picker, oldest_date, newest_date);
            }
        }
        
        ImGui::EndPopup();
    }
}

void
do_debug_window_button(Database *database, Day_View *day_view)
{
    static bool debug_menu_open = false;
    if (ImGui::Button("Debug"))
    {
        debug_menu_open = !debug_menu_open;
        ImGui::SetNextWindowPos(ImVec2(90, 0), true);
    }
    
    if (debug_menu_open)
    {
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoSavedSettings;
        
        ImGui::SetNextWindowSize(ImVec2(850, 690), true);
        ImGui::Begin("Debug Window", &debug_menu_open, flags);
        {
            ImGui::Text("local_programs:");
            ImGui::SameLine(300);
            ImGui::Text("app_names:");
            
            ImGui::BeginChildFrame(26, ImVec2(200, 600));
            for (auto const &pair : database->local_programs)
            {
                // not sure if definately null terminated
                ImGui::BulletText("%s: %u", pair.first.str, pair.second);
            }
            ImGui::EndChildFrame();
            
            ImGui::SameLine();
            
            ImGui::BeginChildFrame(27, ImVec2(500, 600));
            for (auto const &pair : database->app_names)
            {
                // not sure if definately null terminated
                ImGui::BulletText("%u: %s   (%s)", pair.first, 
                                  pair.second.short_name.str, pair.second.full_name.str);
            }
            ImGui::EndChildFrame();
        }
        {
            ImGui::Text("domains:");
            
            ImGui::BeginChildFrame(41, ImVec2(500, 600));
            for (auto const &pair : database->domain_names)
            {
                // not sure if definately null terminated
                ImGui::BulletText("%s: %u", pair.first.str, pair.second);
            }
            ImGui::EndChildFrame();
        }
        
        {
            ImGui::Text("__Day_List__");
            ImGui::Text("days:");
            ImGui::SameLine(500);
            ImGui::Text("blocks:");
            
            Day_List *day_list = &database->day_list;
            
            ImGui::BeginChildFrame(29, ImVec2(400, 600));
            ImGui::Text("day count = %u", day_list->days.size());
            {
                auto start = date::year_month_day{day_list->days.front().date};
                auto end = date::year_month_day{day_list->days.back().date};
                
                std::ostringstream buf;
                buf << start.day() << "/" << start.month() << "/" << start.year() <<  " - " 
                    << end.day() << "/" << end.month() << "/" << end.year(); 
                
                ImGui::SameLine();
                ImGui::Text("%s", buf.str().c_str());
            }
            for (int i = 0; i < day_list->days.size(); ++i)
            {
                auto ymd = date::year_month_day{day_list->days[i].date};
                
                std::ostringstream buf;
                buf << ymd;
                
                ImGui::Text("%s, record count = %u", buf.str().c_str(), day_list->days[i].record_count);
                Record  *records = day_list->days[i].records;
                for (int j = 0; j < day_list->days[i].record_count; ++j)
                {
                    ImGui::BulletText("id = %u, duration = %llis", records[j].id, records[j].duration / MICROSECS_PER_SEC);
                }
            }
            ImGui::EndChildFrame();
            
            ImGui::SameLine();
            
            ImGui::BeginChildFrame(28, ImVec2(300, 600));
            Block *b = day_list->blocks;
            int total_block_count = 0;
            while (b)
            {
                ImGui::Text("block %i, record count = %u", total_block_count, b->count);
                for (int i = 0; i < b->count; ++i)
                {
                    ImGui::Text("id = %u, duration = %llis", b->records[i].id, b->records[i].duration / MICROSECS_PER_SEC);
                }
                b = b->next;
                total_block_count += 1;
            }
            ImGui::EndChildFrame();
        }
        {
            ImGui::Text("Day_View");
            ImGui::BeginChildFrame(30, ImVec2(300, 600));
            ImGui::Text("day view count = %u", day_view->days.size());
            for (int i = 0; i < day_view->days.size(); ++i)
            {
                auto ymd = date::year_month_day{day_view->days[i].date};
                
                std::ostringstream buf;
                buf << ymd;
                
                ImGui::Text("%s, record count = %u", buf.str().c_str(), day_view->days[i].record_count);
                Record  *records = day_view->days[i].records;
                for (int j = 0; j < day_view->days[i].record_count; ++j)
                {
                    ImGui::BulletText("id = %u, duration = %llis", records[j].id, records[j].duration / MICROSECS_PER_SEC);
                }
            }
            ImGui::EndChildFrame();
        }
        
        
#if 0
        ImGui::Text("swaps:");
        ImGui::BeginChildFrame(31, ImVec2(600, 600));
        if (win32_context.swap_count > 0)
        {
            for (int i = 0; i < win32_context.swap_count - 1; ++i)
            {
                s64 dur = win32_context.swaps[i+1].event_time_milliseconds - win32_context.swaps[i].event_time_milliseconds;
                ImGui::BulletText("%s: %fs", win32_context.swaps[i].name, (float)dur / 1000);
            }
        }
        ImGui::EndChildFrame();
#endif
        
        
        ImGui::End();
    } // if debug_menu_open
}

Record *
get_records_in_date_range(Day_View *day_view, u32 *record_count, date::sys_days start_range, date::sys_days end_range)
{
    Record *result = nullptr;
    
    // Dates should be in our days range
    int start_idx = -1;
    int end_idx = -1;
    for (int i = 0; i < day_view->days.size(); ++i)
    {
        if (start_range == day_view->days[i].date)
        {
            start_idx = i;
        }
        if (end_range == day_view->days[i].date)
        {
            end_idx = i;
            Assert(start_idx != -1);
            break;
        }
    }
    
    Assert(start_idx != -1 && end_idx != -1);
    
    s32 total_record_count = 0;
    for (int i = start_idx; i <= end_idx; ++i)
    {
        total_record_count += day_view->days[i].record_count;
    }
    
    if (total_record_count > 0)
    {
        Record *range_records = (Record *)xalloc(total_record_count * sizeof(Record));
        Record *deduplicated_records = (Record *)xalloc(total_record_count * sizeof(Record));
        
        Record *at = range_records;
        for (int i = start_idx; i <= end_idx; ++i)
        {
            memcpy(at, day_view->days[i].records, day_view->days[i].record_count * sizeof(Record));
            at += day_view->days[i].record_count;
        }
        
        // Sort by id
        std::sort(range_records, range_records + total_record_count, 
                  [](Record &a, Record &b) { return a.id < b.id; });
        
        // Merge same id records 
        deduplicated_records[0] = range_records[0];
        int unique_id_count = 1; 
        
        for (int i = 1; i < total_record_count; ++i)
        {
            if (range_records[i].id == deduplicated_records[unique_id_count-1].id)
            {
                deduplicated_records[unique_id_count-1].duration += range_records[i].duration;
            }
            else
            {
                deduplicated_records[unique_id_count] = range_records[i];
                unique_id_count += 1;
            }
        }
        
        // Sort by duration
        std::sort(deduplicated_records, deduplicated_records + unique_id_count, 
                  [](Record &a, Record &b) { return a.duration > b.duration; });
        
        free(range_records);
        
        *record_count = unique_id_count;
        return deduplicated_records;
    }
    
    return result;
}

// Database only needed for app_names, icons, and debug menu
// State needed for datepicker and settings
void draw_ui_and_update(SDL_Window *window, Monitor_State *state, Database *database, Day_View *day_view)
{
    ZoneScoped;
    
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
    style.ChildBorderSize = 1.0f;
    style.WindowRounding = 0.0f;
    
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings
        //| ImGuiWindowFlags_MenuBar
        | ImGuiWindowFlags_NoResize;
    
    ImGui::SetNextWindowPos(ImVec2(0, 0), true);
    ImGui::SetNextWindowSize(ImVec2(850, 690), true);
    ImGui::Begin("Main window", nullptr, flags);
    
    // TODO: Make in exact middle
    ImGui::SameLine(ImGui::GetWindowWidth() * .35f);
    {
        // Date Picker
        
        Date_Picker *date_picker = &state->date_picker;
        
        date::sys_days oldest_date = day_view->days.front().date;
        date::sys_days newest_date = day_view->days.back().date;
        
        // @WaitForInput
        if (ButtonSpecial(ICON_MD_ARROW_BACK, date_picker->is_backwards_disabled))
        {
            date_picker_backwards(date_picker, oldest_date, newest_date);
        }
        
        ImGui::SameLine();
        if (ImGui::Button(date_picker->date_label, ImVec2(200, 0))) // passing zero uses default I guess
        {
            ImGui::OpenPopup("Date Select");
        }
        ImGui::SameLine();
        
        if (ButtonSpecial(ICON_MD_ARROW_FORWARD, date_picker->is_forwards_disabled))
        {
            date_picker_forwards(date_picker, oldest_date, newest_date);
        }
        
        do_date_select_popup(date_picker, oldest_date, newest_date);
    }
    
    ImGui::SameLine(ImGui::GetWindowWidth() * .85f);
    
    {
        // Settings
        
        // @WaitForInput
        if (ImGui::Button(ICON_MD_SETTINGS))
        {
            // init edit_settings
            state->edit_settings = (Edit_Settings *)calloc(1, sizeof(Edit_Settings));
            
            // Not sure if should try to leave only one extra or all possible input boxes
            state->edit_settings->misc_options = state->settings.misc_options;
            
            // TODO: Should be a better way than this
            state->edit_settings->poll_start_time_item = state->settings.misc_options.poll_start_time / 15;
            state->edit_settings->poll_end_time_item = state->settings.misc_options.poll_end_time / 15;
            
            Assert(state->settings.keywords.count <= MAX_KEYWORD_COUNT);
            for (s32 i = 0; i < state->settings.keywords.count; ++i)
            {
                Assert(state->settings.keywords[i].length + 1 <= MAX_KEYWORD_SIZE);
                strcpy(state->edit_settings->pending[i], state->settings.keywords[i].str);
            }
            
            ImGui::OpenPopup("Settings");
        }
        
        bool finished = do_settings_popup(state->edit_settings);
        
        if (finished)
        {
            if (state->edit_settings->update_settings)
            {
                // Empty input boxes are just zeroed array
                i32 pending_count = remove_duplicate_and_empty_keyword_strings(state->edit_settings->pending);
                
                Settings *settings = &state->settings;
                settings->keywords.clear();
                for (int i = 0; i < pending_count; ++i)
                {
                    add_keyword(&state->keyword_arena, settings->keywords, state->edit_settings->pending[i]);
                }
                
                settings->misc_options = state->edit_settings->misc_options;
            }
            
            free(state->edit_settings);
        }
    }
    
    ImGui::SameLine();
    do_debug_window_button(database, day_view);
    
#if 0
    {
        // Debug date stuff
        
        Date_Picker *date_picker = &state->date_picker;
        
        date::sys_days oldest_date = day_view->days.front().date;
        date::sys_days newest_date = day_view->days.back().date;
        
        auto ymd = date::year_month_day{date_picker->start};
        auto ymw = date::year_month_weekday{date_picker->start};
        std::ostringstream buf;
        
        buf << "start_date: " << ymd << ", " <<  ymw;
        ImGui::Text(buf.str().c_str());
        
        ImGui::SameLine();
        ymd = date::year_month_day{oldest_date};
        ymw = date::year_month_weekday{oldest_date};
        std::ostringstream buf2;
        
        buf2 << "     oldest_date; " << ymd << ", " <<  ymw;
        ImGui::Text(buf2.str().c_str());
        
        ymd = date::year_month_day{date_picker->end};
        ymw = date::year_month_weekday{date_picker->end};
        std::ostringstream buf1;
        
        buf1 << "end_date: " << ymd << ", " <<  ymw;
        ImGui::Text(buf1.str().c_str());
        
        ImGui::SameLine();
        
        ymd = date::year_month_day{newest_date};
        ymw = date::year_month_weekday{newest_date};
        std::ostringstream buf3;
        
        buf3 << "      newest_date: " << ymd << ", " <<  ymw;
        ImGui::Text(buf3.str().c_str());
    }
#endif
    {
        // Barplot
        
        // To allow frame border (can just use beginchild instead)
        ImGuiWindowFlags child_flags = 0;
        ImGui::BeginChildFrame(55, ImVec2(ImGui::GetWindowSize().x * 0.9, ImGui::GetWindowSize().y * 0.7), child_flags);
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        time_type sum_duration = 0; // for debug
        
        u32 record_count = 0;
        Record *records = get_records_in_date_range(day_view, &record_count, state->date_picker.start, state->date_picker.end);
        if (records)
        {
            time_type max_duration = records[0].duration;
            
            float max_width = ImGui::GetWindowWidth() * 0.6f;
            float bar_height = 32;//ImGui::GetFrameHeight();
            //ImVec2 max_size = ImVec2(ImGui::CalcItemWidth() * rand_between(0.7f, 0.9f), ImGui::GetFrameHeight());
            
            for (i32 i = 0; i < record_count; ++i)
            {
                Record &record = records[i];
                
                // try with and without id == 0 record
                sum_duration += record.duration; // DEBUG
                
                Bitmap *bmp = nullptr;
                if (is_local_program(record.id))
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
                    
                    bmp = &icon->bitmap;
                    
                    // Don't need to bind texture before this because imgui binds it before draw call
                    ImGui::Image((ImTextureID)(intptr_t)icon->texture_handle, ImVec2((float)icon->bitmap.width, (float)icon->bitmap.height));
                }
                else if (is_website(record.id))
                {
                    ImGui::Text(ICON_MD_PUBLIC);
                }
                
                ImGui::SameLine();
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                
                // TODO: Save records or sizes between frames
                float bar_width = max_width * ((float)record.duration / (float)max_duration);
                if (bar_width > 0)
                {
                    ImVec2 p1 = ImVec2(p0.x + bar_width, p0.y + bar_height);
                    ImU32 col_a = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                    ImU32 col_b = ImGui::GetColorU32(ImVec4(0.2f, 0.4f, 0.0f, 1.0f));
                    draw_list->AddRectFilledMultiColor(p0, p1, col_a, col_b, col_b, col_a);
                }
                
                // This is a hash table search
                // DEBUG
                char *name = "sdfsdf";
                u32 len = 0;
                if (record.id == 0)
                {
                    name = "Not polled time";
                    len = strlen(name);
                }
                else
                {
                    name = database->app_names[record.id].short_name.str;
                    len = database->app_names[record.id].short_name.length;
                }
                
                u32 seconds = record.duration / MICROSECS_PER_SEC;
                
                // This looks better with text more down on bar
                //if (i < 5) 
                ImGui::AlignTextToFramePadding();  // TODO: text needs to be slightly further down
                
                // TODO: Limit name length
                // TextUnformatted doesn't require null terminator, and avoids memory copy also
                ImGui::TextUnformatted(name, name + len);
                
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowSize().x * 0.90);
                
#if MONITOR_DEBUG
                float real_seconds = (float)record.duration / MICROSECS_PER_SEC;
                ImGui::Text("%.2fs", real_seconds); 
#else
                if (seconds < 60)
                {
                    ImGui::Text("%us", seconds); 
                }
                else if (seconds < 3600)
                {
                    ImGui::Text("%um", seconds / 60); 
                }
                else 
                {
                    u32 hours = seconds / 3600;
                    u32 minutes_remainder = (seconds % 3600) / 60;
                    ImGui::Text("%uh %um", hours, minutes_remainder); 
                }
                
#endif
                ImGui::SameLine();
                ImGui::InvisibleButton("##gradient1", ImVec2(bar_width + 10, bar_height));
            }
            
            free(records);
        } // if (records)
        
        ImGui::EndChild();
        
        float red[] = {255.0f, 0, 0};
        float black[] = {0, 0, 0};
        
        double total_runtime_seconds = (double)state->total_runtime / 1000000;
        double sum_duration_seconds = (double)sum_duration / 1000000;
        
        auto now = win32_get_time();
        auto start_time = win32_get_microseconds_elapsed(state->startup_time, now);
        
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
    }
}