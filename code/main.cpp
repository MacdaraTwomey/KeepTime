
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "imgui_freetype.h"

#include "implot.h"

#include "SDl2/SDL.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_syswm.h"

#include "GL/glew.h"
#include "GL/wglew.h"

#include <windows.h>
#undef min
#undef max

#include <commctrl.h>
#include <AtlBase.h>
#include <UIAutomation.h>
#include <shellapi.h>
#include <shlobj_core.h> // SHDefExtractIconA

#include "cian.h"
#include "win32_monitor.h"
#include "resource.h"
#include "platform.h"
#include "graphics.h"
#include "monitor.h"

#include <chrono>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

// TODO: MOVE
enum Window_Status
{
    Window_Just_Visible = 1,
    Window_Just_Hidden = 2,
    Window_Visible = 4,
    Window_Hidden = 8,
};

#include "utilities.cpp" // xalloc, string copy, concat string, make filepath, get_filename_from_path
#include "bitmap.cpp"
#include "win32_monitor.cpp" // needs bitmap functions
#include "monitor.cpp"

#define CONSOLE_ON 1

static char *global_savefile_path;
static char *global_debug_savefile_path;
static bool global_running = true;


// NOTE: This is only used for toggling with the tray icon.
// Use pump messages result to check if visible to avoid the issues with value unexpectly changing.
// Still counts as visible if minimised to the task bar.
static NOTIFYICONDATA global_nid = {};
static Options_Window global_options_win;

#define WINDOW_WIDTH 1240
#define WINDOW_HEIGHT 720

// -----------------------------------------------------------------
// TODO:
// * !!! If GUI is visible don't sleep. But we still want to poll infrequently. So maybe check elapsed poll time.
// * Remember window width and height
// * Unicode correctness
// * Path length correctness
// * Dynamic window, button layout with resizing
// * OpenGL graphing?
// * Stop repeating work getting the firefox URL, maybe use UIAutomation cache?

// * Finalise GUI design (look at task manager and nothings imgui for inspiration)
// * Make api better/clearer, especially graphics api
// * BUG: Program time slows down when mouse is moved quickly within window or when a key is held down
// -----------------------------------------------------------------

// Top bit of id specifies is the 'program' is a normal executable or a website
// where rest of the bits are the actual id of its name.
// For websites whenever a user creates a new keyword that keyword gets an id and the id count increases by 1
// If a keyword is deleted the records with that website id can be given firefox's id.


