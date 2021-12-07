
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

//#include <atlbase.h>
#include "tracy.hpp"
//#include "../tracy/TracyClient.cpp"

#include "cian.h"
#include "monitor.h"
#include "platform.h"

#include "win32_monitor.cpp"
#include "monitor.cpp"
#include "platform.cpp"

#define WINDOW_WIDTH 1240
#define WINDOW_HEIGHT 720

int main(int argc, char* argv[])
{
    
    printf("OPEN\n");
    ZoneScoped;
    
    // Only function that should be called before platform init context
    if (platform_show_other_instance_if_already_running(PROGRAM_NAME))
    {
        return 0;
    }
    
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) != 0)
    {
        return 1;
    }
    
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); 
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0); 
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    
    // TODO: Make it clear that the popup gui is just a 'view' of the 'real' running app, so closing just the window and keeping the app running is clearer, so maybe display a more descriptive window title
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
    SDL_GL_SetSwapInterval(1); // enable VSync (seems this needs to be explicitly set to 0, to achieve no sync, when texting)
    
    if (glewInit() != GLEW_OK)
    {
        return 1;
    }
    
    IMGUI_CHECKVERSION();
    
    if (!platform_init_context(window))
    {
        Assert(0);
        return 1;
    }
    
    // This may just be temporary.
    Monitor_State monitor_state = {};
    
    auto old_time = platform_get_time();
    
    bool imgui_initialised = false;
    
    bool running = true;
    while (running)
    {
        ZoneScopedN("Frame"); // before or after wait
        
        platform_wait_for_event_with_timeout(); 
        
        // Initial state of window is set by events after window is opened 
        Window_Event window_event = Window_No_Change;
        
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // Handles mouse and keypresses only
            if (imgui_initialised) ImGui_ImplSDL2_ProcessEvent(&event);
            
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
                        
                        if (!imgui_initialised)
                        {
                            ImGui::CreateContext();
                            // sets keymap, gets cursors and sets backend flags
                            ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
                            // Queries opengl version, and loader, and sets backend flags for imgui
                            ImGui_ImplOpenGL3_Init("#version 130");
                            imgui_initialised = true;
                        }
                    } break;
                    case SDL_WINDOWEVENT_HIDDEN:
                    {
                        // SDL_SetThreadPriority
                        window_event = Window_Hidden;
                        
                        if (imgui_initialised)
                        {
                            ImGui_ImplOpenGL3_Shutdown();
                            ImGui_ImplSDL2_Shutdown();
                            ImGui::DestroyContext();
                            imgui_initialised = false;
                        }
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
                platform_handle_message(sys_msg);
            }
        }
        
        auto new_time = platform_get_time();
        s64 dt_microseconds = platform_get_microseconds_elapsed(old_time, new_time);
        old_time = new_time;
        
        update(&monitor_state, window, dt_microseconds, window_event);
        
        {
            ZoneScopedN("Swap buffers and glFinish");
            
            // Only swap window when visible, seems to stop GPU usage when hidden
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_SHOWN)
                SDL_GL_SwapWindow(window);
        }
    }
    
    platform_free_context();
    
    if (imgui_initialised)
    {
        // Not needed
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
    
    // Do I need to call these
    SDL_GL_DeleteContext(gl_context); // probably yes
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}

