
#include "monitor.h"
//#include "spectrum.h"
//#include "spectrum.cpp"

constexpr float UI_DEFAULT_BUTTON_WIDTH = 100;


// from monitor.cpp
Icon_Asset * get_app_icon_asset(Database *database, App_Id id);
void add_keyword(Settings *settings, char *str);
String get_app_name(App_List *apps, App_Id id);



#if 1
struct FreeTypeTest
{
    enum FontBuildMode
    {
        FontBuildMode_FreeType,
        FontBuildMode_Stb
    };
    
    FontBuildMode BuildMode;
    bool          WantRebuild;
    float         FontsMultiply;
    int           FontsPadding;
    unsigned int  FontsFlags;
    float font_size;
    
    FreeTypeTest()
    {
        BuildMode = FontBuildMode_FreeType;
        WantRebuild = true;
        FontsMultiply = 1.0f;
        FontsPadding = 1;
        FontsFlags = ImGuiFreeType::LightHinting;;
        font_size = 22.0f;
    }
    
    // Call _BEFORE_ NewFrame()
    bool UpdateRebuild()
    {
        if (!WantRebuild)
            return false;
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->TexGlyphPadding = FontsPadding;
        for (int n = 0; n < io.Fonts->ConfigData.Size; n++)
        {
            ImFontConfig* font_config = (ImFontConfig*)&io.Fonts->ConfigData[n];
            font_config->RasterizerMultiply = FontsMultiply;
            font_config->RasterizerFlags = (BuildMode == FontBuildMode_FreeType) ? FontsFlags : 0x00;
            font_config->SizePixels = font_size;
        }
        if (BuildMode == FontBuildMode_FreeType)
            ImGuiFreeType::BuildFontAtlas(io.Fonts, FontsFlags);
        else if (BuildMode == FontBuildMode_Stb)
            io.Fonts->Build();
        WantRebuild = false;
        return true;
    }
    
    // Call to draw interface
    void ShowFreetypeOptionsWindow()
    {
        ImGui::Begin("FreeType Options");
        ImGui::ShowFontSelector("Fonts");
        WantRebuild |= ImGui::RadioButton("FreeType", (int*)&BuildMode, FontBuildMode_FreeType);
        ImGui::SameLine();
        WantRebuild |= ImGui::RadioButton("Stb (Default)", (int*)&BuildMode, FontBuildMode_Stb);
        WantRebuild |= ImGui::DragFloat("Multiply", &FontsMultiply, 0.001f, 0.0f, 2.0f);
        WantRebuild |= ImGui::DragInt("Padding", &FontsPadding, 0.1f, 0, 16);
        
        ImGui::SetCursorPosY(300);
        ImGui::PushItemWidth(200);
        WantRebuild |= ImGui::InputFloat("Size", &font_size, 1.0f, 0, 2);
        ImGui::PopItemWidth();
        
        if (BuildMode == FontBuildMode_FreeType)
        {
            WantRebuild |= ImGui::CheckboxFlags("NoHinting",     &FontsFlags, ImGuiFreeType::NoHinting);
            WantRebuild |= ImGui::CheckboxFlags("NoAutoHint",    &FontsFlags, ImGuiFreeType::NoAutoHint);
            WantRebuild |= ImGui::CheckboxFlags("ForceAutoHint", &FontsFlags, ImGuiFreeType::ForceAutoHint);
            WantRebuild |= ImGui::CheckboxFlags("LightHinting",  &FontsFlags, ImGuiFreeType::LightHinting);
            WantRebuild |= ImGui::CheckboxFlags("MonoHinting",   &FontsFlags, ImGuiFreeType::MonoHinting);
            WantRebuild |= ImGui::CheckboxFlags("Bold",          &FontsFlags, ImGuiFreeType::Bold);
            WantRebuild |= ImGui::CheckboxFlags("Oblique",       &FontsFlags, ImGuiFreeType::Oblique);
            WantRebuild |= ImGui::CheckboxFlags("Monochrome",    &FontsFlags, ImGuiFreeType::Monochrome);
        }
        ImGui::End();
    }
};
#endif

