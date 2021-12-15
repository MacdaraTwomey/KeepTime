
#include "monitor.h"
#include "IconsMaterialDesign.h"

constexpr float DATE_PICKER_POPUP_WIDTH = 275;
constexpr float DATE_PICKER_RANGE_BUTTON_WIDTH = 70;
constexpr float DATE_PICKER_CALENDAR_BUTTON_WIDTH = 140;


constexpr ImU32 DISABLED_COLOUR = IM_COL32(185, 185, 185, 255);

const char *DaySuffix(int Day)
{
	Assert(Day >= 1 && Day <= 31);
	static const char *Suffixes[] = { "st", "nd", "rd", "th" };
	switch (Day)
	{
        case 1:
        case 21:
        case 31:
		return Suffixes[0];
		break;
        
        case 2:
        case 22:
		return Suffixes[1];
		break;
        
        case 3:
        case 23:
		return Suffixes[2];
		break;
        
        default:
		return Suffixes[3];
		break;
	}
}

bool IsLeapYear(int Year)
{
    // if Year is a integer multiple of 4 (except for years evenly divisible by 100, which are not leap years unless evenly divisible by 400)
    return ((Year % 4 == 0) && !(Year % 100 == 0)) || (Year % 400 == 0);
}

int DaysInMonth(int Month, int Year)
{
	Assert(Month >= 1 && Month <= 12);
    switch (Month)
    {
        case 1: case 3: case 5:
        case 7: case 8: case 10:
        case 12:
        return 31;
        
        case 4: case 6:
        case 9: case 11:
        return 30;
        
        case 2:
        if (IsLeapYear(Year))
            return 29;
        else
            return 28;
    }
    
    return 0;
}

const char *MonthString(int Month)
{
	Assert(Month >= 1 && Month <= 12);
	static const char *Months[] = { "January","February","March","April","May","June",
		"July","August","September","October","November","December" };
	return Months[Month-1];
}

bool ButtonConditionallyFilled(const char* Label, bool NotFilled, const ImVec2& Size = ImVec2(0, 0))
{
    if (NotFilled)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 255, 255, 255));
    }
    bool result = ImGui::Button(Label, Size);
    if (NotFilled)
    {
        ImGui::PopStyleColor();
    }
    return result;
}


bool ButtonSpecial(const char* Label, bool IsDisabled, const ImVec2& Size = ImVec2(0, 0))
{
    if (IsDisabled)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // imgui_internal.h
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    bool Result = ImGui::Button(Label, Size);
    if (IsDisabled)
    {
        ImGui::PopItemFlag(); // imgui_internal.h
        ImGui::PopStyleVar();
    }
    return Result;
}

bool CalendarIsBackwardsDisabled(calendar *Calendar, date::sys_days OldestDate)
{
    auto OldestYMD = date::year_month_day{OldestDate};
    auto PrevMonth = date::year_month{
        Calendar->FirstDayOfMonth.year(),Calendar->FirstDayOfMonth.month()} - date::months{1};
    return (PrevMonth < date::year_month{OldestYMD.year(), OldestYMD.month()}); 
}

bool CalendarIsForwardsDisabled(calendar *Calendar, date::sys_days NewestDate)
{
    auto NewestYmd = date::year_month_day{NewestDate};
    auto NextMonth = date::year_month{
        Calendar->FirstDayOfMonth.year(), Calendar->FirstDayOfMonth.month()} + date::months{1};
    return (NextMonth > date::year_month{NewestYmd.year(), NewestYmd.month()}); 
}

