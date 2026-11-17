@echo off
cd /d "%~dp0"
set "PATH=D:\devkit\bin;D:\devkit\mingw64\bin;D:\devkit\llvm\bin;%PATH%"

echo [D:\devkit] Compilare resurse...
windres app.rc -O coff -o app.res
if errorlevel 1 goto error

echo [D:\devkit] Compilare PulseSubRenamer.exe...
g++ -std=c++20 -O2 -Wall -municode -mwindows -static main.cpp app.res -o PulseSubRenamer.exe -lcomctl32 -lcomdlg32 -lshlwapi -ldwmapi -lole32 -luuid
if errorlevel 1 goto error

echo.
echo [SUCCES] PulseSubRenamer.exe a fost creat in %CD%!
pause
exit /b 0

:error
echo.
echo [EROARE] Compilarea a esuat! Verifica liniile de mai sus.
pause
exit /b 1