void
init_imgui(float font_size)
{
    ImGuiIO& io = ImGui::GetIO(); 
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.IniFilename = NULL; // Disable imgui.ini filecreation
    
    ImGui::StyleColorsLight();
    
#if 0
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_PopupBg]                = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = light_grey;
    colors[ImGuiCol_ScrollbarGrabHovered]   = medium_grey;
    colors[ImGuiCol_ScrollbarGrabActive]    = dark_grey;
    colors[ImGuiCol_CheckMark]              = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    
    colors[ImGuiCol_SliderGrab]             = light_grey;
    colors[ImGuiCol_SliderGrabActive]       = dark_grey;
    
    colors[ImGuiCol_Button]                 = light_grey;
    colors[ImGuiCol_ButtonHovered]          = medium_grey;
    colors[ImGuiCol_ButtonActive]           = dark_grey;
#endif
    
    // TODO: could remove glyphs to reduce size with online tool
    io.Fonts->AddFontFromMemoryCompressedTTF(SourceSansProRegular_compressed_data, SourceSansProRegular_compressed_size, font_size);
    ImFontConfig icons_config; 
    icons_config.MergeMode = true; 
    icons_config.PixelSnapH = true;
    
    // NOTE: glyph ranges must exist at least until atlas is built
    ImVector<ImWchar> range;
    ImFontGlyphRangesBuilder builder;
    builder.AddText(ICON_MD_DATE_RANGE);
    builder.AddText(ICON_MD_ARROW_FORWARD);
    builder.AddText(ICON_MD_ARROW_BACK);
    builder.AddText(ICON_MD_SETTINGS);
    builder.BuildRanges(&range);                          
    
    // TODO: could remove glyphs to reduce size with online tool
    icons_config.GlyphOffset = ImVec2(0, 4); // move glyphs down or else they render too high
    io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\build\\fonts\\MaterialIcons-Regular.ttf", font_size, &icons_config, range.Data);
    
    // NOTE: FreeType assumes blending in linear space rather than gamma space. See FreeType note for FT_Render_Glyph. For correct results you need to be using sRGB and convert to linear space in the pixel shader output. The default Dear ImGui styles will be impacted by this change (alpha values will need tweaking).
    // NOTE: Freetype is an additional dependency/dll ...
#if 1
    // for freetype: call before Build or GetTexDataAsRGBA32 
    unsigned font_flags = ImGuiFreeType::LightHinting;
    for (int n = 0; n < io.Fonts->ConfigData.Size; n++)
    {
        ImFontConfig* font_config = (ImFontConfig*)&io.Fonts->ConfigData[n];
        font_config->RasterizerMultiply = 1.0f;
        font_config->RasterizerFlags = font_flags;
    }
    ImGuiFreeType::BuildFontAtlas(io.Fonts, font_flags);
#else
    // for stb:
    //io.Fonts->Build();
#endif
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 0.0f; // 0 to 12 ? 
    style.WindowBorderSize = 1.0f; // or 1.0
    style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.WindowRounding = 0.0f;
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
    // TODO: I don't love this probably better to just copy to a new array
    
    Arena scratch = {};
    init_arena(&scratch, MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE);
    Array<String, MAX_KEYWORD_COUNT> pending_keywords;
    for (int i = 0; i < MAX_KEYWORD_COUNT; ++i)
    {
        if (keywords[i][0] != '\0') // not empty string
        {
            pending_keywords.add_item(push_string(&scratch, keywords[i]));
        }
    }
    
    