bool DoCalendar(calendar *Calendar, date::sys_days OldestDate, date::sys_days NewestDate)
{
    bool SelectionMade = false;
    
    ImVec2 CalendarSize = ImVec2(300,240);
    ImGui::SetNextWindowSize(CalendarSize);
    if (ImGui::BeginPopup("Calendar"))
    {
        ImGuiStyle& Style = ImGui::GetStyle();
        Style.SelectableTextAlign = ImVec2(1.0f, 0.0f); // make calendar numbers right aligned
        
        static char *Weekdays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        static char *Labels[] = {
            "1","2","3","4","5","6","7","8","9","10",
            "11","12","13","14","15","16","17","18","19","20",
            "21","22","23","24","25","26","27","28","29","30",
            "31"};
        
        if(ButtonSpecial(ICON_MD_ARROW_BACK, Calendar->IsBackwardsDisabled))
        {
            // Have to convert to ymd because subtracting month from ymw may change the day of month
            auto d = date::year_month_day{Calendar->FirstDayOfMonth} - date::months{1};
            Calendar->FirstDayOfMonth = date::year_month_weekday{d};
            Calendar->IsBackwardsDisabled = CalendarIsBackwardsDisabled(Calendar, OldestDate);
            Calendar->IsForwardsDisabled = false;
        }
        
        ImGui::SameLine(ImGui::GetWindowWidth() - 40); 
        
        if(ButtonSpecial(ICON_MD_ARROW_FORWARD, Calendar->IsForwardsDisabled))
        {
            auto d = date::year_month_day{Calendar->FirstDayOfMonth} + date::months{1};
            Calendar->FirstDayOfMonth = date::year_month_weekday{d};
            Calendar->IsBackwardsDisabled = false;
            Calendar->IsForwardsDisabled = CalendarIsForwardsDisabled(Calendar, NewestDate);
        }
        
        // Set Month Year text
        char PageMonthYear[64];
        snprintf(PageMonthYear, 64, "%s %i", 
                 MonthString(unsigned(Calendar->FirstDayOfMonth.month())), int(Calendar->FirstDayOfMonth.year()));
        ImVec2 TextSize = ImGui::CalcTextSize(PageMonthYear);
        
        // Centre text
        ImGui::SameLine(ImGui::GetWindowContentRegionWidth() / 2.0f - (TextSize.x / 2.0f)); 
        ImGui::Text(PageMonthYear); 
        ImGui::Spacing();
        
        s32 SkippedSpots = Calendar->FirstDayOfMonth.weekday().iso_encoding() - 1;
        s32 DaysInMonthCount = DaysInMonth(unsigned(Calendar->FirstDayOfMonth.month()), int(Calendar->FirstDayOfMonth.year()));
        
        s32 DaysBeforeOldest = 0;
        if (Calendar->IsBackwardsDisabled) 
        {
            auto OldestYmd = date::year_month_day{OldestDate};
            DaysBeforeOldest = unsigned(OldestYmd.day()) - 1; 
        }
        
        s32 DaysAfterNewest = 0;
        if (Calendar->IsForwardsDisabled) 
        {
            auto NewestYmd = date::year_month_day{NewestDate};
            DaysAfterNewest = DaysInMonthCount - unsigned(NewestYmd.day()); 
        }
        
        s32 SelectedIndex = -1;
        auto SelectedYmd = date::year_month_day{Calendar->SelectedDate};
        if (Calendar->FirstDayOfMonth.year() == SelectedYmd.year() && Calendar->FirstDayOfMonth.month() == SelectedYmd.month())
        {
            SelectedIndex = unsigned(SelectedYmd.day()) - 1;
        }
        
        ImGui::Columns(7, "mycolumns", false);  // no border
        for (int i = 0; i < ArrayCount(Weekdays); i++)
        {
            ImGui::Text(Weekdays[i]);
            ImGui::NextColumn();
        }
        ImGui::Separator();
        
        for (int i = 0; i < SkippedSpots; i++)
        {
            ImGui::NextColumn();
        }
        
        int LabelIndex = 0;
        for (int i = 0; i < DaysBeforeOldest; i++, LabelIndex++)
        {
            ImGui::Selectable(Labels[LabelIndex], false, ImGuiSelectableFlags_Disabled);
            ImGui::NextColumn();
        }
        
        int EnabledDays = DaysInMonthCount - (DaysBeforeOldest + DaysAfterNewest);
        for (int i = 0; i < EnabledDays; i++, LabelIndex++)
        {
            bool Selected = (SelectedIndex == LabelIndex);
            if (ImGui::Selectable(Labels[LabelIndex], Selected)) 
            {
                Calendar->SelectedDate = date::sys_days{Calendar->FirstDayOfMonth} + date::days{LabelIndex};
                SelectionMade = true;
            }
            ImGui::NextColumn();
        }
        
        for (int i = 0; i < DaysAfterNewest; i++, LabelIndex++)
        {
            ImGui::Selectable(Labels[LabelIndex], false, ImGuiSelectableFlags_Disabled);
            ImGui::NextColumn();
        }
        
        Style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
        ImGui::EndPopup();
    }
    
    return SelectionMade;
}

