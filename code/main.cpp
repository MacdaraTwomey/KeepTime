
#include "imgui.h" // include before other imgui
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

#include "utilities.cpp" // xalloc, string copy, concat string, make filepath, get_filename_from_path
#include "bitmap.cpp"
#include "win32_monitor.cpp" // needs bitmap functions
#include "monitor.cpp"

static char *global_savefile_path;
static char *global_debug_savefile_path;

#define WINDOW_WIDTH 1240
#define WINDOW_HEIGHT 720

void
init_imgui(SDL_Window *window, SDL_GLContext gl_context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context); // just sets keymap, gets cursors and sets backend flags
    ImGui_ImplOpenGL3_Init("#version 130"); // just sets what version of opengl, and backend flags for imgui
    
    ImGuiIO& io = ImGui::GetIO(); 
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.IniFilename = NULL; // Disable imgui.ini filecreation
    
    ImGui::StyleColorsLight();
    //ImGui::Spectrum::StyleColorsSpectrum(); 
    
#if 0
    auto light_grey = ImVec4(0.79f, 0.79f, 0.79f, 0.80f);
    auto medium_grey = ImVec4(0.63f, 0.63f, 0.63f, 0.80f);
    auto dark_grey = ImVec4(0.63f, 0.63f, 0.63f, 1.00f);
    
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
    // SourceSansProRegular_compressed_data is in helper.h
    io.Fonts->AddFontFromMemoryCompressedTTF(SourceSansProRegular_compressed_data, SourceSansProRegular_compressed_size, 22.0f);
    ImFontConfig icons_config; 
    icons_config.MergeMode = true; 
    icons_config.PixelSnapH = true;
    
    // NOTE: glyph ranges must exist at least until atlas is built
    ImVector<ImWchar> range_22;
    ImFontGlyphRangesBuilder builder_22;
    builder_22.AddText(ICON_MD_DATE_RANGE);
    builder_22.AddText(ICON_MD_ARROW_FORWARD);
    builder_22.AddText(ICON_MD_ARROW_BACK);
    builder_22.AddText(ICON_MD_SETTINGS);
    builder_22.BuildRanges(&range_22);                          
    
    // TODO: could remove glyphs to reduce size with online tool
    icons_config.GlyphOffset = ImVec2(0, 4); // move glyphs down or else they render too high
    io.Fonts->AddFontFromFileTTF("c:\\dev\\projects\\monitor\\build\\fonts\\MaterialIcons-Regular.ttf", 22.0f, &icons_config, range_22.Data);
    
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

u32
platform_SDL_get_monitor_refresh_rate_milliseconds()
{
    // or if 0 is not correct, could pass in sdl_window and get index with SDL_GetWindowDisplayIndex
    SDL_DisplayMode display_mode;
    int result = SDL_GetCurrentDisplayMode(0, &display_mode);
    if (result == 0)
    {
        return 16; // 60fps default
    }
    else
    {
        return display_mode.refresh_rate;
    }
}

Uint32 
timer_callback(Uint32 interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;
    
    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;
    
    event.type = SDL_USEREVENT;
    event.user = userevent;
    
    SDL_PushEvent(&event);
    return(interval);
}


