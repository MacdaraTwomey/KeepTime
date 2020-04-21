@echo off


:: To remake .res must recompile .rc file wiith
::  > rc icon.rc

:: /DUSE_SSLEAY /DUSE_OPENSSL

set DefineFlags=-DCYCLE

pushd build

:: Turn off EHsc in future

rc icon.rc

cl %DefineFlags% c:\dev\projects\monitor\code\win32_monitor.cpp /I..\curl_7.69.1\include -W2 /EHsc -nologo  -Zi /link ..\curl_7.69.1\x64_debug_dll_MDd\libcurl_debug.lib user32.lib shell32.lib Gdi32.lib Comctl32.lib icon.res /out:monitor.exe

popd
