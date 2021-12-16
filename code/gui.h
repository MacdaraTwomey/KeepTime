#ifndef GUI_H
#define GUI_H

#include "base.h"
#include "date.h"
#include "apps.h"

struct ImFont;

// Seems that all sizes work except 16 and except larger sizes (64 works though)
static constexpr u32 ICON_SIZE = 32;

static constexpr u32 DEFAULT_PROGRAM_ICON_INDEX = 0;
static constexpr u32 DEFAULT_WEBSITE_ICON_INDEX = DEFAULT_PROGRAM_ICON_INDEX; // TODO: fixup
static constexpr u32 MAX_ICON_COUNT = 10000;

struct icon_asset
{
    u32 TextureHandle;
    s32 Width;
    s32 Height;
    bool Loaded;
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
    
    // Icons make sense as a fixed size pool allocator (or we could just never free the icon memory)
    // TODO: These are always a fixed size right? so do we need to store width height
    icon_asset *Icons;
    u64 IconCount;
    
    arena Arena; 
    
    date_picker DatePicker;
    
    edit_settings EditSettings;
    
    ImFont *SmallFont;
    bool DateRangeChanged;
    
    record *SortedRecords;
    u64 SortedRecordCount;
    
    bool IsLoaded;
};



#endif //GUI_H
