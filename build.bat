@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

SET includeFlags=/Isrc /Isrc/renderer /I%VULKAN_SDK%/Include 
SET linkerFlags=/link /LIBPATH:%VULKAN_SDK%/Lib vulkan-1.lib user32.lib opengl32.lib Gdi32.lib
SET defines=/D DEBUG /D WINDOWS_BUILD

if not exist build\NUL mkdir build

echo "Building main..."
cl /EHsc /Z7 /std:c++17 /Fe"main" /Fobuild/ %defines% %includeFlags% src/platform/win32_platform.cpp %linkerFlags%

@REM Play sound to indicate Building is completed, fun
powershell -c (New-Object Media.SoundPlayer "building-completed.wav").PlaySync()
EXIT