@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Convenience wrapper for the local MSYS2 GNU make toolchain. The LuaJIT
rem top-level Makefile drives src/Makefile and the smoke/C API tests.
if "%MSYS_ROOT%"=="" (
  if exist "H:\p4\gl_home_u4\pristine\ruby\msys64\usr\bin\make.exe" (
    set "MSYS_ROOT=H:\p4\gl_home_u4\pristine\ruby\msys64"
  ) else if exist "H:\p4\gl_home_u4\pristine\msys64\usr\bin\make.exe" (
    set "MSYS_ROOT=H:\p4\gl_home_u4\pristine\msys64"
  ) else (
    set "MSYS_ROOT=D:\p4_gl2\pristine\ruby\Ruby33-x64\msys64"
  )
)
set "MSYS_BIN=%MSYS_ROOT%\usr\bin"
set "UCRT_BIN=%MSYS_ROOT%\ucrt64\bin"
set "MINGW64_BIN=%MSYS_ROOT%\mingw64\bin"
if "%GNUMAKE%"=="" set "GNUMAKE=%MSYS_BIN%\make.exe"

if not exist "%GNUMAKE%" (
  echo [build.bat] make.exe not found: %GNUMAKE%
  echo [build.bat] Set MSYS_ROOT or GNUMAKE to the correct toolchain path.
  exit /b 1
)

if not exist "%MSYS_ROOT%\tmp" mkdir "%MSYS_ROOT%\tmp" >nul 2>nul
if "%LUA54_SRC_DIR%"=="" if exist "H:\p4\gl_home_u4\pristine\tools\lua\source\lua-5.4.8\lua.h" (
  set "LUA54_SRC_DIR=H:\p4\gl_home_u4\pristine\tools\lua\source\lua-5.4.8"
)
if "%LUA54_TESTES_DIR%"=="" if not "%LUA54_SRC_DIR%"=="" if exist "%LUA54_SRC_DIR%\testes\all.lua" (
  set "LUA54_TESTES_DIR=%LUA54_SRC_DIR%\testes"
)

if exist "%UCRT_BIN%\gcc.exe" (
  set "PATH=%UCRT_BIN%;%MSYS_BIN%;%MINGW64_BIN%;%PATH%"
) else if exist "%MINGW64_BIN%\gcc.exe" (
  set "PATH=%MINGW64_BIN%;%MSYS_BIN%;%UCRT_BIN%;%PATH%"
) else (
  set "PATH=%MSYS_BIN%;%UCRT_BIN%;%MINGW64_BIN%;%PATH%"
)