void InitCalendar(calendar *Calendar, date::sys_days SelectedDate, 
                  date::sys_days OldestDate, date::sys_days NewestDate)
{
    Calendar->SelectedDate = SelectedDate;
    
    // Want to show the calendar month with our current selected date
    auto ymd = date::year_month_day{SelectedDate};
    auto NewDate = ymd.year()/ymd.month()/date::day{1};
    Calendar->FirstDayOfMonth = date::year_month_weekday{NewDate};
    
    Calendar->IsBackwardsDisabled = CalendarIsBackwardsDisabled(Calendar, OldestDate);
    Calendar->IsForwardsDisabled  = CalendarIsForwardsDisabled(Calendar, NewestDate);
    
    auto d = date::year_month_day{SelectedDate};
    snprintf(Calendar->ButtonLabel, ArrayCount(Calendar->ButtonLabel), 
             ICON_MD_DATE_RANGE " %u/%02u/%i", unsigned(d.day()), unsigned(d.month()), int(d.year()));
}

bool DoCalendarButton(calendar *Calendar, date::sys_days OldestDate, date::sys_days NewestDate)
{
    ImGui::PushID((void *)&Calendar->SelectedDate);
    if (ImGui::Button(Calendar->ButtonLabel, ImVec2(DATE_PICKER_CALENDAR_BUTTON_WIDTH, 0)))
    {
        // Calendar should be initialised already
        ImGui::OpenPopup("Calendar");
        
        // Show the calendar month with our current selected date
        auto ymd = date::year_month_day{Calendar->SelectedDate};
        auto NewDate = ymd.year()/ymd.month()/date::day{1};
        Calendar->FirstDayOfMonth = date::year_month_weekday{NewDate};
    }
    
    bool ChangedSelection = DoCalendar(Calendar, OldestDate, NewestDate);
    if (ChangedSelection)
    {
        auto d = date::year_month_day{Calendar->SelectedDate};
        snprintf(Calendar->ButtonLabel, ArrayCount(Calendar->ButtonLabel), ICON_MD_DATE_RANGE " %u/%02u/%i", 
                 unsigned(d.day()), unsigned(d.month()), int(d.year()));
    }
    
    ImGui::PopID();
    
    return ChangedSelection;
}


void DatePickerUpdateLabel(date_picker *DatePicker, date::sys_days OldestDate, date::sys_days NewestDate)
{
    if (DatePicker->RangeType == Range_Type_Daily)
    {
        if (DatePicker->Start == NewestDate)
            snprintf(DatePicker->DateLabel, ArrayCount(DatePicker->DateLabel), "Today");
        else if (DatePicker->Start == NewestDate - date::days{1})
            snprintf(DatePicker->DateLabel, ArrayCount(DatePicker->DateLabel), "Yesterday");
        else
        {
            auto d1 = date::year_month_day{DatePicker->Start};
            snprintf(DatePicker->DateLabel, ArrayCount(DatePicker->DateLabel), 
                     "%u %s %i", unsigned(d1.day()), MonthString(unsigned(d1.month())), int(d1.year()));
        }
    }
    else if (DatePicker->RangeType == Range_Type_Weekly || 
             DatePicker->RangeType == Range_Type_Custom)
    {
        auto d1 = date::year_month_day{DatePicker->Start};
        auto d2 = date::year_month_day{DatePicker->End};
        snprintf(DatePicker->DateLabel, ArrayCount(DatePicker->DateLabel), 
                 "%u %s %i - %u %s %i", 
                 unsigned(d1.day()), MonthString(unsigned(d1.month())), int(d1.year()),
                 unsigned(d2.day()), MonthString(unsigned(d2.month())), int(d2.year()));
    }
    else if (DatePicker->RangeType == Range_Type_Monthly)
    {
        auto d1 = date::year_month_day{DatePicker->Start};
        snprintf(DatePicker->DateLabel, ArrayCount(DatePicker->DateLabel), 
                 "%s %i", MonthString(unsigned(d1.month())), int(d1.year()));
    }
}

void DatePickerClipAndUpdate(date_picker *DatePicker, date::sys_days OldestDate, date::sys_days NewestDate)
{
    DatePicker->Start = Clamp(DatePicker->Start, OldestDate, NewestDate);
    DatePicker->End = Clamp(DatePicker->End, OldestDate, NewestDate);
    
    Assert(DatePicker->Start <= DatePicker->End);
    Assert(DatePicker->Start >= OldestDate);
    Assert(DatePicker->End <= NewestDate);
    
    // If start is clipped then there is always no earlier day/week/month (can't go backwards)
    // Or start is not clipped but is on the oldest date (can't go backwards)
    // If start is not clipped then must be at the beginning of a week or month, so any earlier date (i.e. oldest) would have to be in another week or month (can go backwards)
    
    DatePicker->IsBackwardsDisabled = (DatePicker->Start == OldestDate) || DatePicker->RangeType == Range_Type_Custom;
    DatePicker->IsForwardsDisabled = (DatePicker->End == NewestDate) || DatePicker->RangeType == Range_Type_Custom;
    
    DatePickerUpdateLabel(DatePicker, OldestDate, NewestDate);
}

