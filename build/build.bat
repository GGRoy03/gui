@echo off
SETLOCAL

:: ------------------------------------
:: Config
:: ------------------------------------
set "CXX=clang-cl"

set "TARGET=void_engine.exe"
set "SRCS=..\incremental_build.cpp"
set "INCLUDES=..\..\ui_engine"
set "OUT_DIR=."

:: ------------------------------------
:: Build type
:: ------------------------------------
if /I "%~1"=="release" (
    set "TARGET=void_engine_release.exe"
    set "BUILD=release"
    set "CXXFLAGS=/O2 /Zi -maes -Wno-deprecated-declarations /std:c++20 /EHsc"
    set "LDFLAGS=/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"
    set "PDBNAME=void_engine_release.pdb"
) else (
    set "BUILD=debug"
    set "CXXFLAGS=/Od /Zi /W3 -maes -Wno-unused-function -Wno-deprecated-declarations /std:c++20 /EHsc /DDEBUG"
    set "LDFLAGS=/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"
    set "PDBNAME=void_engine.pdb"
)

echo Building %TARGET% (%BUILD%)...
echo CXXFLAGS: %CXXFLAGS%
echo.

:: ------------------------------------
:: One-step compile + link
:: ------------------------------------
"%CXX%" %SRCS% ^
    /I "%INCLUDES%" ^
    %CXXFLAGS% ^
    /Fe"%OUT_DIR%\%TARGET%" ^
    /link %LDFLAGS% /DEBUG /PDB:"%OUT_DIR%\%PDBNAME%"

if errorlevel 1 (
    echo *** Build failed ***
    exit /b 1
)

echo *** BUILD SUCCEEDED: %OUT_DIR%\%TARGET% ***
ENDLOCAL
exit /b 0