#if 0    
	// NOTE: This assumes that keywords that are 'empty' should have no garbage data (dear imgu needs to set
	// empty array slots to '\0';
	s32 total_count = 0;
	for (s32 l = 0, r = MAX_KEYWORD_COUNT; l < r;)
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
#endif
    
	// Try to add words from pending keywords into keywords array (not adding duplicates)
    memset(keywords, 0, MAX_KEYWORD_COUNT*MAX_KEYWORD_SIZE);
    s32 total_count = 0;
	for (s32 i = 0; i < pending_keywords.count; ++i)
	{
        bool no_matches_in_keywords = true;
		for (s32 j = i + 1; j < total_count; ++j)
		{
			if (string_equals(pending_keywords[i], keywords[j]))
			{
                no_matches_in_keywords = false;
                break;
			}
        }
        
        if (no_matches_in_keywords)
        {
            strcpy(keywords[total_count], pending_keywords[i].str);
            total_count += 1;
        }
	}
    
    free_arena(&scratch);
    
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

void
do_misc_settings(Edit_Settings *edit_settings)
{
    //ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.30f);
    
#if 0
    // TODO: Do i need to use bool16 for this (this is workaround)
    bool selected = edit_settings->misc_options.run_at_system_startup;
    ImGui::Checkbox("Run at startup", &selected);
    edit_settings->misc_options.run_at_system_startup = selected;
#endif
    
    // TODO: DEBUG make the step much greater
    float freq_secs = (float)edit_settings->misc_options.poll_frequency_milliseconds / MILLISECS_PER_SEC;
    ImGui::PushItemWidth(150);
    if (ImGui::InputFloat("Update frequency (s)", &freq_secs, 0.01f, 1.0f, 2))
    {
        if (freq_secs < POLL_FREQUENCY_MIN_SECONDS) 
            freq_secs = POLL_FREQUENCY_MIN_SECONDS;
        else if (freq_secs > POLL_FREQUENCY_MAX_SECONDS) 
            freq_secs = POLL_FREQUENCY_MAX_SECONDS;
        else 
            edit_settings->misc_options.poll_frequency_milliseconds = (u32)(freq_secs * MILLISECS_PER_SEC);
    }
    ImGui::PopItemWidth();
    
    //ImGui::PopItemWidth();
    
    if (ImGui::Button("About"))
    {
        ImGui::OpenPopup("About " PROGRAM_NAME);
    }
    
    bool about_open = true;
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetWindowWidth() * 0.5, 0));
    if (ImGui::BeginPopupModal("About " PROGRAM_NAME, &about_open, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text(PROGRAM_NAME);
        ImGui::Text("Version " VERSION_STRING);
        ImGui::Text("Made by " NAME_STRING);
        ImGui::Text("License " LICENSE_STRING);
        
        ImGui::NewLine();
        if (ImGui::Button("OK", ImVec2(UI_DEFAULT_BUTTON_WIDTH, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    
    char *times[] = {
        "12:00 AM",
        "12:15 AM",
        "12:30 AM",
        "etc..."
    };
    
#if 0    
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
#endif
    
}

void
do_keyword_input_list(Edit_Settings *edit_settings)
{
    bool all = true;
    bool custom = false;
    bool none = false;
    if (ImGui::RadioButton("All", all))
    {
        
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Custom", custom))
    {
        
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("None", none))
    {
        
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 50);
    
    s32 give_focus = -1;
    if (ImGui::Button("Add..."))
    {
        // Give focus to first free input box
        for (int i = 0; i < MAX_KEYWORD_COUNT; ++i)
        {
            if (edit_settings->pending[i][0] == '\0')
            {
                give_focus = i;
                break;
            }
        }
        
        if (give_focus == -1)
        {
            // All boxes full
            edit_settings->keyword_limit_error = true;
            edit_settings->keyword_disabled_character_error = false;
        }
    }
    
    // Need to do this before potentially setting error, to avoid immediately clearing error, when the mouse click to add button is pressed, TODO: Is this true?
    if (edit_settings->keyword_limit_error || edit_settings->keyword_disabled_character_error) 
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(200,0,0,255), edit_settings->keyword_limit_error ? 
                           "   * Maximum number of keywords reached (100)" : 
                           "   * Valid characters are A-Z a-z 0-9 . -");
        
        // clear error message on click 
        if (ImGui::IsAnyMouseDown())
        {
            edit_settings->keyword_limit_error = false;
            edit_settings->keyword_disabled_character_error = false;
        }
    }
    
    //ImGui::SameLine();
#if 0
    //HelpMarker("As with every widgets in dear imgui, we never modify values unless there is a user interaction.\nYou can override the clamping limits by using CTRL+Click to input a value.");
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
#endif
    
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
        float list_width = ImGui::GetWindowWidth();
        ImGui::BeginChild(222, ImVec2(list_width, 0), true);
        
        // If I use WindowDrawList must subtract padding or else lines are obscured by below text input box
        // can use ForegroundDrawList but this is not clipped to keyword child window
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        //ImDrawList* draw_list = ImGui::GetForeGroundDrawList();
        
        b32 entered_disabled_character = false;
        
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0); // remove input box frame
        ImGui::PushItemWidth(list_width*0.95);
        for (s32 i = 0; i < number_of_boxes; ++i)
        {
            if (i == give_focus)
            {
                ImGui::SetKeyboardFocusHere();
                give_focus = -1;
            }
            
            auto filter = [](ImGuiTextEditCallbackData* data) -> int {
                // Domain names only allow alphanumeric and hyphen, and dots to separate subdomains
                // No UTF-8 allowed to be pasted either
                ImWchar c = data->EventChar;
                if ((c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= 0 && c <= 9) ||
                    c == '-' || c == '.')
                {
                    return 0; // allow
                }
                else
                {
                    b32 *ptr_to_entered_disabled_character = (b32 *)data->UserData;
                    *ptr_to_entered_disabled_character = true;
                    return 1;
                }
            };
            
            ImGui::PushID(i);
            ImGui::InputText("", edit_settings->pending[i], array_count(edit_settings->pending[i]), ImGuiInputTextFlags_CallbackCharFilter, filter, &entered_disabled_character);
            ImGui::PopID();
            
            float padding = 3;
            u32 grey = 150;
            ImVec2 pos = ImGui::GetCursorScreenPos();
            pos.y -= padding;
            draw_list->AddLine(pos, ImVec2(pos.x + list_width, pos.y), IM_COL32(grey,grey,grey,255), 1.0f);
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleVar();
        
        if (entered_disabled_character)
        {
            edit_settings->keyword_disabled_character_error = true;
            edit_settings->keyword_limit_error = false;
        }
        
        ImGui::EndChildFrame();
    }
}

bool
do_settings_popup(Settings *settings, Edit_Settings *edit_settings)
{
    bool update_settings = false;
    bool open = true;
    
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowWidth() / 2, ImGui::GetWindowHeight() / 2), ImGuiCond_Appearing, ImVec2(0.5, 0.5));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetWindowWidth() * 0.6, ImGui::GetWindowHeight() * 0.8));
    if (ImGui::BeginPopupModal("Settings", &open, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove))
    {
        ImGui::BeginChild(222, ImVec2(0,ImGui::GetWindowHeight() * 0.85), false);
        {
            ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
            if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
            {
                if (ImGui::BeginTabItem("UI"))
                {
                    do_misc_settings(edit_settings);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Website keywords"))
                {
                    do_keyword_input_list(edit_settings);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            
            ImGui::EndChild();
        }
        
        if (ImGui::Button("OK", ImVec2(UI_DEFAULT_BUTTON_WIDTH, 0)))
        {
            update_settings = true;
            ImGui::CloseCurrentPopup();
            open = false;
            
            // if (invalid settings) make many saying
            // "invalid settings"
            // [cancel][close settings without saving]
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(UI_DEFAULT_BUTTON_WIDTH, 0)))
        {
            // Don't update keywords
            ImGui::CloseCurrentPopup();
            open = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply", ImVec2(UI_DEFAULT_BUTTON_WIDTH, 0)))
        {
            update_settings = true;
        }
        
        ImGui::EndPopup(); // only close if BeginPopupModal returns true
    }
    
    if (update_settings)
    {
        // Empty input boxes are just zeroed arrays
        i32 pending_count = remove_duplicate_and_empty_keyword_strings(edit_settings->pending);
        
        settings->keywords.clear();
        reset_arena(&settings->keyword_arena);
        for (int i = 0; i < pending_count; ++i)
        {
            add_keyword(settings, edit_settings->pending[i]);
        }
        
        settings->misc_options = edit_settings->misc_options;
    }
    
    return !open;
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
    ImVec2 pos = ImVec2(ImGui::GetWindowWidth() / 2.0f, ImGui::GetCursorPosY() + 5);
    ImGui::SetNextWindowPos(pos, true, ImVec2(0.5f, 0)); // centre on x
    
    //ImGui::SetNextWindowSize(ImVec2(300,300));
    
    if (ImGui::BeginPopup("Date Select"))
    {
        // @WaitOnInput: this may change the date displayed on button
        
        char *items[] = {"Day", "Week", "Month", "Custom"};
        
        int prev_range_type = date_picker->range_type;
        
        
#if 0
        ImVec2 button_size = ImVec2(63, ImGui::GetFrameHeight());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 10.0f));
        if (ImGui::Button(items[0], button_size))
        {
            date_picker->range_type = Range_Type_Daily;
        }
        ImGui::SameLine();
        if (ImGui::Button(items[1], button_size))
        {
            date_picker->range_type = Range_Type_Weekly;
            
        }
        ImGui::SameLine();
        if (ImGui::Button(items[2], button_size))
        {
            date_picker->range_type = Range_Type_Monthly;
            
        }
        ImGui::SameLine();
        if (ImGui::Button(items[3], button_size))
        {
            date_picker->range_type = Range_Type_Custom;
        }
        ImGui::PopStyleVar();
#else
        if (ImGui::Combo("##picker", (int *)&date_picker->range_type, items, array_count(items)))
        {
            
        }
#endif
        
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



void
init_date_picker(Date_Picker *picker, date::sys_days current_date, 
                 date::sys_days oldest_date, date::sys_days newest_date)
{
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
    date_picker_clip_and_update(picker, oldest_date, newest_date);
    
    init_calendar(&picker->first_calendar, current_date, oldest_date, newest_date);
    init_calendar(&picker->second_calendar, current_date, oldest_date, newest_date);
}

// Database only needed for app_list, icons array, and debug menu
// State needed for datepicker and settings

FreeTypeTest freetype_test;

void 
draw_ui_and_update(SDL_Window *window, Monitor_State *state, Database *database, Day_View *day_view)
{
    ZoneScoped;
    
    App_List *apps = &database->apps;
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    
#if 0
    ImGui::NewFrame();
    bool show_demo_window = true;
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);
#else
    // Light hinting with freetype looks better than stb font renderer (at least at size 22 pixels)
    if (freetype_test.UpdateRebuild())
    {
        // REUPLOAD FONT TEXTURE TO GPU
        ImGui_ImplOpenGL3_DestroyDeviceObjects();
        ImGui_ImplOpenGL3_CreateDeviceObjects();
    }
    ImGui::NewFrame();
    
    bool show_demo_window = true;
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);
    
    ImGui::SetNextWindowPos(ImVec2(900, 0), true);
    freetype_test.ShowFreetypeOptionsWindow();
    
#endif
    
    
    
    ImGuiIO& io = ImGui::GetIO();
    
    
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoResize;
    
    static bool fullscreen = false;
    
    ImGui::SetNextWindowPos(ImVec2(0, 0), true);
    
    if (fullscreen)
        ImGui::SetNextWindowSize(io.DisplaySize); 
    else
        ImGui::SetNextWindowSize(ImVec2(850, 690), true);
    
    ImGui::Begin("Main window", nullptr, flags);
    
    ImGui::Checkbox("Fullscreen", &fullscreen);
    ImGui::SameLine();
    if (ImGui::Button("Hide"))
    {
        // debug: simulate effect of pressing x button
        win32_hide_win();
    }
    ImGui::SameLine();
    static bool UI_time = true;
    if (ImGui::Checkbox("SleepT", &UI_time))
    {
        if (UI_time)
        {
            platform_set_sleep_time(16);
        }
        else
        {
            platform_set_sleep_time(100);
        }
    }
    
    // TODO: Make in exact middle
    ImGui::SameLine(ImGui::GetWindowWidth() * 0.35f);
    
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
        if (ImGui::Button(date_picker->date_label, ImVec2(250, 0))) // passing zero uses default I guess
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
        
        bool closed = do_settings_popup(&state->settings, state->edit_settings);
        if (closed)
        {
            free(state->edit_settings);
            state->edit_settings = nullptr;
        }
    }
    
    ImGui::SameLine();
    
    // forward declare
    void do_debug_window_button(Database *database, Day_View *day_view);
    
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
        u32 one = 1;
        static u32 text_down = 5;
        //ImGui::InputScalar("text down", ImGuiDataType_U32, &text_down, &one);
        
        static u32 text_right = 7;
        //ImGui::InputScalar("text right", ImGuiDataType_U32, &text_right, &one);
        
        static float bar_height = 32;
        //ImGui::InputFloat("bar_height", &bar_height, 1.0f,0.0f,"%.3f");
        
        static float bar_down = 1;
        //ImGui::InputFloat("bar_down", &bar_down, 1.0f,0.0f,"%.3f");
        
        ImGui::BeginChild(55, ImVec2(0,ImGui::GetWindowHeight() * 0.9), true);
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        // TODO: Save records or sizes between frames
        u32 record_count = 0;
        Record *records = get_records_in_date_range(day_view, &record_count, state->date_picker.start, state->date_picker.end);
        if (records)
        {
            // TODO: Text position seems to depend on more than this, when window is resized
            time_type max_duration = records[0].duration;
            float max_bar_width = ImGui::GetWindowWidth() * 0.6f;
            float max_text_width = ImGui::GetWindowWidth() - 75.0f;
            float time_text_start = max_text_width + 5.0f;
            
            for (i32 i = 0; i < record_count; ++i)
            {
                Record &record = records[i];
                
                // if (record.id == 0) continue;
                
                if (i != 0)
                {
                    // After moving app name text down for in the above line we need to now move this whole line up to compensate for the added space (for every line but the first).
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    pos.y -= text_down; 
                    ImGui::SetCursorScreenPos(pos);
                }
                
                // TODO: Check if pos of bar or image will be clipped entirely, and maybe skip rest of loop
                
                Icon_Asset *icon = get_app_icon_asset(database, record.id);
                if (icon)
                {
                    auto border = ImVec4(0,0,0,255);
                    ImGui::Image((ImTextureID)(intptr_t)icon->texture_handle, ImVec2((float)icon->bitmap.width, (float)icon->bitmap.height),{0,0},{1,1},{1,1,1,1}, border);
                }
                
                // To start rect where text start
                ImGui::SameLine();
                
                // TODO: make sure colours stay with their record, in order of records.
                // ... But we probably won't have a live display, so colour change probably not be noticed between
                // separete openings of the ui, when the ordering of records can change.
                // The only thing is seeing the same app in different weeks/days etc with a different colour, which probably does mean each app needs a assigned colour per ui run. Can maybe put this in Icon_Asset and call it UI_Asset
                ImU32 colours[] = {
#if 1
                    // pastel
                    IM_COL32(255, 154, 162, 255),
                    IM_COL32(255, 183, 178, 255),
                    IM_COL32(255, 218, 193, 255),
                    IM_COL32(226, 240, 203, 255),
                    IM_COL32(181, 234, 215, 255),
                    IM_COL32(199, 206, 234, 255),
                    
                    //
                    //IM_COL32(247, 217, 196, 255),
                    IM_COL32(250, 237, 203, 255),
                    IM_COL32(201, 228, 222, 255),
                    IM_COL32(198, 222, 241, 255),
                    IM_COL32(219, 205, 240, 255),
                    IM_COL32(242, 198, 222, 255),
                    //IM_COL32(249, 198, 201, 255),
#endif
                    
#if 0                    
                    // Divergent
                    IM_COL32(84, 71, 140, 255),
                    IM_COL32(44, 105, 154, 255),
                    IM_COL32(4, 139, 168, 255),
                    IM_COL32(13, 179, 158, 255),
                    IM_COL32(22, 219, 147, 255),
                    IM_COL32(131, 227, 119, 255),
                    IM_COL32(185, 231, 105, 255),
                    IM_COL32(239, 234, 90, 255),
                    IM_COL32(241, 196, 83, 255),
                    IM_COL32(242, 158, 76, 255),
#endif
                    
                    
                };
                
                ImVec2 plot_pos = ImGui::GetWindowPos();
                draw_list->PushClipRect(plot_pos,
                                        ImVec2(plot_pos.x + max_text_width, plot_pos.y + ImGui::GetWindowHeight()), true);
                float bar_width = floorf(max_bar_width * ((float)record.duration / (float)max_duration));
                if (bar_width > 5)
                {
                    ImVec2 bar0 = ImGui::GetCursorScreenPos();
                    bar0.x += 1;
                    bar0.y += bar_down; // prabably wants to be 1
                    ImVec2 bar1 = ImVec2(bar0.x + bar_width - 1, bar0.y + bar_height - 1);
                    ImVec2 outline0 = ImVec2(bar0.x-1, bar0.y-1);
                    ImVec2 outline1 = ImVec2(bar1.x+1, bar1.y+1);
                    
                    draw_list->AddRect(outline0, outline1, IM_COL32(0, 0, 0, 255));
                    draw_list->AddRectFilled(bar0, bar1, colours[i % array_count(colours)]);                    
                }
                
                String name = {};
                if (record.id == 0)
                {
                    // DEBUG
                    name = make_string_from_literal("Not polled time");
                }
                else
                {
                    // NOTE: We will just skipp apps with id 0 instead
                    // returns string with just a space " " if for some reason there is no app with that id (or id == 0)
                    name = get_app_name(apps, record.id);
                }
                
                ImVec2 pos = ImGui::GetCursorScreenPos();
                pos.x += text_right;
                pos.y += text_down;
                ImGui::SetCursorScreenPos(pos);
                
                ImGui::TextUnformatted(name.str, name.str + name.length);
                
                draw_list->PopClipRect();
                // Text underline
                //ImVec2 max_r = ImGui::GetItemRectMax();
                //ImVec2 min_r = ImGui::GetItemRectMin();
                //draw_list->AddLine(ImVec2(min_r.x, max_r.y), ImVec2(min_r.x + 1000, max_r.y), IM_COL32(255,0,0,255), 1.0f);
                
                
                
                
#if MONITOR_DEBUG
                float real_seconds = (float)record.duration / MICROSECS_PER_SEC;
                char time_text[32];
                snprintf(time_text, array_count(time_text), "%.2fs", real_seconds);
                ImVec2 text_size = ImGui::CalcTextSize(time_text);
                ImGui::SameLine(ImGui::GetWindowWidth() - (text_size.x + 30));
                ImGui::Text(time_text); 
#else
                u32 seconds = record.duration / MICROSECS_PER_SEC;
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
                //ImGui::SameLine();
                //ImGui::InvisibleButton("##gradient1", ImVec2(bar_width + 10, bar_height));
            }
            
            free(records);
        } // if (records)
        
        ImGui::EndChild();
        
        float red[] = {255.0f, 0, 0};
        float black[] = {0, 0, 0};
        
        //double total_runtime_seconds = (double)state->total_runtime / 1000000;
        //double sum_duration_seconds = (double)sum_duration / 1000000;
        
        auto now = win32_get_time();
        //auto start_time = win32_get_microseconds_elapsed(state->startup_time, now);
        
        //double time_since_startup_seconds = (double)start_time / 1000000;
        
        //double diff_seconds2 = (double)(start_time - sum_duration) / 1000000;
        
        
        //ImGui::Text("Total runtime:        %.5fs", total_runtime_seconds);
        //ImGui::Text("Sum duration:        %.5fs", sum_duration_seconds);
        
        
        ImGuiStyle& style = ImGui::GetStyle();
        
        //ImGui::SameLine();
        memcpy((void *)style.Colors, red, 3 * sizeof(float));
        //ImGui::Text("diff: %llu", state->total_runtime - sum_duration);
        memcpy((void *)style.Colors, black, 3 * sizeof(float));
        
        
        //ImGui::Text("time since startup: %.5fs", time_since_startup_seconds);
        
        memcpy((void *)style.Colors, red, 3 * sizeof(float));
        //ImGui::Text("diff between time since startup and sum duration: %.5fs", diff_seconds2);
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

