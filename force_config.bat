@echo off
setlocal

:: 1. Initialize the Environment (VS 2026 Build Tools)
call "C:\PROGRA~2\MICROS~2\18\BUILDT~1\VC\Auxiliary\Build\vcvarsall.bat" x64

:: 2. Wipe the old build to clear the "missing target" errors
if exist build rmdir /s /q build

:: 3. Run CMake
:: Note: We don't manually set CC/CXX here; vcvarsall has already put 'cl.exe' in your PATH.
cmake -B build -G "Ninja" ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DJUCE_PATH="E:/DEVELOPMENT/AI/JUCE"

endlocal
pause