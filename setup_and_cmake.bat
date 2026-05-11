@echo off
:: Set Compilers
set "CC=C:\PROGRA~2\MICROS~2\18\BUILDT~1\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64\cl.exe"
set "CXX=C:\PROGRA~2\MICROS~2\18\BUILDT~1\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64\cl.exe"

:: Set Tool Paths for CMake
set "RC_PATH=C:\PROGRA~2\WI3CF2~1\10\bin\10.0.26100.0\x64\rc.exe"
set "MT_PATH=C:\PROGRA~2\WI3CF2~1\10\bin\10.0.26100.0\x64\mt.exe"

:: Set the LIBRARY paths (This fixes LNK1104)
set "LIB=C:\PROGRA~2\WI3CF2~1\10\Lib\10.0.26100.0\um\x64;C:\PROGRA~2\WI3CF2~1\10\Lib\10.0.26100.0\ucrt\x64;C:\PROGRA~2\MICROS~2\18\BUILDT~1\VC\Tools\MSVC\14.29.30133\lib\x64"

:: Set the INCLUDE paths (Just in case)
set "INCLUDE=C:\PROGRA~2\WI3CF2~1\10\Include\10.0.26100.0\um;C:\PROGRA~2\WI3CF2~1\10\Include\10.0.26100.0\ucrt;C:\PROGRA~2\WI3CF2~1\10\Include\10.0.26100.0\shared;C:\PROGRA~2\MICROS~2\18\BUILDT~1\VC\Tools\MSVC\14.29.30133\include"

:: Run CMake
cmake -B build -S . -G "Ninja" ^
-DCMAKE_MAKE_PROGRAM="C:/PROGRA~2/MICROS~2/18/BUILDT~1/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" ^
-DCMAKE_RC_COMPILER="%RC_PATH%" ^
-DCMAKE_MT="%MT_PATH%" ^
-DJUCE_PATH="E:/DEVELOPMENT/AI/JUCE"