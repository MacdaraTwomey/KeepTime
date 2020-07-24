@echo off

:: /DUSE_SSLEAY /DUSE_OPENSSL

:: /FC outputs full source file paths
:: /subsystem:windows needed for SDL

SET DefineFlags=-DMONITOR_DEBUG
:: -DTRACY_ENABLE
:: -DRECORD_ALL_ACTIVE_WINDOWS 

set LinkedLibraries=..\SDL2\SDL2.lib ..\SDL2\SDL2main.lib ..\GLEW21\glew32.lib ..\imgui\imgui.lib ..\freetype\freetype.lib opengl32.lib user32.lib shell32.lib Gdi32.lib Comctl32.lib ..\curl_7.69.1\x64_debug_dll_MDd\libcurl_debug.lib 

set SourceFiles=..\code\main.cpp

set ImguiSourceFiles=..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_widgets.cpp ..\imgui\imgui_impl_sdl.cpp ..\imgui\imgui_impl_opengl3.cpp ..\imgui\imgui_freetype.cpp  ..\imgui\imgui_demo.cpp

set IncludeDirs=/I..\imgui /I..\curl_7.69.1\include /I..\SDL2\include /I..\GLEW21\include /I..\freetype\include /I..\tracy

:: binary_to_compressed\binary_to_compressed_c.exe build\fonts\Roboto-Medium.ttf roboto_font_file > code\compressed_fonts\roboto_font_file.cpp

pushd build

::rc icon.rc

cl %DefineFlags% %SourceFiles% %IncludeDirs% /FC -W2 /MTd /EHsc -nologo -Zi /link %LinkedLibraries%  icon.res /subsystem:windows /out:monitor.exe

:: BUILD ALL FILES
::cl %DefineFlags% %SourceFiles% %ImguiSourceFiles% %IncludeDirs% /FC -W2 /EHsc -nologo -Zi /link %LinkedLibraries% icon.res /subsystem:windows /out:monitor.exe

:: BUILD IMGUI DLL (REQUIRES CHANGING defining IMGUI_API as  __declspec(dllexport) in imgui.h 
::cl ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_widgets.cpp ..\imgui\imgui_freetype.cpp  ..\imgui\imgui_impl_sdl.cpp ..\imgui\imgui_impl_opengl3.cpp ..\imgui\imgui_demo.cpp %IncludeDirs% -LD -W2 -nologo -Zi /link ..\GLEW21\glew32.lib ..\freetype\freetype.lib  ..\SDL2\SDL2.lib ..\SDL2\SDL2main.lib opengl32.lib user32.lib shell32.lib

popd