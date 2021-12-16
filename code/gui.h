#ifndef UI_H
#define UI_H

#include "imgui.h" // ImFont
#include "date.h"
#include "apps.h"

#include <vector>

// Seems that all sizes work except 16 and except larger sizes (64 works though)
constexpr u32 ICON_SIZE = 32;

constexpr u32 DEFAULT_LOCAL_PROGRAM_ICON_INDEX = 0;
constexpr u32 DEFAULT_WEBSITE_ICON_INDEX = 1;

struct Record;
struct Day;

struct Icon_Asset
{
    u32 texture_handle;
    int width;
    int height;
};

struct calendar
{
    date::year_month_weekday FirstDayOfMonth;
    date::sys_days SelectedDate;
    
    char ButtonLabel[64]; // set on ui initialisation
    bool IsBackwardsDisabled; // set when popup opened
    bool IsForwardsDisabled; // set when popup opened
};

enum range_type : int
{
    Range_Type_Daily = 0, // code relies on these number values/order
    Range_Type_Weekly,
    Range_Type_Monthly,
    Range_Type_Custom,
};

struct date_picker
{
    // TODO: When new days are added do we update end_date
    range_type RangeType;
    
    date::sys_days Start;
    date::sys_days End;
    
    // Longest string "30st September 2020 - 31st September 2020"
    // 41 chars
    char DateLabel[64];
    bool IsBackwardsDisabled;
    bool IsForwardsDisabled;
    calendar FirstCalendar;
    calendar SecondCalendar;
};

struct edit_settings
{
    temp_memory TempMemory;
    misc_options MiscOptions;
    
    char **Keywords;
    
    bool KeywordLimitError;
    bool KeywordDisabledCharacterError;
};

struct gui
{
    // TODO: This probably makes more sense as a hash table, as not all ids will have a icon (array would essentially be sparse)
    //std::vector<s32> icon_indexes;  // -1 means not loaded
    //std::vector<Icon_Asset> icons;
    //u32 *icon_bitmap_storage;
    
    arena Arena; 
    
    date_picker DatePicker;
    
    edit_settings EditSettings;
    
    ImFont *SmallFont;
    bool DateRangeChanged;
    
    bool IsLoaded;
};



#endif //UI_H