void
create_world_icon_source_file(char *png_file, char *cpp_file, int dimension)
{
    static bool done = false;
    if (!done)
    {
        int x = 0;
        int y = 0;
        int channels = 0;
        
        u32 *pixels = (u32 *)stbi_load(png_file, &x, &y, 0, 4);
        Assert(pixels);
        
        Assert(x == dimension);
        Assert(y == dimension);
        //Assert(channels == 4);
        
        int size = x*y*4;
        
        FILE *out = fopen(cpp_file, "w");
        Assert(out);
        
        fprintf(out, "static const unsigned int world_icon_size = %d;\n", size);
        fprintf(out, "static const unsigned int world_icon_width = %d;\n", x);
        fprintf(out, "static const unsigned int world_icon_height = %d;\n", y);
        fprintf(out, "static const unsigned int world_icon_data[%d/4] =\n{", size);
        
        int column = 0;
        for (int i = 0; i < x*y; ++i)
        {
            unsigned d = pixels[i];
            if ((column++ % 8) == 0)
                fprintf(out, "\n    0x%08x, ", d);
            else
                fprintf(out, "0x%08x, ", d);
        }
        fprintf(out, "\n};\n\n");
        
        fclose(out);
        
        free(pixels);
        done = true;
    }
}

void
do_debug_window_button(Database *database, Day_View *day_view)
{
    App_List *apps = &database->apps;
    
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
        
        char buf[2000];
        auto copy_and_add_null = [](char *buf, String s) {
            memcpy(buf, s.str, s.length);
            buf[s.length] = 0;
        };
        
        {
            
            ImGui::Text("short_name : ids in local_program_ids and website_ids");
            ImGui::BeginChildFrame(26, ImVec2(ImGui::GetWindowWidth() * 0.4, 600));
            for (auto &pair : apps->local_program_ids)
            {
                // not sure if definately null terminated
                copy_and_add_null(buf, pair.first);
                ImGui::BulletText("%s: %u", buf, pair.second);
            }
            for (auto &pair : apps->website_ids)
            {
                // not sure if definately null terminated
                copy_and_add_null(buf, pair.first);
                ImGui::BulletText("%s: %u", buf, pair.second);
            }
            ImGui::EndChildFrame();
        }
        ImGui::SameLine();
        {
            ImGui::Text("full names in local_programs");
            ImGui::BeginChildFrame(27, ImVec2(ImGui::GetWindowWidth() * 0.6, 600));
            for (auto &info : apps->local_programs)
            {
                // not sure if definately null terminated
                copy_and_add_null(buf, info.full_name);
                ImGui::BulletText("%s", buf);
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
            
#if 0
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
#endif
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
