@echo off


:: To remake .res must recompile .rc file wiith 
::  > rc icon.rc

cd
pushd build
cd

:: Turn off EHsc in future
:: rc icon.rc
cl c:\dev\projects\monitor\code\monitor.cpp /EHsc -nologo -Zi /link user32.lib shell32.lib Gdi32.lib icon.res

popd
