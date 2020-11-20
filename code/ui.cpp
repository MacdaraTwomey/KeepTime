
#include "monitor.h"
#include "IconsMaterialDesign.h"

constexpr float UI_DEFAULT_BUTTON_WIDTH = 100;
constexpr ImGuiID UI_KEYWORD_LIST_ID = 800;
constexpr ImGuiID UI_SETTINGS_ID = 801;
constexpr ImGuiID UI_BARPLOT_ID = 802;

constexpr float SMALL_BUTTON_WIDTH = 30; // default for material icon left arrow
constexpr float DATE_SELECT_WIDTH = 250; 

constexpr float NORMAL_FONT_SIZE = 22.0f;
constexpr float SMALL_FONT_SIZE = 20.0f;

/// debug
static u32 g_text_down = 5;
static u32 g_text_right = 7;
static float g_bar_height = ICON_SIZE;
static float g_bar_down = 1;
static bool g_fullscreen = !MONITOR_DEBUG; 

#define SHOW_FREETYPE_OPTIONS_WINDOW (MONITOR_DEBUG && 0)

// from monitor.cpp
Icon_Asset *get_app_icon_asset(App_List *apps, UI_State *ui, App_Id id);
String get_app_name(App_List *apps, App_Id id);
void add_keyword(Settings *settings, char *str);


void
init_imgui_fonts_and_style(ImFont **small_font)
{
    ImGuiIO& io = ImGui::GetIO(); 
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.IniFilename = NULL; // Disable imgui.ini filecreation
    
    ImGui::StyleColorsLight();
    
#if 0
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
    
    ImVec4* colors = ImGui::GetStyle().Colors;
    //colors[ImGuiCol_FrameBg] = ImVec4(255, 255, 255, 255.00f);
    
#if 0    
    static const ImWchar basic_latin[] = {
        0x0020, 0x007E,
        0,
    };
#endif
    
    static const ImWchar icon_range[] = {
        0xe8b8, 0xe8b8, // settings
        0xe916, 0xe916, // data range
        0xe5c8, 0xe5c8, // arrow forward
        0xe5c4, 0xe5c4, // arrow back
        0,
    };
    
    ImFontConfig config; 
    config.MergeMode = true; 
    config.PixelSnapH = true;
    config.GlyphOffset = ImVec2(0, 4); // move icon glyphs down or else they render too high
    
    ImFont *normal_font = io.Fonts->AddFontFromMemoryCompressedTTF(SourceSansProRegular_compressed_data, SourceSansProRegular_compressed_size, NORMAL_FONT_SIZE, nullptr);
    
    // This adds 56000 IndexAdvanceX floats because it is based off the max codepoint so unused glyph indexes just have default advance
    io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\build\\fonts\\MaterialIcons-Regular.ttf", NORMAL_FONT_SIZE, &config, icon_range);
    
    // Subset is 2K instead of 126K not a buig deal (before compression)
    // https://transfonter.org/
    // "c:\\dev\\projects\\monitor\\build\\fonts\\subset-MaterialIcons-Regular.ttf"
    
    ImFont *loaded_small_font = io.Fonts->AddFontFromMemoryCompressedTTF(SourceSansProRegular_compressed_data, SourceSansProRegular_compressed_size, SMALL_FONT_SIZE, nullptr);
    
    
    // NOTE: FreeType assumes blending in linear space rather than gamma space. See FreeType note for FT_Render_Glyph. For correct results you need to be using sRGB and convert to linear space in the pixel shader output. The default Dear ImGui styles will be impacted by this change (alpha values will need tweaking).
    // NOTE: Freetype is an additional dependency/dll ...
#if 1
    // for freetype: call before Build or GetTexDataAsRGBA32 
    unsigned font_flags = ImGuiFreeType::LightHinting;
    for (int n = 0; n < io.Fonts->ConfigData.Size; n++)
    {
        ImFontConfig* font_config = (ImFontConfig*)&io.Fonts->ConfigData[n];
        font_config->RasterizerFlags = font_flags;
    }
    ImGuiFreeType::BuildFontAtlas(io.Fonts, font_flags);
#else
    // for stb:
    //io.Fonts->Build();
#endif
    
    *small_font = loaded_small_font;
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 0.0f; // 0 to 12 ? 
    style.WindowBorderSize = 1.0f; // or 1.0
    style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.WindowRounding = 0.0f;
}