#if 0
// put this at/around wher ImGui::NewFrame() is
// Start the Dear ImGui frame
if (freetype_test.UpdateRebuild())
{
    // REUPLOAD FONT TEXTURE TO GPU
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    ImGui_ImplOpenGL3_CreateDeviceObjects();
}
ImGui::NewFrame();
freetype_test.ShowFreetypeOptionsWindow();

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
    
    FreeTypeTest()
    {
        BuildMode = FontBuildMode_FreeType;
        WantRebuild = true;
        FontsMultiply = 1.0f;
        FontsPadding = 1;
        FontsFlags = 0;
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

int main(int argc, char* argv[])
{
    
#if CONSOLE_ON
    HANDLE con_in = win32_create_console();
#endif
    
    // Win32 stuff
    // TODO: Why don't we need to link ole32.lib?
    // Call this because we use CoCreateInstance in UIAutomation
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED); // equivalent to CoInitialize(NULL)
    
    
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS) != 0)
    {
        return 1;
    }
    
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); 
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0); 
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    
    
    SDL_Window* window = SDL_CreateWindow("Monitor",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // enable VSync
    
    // Don't ifnore special system-specific messages that unhandled
    // Allows us to get SDL_SYSWMEVENT
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    
    if (glewInit() != GLEW_OK)
    {
        printf("glewInit failed\n");
        return 1;
    }
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.IniFilename = NULL; // Disable imgui.ini filecreation
    
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    ImGui::StyleColorsLight();
    
    //io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\fonts\\DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\fonts\\Karla-Regular.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\fonts\\Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\fonts\\Cousine-Regular.ttf", 16.0f);
    io.Fonts->AddFontFromFileTTF("c:\\windows\\fonts\\seguisym.ttf", 22.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\windows\\fonts\\times.ttf", 16.0f);
    //io.Fonts->AddFontDefault();
    
    unsigned int flags = ImGuiFreeType::NoHinting;
    ImGuiFreeType::BuildFontAtlas(io.Fonts, flags);
    io.Fonts->Build();
    //io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    
    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");
    
    // Compare freetype options, and stb_truetype
    //FreeTypeTest freetype_test;
    
    
    {
        HWND hwnd = 0;
        
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version); /* initialize info structure with SDL version info */
        if(SDL_GetWindowWMInfo(window,&info)) {
            hwnd = info.info.win.window;
            HINSTANCE instance = info.info.win.hinstance;
        }
        
        // TODO: To maintain compatibility with older and newer versions of shell32.dll while using
        // current header files may need to check which version of shell32.dll is installed so the
        // cbSize of NOTIFYICONDATA can be initialised correctly.
        // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa
        global_nid = {};
        global_nid.cbSize = sizeof(NOTIFYICONDATA);
        global_nid.hWnd = hwnd;
        global_nid.uID = ID_TRAY_APP_ICON; // // Not sure why we need this
        global_nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
        global_nid.uCallbackMessage = CUSTOM_WM_TRAY_ICON;
        // Recommented you provide 32x32 icon for higher DPI systems
        // Can use LoadIconMetric to specify correct one with correct settings is used
        global_nid.hIcon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_MAIN_ICON));
        
        // TODO: This should say "running"
        TCHAR tooltip[] = {"Tooltip dinosaur"}; // Use max 64 chars
        strncpy(global_nid.szTip, tooltip, sizeof(tooltip));
        
        Shell_NotifyIconA(NIM_ADD, &global_nid);
    }
    
    {
        char *exe_dir_path = SDL_GetBasePath();
        global_savefile_path = make_filepath_with_dir(exe_dir_path, "savefile.mpt");
        if (!global_savefile_path) return 1;
        global_debug_savefile_path = make_filepath_with_dir(exe_dir_path, "debug_savefile.txt");
        if (!global_debug_savefile_path) return 1;
        
        SDL_free(exe_dir_path);
    }
    
    // test is gui window already open, maybe use FindWindowA(), or use mutex
    
    // THis may just be temporary.
    Monitor_State monitor_state = {};
    monitor_state.is_initialised = false;
    
    Uint32 sdl_flags = SDL_GetWindowFlags(window);
    u32 window_status = 0;
    if (sdl_flags & SDL_WINDOW_SHOWN) window_status |= Window_Visible|Window_Just_Visible;
    else if (sdl_flags & SDL_WINDOW_HIDDEN) window_status |= Window_Hidden|Window_Just_Hidden;
    
    auto old_time = Steady_Clock::now();
    
    while (global_running)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                window_status = Window_Just_Hidden|Window_Hidden;
                global_running = false;
            }
            else if (event.type == SDL_WINDOWEVENT) 
            {
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_SHOWN:
                    {
                        window_status = Window_Just_Visible|Window_Visible;
                    } break;
                    case SDL_WINDOWEVENT_HIDDEN:
                    {
                        window_status = Window_Just_Hidden|Window_Hidden;
                    } break;
                    case SDL_WINDOWEVENT_CLOSE:
                    {
                        window_status = Window_Just_Hidden|Window_Hidden;
                        global_running = false;
                        
                        // win32
                        Shell_NotifyIconA(NIM_DELETE, &global_nid);
                    } break;
                }
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) 
            {
                global_running = false;
            }
            else if (event.type == SDL_SYSWMEVENT)
            {
                // Win32 messages unhandled by SDL
                SDL_SysWMmsg *sys_msg = event.syswm.msg;
                HWND hwnd = sys_msg->msg.win.hwnd;
                UINT msg = sys_msg->msg.win.msg;
                WPARAM wParam = sys_msg->msg.win.wParam;
                LPARAM lParam = sys_msg->msg.win.lParam;
                
                if (msg == CUSTOM_WM_TRAY_ICON) 
                {
                    switch (LOWORD(lParam))
                    {
                        case WM_RBUTTONUP:
                        {
                            // Could open a context menu here with option to quit etc.
                        } break;
                        case WM_LBUTTONUP:
                        {
                            // NOTE: For now we just toggle with tray icon but, we will in future toggle with X button
                            u32 win_flags = SDL_GetWindowFlags(window);
                            if (win_flags & SDL_WINDOW_SHOWN)
                            {
                                SDL_HideWindow(window);
                            }
                            else
                            {
                                SDL_ShowWindow(window);
                                
                                // old win32
                                //ShowWindow(window, SW_SHOW);
                                //ShowWindow(window, SW_RESTORE); // If window was minimised and hidden, also unminimise
                                //SetForegroundWindow(window); // Need this to allow 'full focus' on window after showing
                            }
                        } break;
                    }
                }
            }
        }
        
        // Steady clock also accounts for time paused in debugger etc, so can introduce bugs that aren't there normally when debugging.
        auto new_time = Steady_Clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(new_time - old_time);
        old_time = new_time;
        time_type dt = diff.count();
        
        // Maybe pass in poll stuff here which would allow us to avoid timer stuff in app layer,
        // or could call poll windows from app layer, when we recieve a timer elapsed flag.
        // We might want to change frequency that we poll, so may need platform_change_wakeup_frequency()
        update(&monitor_state, window, dt, window_status);
        
        // I'm assuming this does nothing when the window is hidden
        SDL_GL_SwapWindow(window);
    }
    
    // Win32 
    Shell_NotifyIconA(NIM_DELETE, &global_nid);
    CoUninitialize();
    FreeConsole();
    
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
