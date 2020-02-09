

// Make into year|month|day to allow sorting?

// Day (1-31) Month (0-11) Years after 2000 (0-127)
static constexpr u16 DayMask   = 0b1111100000000000;
static constexpr u16 MonthMask = 0b0000011110000000;
static constexpr u16 YearMask  = 0b0000000001111111;


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
        if (year % 4 == 0)
            return 29;
        else
            return 28;
    }
    
    return 0;
}

struct Date
{
    int day; // 1-31
    int month; // 0-11
    int year;  // 0 AD to INT_MAX AD
    
    void set_to_prev_day()
    {
        if (day == 1 && month == 0)
        {
            rvl_assert(year > 0);
            year -= 1;
        }
        
        month -= 1;
        if (month == -1) month = 11;
        
        day -= 1;
        if (day == 0)
        {
            day = days_in_month(month, year);
        }
    }
    
    
    void set_to_next_day()
    {
        if (day == days_in_month(day, year))
        {
            if (month == 11)
            {
                year += 1;
                month = 0;
                day = 1;
            }
            else
            {
                month += 1;
                day = 1;
            }
        }
        else
        {
            day += 1;
        }
    }
    
    bool is_same(Date date)
    {
        return (day == date.day && month == date.month && year == date.year);
    }
    
    bool is_later_than(Date date)
    {
        return ((year > date.year) ||
                (year == date.year && month > date.month) ||
                (year == date.year && month == date.month && day > date.day));
    }
};


// Not sure if it matters if it isn't
static_assert(std::is_pod<Date>::value, "Isn't POD");


bool
valid_date(Date date)
{
    if (date.day > 0 ||
        date.month >= 0 || date.month <= 11 ||
        date.year >= 2019 || date.year <= 2200)
    {
        if (date.day <= days_in_month(date.month, date.year))
        {
            return true;
        }
    }
    
    return false;
}

Date
unpack_16_bit_date(u16 date)
{
    Date d;
    d.day   = (date & DayMask) >> 11;
    d.month = (date & MonthMask) >> 7;
    d.year  = (date & YearMask) + 2000;
    
    return d;
}

u16
pack_16_bit_date(Date date)
{
    u16 day_bits   = (u16)date.day << 11;
    u16 month_bits = (u16)date.month << 7;
    u16 year_bits  = (u16)date.year - 2000;
    
    return day_bits|month_bits|year_bits;
}

bool
valid_16_bit_date(u16 date)
{
    Date d = unpack_16_bit_date(date);
    return valid_date(d);
}

u16 get_current_date()
{
    time_t time_since_epoch = time(NULL);
    tm *local_time = localtime(&time_since_epoch);
    local_time->tm_year += 1900;
    
    // Tues night -------------------->|Mon morning (DayStart == 2*60)
    // Tues night ---->|Mon morning ------------>   (DayStart == 0*60)
    // | 22:00 | 23:00 | 00:00 | 01:00 | 02:00 |
    Date date {local_time->tm_mday, local_time->tm_mon, local_time->tm_year};
    
    return pack_16_bit_date(date); 
}

// fake very quick days
u16 get_current_date(double time)
{
    static int c = 1;
    u16 date = get_current_date();
    if (time > 3.0 * c)
    {
        Date d = unpack_16_bit_date(date);
        for (int i = 0; i < c; ++i)
        {
            d.set_to_next_day();
        }
        date = pack_16_bit_date(d);
        c += 1;
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
    Date date {local_time->tm_mday, local_time->tm_mon, local_time->tm_year};
    
    i32 current_minute = local_time->tm_hour*60 + local_time->tm_min;
    if (current_minute < day_start_time)
    {
        date.set_to_prev_day();
    }
    
    return date;
}