void
load_ui(UI_State *ui, u32 app_count, 
        date::sys_days current_date, date::sys_days oldest_date, date::sys_days newest_date)
{
    init_imgui_fonts_and_style(&ui->small_font);
    
    ui->icon_indexes.reserve(app_count + 200); 
    ui->icon_indexes.assign(app_count, -1); // set size and fill with -1
    ui->icons.reserve(200);
    
    ui->icon_bitmap_storage = (u32 *)xalloc(sizeof(u32)*ICON_SIZE*ICON_SIZE);
    
    load_default_icon_assets(ui->icons);
    
    //ui->day_view = get_day_view(&state->day_list); 
    
    // TODO: Get oldest and newest date from day_view associated with current ui instance
    // TODO: Don't reset this so it is on same range when user opens ui again
    init_date_picker(&ui->date_picker, current_date, oldest_date, newest_date);
    
    ui->date_range_changed = true; // so record array is made the first time
    ui->open = true;
}

void
unload_ui(UI_State *ui)
{
    // safer to clear to zero for debugging though
    *ui = {};
    ui->date_picker = {}; // TODO: Don't reset this so it is on same range when user opens ui again
    
    ui->record_count = 0;
    free(ui->sorted_records);
    
    unload_all_icon_assets(ui);
    if (ui->edit_settings)
    {
        free(ui->edit_settings);
    }
    
    free(ui->icon_bitmap_storage);
    
    free_day_view(&ui->day_view);
    
    ui->open = false;
}

