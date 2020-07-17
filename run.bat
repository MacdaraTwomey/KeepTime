@echo off


call build.bat

if %errorlevel%==0 (
   goto good
)

goto bad

:good
   echo Success
   build\monitor.exe
   goto end

:bad
   echo Failure
   goto end


:end