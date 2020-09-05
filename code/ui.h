#ifndef UI_H
#define UI_H

#include "imgui.h" // ImFont
#include "apps.h"

#include "date.h"

#include <vector>

// Seems that all sizes work except 16 and except larger sizes (64 works though)
constexpr u32 ICON_SIZE = 32;

constexpr u32 DEFAULT_LOCAL_PROGRAM_ICON_INDEX = 0;
constexpr u32 DEFAULT_WEBSITE_ICON_INDEX = 1;

struct Record;
struct Day;

struct Day_View
{
    // feels somewhat unneeded, but the concept is good
    std::vector<Day> days;
    Record *copy_of_current_days_records;
};

struct Icon_Asset
{
    u32 texture_handle;
    int width;
    int height;
};

typedef char Keyword_Array[MAX_KEYWORD_COUNT][MAX_KEYWORD_SIZE];

struct Edit_Settings
{
    // This array can have blank strings representing empty input boxes in between valid strings
    Keyword_Array pending;
    Misc_Options misc;
    bool keyword_limit_error;
    bool keyword_disabled_character_error;
};

struct Calendar
{
    date::year_month_weekday first_day_of_month;
    date::sys_days selected_date;
    
    char button_label[64]; // set on ui initialisation
    bool is_backwards_disabled; // set when popup opened
    bool is_forwards_disabled; // set when popup opened
};

enum Range_Type : int
{
    Range_Type_Daily = 0, // code relies on these number values/order
    Range_Type_Weekly,
    Range_Type_Monthly,
    Range_Type_Custom,
};

struct Date_Picker
{
    // TODO: When new days are added do we update end_date
    Range_Type range_type;
    
    date::sys_days start;
    date::sys_days end;
    
    // Longest string "30st September 2020 - 31st September 2020"
    // 41 chars
    char date_label[64];
    bool is_backwards_disabled;
    bool is_forwards_disabled;
    Calendar first_calendar;
    Calendar second_calendar;
};

struct UI_State
{
    // TODO: This probably makes more sense as a hash table, as not all ids will have a icon (array would essentially be sparse)
    std::vector<s32> icon_indexes;  // -1 means not loaded
    std::vector<Icon_Asset> icons;
    u32 *icon_bitmap_storage;
    
    Date_Picker date_picker;
    Day_View day_view;
    Edit_Settings *edit_settings; // allocated when needed
    
    Record *sorted_records;
    u32 record_count;
    
    ImFont *small_font;
    bool date_range_changed;
    bool open;
};


#endif //UI_H