set "CPU_THREADS="
set "PERF_BUILD_JOBS="
set "DEFAULT_BUILD_JOBS="
set "MAX_BUILD_JOBS="
set "BUILD_JOBS_SOURCE=auto-max"
for /f "usebackq delims=" %%C in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "$sum=0; foreach ($cpu in (Get-CimInstance Win32_Processor)) { $sum += $cpu.NumberOfLogicalProcessors }; if ($sum -gt 0) { [int]$sum }" 2^>nul`) do (
  if not "%%C"=="" set "CPU_THREADS=%%C"
)
if "!CPU_THREADS!"=="" if not "%NUMBER_OF_PROCESSORS%"=="" set "CPU_THREADS=%NUMBER_OF_PROCESSORS%"
if not "!CPU_THREADS!"=="" (
  rem Default to the most aggressive local job count. Short compile/test gates
  rem often leave CPU time idle at exactly one job per logical processor, while
  rem BUILD_JOBS=perf/logical or an explicit -jN can lower this for interactive use.
  set /a "PERF_BUILD_JOBS=!CPU_THREADS! + (!CPU_THREADS! + 1) / 2"
  set /a "MAX_BUILD_JOBS=!CPU_THREADS! * 2"
  set "DEFAULT_BUILD_JOBS=!MAX_BUILD_JOBS!"
)
if "!PERF_BUILD_JOBS!"=="" set "PERF_BUILD_JOBS=3"
if "!MAX_BUILD_JOBS!"=="" set "MAX_BUILD_JOBS=4"
if "!DEFAULT_BUILD_JOBS!"=="" set "DEFAULT_BUILD_JOBS=!MAX_BUILD_JOBS!"
if !PERF_BUILD_JOBS! LSS 1 set "PERF_BUILD_JOBS=1"
if !DEFAULT_BUILD_JOBS! LSS 1 set "DEFAULT_BUILD_JOBS=1"
if !MAX_BUILD_JOBS! LSS 1 set "MAX_BUILD_JOBS=1"
if "!CPU_THREADS!"=="" set "CPU_THREADS=!DEFAULT_BUILD_JOBS!"
if "%BUILD_JOBS%"=="" (
  set "BUILD_JOBS=!DEFAULT_BUILD_JOBS!"
) else (
  if /I "!BUILD_JOBS!"=="auto" (
    set "BUILD_JOBS=!DEFAULT_BUILD_JOBS!"
    set "BUILD_JOBS_SOURCE=BUILD_JOBS=auto-max"
  ) else if /I "!BUILD_JOBS!"=="perf" (
    set "BUILD_JOBS=!PERF_BUILD_JOBS!"
    set "BUILD_JOBS_SOURCE=BUILD_JOBS=perf"
  ) else if /I "!BUILD_JOBS!"=="max" (
    set "BUILD_JOBS=!MAX_BUILD_JOBS!"
    set "BUILD_JOBS_SOURCE=BUILD_JOBS=max"
  ) else if /I "!BUILD_JOBS!"=="logical" (
    set "BUILD_JOBS=!CPU_THREADS!"
    set "BUILD_JOBS_SOURCE=BUILD_JOBS=logical"
  ) else (
    set "BUILD_JOBS_SOURCE=BUILD_JOBS"
    set "BUILD_JOBS_NUM=1"
    for /f "delims=0123456789" %%N in ("!BUILD_JOBS!") do set "BUILD_JOBS_NUM="
    if "!BUILD_JOBS_NUM!"=="" (
      echo [build.bat] BUILD_JOBS must be a positive integer, auto, perf, max, or logical: !BUILD_JOBS!
      exit /b 1
    )
  )
)
if "!BUILD_JOBS!"=="" set "BUILD_JOBS=!DEFAULT_BUILD_JOBS!"
if !BUILD_JOBS! LSS 1 set "BUILD_JOBS=1"

set "MAKE_JOBS=-j!BUILD_JOBS!"
set "MAKE_ARGS=%*"
set "SAW_MAKE_J="
set "EXPECT_MAKE_JOBS="
for %%A in (%*) do (
  set "ARG=%%~A"
  if "!EXPECT_MAKE_JOBS!"=="1" (
    set "REQ_JOBS=!ARG!"
    set "REQ_JOBS_NUM=1"
    if "!REQ_JOBS!"=="" set "REQ_JOBS_NUM="
    for /f "delims=0123456789" %%N in ("!REQ_JOBS!") do set "REQ_JOBS_NUM="
    if not "!REQ_JOBS_NUM!"=="" (
      if !REQ_JOBS! LSS 1 set "REQ_JOBS=1"
      set "BUILD_JOBS=!REQ_JOBS!"
      set "BUILD_JOBS_SOURCE=-j"
      set "MAKE_JOBS=-j!REQ_JOBS!"
    )
    set "EXPECT_MAKE_JOBS="
  )
  rem GNU make treats bare -j as unlimited jobs. Strip it and use the detected
  rem or requested MAKE_JOBS computed above. If the next token is numeric, treat
  rem "-j 4" like "-j4" so the number is not forwarded as a make target.
  if /I "!ARG!"=="-j" (
    set "SAW_MAKE_J=1"
    set "BUILD_JOBS_SOURCE=-j"
    set "EXPECT_MAKE_JOBS=1"
  )
  rem Numeric -jN explicitly overrides the detected job count. Invalid forms are
  rem forwarded to make so make can reject them.
  if /I "!ARG:~0,2!"=="-j" if /I not "!ARG!"=="-j" (
    set "REQ_JOBS=!ARG:~2!"
    set "REQ_JOBS_NUM=1"
    if "!REQ_JOBS!"=="" set "REQ_JOBS_NUM="
    for /f "delims=0123456789" %%N in ("!REQ_JOBS!") do set "REQ_JOBS_NUM="
    if not "!REQ_JOBS_NUM!"=="" (
      set "SAW_MAKE_J=1"
      if !REQ_JOBS! LSS 1 set "REQ_JOBS=1"
      set "BUILD_JOBS=!REQ_JOBS!"
      set "BUILD_JOBS_SOURCE=-j"
      set "MAKE_JOBS=-j!REQ_JOBS!"
    )
  )
)

if "%~1"=="" goto :TEST
if /I "%~1"=="help" goto :HELP
if /I "%~1"=="-h" goto :HELP
if /I "%~1"=="--help" goto :HELP
if /I "%~1"=="/?" goto :HELP
if /I "%~1"=="build" goto :BUILD
if /I "%~1"=="all" goto :FORWARD
if /I "%~1"=="test" goto :TEST
if /I "%~1"=="default" goto :DEFAULT
if /I "%~1"=="lua54build" goto :LUA54_BUILD
if /I "%~1"=="lua54" goto :LUA54
if /I "%~1"=="lua54quick" goto :LUA54_QUICK
if /I "%~1"=="lua54nogc64" goto :LUA54_NOGC64
if /I "%~1"=="lua54compat53" goto :LUA54COMPAT53
if /I "%~1"=="lua54perf" goto :LUA54PERF
if /I "%~1"=="official54" goto :OFFICIAL54
if /I "%~1"=="lua54official" goto :OFFICIAL54
if /I "%~1"=="platform" goto :PLATFORM
if /I "%~1"=="platformprobe" goto :PLATFORM_PROBE
if /I "%~1"=="platformpc" goto :PLATFORM_PC
if /I "%~1"=="platformandroid" goto :PLATFORM_ANDROID
if /I "%~1"=="platformios" goto :PLATFORM_IOS
if /I "%~1"=="platformemscripten" goto :PLATFORM_EMSCRIPTEN
if /I "%~1"=="smoke" goto :SMOKE
if /I "%~1"=="smoke54" goto :SMOKE54
if /I "%~1"=="smoke54quick" goto :SMOKE54_QUICK
if /I "%~1"=="rebuild" goto :REBUILD
goto :FORWARD

:HELP
echo Usage: build.bat [target or make args]
echo.
echo Common targets:
echo   build       Incrementally build default LuaJIT artifacts.
echo   test        Run default, Lua 5.4 and non-GC64 smoke tests. This is default.
echo   default     Run the incremental default compatibility smoke and C API smoke.
echo   lua54build  Incrementally build Lua 5.4 compatibility artifacts.
echo   lua54       Run the Lua 5.4 compatibility smoke and C API smoke.
echo   lua54quick  Run incremental Lua 5.4 smoke and C API smoke.
echo   lua54nogc64 Run the Lua 5.4 x64 non-GC64 full smoke and JIT smoke.
echo   lua54compat53 Run the Lua 5.4 LUA_COMPAT_5_3 runtime smoke.
echo   lua54perf   Run Lua 5.4 perf/memory smoke with fixed jit.opt profiles and JIT off.
echo   official54  Run the current official Lua 5.4.8 compatibility matrix.
echo   platform    Run the PC/Android/iOS/Emscripten Lua 5.4 platform matrix.
echo   platformprobe Probe iOS/Emscripten toolchain availability without building PC/Android.
echo   smoke       Run the default Lua smoke test only.
echo   smoke54     Run the Lua 5.4 compatibility Lua smoke test only.
echo   smoke54quick Run incremental Lua 5.4 compatibility Lua smoke test only.
echo   clean       Forward to make clean.
echo   rebuild     Run clean, then build.
echo.
echo Any other arguments are forwarded to GNU make unchanged.
echo Perf profiles pin opt level, hotloop, and hotexit; override with LUA54_PERF_JIT_OPTS.
echo Parallelism defaults to max mode, about 2x detected logical processors; override with BUILD_JOBS=N, BUILD_JOBS=auto, BUILD_JOBS=perf, BUILD_JOBS=max, BUILD_JOBS=logical, or -jN.
echo BUILD_JOBS=perf uses about 1.5x detected logical processors; BUILD_JOBS=logical uses exactly the detected processor count. Bare -j is normalized to the current job count.
echo Quick Lua 5.4 targets clean only when the saved build flags change.
echo Examples: build.bat lua54quick -j64   or   set BUILD_JOBS=perf
exit /b 0

:BUILD
call :SET_REST %*
set "MAKE_ARGS=build-default-incremental%REST_ARGS%"
goto :FORWARD

:REBUILD
call :SET_REST %*
call :RUN clean%REST_ARGS%
if errorlevel 1 exit /b !ERRORLEVEL!
set "MAKE_ARGS=all%REST_ARGS%"
goto :FORWARD

:SMOKE
call :SET_REST %*
call :RUN smoketest%REST_ARGS%
exit /b !ERRORLEVEL!

:SMOKE54
call :SET_REST %*
call :RUN smoketest-lua54compat%REST_ARGS%
exit /b !ERRORLEVEL!

:SMOKE54_QUICK
call :SET_REST %*
call :RUN smoketest-lua54compat-quick%REST_ARGS%
exit /b !ERRORLEVEL!

:DEFAULT
call :SET_REST %*
call :RUN smoketest-capi-default%REST_ARGS%
exit /b !ERRORLEVEL!

:LUA54_BUILD
call :SET_REST %*
call :RUN build-lua54compat-incremental%REST_ARGS%
exit /b !ERRORLEVEL!

:LUA54
call :SET_REST %*
call :RUN smoketest-capi-lua54compat%REST_ARGS%
exit /b !ERRORLEVEL!

:LUA54_QUICK
call :SET_REST %*
call :RUN smoketest-capi-lua54compat-quick%REST_ARGS%
exit /b !ERRORLEVEL!

:LUA54_NOGC64
call :SET_REST %*
call :RUN smoketest-lua54compat-nogc64%REST_ARGS%
exit /b !ERRORLEVEL!

:LUA54COMPAT53
call :SET_REST %*
call :RUN smoketest-lua54compat53%REST_ARGS%
exit /b !ERRORLEVEL!

:LUA54PERF
call :SET_REST %*
call :RUN smoketest-perf-lua54compat%REST_ARGS%
exit /b !ERRORLEVEL!

:OFFICIAL54
call :SET_REST %*
call :RUN smoketest-official-lua54compat%REST_ARGS%
exit /b !ERRORLEVEL!

:PLATFORM
call :SET_REST %*
call :RUN_PLATFORM all%REST_ARGS%
exit /b !ERRORLEVEL!

:PLATFORM_PROBE
call :SET_REST %*
call :RUN_PLATFORM probe%REST_ARGS%
exit /b !ERRORLEVEL!

:PLATFORM_PC
call :SET_REST %*
call :RUN_PLATFORM pc%REST_ARGS%
exit /b !ERRORLEVEL!

:PLATFORM_ANDROID
call :SET_REST %*
call :RUN_PLATFORM android%REST_ARGS%
exit /b !ERRORLEVEL!

:PLATFORM_IOS
call :SET_REST %*
call :RUN_PLATFORM ios%REST_ARGS%
exit /b !ERRORLEVEL!

:PLATFORM_EMSCRIPTEN
call :SET_REST %*
call :RUN_PLATFORM emscripten%REST_ARGS%
exit /b !ERRORLEVEL!

:TEST
call :SET_REST %*
call :RUN smoketest-capi-default%REST_ARGS%
if errorlevel 1 exit /b !ERRORLEVEL!
call :RUN smoketest-capi-lua54compat%REST_ARGS%
if errorlevel 1 exit /b !ERRORLEVEL!
call :RUN smoketest-perf-lua54compat%REST_ARGS%
if errorlevel 1 exit /b !ERRORLEVEL!
call :RUN smoketest-lua54compat-nogc64%REST_ARGS%
exit /b !ERRORLEVEL!

:FORWARD
call :RUN %MAKE_ARGS%
exit /b !ERRORLEVEL!

:RUN
call :PRINT_JOBS
if not "!SAW_MAKE_J!"=="1" (
  echo [build.bat] "%GNUMAKE%" %MAKE_JOBS% %*
  "%GNUMAKE%" %MAKE_JOBS% %*
  exit /b !ERRORLEVEL!
)
set "RUN_ARGS="
set "SKIP_MAKE_JOBS="
for %%A in (%*) do (
  set "ARG=%%~A"
  set "KEEP_ARG=1"
  if "!SKIP_MAKE_JOBS!"=="1" (
    set "REQ_JOBS=!ARG!"
    set "REQ_JOBS_NUM=1"
    if "!REQ_JOBS!"=="" set "REQ_JOBS_NUM="
    for /f "delims=0123456789" %%N in ("!REQ_JOBS!") do set "REQ_JOBS_NUM="
    if not "!REQ_JOBS_NUM!"=="" set "KEEP_ARG="
    set "SKIP_MAKE_JOBS="
  )
  set "STRIP_MAKE_J="
  if "!KEEP_ARG!"=="1" if /I "!ARG!"=="-j" (
    set "STRIP_MAKE_J=1"
    set "SKIP_MAKE_JOBS=1"
  )
  if "!KEEP_ARG!"=="1" if /I "!ARG:~0,2!"=="-j" if /I not "!ARG!"=="-j" (
    set "REQ_JOBS=!ARG:~2!"
    set "REQ_JOBS_NUM=1"
    if "!REQ_JOBS!"=="" set "REQ_JOBS_NUM="
    for /f "delims=0123456789" %%N in ("!REQ_JOBS!") do set "REQ_JOBS_NUM="
    if not "!REQ_JOBS_NUM!"=="" set "STRIP_MAKE_J=1"
  )
  if "!STRIP_MAKE_J!"=="1" set "KEEP_ARG="
  if "!KEEP_ARG!"=="1" set "RUN_ARGS=!RUN_ARGS! %%A"
)
echo [build.bat] "%GNUMAKE%" %MAKE_JOBS% !RUN_ARGS!
"%GNUMAKE%" %MAKE_JOBS% !RUN_ARGS!
exit /b !ERRORLEVEL!

:RUN_PLATFORM
call :PRINT_JOBS
set "PLATFORM_ARGS="
set "SKIP_PLATFORM_JOBS="
for %%A in (%*) do (
  set "ARG=%%~A"
  set "KEEP_ARG=1"
  if "!SKIP_PLATFORM_JOBS!"=="1" (
    set "REQ_JOBS=!ARG!"
    set "REQ_JOBS_NUM=1"
    if "!REQ_JOBS!"=="" set "REQ_JOBS_NUM="
    for /f "delims=0123456789" %%N in ("!REQ_JOBS!") do set "REQ_JOBS_NUM="
    if not "!REQ_JOBS_NUM!"=="" set "KEEP_ARG="
    set "SKIP_PLATFORM_JOBS="
  )
  if "!KEEP_ARG!"=="1" if /I "!ARG!"=="-j" (
    set "KEEP_ARG="
    set "SKIP_PLATFORM_JOBS=1"
  )
  if "!KEEP_ARG!"=="1" if /I "!ARG:~0,2!"=="-j" if /I not "!ARG!"=="-j" (
    set "REQ_JOBS=!ARG:~2!"
    set "REQ_JOBS_NUM=1"
    if "!REQ_JOBS!"=="" set "REQ_JOBS_NUM="
    for /f "delims=0123456789" %%N in ("!REQ_JOBS!") do set "REQ_JOBS_NUM="
    if not "!REQ_JOBS_NUM!"=="" set "KEEP_ARG="
  )
  if "!KEEP_ARG!"=="1" set "PLATFORM_ARGS=!PLATFORM_ARGS! %%A"
)
echo [build.bat] powershell -ExecutionPolicy Bypass -File tools\lua54_platform_matrix.ps1 -Target !PLATFORM_ARGS! ^(BUILD_JOBS=!BUILD_JOBS!^)
powershell -ExecutionPolicy Bypass -File tools\lua54_platform_matrix.ps1 -Target !PLATFORM_ARGS!
exit /b !ERRORLEVEL!

:SET_REST
set "REST_ARGS="
shift
:SET_REST_LOOP
if "%~1"=="" exit /b 0
rem Preserve quoted make variable assignments such as
rem "XCFLAGS=-DFOO -DBAR"; stripping quotes would split them into make options.
set "REST_ARGS=!REST_ARGS! ^"%~1^""
shift
goto :SET_REST_LOOP

:PRINT_JOBS
if not "!PRINTED_BUILD_JOBS!"=="1" (
  echo [build.bat] detected !CPU_THREADS! logical processors; using !BUILD_JOBS! make jobs ^(!BUILD_JOBS_SOURCE!^).
  set "PRINTED_BUILD_JOBS=1"
)
exit /b 0
