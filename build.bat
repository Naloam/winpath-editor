@echo off
REM build.bat — Build WinPath Editor
REM Usage: build.bat [msvc|mingw]

setlocal
if not exist build mkdir build

if "%1"=="mingw" goto :mingw
if "%1"=="msvc" goto :msvc

REM Auto-detect: prefer MSVC
where cl.exe >nul 2>&1
if %errorlevel%==0 goto :msvc
where gcc >nul 2>&1
if %errorlevel%==0 goto :mingw

echo [ERROR] No C compiler found. Install MSVC (run from Developer Command Prompt) or MinGW-w64.
exit /b 1

:msvc
echo [INFO] Building with MSVC ...
cl.exe /nologo /O1 /Os /W4 /GS /guard:cf /DUNICODE /D_UNICODE ^
    /Fobuild\ ^
    /Febuild\winpath.exe ^
    src\main.c src\ui.c src\path.c src\registry.c src\resource.rc ^
    /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF ^
    /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA ^
    /INCREMENTAL:NO /MANIFEST:NO ^
    comctl32.lib advapi32.lib shell32.lib user32.lib gdi32.lib shlwapi.lib ole32.lib
if %errorlevel% neq 0 (
    echo [ERROR] MSVC build failed.
    exit /b 1
)
echo [OK] Build complete: build\winpath.exe
goto :end

:mingw
echo [INFO] Building with MinGW ...

REM Step 1: Compile resource file
windres -DUNICODE -D_UNICODE -Isrc -Ires src\resource.rc -o build\resource.o
if %errorlevel% neq 0 (
    echo [ERROR] Resource compilation failed.
    exit /b 1
)

REM Step 2: Compile C source files
set CFLAGS=-Os -Wall -Wextra -DUNICODE -D_UNICODE -mwindows
gcc %CFLAGS% -c src\main.c -o build\main.o
if %errorlevel% neq 0 goto :gcc_fail
gcc %CFLAGS% -c src\ui.c -o build\ui.o
if %errorlevel% neq 0 goto :gcc_fail
gcc %CFLAGS% -c src\path.c -o build\path.o
if %errorlevel% neq 0 goto :gcc_fail
gcc %CFLAGS% -c src\registry.c -o build\registry.o
if %errorlevel% neq 0 goto :gcc_fail

REM Step 3: Link (with ASLR, NX, high-entropy VA)
gcc -mwindows -municode -o build\winpath.exe ^
    build\main.o build\ui.o build\path.o build\registry.o build\resource.o ^
    -lcomctl32 -ladvapi32 -lshell32 -luser32 -lgdi32 -lshlwapi -lole32 ^
    -Wl,--gc-sections,--dynamicbase,--nxcompat,--high-entropy-va
if %errorlevel% neq 0 goto :gcc_fail

REM Clean up object files
del /q build\*.o 2>nul

echo [OK] Build complete: build\winpath.exe
goto :end

:gcc_fail
echo [ERROR] MinGW build failed.
exit /b 1

:end
