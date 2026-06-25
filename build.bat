@echo off
set "CDIR=%~dp0"
set "CDIR=%CDIR:~0,-1%"

taskkill /f /im qemu-system-i386.exe >nul 2>&1

echo Building project...
docker build -q -t aurux-builder .
if %errorlevel% neq 0 (
    echo Docker build failed!
    exit /b %errorlevel%
)

docker run --rm -v "%CDIR%:/app" aurux-builder make
if %errorlevel% neq 0 (
    echo Make failed!
    exit /b %errorlevel%
)

echo Starting QEMU...
qemu-system-i386 -kernel build/kernel.bin -hda build/aurux.img -m 256