void DatePickerBackwards(date_picker *DatePicker, date::sys_days OldestDate, date::sys_days NewestDate) 
{
    if (DatePicker->RangeType == Range_Type_Daily)
    {
        DatePicker->Start -= date::days{1};
        DatePicker->End -= date::days{1};
    }
    else if (DatePicker->RangeType == Range_Type_Weekly)
    {
        // start End stay on monday and sunday of week unless clipped
        DatePicker->Start -= date::days{7};
        DatePicker->End -= date::days{7};
        
        // If End is currently clipped to the newest date then it may not be on sunday, so subtracting 7 days will reduce the date by too much, so need to add days to get back to the weeks sunday
        // This is not a problem for the start date if we are going backwards
        auto DayOfWeek = date::weekday{DatePicker->End};
        if (DayOfWeek != date::Sunday)
        {
            auto DaysToNextSunday = date::Sunday - DayOfWeek;
            DatePicker->End +=  DaysToNextSunday;
        }
    }
    else if (DatePicker->RangeType == Range_Type_Monthly)
    {
        auto d = date::year_month_day{DatePicker->Start} - date::months{1};
        DatePicker->Start = date::sys_days{d.year()/d.month()/1};
        DatePicker->End = date::sys_days{d.year()/d.month()/date::last};
    }
    
    DatePickerClipAndUpdate(DatePicker, OldestDate, NewestDate);
}

void DatePickerForwards(date_picker *DatePicker, date::sys_days OldestDate, date::sys_days NewestDate) 
{
    if (DatePicker->RangeType == Range_Type_Daily)
    {
        DatePicker->Start += date::days{1};
        DatePicker->End += date::days{1};
    }
    else if (DatePicker->RangeType == Range_Type_Weekly)
    {
        // start End stay on monday and sunday of week unless clipped
        DatePicker->Start += date::days{7};
        DatePicker->End += date::days{7};
        
        // If we overshot our monday because start was clipped to a non-monday day, we need to move back
        auto DayOfWeek = date::weekday{DatePicker->Start};
        if (DayOfWeek != date::Monday)
        {
            auto DaysBackToMonday = DayOfWeek - date::Monday;
            DatePicker->Start -= DaysBackToMonday;
        }
    }
    else if (DatePicker->RangeType == Range_Type_Monthly)
    {
        auto d = date::year_month_day{DatePicker->End} + date::months{1};
        DatePicker->Start = date::sys_days{d.year()/d.month()/1};
        DatePicker->End = date::sys_days{d.year()/d.month()/date::last};
    }
    
    DatePickerClipAndUpdate(DatePicker, OldestDate, NewestDate);
}

void InitDatePicker(date_picker *DatePicker, date::sys_days CurrentDate, 
                    date::sys_days RecordedOldestDate, date::sys_days RecordedNewestDate)
{
    // Current used for selected date of calendar and the picker
    // oldest and newest used for date picker and calendar limits
    
    // TODO: What if we loaded from a file and have no dates near CurrentDate
    // either need get_records_in_date_range to return no records or make date picker
    // initialised within range
    // For now allow zero records to be displayed,  also good is user installed for first time and has no records
    
    if (CurrentDate > RecordedNewestDate) 
    {
        CurrentDate = RecordedNewestDate;
    }
    
#if 0
    Datepicker->RangeType = Range_Type_Daily;
    Datepicker->Start = CurrentDate;
    Datepicker->End = CurrentDate;
#else
    // DEBUG: Default to monthly
    auto ymd = date::year_month_day{CurrentDate};
    DatePicker->RangeType = Range_Type_Monthly;
    DatePicker->Start = date::sys_days{ymd.year()/ymd.month()/1};
    DatePicker->End   = date::sys_days{ymd.year()/ymd.month()/date::last};
#endif
    
    // sets label and if buttons are disabled 
    DatePickerClipAndUpdate(DatePicker, RecordedOldestDate, RecordedNewestDate);
    
    InitCalendar(&DatePicker->FirstCalendar, CurrentDate, RecordedOldestDate, RecordedNewestDate);
    InitCalendar(&DatePicker->SecondCalendar, CurrentDate, RecordedOldestDate, RecordedNewestDate);
}

