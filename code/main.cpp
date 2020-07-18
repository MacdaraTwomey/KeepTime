
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "imgui_freetype.h"
#include "imgui_internal.h" // used to extend imgui, no guaranteed forward compatibility

#include "SDl2/SDL.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_syswm.h"

#include "GL/glew.h"
#include "GL/wglew.h"

#include <windows.h>
#undef min
#undef max

#include "cian.h"
#include "monitor.h"
#include "graphics.h"
#include "platform.h"

#include "resource.h"

#include <chrono>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

#include "IconsMaterialDesign.h"

#include <atlbase.h>
#include "tracy.hpp"
#include "../tracy/TracyClient.cpp"

static float ico_u0;
static float ico_u1;
static float ico_v0;
static float ico_v1;
static float ico_w;
static float ico_h;
static u64 ico_tex;

#include "utilities.cpp" // xalloc, string copy, concat string, make filepath, get_filename_from_path
#include "bitmap.cpp"
#include "win32_monitor.cpp" // needs bitmap functions
#include "monitor.cpp"

static char *global_savefile_path;
static char *global_debug_savefile_path;
static bool global_running = true;

#define WINDOW_WIDTH 1240
#define WINDOW_HEIGHT 720



void
init_imgui(SDL_Window *window, SDL_GLContext gl_context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context); // just sets keymap, gets cursors and sets backend flags
    ImGui_ImplOpenGL3_Init("#version 130"); // just sets what version of opengl, and backend flags for imgui
    
    ImGuiIO& io = ImGui::GetIO(); 
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.IniFilename = NULL; // Disable imgui.ini filecreation
    
    
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    //ImGui::StyleColorsLight();
    
    //io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\fonts\\DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\fonts\\Karla-Regular.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\fonts\\Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\fonts\\Cousine-Regular.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\windows\\fonts\\times.ttf", 16.0f);
    //io.Fonts->AddFontDefault();
    
    //#include "compressed_fonts/roboto_font_file.cpp"
    //ImFont* font = 
    //io.Fonts->AddFontFromMemoryCompressedTTF(roboto_font_file_compressed_data, roboto_font_file_compressed_size, 22.0f);
    
    // make just add ascii characters?
    //io.Fonts->AddFontFromFileTTF("c:\\windows\\fonts\\seguisym.ttf", 22.0f);
    
    // SourceSansProRegular_compressed_data is in helper.h
    io.Fonts->AddFontFromMemoryCompressedTTF(SourceSansProRegular_compressed_data, SourceSansProRegular_compressed_size, 22.0f);
    
    // NOTE: glyph ranges must exist at least until atlas is built
    
    //static const ImWchar icon_range[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
    //builder.AddRanges(icon_range); // calls add char looping over 2byte chars in range
    
    ImFontConfig icons_config; 
    icons_config.MergeMode = true; 
    icons_config.PixelSnapH = true;
    
    ImVector<ImWchar> range_32;
    ImFontGlyphRangesBuilder builder_32;
    builder_32.AddText(ICON_MD_PUBLIC);
    builder_32.BuildRanges(&range_32);                          
    
    ImVector<ImWchar> range_22;
    ImFontGlyphRangesBuilder builder_22;
    builder_22.AddText(ICON_MD_DATE_RANGE);
    builder_22.AddText(ICON_MD_ARROW_FORWARD);
    builder_22.AddText(ICON_MD_ARROW_BACK);
    builder_22.AddText(ICON_MD_SETTINGS);
    builder_22.BuildRanges(&range_22);                          
    
    icons_config.GlyphOffset = ImVec2(0, 13); // move glyphs down or else they render too high
    io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\build\\fonts\\MaterialIcons-Regular.ttf", 36.0f, &icons_config, range_32.Data);
    
    icons_config.GlyphOffset = ImVec2(0, 4); // move glyphs down or else they render too high
    io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\build\\fonts\\MaterialIcons-Regular.ttf", 22.0f, &icons_config, range_22.Data);
    
    
    unsigned int flags = ImGuiFreeType::NoHinting;
    ImGuiFreeType::BuildFontAtlas(io.Fonts, flags);
    io.Fonts->Build();
    
    
    ImFontAtlas *atlas = io.Fonts;
    ImFont *font = atlas->Fonts[0];
    
    ico_tex = 1;//(u64)atlas->TexID; // should be 1?
    
    
    const ImFontGlyph *g = font->FindGlyph(0xe80b);
    ico_u0 = g->U0;
    ico_u1 = g->U1;
    ico_v0 = g->V0;
    ico_v1 = g->V1;
    ico_w  = g->X1 - g->X0;
    ico_h  = g->Y1 - g->Y0;
    
    
#if 0    
    // TODO: Compare freetype options, and stb_truetype
    //FreeTypeTest freetype_test;
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 0.0f; // 0 to 12 ? 
    style.WindowBorderSize = 1.0f; // or 1.0
    style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.WindowRounding = 0.0f;
#endif
    
    ImGui::Spectrum::StyleColorsSpectrum(); 
}

