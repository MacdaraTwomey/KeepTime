
#include "monitor.h"
#include "IconsMaterialDesign.h"

constexpr float DATE_PICKER_POPUP_WIDTH = 275;
constexpr float DATE_PICKER_RANGE_BUTTON_WIDTH = 70;
constexpr float DATE_PICKER_CALENDAR_BUTTON_WIDTH = 140;


constexpr ImU32 DISABLED_COLOUR = IM_COL32(185, 185, 185, 255);


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

bool
ButtonConditionallyFilled(const char* label, bool not_filled, const ImVec2& size = ImVec2(0, 0))
{
    if (not_filled)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 255, 255, 255));
    }
    bool result = ImGui::Button(label, size);
    if (not_filled)
    {
        ImGui::PopStyleColor();
    }
    return result;
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
    
    ImVec2 calendar_size = ImVec2(300,240);
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
    if (ImGui::Button(calendar->button_label, ImVec2(DATE_PICKER_CALENDAR_BUTTON_WIDTH, 0)))
    {
        // Calendar should be initialised already
        ImGui::OpenPopup("Calendar");
        
        // Show the calendar month with our current selected date
        auto ymd = date::year_month_day{calendar->selected_date};
        auto new_date = ymd.year()/ymd.month()/date::day{1};
        calendar->first_day_of_month = date::year_month_weekday{new_date};
    }
    
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
        if (date_picker->start == newest_date)
            snprintf(date_picker->date_label, array_count(date_picker->date_label), "Today");
        else if (date_picker->start == newest_date - date::days{1})
            snprintf(date_picker->date_label, array_count(date_picker->date_label), "Yesterday");
        else
        {
            auto d1 = date::year_month_day{date_picker->start};
            snprintf(date_picker->date_label, array_count(date_picker->date_label), 
                     "%u %s %i", unsigned(d1.day()), month_string(unsigned(d1.month())), int(d1.year()));
        }
    }
    else if (date_picker->range_type == Range_Type_Weekly || 
             date_picker->range_type == Range_Type_Custom)
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
}

void 
date_picker_clip_and_update(Date_Picker *date_picker, date::sys_days oldest_date, date::sys_days newest_date)
{
    date_picker->start = clamp(date_picker->start, oldest_date, newest_date);
    date_picker->end = clamp(date_picker->end, oldest_date, newest_date);
    
    Assert(date_picker->start <= date_picker->end);
    Assert(date_picker->start >= oldest_date);
    Assert(date_picker->end <= newest_date);
    
    // If start is clipped then there is always no earlier day/week/month (can't go backwards)
    // Or start is not clipped but is on the oldest date (can't go backwards)
    // If start is not clipped then must be at the beginning of a week or month, so any earlier date (i.e. oldest) would have to be in another week or month (can go backwards)
    
    date_picker->is_backwards_disabled = (date_picker->start == oldest_date) || date_picker->range_type == Range_Type_Custom;
    date_picker->is_forwards_disabled = (date_picker->end == newest_date) || date_picker->range_type == Range_Type_Custom;
    
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
init_date_picker(Date_Picker *picker, date::sys_days current_date, 
                 date::sys_days recorded_oldest_date, date::sys_days recorded_newest_date)
{
    // Current used for selected date of calendar and the picker
    // oldest and newest used for date picker and calendar limits
    
    // TODO: What if we loaded from a file and have no dates near current_date
    // either need get_records_in_date_range to return no records or make date picker
    // initialised within range
    // For now allow zero records to be displayed,  also good is user installed for first time and has no records
    
    if (current_date > recorded_newest_date) 
    {
        current_date = recorded_newest_date;
    }
    
#if 0
    picker->range_type = Range_Type_Daily;
    picker->start = current_date;
    picker->end = current_date;
#else
    // DEBUG: Default to monthly
    auto ymd = date::year_month_day{current_date};
    picker->range_type = Range_Type_Monthly;
    picker->start = date::sys_days{ymd.year()/ymd.month()/1};
    picker->end   = date::sys_days{ymd.year()/ymd.month()/date::last};
#endif
    
    // sets label and if buttons are disabled 
    date_picker_clip_and_update(picker, recorded_oldest_date, recorded_newest_date);
    
    init_calendar(&picker->first_calendar, current_date, recorded_oldest_date, recorded_newest_date);
    init_calendar(&picker->second_calendar, current_date, recorded_oldest_date, recorded_newest_date);
}

bool
do_date_select_popup(Date_Picker *date_picker, date::sys_days oldest_date, date::sys_days newest_date)
{
    bool range_changed = false;
    ImVec2 pos = ImVec2(ImGui::GetWindowWidth() / 2.0f, ImGui::GetCursorPosY() + 3);
    ImGui::SetNextWindowPos(pos, true, ImVec2(0.5f, 0)); // centre on x
    
    //ImGui::SetNextWindowSize(ImVec2(300,300));
    
    if (ImGui::BeginPopup("Date Select"))
    {
        char *items[] = {"Day", "Week", "Month", "Custom"};
        
        int prev_range_type = date_picker->range_type;
        
#if 1
        ImVec2 button_size = ImVec2(DATE_PICKER_RANGE_BUTTON_WIDTH, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 10.0f));
        if (ButtonConditionallyFilled(items[0], date_picker->range_type != Range_Type_Daily, button_size))
        {
            date_picker->range_type = Range_Type_Daily;
        }
        ImGui::SameLine();
        if (ButtonConditionallyFilled(items[1], date_picker->range_type != Range_Type_Weekly, button_size))
        {
            date_picker->range_type = Range_Type_Weekly;
        }
        ImGui::SameLine();
        if (ButtonConditionallyFilled(items[2], date_picker->range_type != Range_Type_Monthly, button_size))
        {
            date_picker->range_type = Range_Type_Monthly;
        }
        ImGui::SameLine();
        if (ButtonConditionallyFilled(items[3], date_picker->range_type != Range_Type_Custom, button_size))
        {
            date_picker->range_type = Range_Type_Custom;
        }
        ImGui::PopStyleVar();
#else
        ImGui::Combo("##picker", (int *)&date_picker->range_type, items, array_count(items));
#endif
        if (date_picker->range_type != prev_range_type)
        {
            range_changed = true;
            if (date_picker->range_type == Range_Type_Daily)
            {
                // end date stays the same
                date_picker->start = date_picker->end;
            }
            else if  (date_picker->range_type == Range_Type_Weekly)
            {
                // Select the week that end date was in
                auto days_to_next_sunday = date::Sunday - date::weekday{date_picker->end};
                
                date_picker->end = date_picker->end + days_to_next_sunday;
                date_picker->start = date_picker->end - date::days{6};
            }
            else if  (date_picker->range_type == Range_Type_Monthly)
            {
                // Select the month that end date was in
                auto ymd = date::year_month_day{date_picker->end};
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
            
            date_picker_clip_and_update(date_picker, oldest_date, newest_date);
        }
        
        if (do_calendar_button(&date_picker->first_calendar, oldest_date, newest_date))
        {
            if (date_picker->range_type == Range_Type_Custom)
            {
                range_changed = true;
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
                range_changed = true;
                date_picker->start = std::min(date_picker->first_calendar.selected_date, date_picker->second_calendar.selected_date);
                date_picker->end = std::max(date_picker->first_calendar.selected_date, date_picker->second_calendar.selected_date);
                date_picker_clip_and_update(date_picker, oldest_date, newest_date);
            }
        }
        
        ImGui::EndPopup();
    }
    
    return range_changed;
}
