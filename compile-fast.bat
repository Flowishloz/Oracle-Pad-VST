@echo off
cd /d E:\DEVELOPMENT\AI\Oracle-Pad-VST

echo [1/3] Configuring (incremental — no wipe)...
cmake -B build -A x64 -T host=x64
if errorlevel 1 ( echo ERROR: CMake configure failed. & exit /b 1 )

echo [2/3] Compiling Release...
cmake --build build --config Release --target OraclePad_All -j 4
if errorlevel 1 ( echo ERROR: Build failed. & exit /b 1 )

echo [3/3] Deploying VST3 to E:\Music\Plugins\VST3...
if not exist "E:\Music\Plugins\VST3\OraclePad.vst3\Contents\x86_64-win" mkdir "E:\Music\Plugins\VST3\OraclePad.vst3\Contents\x86_64-win"
xcopy /Y "build\OraclePad_artefacts\Release\VST3\OraclePad.vst3\Contents\x86_64-win\OraclePad.vst3" "E:\Music\Plugins\VST3\OraclePad.vst3\Contents\x86_64-win\"
if errorlevel 1 ( echo ERROR: VST3 deploy to E:\Music\Plugins failed. & exit /b 1 )

echo.
echo SUCCESS: ORACLE-PAD COMPILED AND DEPLOYED
exit /b 0