#if SHOW_FREETYPE_OPTIONS_WINDOW
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
    bool first;
    
    FreeTypeTest()
    {
        BuildMode = FontBuildMode_FreeType;
        WantRebuild = true;
        FontsMultiply = 1.0f;
        FontsPadding = 1;
        FontsFlags = ImGuiFreeType::LightHinting;;
        font_size = NORMAL_FONT_SIZE;
        first = true;
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
        if (first) { ImGui::SetNextWindowPos(ImVec2(900, 0), true); first = false; }
        
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

FreeTypeTest freetype_test; // and construct this object

#endif

s32
remove_duplicate_and_empty_keyword_strings(Keyword_Array keywords)
{
    // TODO: I don't love this probably better to just copy to a new array
    
    Arena scratch = {};
    init_arena(&scratch, MAX_KEYWORD_COUNT * MAX_KEYWORD_SIZE, 0);
    Array<String, MAX_KEYWORD_COUNT> pending_keywords;
    for (int i = 0; i < MAX_KEYWORD_COUNT; ++i)
    {
        if (keywords[i][0] != '\0') // not empty string
        {
            pending_keywords.add_item(push_string(&scratch, keywords[i]));
        }
    }
    
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
    
    // TODO: DEBUG make the step much greater
    float freq_secs = (float)edit_settings->misc.poll_frequency_milliseconds / MILLISECS_PER_SEC;
    ImGui::PushItemWidth(150);
    if (ImGui::InputFloat("Update frequency (s)", &freq_secs, 0.01f, 1.0f, 2))
    {
        if (freq_secs < POLL_FREQUENCY_MIN_SECONDS) 
            freq_secs = POLL_FREQUENCY_MIN_SECONDS;
        else if (freq_secs > POLL_FREQUENCY_MAX_SECONDS) 
            freq_secs = POLL_FREQUENCY_MAX_SECONDS;
        else 
            edit_settings->misc.poll_frequency_milliseconds = (u32)(freq_secs * MILLISECS_PER_SEC);
    }
    ImGui::PopItemWidth();
    
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
    
#if 0    
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
#endif
    
}

void
do_keyword_input_list(Edit_Settings *edit_settings)
{
    ImGui::RadioButton("All", (int *)&edit_settings->misc.keyword_status, Settings_Misc_Keyword_All); 
    ImGui::SameLine();
    ImGui::RadioButton("Custom", (int *)&edit_settings->misc.keyword_status, Settings_Misc_Keyword_Custom);
    ImGui::SameLine();
    ImGui::RadioButton("None", (int *)&edit_settings->misc.keyword_status, Settings_Misc_Keyword_None);
    
    constexpr float ADD_BUTTON_WIDTH = 80;
    ImGui::SameLine(ImGui::GetWindowWidth() - ADD_BUTTON_WIDTH);
    
    s32 give_focus = -1;
    
    bool disable_keywords = edit_settings->misc.keyword_status != Settings_Misc_Keyword_Custom;
    
    if (ButtonSpecial("Add...", disable_keywords, ImVec2(ADD_BUTTON_WIDTH, 0)))
    {
        // Give focus to first free input box
        for (s32 i = 0; i < MAX_KEYWORD_COUNT; ++i)
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
    for (s32 i = 0; i < MAX_KEYWORD_COUNT; ++i)
    {
        if (edit_settings->pending[i][0] != '\0')
        {
            index_of_last_keyword = i;
        }
    }
    
    // show more input boxes past our last used box
    s32 number_of_boxes = std::min(index_of_last_keyword + 8, MAX_KEYWORD_COUNT);
    
    {
        float list_width = ImGui::GetWindowWidth();
        ImGui::BeginChild(UI_KEYWORD_LIST_ID, ImVec2(list_width, 0), true);
        
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
            
            ImU32 line_colour = IM_COL32(150, 150, 150, 255);
            
            if (disable_keywords) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // imgui_internal.h
                ImGui::PushStyleColor(ImGuiCol_Text, DISABLED_COLOUR);
                line_colour = DISABLED_COLOUR;
            }
            
            ImGui::PushID(i);
            ImGui::InputText("", edit_settings->pending[i], array_count(edit_settings->pending[i]), ImGuiInputTextFlags_CallbackCharFilter, filter, &entered_disabled_character);
            ImGui::PopID();
            
            if (disable_keywords) {
                ImGui::PopItemFlag();
                ImGui::PopStyleColor();
            }
            
            float padding = 3;
            ImVec2 pos = ImGui::GetCursorScreenPos();
            pos.y -= padding;
            draw_list->AddLine(pos, ImVec2(pos.x + list_width, pos.y), line_colour, 1.0f);
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
do_settings_popup(Settings *settings, Edit_Settings *edit_settings, ImFont *small_font)
{
    bool update_settings = false;
    bool open = true;
    
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowWidth() / 2, ImGui::GetWindowHeight() / 2), ImGuiCond_Appearing, ImVec2(0.5, 0.5));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetWindowWidth() * 0.6, ImGui::GetWindowHeight() * 0.8));
    if (ImGui::BeginPopupModal("Settings", &open, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove))
    {
        
        ImGui::BeginChild(UI_SETTINGS_ID, ImVec2(0,ImGui::GetWindowHeight() * 0.85), false);
        {
            
            ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
            if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
            {
                if (ImGui::BeginTabItem("UI"))
                {
                    ImGui::PushFont(small_font);
                    do_misc_settings(edit_settings);
                    ImGui::EndTabItem();
                    ImGui::PopFont();
                }
                if (ImGui::BeginTabItem("Website keywords"))
                {
                    ImGui::PushFont(small_font);
                    do_keyword_input_list(edit_settings);
                    ImGui::EndTabItem();
                    ImGui::PopFont();
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
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(UI_DEFAULT_BUTTON_WIDTH, 0)))
        {
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
        s32 pending_count = remove_duplicate_and_empty_keyword_strings(edit_settings->pending);
        apply_new_settings(settings, edit_settings, pending_count);
    }
    
    return !open;
}

void
draw_record_time_text(s64 duration)
{
    u32 seconds = duration / MICROSECS_PER_SEC;
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
    
#if MONITOR_DEBUG
    ImGui::SameLine();
    float real_seconds = (float)duration / MICROSECS_PER_SEC;
    char time_text[32];
    snprintf(time_text, array_count(time_text), "(%.2fs)  -", real_seconds);
    ImVec2 text_size = ImGui::CalcTextSize(time_text);
    
    //ImGui::SameLine(ImGui::GetWindowWidth() - (text_size.x + 30));
    
    ImGui::Text(time_text); 
#endif
}


Record *
get_records_in_date_range(Day_View *day_view, u32 *record_count, 
                          date::sys_days start_range, date::sys_days end_range)
{
    Record *result = nullptr;
    
    // Allow no days to be in range for robustness
    
    // start: |5   8|
    // start:                               |23   24|
    // start:         |10             21|       
    // Day_View: 7 8 9 11 12 13 14 20 21 22
    
    int start_idx = -1;
    int end_idx = -1;
    for (int i = 0; i < day_view->days.size(); ++i)
    {
        date::sys_days date = day_view->days[i].date;
        if (start_idx == -1 && start_range <= date) 
        {
            start_idx = i;
        }
        if (end_range >= date) 
        {
            end_range = date;
        }
    }
    
    if (start_idx != -1 && end_idx != -1) 
    {
        *record_count = 0;
        return result;
    }
    
    if (start_idx == -1) 
    {
        start_idx = 0;
    }
    if (end_idx == -1)
    {
        end_idx = day_view->days.size() - 1;
    }
    
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


//@main
void 
draw_ui_and_update(SDL_Window *window, UI_State *ui, Settings *settings, App_List *apps, Monitor_State *state)
{
    ZoneScoped;
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);
    
#if SHOW_FREETYPE_OPTIONS_WINDOW
    // Light hinting with freetype looks better than stb font renderer (at least at size 22 pixels)
    if (freetype_test.UpdateRebuild())
    {
        // REUPLOAD FONT TEXTURE TO GPU
        ImGui_ImplOpenGL3_DestroyDeviceObjects();
        ImGui_ImplOpenGL3_CreateDeviceObjects();
    }
#endif
    
    ImGui::NewFrame();
    
#if MONITOR_DEBUG
    bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);
    
    {
        ImGuiIO& io = ImGui::GetIO();
        // SDL_SCANCODE_D == 7
        if (io.KeysDown[7] && io.KeyCtrl)
        {
            ImGui::SetNextWindowFocus();
        }
        
        void debug_ui_options(Monitor_State *state);
        debug_ui_options(state);
    }
#endif
    
#if SHOW_FREETYPE_OPTIONS_WINDOW
    ImGui::SetNextWindowPos(ImVec2(900, 0), true);
    freetype_test.ShowFreetypeOptionsWindow();
#endif
    
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoResize;
    
    ImGui::SetNextWindowPos(ImVec2(0, 0), true);
    
    ImGuiIO& io = ImGui::GetIO();
    if (g_fullscreen)
        ImGui::SetNextWindowSize(io.DisplaySize); 
    else
        ImGui::SetNextWindowSize(ImVec2(850, 690), true);
    
    ImGui::Begin("Main window", nullptr, flags);
    {
        // Centre date navigation bar
        // Padding is 8
        ImGui::SetCursorPosX(((ImGui::GetWindowWidth() - (SMALL_BUTTON_WIDTH * 2 + DATE_SELECT_WIDTH)) / 2) - 8);
        
        Date_Picker *date_picker = &ui->date_picker;
        
        date::sys_days oldest_date = ui->day_view.days.front().date;
        date::sys_days newest_date = ui->day_view.days.back().date;
        
        if (ButtonSpecial(ICON_MD_ARROW_BACK, date_picker->is_backwards_disabled))
        {
            ui->date_range_changed = true;
            date_picker_backwards(date_picker, oldest_date, newest_date);
        }
        
        ImGui::SameLine();
        if (ImGui::Button(date_picker->date_label, ImVec2(DATE_SELECT_WIDTH, 0))) 
        {
            ImGui::OpenPopup("Date Select");
        }
        ImGui::SameLine();
        
        if (ButtonSpecial(ICON_MD_ARROW_FORWARD, date_picker->is_forwards_disabled))
        {
            ui->date_range_changed = true;
            date_picker_forwards(date_picker, oldest_date, newest_date);
        }
        
        if (do_date_select_popup(date_picker, oldest_date, newest_date))
        {
            ui->date_range_changed = true;
        }
        
#if 0
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        float centre_x = ImGui::GetWindowWidth()/2;
        draw_list->AddLine({centre_x,0}, {centre_x, 300}, IM_COL32_BLACK, 1.0f);
        draw_list->AddLine({centre_x - (DATE_SELECT_WIDTH / 2), 20}, 
                           {centre_x, 20}, IM_COL32_BLACK, 1.0f);
        draw_list->AddLine({centre_x, 20}, 
                           {centre_x + (DATE_SELECT_WIDTH / 2), 20}, IM_COL32_BLACK, 1.0f);
#endif
    }
    
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::SameLine(ImGui::GetWindowWidth() - (SMALL_BUTTON_WIDTH + style.WindowPadding.x));
    
    {
        if (ImGui::Button(ICON_MD_SETTINGS))
        {
            // init edit_settings
            ui->edit_settings = (Edit_Settings *)calloc(1, sizeof(Edit_Settings));
            
            // Not sure if should try to leave only one extra or all possible input boxes
            ui->edit_settings->misc = settings->misc;
            
            Assert(settings->keywords.count <= MAX_KEYWORD_COUNT);
            for (s32 i = 0; i < settings->keywords.count; ++i)
            {
                Assert(settings->keywords[i].length + 1 <= MAX_KEYWORD_SIZE);
                strcpy(ui->edit_settings->pending[i], settings->keywords[i].str);
            }
            
            ImGui::OpenPopup("Settings");
        }
        
        bool closed = do_settings_popup(settings, ui->edit_settings, ui->small_font);
        if (closed)
        {
            free(ui->edit_settings);
            ui->edit_settings = nullptr;
        }
    }
    
    {
        ImGui::BeginChildFrame(UI_BARPLOT_ID, {0,0});
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        // debug
        ui->date_range_changed = true;
        
        // TODO: Consider doing this elsewhere
        if (ui->date_range_changed)
        {
            free(ui->sorted_records);
            ui->sorted_records = nullptr;
            ui->sorted_records = get_records_in_date_range(&ui->day_view, &ui->record_count, 
                                                           ui->date_picker.start, ui->date_picker.end);
            ui->date_range_changed = false;
        }
        
        s64 max_duration = (ui->sorted_records != nullptr) ? ui->sorted_records[0].duration : 1;
        float max_bar_width = ImGui::GetWindowWidth() * 0.9f;
        float pixels_per_duration = max_bar_width / max_duration;
        
        for (i32 i = 0; i < ui->record_count; ++i)
        {
            Record &record = ui->sorted_records[i];
            
            if (record.id == 0) continue;
            
            if (i != 0)
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                pos.y -= g_text_down; 
                ImGui::SetCursorScreenPos(pos);
            }
            
            Icon_Asset *icon = get_app_icon_asset(apps, ui, record.id);
            if (icon)
            {
                auto border = ImVec4(0,0,0,255);
                ImGui::Image((ImTextureID)(intptr_t)icon->texture_handle, ImVec2((float)icon->width, (float)icon->height));
                //ImGui::Image((ImTextureID)(intptr_t)icon->texture_handle, ImVec2((float)icon->bitmap.width, (float)icon->bitmap.height),{0,0},{1,1},{1,1,1,1}, border);
            }
            
            // To start rect where text start
            ImGui::SameLine();
            
            // TODO: make sure colours stay with their record, in order of records.
            // ... But we probably won't have a live display, so colour change probably not be noticed between
            // separete openings of the ui, when the ordering of records can change.
            // The only thing is seeing the same app in different weeks/days etc with a different colour, which probably does mean each app needs a assigned colour per ui run. Can maybe put this in Icon_Asset and call it UI_Asset
            ImU32 colours[] = {
                IM_COL32(255, 154, 162, 255),
                IM_COL32(255, 183, 178, 255),
                IM_COL32(255, 218, 193, 255),
                IM_COL32(226, 240, 203, 255),
                IM_COL32(181, 234, 215, 255),
                IM_COL32(199, 206, 234, 255),
                //
                IM_COL32(250, 237, 203, 255),
                IM_COL32(201, 228, 222, 255),
                IM_COL32(198, 222, 241, 255),
                IM_COL32(219, 205, 240, 255),
                IM_COL32(242, 198, 222, 255),
            };
            
            float bar_width = floorf(pixels_per_duration * (float)record.duration);
            if (bar_width > 5)
            {
                ImVec2 bar0 = ImGui::GetCursorScreenPos();
                bar0.x += 1;
                bar0.y += g_bar_down; // prabably wants to be 1
                ImVec2 bar1 = ImVec2(bar0.x + bar_width - 1, bar0.y + g_bar_height - 1);
                ImVec2 outline0 = ImVec2(bar0.x-1, bar0.y-1);
                ImVec2 outline1 = ImVec2(bar1.x+1, bar1.y+1);
                
                draw_list->AddRect(outline0, outline1, IM_COL32_BLACK);
                draw_list->AddRectFilled(bar0, bar1, colours[i % array_count(colours)]);                    
            }
            
            ImVec2 pos = ImGui::GetCursorScreenPos();
            pos.x += g_text_right;
            pos.y += g_text_down;
            ImGui::SetCursorScreenPos(pos);
            
            draw_record_time_text(record.duration);
            ImGui::SameLine();
            
            String name = get_app_name(apps, record.id);
            ImGui::TextUnformatted(name.str, name.str + name.length);
        }
        
        ImGui::EndChild();
        
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
debug_ui_options(Monitor_State *state)
{
    static bool first = true;
    if (first) { ImGui::SetNextWindowPos(ImVec2(860, 20), true); first = false; }
    
    ImGui::Begin("DEBUG Options");
    
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    
    ImGui::Checkbox("Fullscreen", &g_fullscreen);
    ImGui::SameLine();
    if (ImGui::Button("Hide"))
    {
        // debug: simulate effect of pressing x button
        win32_hide_win();
    }
    ImGui::SameLine();
    static bool g_UI_time = true;
    if (ImGui::Checkbox("SleepT", &g_UI_time))
    {
        if (g_UI_time)
        {
            platform_set_sleep_time(16);
        }
        else
        {
            platform_set_sleep_time(100);
        }
    }
    
    u32 one = 1;
    ImGui::InputScalar("text down", ImGuiDataType_U32, &g_text_down, &one);
    ImGui::InputScalar("text right", ImGuiDataType_U32, &g_text_right, &one);
    ImGui::InputFloat("bar_height", &g_bar_height, 1.0f,0.0f,"%.3f");
    ImGui::InputFloat("bar_down", &g_bar_down, 1.0f,0.0f,"%.3f");
    
    // forward declare
    void do_debug_window_button(Monitor_State *state);
    void draw_debug_date_text(Monitor_State *state);
    
    ImGui::Text("Ctrl D to show this window");
    do_debug_window_button(state);
    draw_debug_date_text(state);
    ImGui::End();
}

void
draw_debug_date_text(Monitor_State *state)
{
    // Debug date stuff
    
    Date_Picker *date_picker = &state->ui.date_picker;
    Day_List *day_list = &state->day_list;
    Day_View *day_view = &state->ui.day_view;
    
    date::sys_days oldest_date = day_view->days.front().date;
    date::sys_days newest_date = day_view->days.back().date;
    
    auto ymd = date::year_month_day{date_picker->start};
    auto ymw = date::year_month_weekday{date_picker->start};
    std::ostringstream buf;
    
    buf << "start_date: " << ymd << ", " <<  ymw;
    ImGui::Text(buf.str().c_str());
    
    ymd = date::year_month_day{oldest_date};
    ymw = date::year_month_weekday{oldest_date};
    std::ostringstream buf2;
    
    ymd = date::year_month_day{date_picker->end};
    ymw = date::year_month_weekday{date_picker->end};
    std::ostringstream buf1;
    
    buf1 << "end_date: " << ymd << ", " <<  ymw;
    ImGui::Text(buf1.str().c_str());
    
    buf2 << "oldest_date; " << ymd << ", " <<  ymw;
    ImGui::Text(buf2.str().c_str());
    
    ymd = date::year_month_day{newest_date};
    ymw = date::year_month_weekday{newest_date};
    std::ostringstream buf3;
    
    buf3 << "newest_date: " << ymd << ", " <<  ymw;
    ImGui::Text(buf3.str().c_str());
}

void
do_debug_window_button(Monitor_State *state)
{
    App_List *apps = &state->apps;
    
    Date_Picker *date_picker = &state->ui.date_picker;
    Day_List *day_list = &state->day_list;
    Day_View *day_view = &state->ui.day_view;
    
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
        ImGui::PushFont(state->ui.small_font);
        {
            
            ImGui::Text("short_name : ids in local_program_ids and website_ids");
            ImGui::BeginChildFrame(26, ImVec2(ImGui::GetWindowWidth()*0.8, 600));
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
            ImGui::BeginChildFrame(27, ImVec2(ImGui::GetWindowWidth()*0.95, 600));
            for (auto &info : apps->local_programs)
            {
                // not sure if definately null terminated
                copy_and_add_null(buf, info.full_name);
                ImGui::TextWrapped("- %s", buf);
            }
            ImGui::EndChildFrame();
        }
        
        {
            ImGui::Text("__Day_List__");
            ImGui::Text("days:");
            ImGui::SameLine(500);
            ImGui::Text("blocks:");
            
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
        ImGui::PopFont();
        
        ImGui::End();
    } // if debug_menu_open
}

