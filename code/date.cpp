#include <time.h>
#include "monitor.h"

// Dates
// Library has two main types used in this program:

// sys_days - which is the main canonical calendrical type of the library, and is a count of days since the 
// 1970 epoch. This is good for arithmetic using days because it is just integer operations. Under the hood it is
// just:
//     using sys_time    = std::chrono::time_point<std::chrono::system_clock, Duration>
//     using sys_days    = sys_time<days>;
// where the Duration days is just represented by an integer and has a period with a ration of 24 hours 

int days_in_month(int month, int year)
{
    switch (month)
    {
        case 0: case 2: case 4:
        case 6: case 7: case 9:
        case 11:
        return 31;
        
        case 3: case 5:
        case 8: case 10:
        return 30;
        
        case 1:
        if (((year % 4 == 0) && (year % 100 != 0)) || (year % 400))
            return 29;
        else
            return 28;
    }
    
    return 0;
}

void set_to_prev_day(Date *date)
{
    if (date->dd == 1 && date->mm == 0)
    {
        rvl_assert(date->yy > 0);
        date->yy -= 1;
    }
    
    date->mm -= 1;
    if (date->mm == -1) date->mm = 11;
    
    date->dd -= 1;
    if (date->dd == 0)
    {
        date->dd = days_in_month(date->mm, date->yy);
    }
}


void set_to_next_day(Date *date)
{
    if (date->dd == days_in_month(date->mm, date->yy))
    {
        if (date->mm == 11)
        {
            date->yy += 1;
            date->mm = 0;
            date->dd = 1;
        }
        else
        {
            date->mm += 1;
            date->dd = 1;
        }
    }
    else
    {
        date->dd += 1;
    }
}

bool is_later_than(Date date1, Date date2)
{
    return ((date1.yy > date2.yy) ||
            (date1.yy == date2.yy && date1.mm > date2.mm) ||
            (date1.yy == date2.yy && date1.mm == date2.mm && date1.dd > date2.dd));
}


// Not sure if it matters if it isn't
static_assert(std::is_pod<Date>::value, "Isn't POD");


bool
valid_date(Date date)
{
    if (date.dd > 0 ||
        date.mm >= 0 || date.mm <= 11 ||
        date.yy < 2200) // Very high upper limmit
    {
        if (date.dd <= days_in_month(date.mm, date.yy))
        {
            return true;
        }
    }
    
    return false;
}

Date get_current_date()
{
    time_t time_since_epoch = time(NULL);
    tm *local_time = localtime(&time_since_epoch);
    local_time->tm_year += 1900;
    
    // Tues night -------------------->|Mon morning (DayStart == 2*60)
    // Tues night ---->|Mon morning ------------>   (DayStart == 0*60)
    // | 22:00 | 23:00 | 00:00 | 01:00 | 02:00 |
    Date date {(u8)local_time->tm_mday, (u8)local_time->tm_mon, (u16)local_time->tm_year};
    
    return date;
}

// fake very quick days
Date get_current_date(s64 time)
{
    Date date = get_current_date();
    
    int iterations = time / 2.0;
    for (int i = 0; i < iterations; ++i)
    {
        set_to_next_day(&date);
    }
    
    return date; 
}

Date
get_current_canonical_date(u16 day_start_time)
{
    time_t time_since_epoch = time(NULL);
    tm *local_time = localtime(&time_since_epoch);
    local_time->tm_year += 1900;
    
    // Tues night -------------------->|Mon morning (DayStart == 2*60)
    // Tues night ---->|Mon morning ------------>   (DayStart == 0*60)
    // | 22:00 | 23:00 | 00:00 | 01:00 | 02:00 |
    Date date {(u8)local_time->tm_mday, (u8)local_time->tm_mon, (u16)local_time->tm_year};
    
    i32 current_minute = local_time->tm_hour*60 + local_time->tm_min;
    if (current_minute < day_start_time)
    {
        set_to_prev_day(&date);
    }
    
    return date;
}

u32 leap_years_count(u16 year)
{
    return year/4 - year/100 + year/400;
}

u32 day_count(Date date)
{
    // Since 01/01/0000 AD (well more like since 00/00/0000 I think)
    u32 days_count = date.yy*365 + date.dd;
    u8 days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; // Not counting possible leap days
    u32 days_in_month_accumulative[12] = {31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
    for (u32 i = 0; i < date.mm; ++i)
    {
        days_count += days_in_month[i];
    }
    
    rvl_assert(days_count == date.yy*365 + date.dd + days_in_month_accumulative[std::max(0, date.mm-1)]);
    
    // If it is jan or feb don't add this year's possible leap day, it is already added in by the day field.
    u32 leap_years = (date.mm <= 1) ? leap_years_count(date.yy - 1) : leap_years_count(date.yy);
    days_count += leap_years;
    
    return days_count;
}

s32 day_difference(Date date1, Date date2)
{
    // TODO: Better system, there are probably lots of complexities here
    rvl_assert(valid_date(date1) && valid_date(date2));
    
    s32 diff = (s32)day_count(date1) - (s32)day_count(date2);
    
    return diff;
}