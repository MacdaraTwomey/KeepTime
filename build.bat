@echo off

:: /subsystem:windows needed for SDL

SET DefineFlags=-DMONITOR_DEBUG=1 ^
 -DRECORD_ALL_ACTIVE_WINDOWS=0 ^
 -DFAKE_RECORDS=1
:: -DTRACY_ENABLE

set LinkedLibraries=..\SDL2\SDL2.lib ..\SDL2\SDL2main.lib ..\GLEW21\glew32.lib ..\imgui\imgui.lib ..\freetype\freetype.lib opengl32.lib user32.lib shell32.lib Gdi32.lib Comctl32.lib 

set SourceFiles=..\code\main.cpp

set ImguiSourceFiles=..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_widgets.cpp ..\imgui\imgui_impl_sdl.cpp ..\imgui\imgui_impl_opengl3.cpp ..\imgui\imgui_freetype.cpp  ..\imgui\imgui_demo.cpp

::set IncludeDirs=/I..\imgui /I..\SDL2\include /I..\GLEW21\include /I..\freetype\include /I..\tracy
set IncludeDirs=/I..\imgui /I..\freetype\include /I..\tracy

:: binary_to_compressed\binary_to_compressed_c.exe build\fonts\Roboto-Medium.ttf roboto_font_file > code\compressed_fonts\roboto_font_file.cpp

pushd build

:: rc icon.rc

:: TODO: Try /MP for multiple process build to make faster?

cl ..\code\first.cpp %IncludeDirs% /FC -W2 /MTd /EHsc -nologo -Zi /link imgui.lib opengl32.lib gdi32.lib shell32.lib Comctl32.lib icon.res user32.lib /subsystem:windows /out:monitor.exe

:: Normal dev build
:: cl %DefineFlags% %SourceFiles% %IncludeDirs% /FC -W2 /MTd /EHsc -nologo -Zi /link %LinkedLibraries%  icon.res /subsystem:windows /out:monitor.exe

:: BUILD ALL FILES
::cl %DefineFlags% %SourceFiles% %ImguiSourceFiles% %IncludeDirs% /FC -W2 /EHsc -nologo -Zi /link %LinkedLibraries% icon.res /subsystem:windows /out:monitor.exe

:: BUILD IMGUI DLL (REQUIRES CHANGING defining IMGUI_API as  __declspec(dllexport) in imgui.h 
::cl ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_widgets.cpp ..\imgui\imgui_freetype.cpp  ..\imgui\imgui_impl_sdl.cpp ..\imgui\imgui_impl_opengl3.cpp ..\imgui\imgui_demo.cpp %IncludeDirs% -LD -W2 -nologo -Zi /link ..\GLEW21\glew32.lib ..\freetype\freetype.lib  ..\SDL2\SDL2.lib ..\SDL2\SDL2main.lib opengl32.lib user32.lib shell32.lib

:: BUILD IMGUI DLL (REQUIRES CHANGING defining IMGUI_API as  __declspec(dllexport) in imgui.h 
:: win32 + OpenGL build, -Zi
::cl ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_widgets.cpp ..\imgui\imgui_tables.cpp ..\imgui\misc\freetype\imgui_freetype.cpp ..\imgui\backends\imgui_impl_win32.cpp ..\imgui\backends\imgui_impl_opengl3.cpp ..\imgui\imgui_demo.cpp %IncludeDirs% -LD -W2 -nologo -Zi /link ..\freetype\freetype.lib opengl32.lib user32.lib shell32.lib

:: Build ImGui as a static library
:: cl /c ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_widgets.cpp ..\imgui\imgui_tables.cpp  ..\imgui\imgui_demo.cpp ..\imgui\misc\freetype\imgui_freetype.cpp ..\imgui\backends\imgui_impl_win32.cpp ..\imgui\backends\imgui_impl_opengl3.cpp %IncludeDirs% -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD -W2 -Zi /MTd

:: lib imgui.obj imgui_draw.obj imgui_widgets.obj imgui_tables.obj imgui_demo.obj imgui_freetype.obj imgui_impl_win32.obj imgui_impl_opengl3.obj 

popd

