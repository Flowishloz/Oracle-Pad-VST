@echo off
cd /d E:\DEVELOPMENT\AI\Oracle-Pad-VST

echo [1/2] Configuring (incremental — no wipe)...
cmake -B build -A x64 -T host=x64
if errorlevel 1 ( echo ERROR: CMake configure failed. & exit /b 1 )

echo [2/2] Compiling Release...
cmake --build build --config Release --target OraclePad_All -j 4
if errorlevel 1 ( echo ERROR: Build failed. & exit /b 1 )

echo.
echo SUCCESS: ALL-PASS CLOUD DEPLOYED
exit /b 0
