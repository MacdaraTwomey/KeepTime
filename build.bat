@echo off


:: To remake .res must recompile .rc file wiith
::  > rc icon.rc

:: /DUSE_SSLEAY /DUSE_OPENSSL

:: /FC outputs full source file paths
:: /subsystem:windows needed for SDL

:: ..\imgui\imgui_demo.cpp just for Imgui::showDemoWindow()

set DefineFlags=-DCYCLE

set LinkedLibraries=..\curl_7.69.1\x64_debug_dll_MDd\libcurl_debug.lib ..\SDL2\SDL2.lib ..\SDL2\SDL2main.lib ..\GLEW21\glew32.lib ..\freetype\freetype.lib opengl32.lib user32.lib shell32.lib Gdi32.lib Comctl32.lib

set SourceFiles=..\code\main.cpp ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_widgets.cpp ..\imgui\imgui_impl_sdl.cpp ..\imgui\imgui_impl_opengl3.cpp ..\imgui\imgui_freetype.cpp ..\imgui\imgui_demo.cpp

set IncludeDirs=/I..\imgui /I..\curl_7.69.1\include /I..\SDL2\include /I..\GLEW21\include /I..\freetype\include
pushd build

:: Turn off EHsc in future

rc icon.rc

cl %DefineFlags% %SourceFiles% %IncludeDirs% /FC -W2 /EHsc -nologo  -Zi  /link %LinkedLibraries% icon.res /subsystem:windows /out:monitor.exe

popd