bool DoDateSelectPopup(date_picker *DatePicker, date::sys_days OldestDate, date::sys_days NewestDate)
{
    bool RangeChanged = false;
    ImVec2 Pos = ImVec2(ImGui::GetWindowWidth() / 2.0f, ImGui::GetCursorPosY() + 3);
    ImGui::SetNextWindowPos(Pos, true, ImVec2(0.5f, 0)); // centre on x
    
    //ImGui::SetNextWindowSize(ImVec2(300,300));
    
    if (ImGui::BeginPopup("Date Select"))
    {
        char *Items[] = {"Day", "Week", "Month", "Custom"};
        
        int PrevRangeType = DatePicker->RangeType;
        
#if 1
        ImVec2 ButtonSize = ImVec2(DATE_PICKER_RANGE_BUTTON_WIDTH, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 10.0f));
        if (ButtonConditionallyFilled(Items[0], DatePicker->RangeType != Range_Type_Daily, ButtonSize))
        {
            DatePicker->RangeType = Range_Type_Daily;
        }
        ImGui::SameLine();
        if (ButtonConditionallyFilled(Items[1], DatePicker->RangeType != Range_Type_Weekly, ButtonSize))
        {
            DatePicker->RangeType = Range_Type_Weekly;
        }
        ImGui::SameLine();
        if (ButtonConditionallyFilled(Items[2], DatePicker->RangeType != Range_Type_Monthly, ButtonSize))
        {
            DatePicker->RangeType = Range_Type_Monthly;
        }
        ImGui::SameLine();
        if (ButtonConditionallyFilled(Items[3], DatePicker->RangeType != Range_Type_Custom, ButtonSize))
        {
            DatePicker->RangeType = Range_Type_Custom;
        }
        ImGui::PopStyleVar();
#else
        ImGui::Combo("##picker", (int *)&DatePicker->RangeType, Items, ArrayCount(Items));
#endif
        if (DatePicker->RangeType != PrevRangeType)
        {
            RangeChanged = true;
            if (DatePicker->RangeType == Range_Type_Daily)
            {
                // end date stays the same
                DatePicker->Start = DatePicker->End;
            }
            else if  (DatePicker->RangeType == Range_Type_Weekly)
            {
                // Select the week that end date was in
                auto DaysToNextSunday = date::Sunday - date::weekday{DatePicker->End};
                
                DatePicker->End = DatePicker->End + DaysToNextSunday;
                DatePicker->Start = DatePicker->End - date::days{6};
            }
            else if  (DatePicker->RangeType == Range_Type_Monthly)
            {
                // Select the month that end date was in
                auto ymd = date::year_month_day{DatePicker->End};
                DatePicker->Start = date::sys_days{ymd.year()/ymd.month()/1};
                DatePicker->End   = date::sys_days{ymd.year()/ymd.month()/date::last};
            }
            else if (DatePicker->RangeType == Range_Type_Custom)
            {
                // Either calendar can be before other
                DatePicker->Start = std::min(DatePicker->FirstCalendar.SelectedDate, DatePicker->SecondCalendar.SelectedDate);
                DatePicker->End = std::max(DatePicker->FirstCalendar.SelectedDate, DatePicker->SecondCalendar.SelectedDate);
            }
            else
            {
                Assert(0);
            }
            
            DatePickerClipAndUpdate(DatePicker, OldestDate, NewestDate);
        }
        
        if (DoCalendarButton(&DatePicker->FirstCalendar, OldestDate, NewestDate))
        {
            if (DatePicker->RangeType == Range_Type_Custom)
            {
                RangeChanged = true;
                DatePicker->Start = std::min(DatePicker->FirstCalendar.SelectedDate, DatePicker->SecondCalendar.SelectedDate);
                DatePicker->End = std::max(DatePicker->FirstCalendar.SelectedDate, DatePicker->SecondCalendar.SelectedDate);
                DatePickerClipAndUpdate(DatePicker, OldestDate, NewestDate);
            }
        } 
        ImGui::SameLine();
        if (DoCalendarButton(&DatePicker->SecondCalendar, OldestDate, NewestDate))
        {
            if (DatePicker->RangeType == Range_Type_Custom)
            {
                RangeChanged = true;
                DatePicker->Start = std::min(DatePicker->FirstCalendar.SelectedDate, DatePicker->SecondCalendar.SelectedDate);
                DatePicker->End = std::max(DatePicker->FirstCalendar.SelectedDate, DatePicker->SecondCalendar.SelectedDate);
                DatePickerClipAndUpdate(DatePicker, OldestDate, NewestDate);
            }
        }
        
        ImGui::EndPopup();
    }
    
    return RangeChanged;
}