int main(int argc, char* argv[])
{
    ZoneScoped;
    
    if (platform_show_other_instance_if_already_running())
    {
        return 0;
    }
    
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
    
    // TODO: Make it clear that the popup gui is just a 'view' of the 'real' running app, so closing just the window and keeping the app running is clearer.
    SDL_Window* window = SDL_CreateWindow(PROGRAM_NAME,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    
    // Window should start hidden in release
    //SDL_HideWindow(window);
    //SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW); // maybe
    
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    
    // NOTE: VSync is not on every computer so need to manage own framerate for the UI in addition to the normal polling rate we also manage.
    // Also VSync rate varies 60 fps, 30 fps
    SDL_GL_SetSwapInterval(1); // enable VSync
    
    if (glewInit() != GLEW_OK)
    {
        return 1;
    }
    
    init_imgui(window, gl_context);
    
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version); /* initialize info structure with SDL version info */
    if(SDL_GetWindowWMInfo(window,&info)) {
        init_win32_context(info.info.win.window);
    }
    else
    {
        Assert(0);
        return 1;
    }
    
    {
        char *exe_dir_path = SDL_GetBasePath();
        global_savefile_path = make_filepath_with_dir(exe_dir_path, "savefile.mpt");
        if (!global_savefile_path) return 1;
        global_debug_savefile_path = make_filepath_with_dir(exe_dir_path, "debug_savefile.txt");
        if (!global_debug_savefile_path) return 1;
        SDL_free(exe_dir_path);
    }
    
    // This may just be temporary.
    Monitor_State monitor_state = {};
    
    float fps = 60;
    float seconds_per_frame = 1/fps;
    u32 target_frame_time = (s64)floor(seconds_per_frame * 1000.0f); // in ms
    SDL_TimerID timer_id = SDL_AddTimer(target_frame_time, timer_callback, nullptr);
    
    auto old_time = win32_get_time();
    
    bool running = true;
    while (running)
    {
        ZoneScopedN("Frame");
        
        //auto frame_start_time = win32_get_time();
        
        // Initial state of window is set by events after window is opened 
        Window_Event window_event = Window_No_Change;
        
        SDL_Event event;
        int got_event = SDL_WaitEvent(&event);
        do
        {
            if (!got_event) break;
            
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                window_event = Window_Closed;
                running = false;
            }
            else if (event.type == SDL_USEREVENT)
            {
                int x = 3;
            }
            else if (event.type == SDL_WINDOWEVENT) 
            {
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_SHOWN:
                    {
                        // SDL_SetThreadPriority
                        window_event = Window_Shown;
                    } break;
                    case SDL_WINDOWEVENT_HIDDEN:
                    {
                        // SDL_SetThreadPriority
                        window_event = Window_Hidden;
                    } break;
                    case SDL_WINDOWEVENT_CLOSE:
                    {
                        window_event = Window_Hidden;
                        //window_event = Window_Closed;
                        running = false;
                    } break;
                    // Don't really care about rendering during resizing
                }
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) 
            {
                window_event = Window_Closed;
                running = false;
            }
            else if (event.type == SDL_SYSWMEVENT) 
            {
                SDL_SysWMmsg *sys_msg = event.syswm.msg;
                win32_handle_message(sys_msg->msg.win.msg, sys_msg->msg.win.lParam, sys_msg->msg.win.wParam);
            }
        }
        while (SDL_PollEvent(&event)); // if more events keep getting them
        
        
        auto new_time = win32_get_time();
        s64 dt_microseconds = win32_get_microseconds_elapsed(old_time, new_time);
        old_time = new_time;
        
        u32 desired_frame_time = update(&monitor_state, window, dt_microseconds, window_event);
        if (desired_frame_time != target_frame_time)
        {
            target_frame_time = desired_frame_time;
            
            Assert(timer_id);
            SDL_RemoveTimer(timer_id);
            timer_id = SDL_AddTimer(target_frame_time, timer_callback, nullptr);
        }
        
        // NOTE: With the self regulated 60fps, cpu usage spikes to 25% for 30 secs then goes to 1% and stays there
        // this may have been the case before i just didn't wait long enough for cpu usage to go down, but probably not because we were not sleeping, but just letting GPU sleep us.
        
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
// SDL wait until event functions are just a loop that waits 1ms at a time peeping for events ...
auto frame_end_time = win32_get_time();
s64 frame_time_elapsed_microseconds = win32_get_microseconds_elapsed(frame_start_time, frame_end_time);
s64 remaining = target_time_per_frame - frame_time_elapsed_microseconds;
while (remaining > 0) 
{
    SDL_PumpEvents();
    SDL_Event e;
    
    switch (SDL_PeepEvents(&e, 1, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
        case -1: // failure
        case 0: // no events
        {
            SDL_Delay(1); // millisecs
            auto now = win32_get_time();
            s64 time_elapsed = win32_get_microseconds_elapsed(frame_end_time, now);
            remaining -= time_elapsed;
        } break;
        
        default: // Has events
        remaining = 0;
        break;
    }
}
//or
auto frame_end_time = win32_get_time();
s64 frame_time_elapsed_microseconds = win32_get_microseconds_elapsed(frame_start_time, frame_end_time);
s64 remaining = target_time_per_frame - frame_time_elapsed_microseconds;
SDL_Event e;
SDL_WaitEventTimeout(&e, remaining / 1000);
#endif