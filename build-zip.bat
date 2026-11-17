@echo off
setlocal
cd /d "%~dp0"

taskkill /f /im PulseSubRenamer.exe >nul 2>&1

set "DEVKIT=D:\devkit"
set "PATH=%DEVKIT%\bin;%DEVKIT%\mingw64\bin;%PATH%"

if exist "release" rmdir /s /q "release"
mkdir "release\PulseSubRenamer"

echo [D:\devkit] Compiling resources...
windres app.rc -O coff -o app.res

echo [D:\devkit] Compiling PulseSubRenamer.exe...
g++ -std=c++20 -O2 -Wall -municode -mwindows main.cpp app.res -o release\PulseSubRenamer\PulseSubRenamer.exe -lcomdlg32 -lole32 -luuid -lshell32 -lshlwapi -ldwmapi

del app.res 2>nul

if exist "release\PulseSubRenamer\PulseSubRenamer.exe" (
    if exist "README.md" copy /y "README.md" "release\PulseSubRenamer\README.md" >nul
    if exist "LICENSE" copy /y "LICENSE" "release\PulseSubRenamer\LICENSE" >nul
    powershell -NoProfile -Command "Compress-Archive -Path 'release\PulseSubRenamer' -DestinationPath 'release\PulseSubRenamer-v1.0.0-win64.zip' -Force"
    echo [SUCCESS] Portable folder: release\PulseSubRenamer\
    echo [SUCCESS] GitHub Release archive: release\PulseSubRenamer-v1.0.0-win64.zip
) else (
    echo [ERROR] Build failed. Check the errors above.
)
pause
