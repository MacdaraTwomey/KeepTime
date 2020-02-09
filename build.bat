@echo off


set QT_INCLUDE="c:\qt\5.12.2\msvc2017_64\include"
set QT_LIB="c:\qt\5.12.2\msvc2017_64\lib"

:: rc icon.rc

:: TODO: Emacs run button

cd
pushd build
cd

:: Turn off EHsc in future
cl c:\dev\projects\monitor\code\monitor.cpp /EHsc -nologo -Zi /link user32.lib icon.res

popd
