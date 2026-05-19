@echo off
cd /d E:\DEVELOPMENT\AI\Oracle-Pad-VST

echo [1/4] Wiping old build...
rmdir /s /q build

echo [2/4] Configuring (x64)...
cmake -B build -A x64 -T host=x64
if errorlevel 1 ( echo ERROR: CMake configure failed. & pause & exit /b 1 )

echo [3/4] Compiling Release...
cmake --build build --config Release --target OraclePad_All -j 4
if errorlevel 1 ( echo ERROR: Build failed. & pause & exit /b 1 )

echo.
echo SUCCESS: VST3 DEPLOYED TO ABLETON
pause