int main(int argc, char* argv[])
{
    ZoneScoped;
    
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
    
    if (glewInit() != GLEW_OK)
    {
        printf("glewInit failed\n");
        return 1;
    }
    
    init_imgui(window, gl_context);
    
#if defined(_WIN32)
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version); /* initialize info structure with SDL version info */
    if(SDL_GetWindowWMInfo(window,&info)) {
        init_win32_context(info.info.win.window);
    }
    else
    {
        Assert(0);
    }
#endif // if _WIN32
    
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
    
    auto old_time = win32_get_time();
    
    while (global_running)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        
        //OPTICK_FRAME("MainThread");
        ZoneScopedN("Frame");
        
        // clear just visible, just hidden flags
        window_status = window_status & (Window_Visible | Window_Hidden);
        
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
                        
#if defined(_WIN32)
                        Shell_NotifyIconA(NIM_DELETE, &win32_context.nid);
#endif
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
                                SDL_RaiseWindow(window);
                            }
                        } break;
                    }
                }
            }
        }
        
        // TODO: NOTE: The vsync swap interval seems to affect the speed at which times diverge, with lower frame rates having a lower divergence.
        
        // Steady clock also accounts for time paused in debugger etc, so can introduce bugs that aren't there normally when debugging.
        auto new_time = win32_get_time();
        s64 dt_microseconds = win32_get_microseconds_elapsed(old_time, new_time);
        old_time = new_time;
        
        // Maybe pass in poll stuff here which would allow us to avoid timer stuff in app layer,
        // or could call poll windows from app layer, when we recieve a timer elapsed flag.
        // We might want to change frequency that we poll, so may need platform_change_wakeup_frequency()
        update(&monitor_state, window, dt_microseconds, window_status);
        
        
        // Sleeping longer reduces the time spent waiting for vblank in a opengl driver context (making task manager report lower CPU usage)
        //Sleep(10);
        
        // vsync may not be supported, or be disabled by user, so need to limit frame rate to 60fps anyway, to stop high refresh in scenario where user is making lots on input
        {
            ZoneScopedN("Swap buffers and glFinish");
            
            // From Khronos OpenGl wiki
            //As such, you should only use glFinish when you are doing something that the specification specifically states will not be synchronous. 
            
            // Adding this aswell makes CPU usage what it should be, but graphics cards are complex and some implementations may not have finished processing after this call
            
            // doesn"t return until all sent render commands are completed (pixels are in targeted framebuffers for rendering commands)
            //glFinish();
            
            // I'm assuming this does nothing when the window is hidden
            // So normally I think this is non_blocking (on my machine), but does block on next opengl call is there is still work to do, I think
            // With VSync on (or off for that matter), your render loop will block when the driver decides it should block. This may be when you swap buffers, but it may not be. It could be inside some random GL command when the driver FIFO has just filled up. It could also be when the GPU has gotten too many frames behind the commands you’re submitting on the CPU. In any case, you can force the driver to block on VSync by following SwapBuffers with a glFinish() call. That will force any conformant driver to wait until it can swap and does swap, which is (in a VSync-limited case with VSync enabled when VSync isn’t being “virtualized” by a compositor) typically after it hits the vertical retrace in the video signal. This is fine to do on a desktop GPU, but don’t do this on a tile-based mobile GPU
            // - Khronos Senior Community member
            
            // Another post - Senior member
            // That’s what SwapBuffers pretty much does. On drivers I’m familiar with, it merely queues a “I’m done; you can swap when ready” event on the command queue. If the command queue isn’t full (by driver-internal criteria), then you get the CPU back and can do what you want – before the Swap has occurred. Some driver’s will buffer up as much as another frame or two of commands before they block the draw thread in a GL call because their “queue full” criteria is met.
            
            //To force the driver “not” to do that (on a desktop discrete GPU only; i.e. sort-last architecture) and instead wait for the swap, you put a glFinish() right after SwapBuffers. When that returns, you know that the SwapBuffers has occurred, which means you’ve synchronized your draw thread with the VSync clock.
            SDL_GL_SwapWindow(window);
            
            // block until all GL execution is complete
            // Appers to make the waiting for vblank to happen here not during a later openGL call next frame such as glGetIntegerv();
            // glGetIntegerv may actually be triggering a glFinish internally
            
            //glFinish();
        }
        
        {
            ZoneScopedN("glcall after swap");
            // This causes a wait before executing (instead of a wait later in next frame opengl_impl_drawrenderdata())
            GLenum last_active_texture;
            glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
            // This doesn't causes wait here (happens in opengl_impl_drawrenderdata)
            //ImGuiIO& io = ImGui::GetIO();
            //glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        }
        
        // I think this or the impl_render_draw_data is waiting and windows is reporting it as cpu usage, even though it't not (other processes can still use cpu when driver stuff is waiting)
        //void swap_win_profiled(SDL_Window *window);
        //swap_win_profiled(window);
    }
    
#if defined(_WIN32)
    win32_free_context();
#endif
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}


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
