@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Convenience wrapper for the local MSYS2 GNU make toolchain. The LuaJIT
rem top-level Makefile drives src/Makefile and the smoke/C API tests.
if "%MSYS_ROOT%"=="" set "MSYS_ROOT=D:\p4_gl2\pristine\ruby\Ruby33-x64\msys64"
set "MSYS_BIN=%MSYS_ROOT%\usr\bin"
set "UCRT_BIN=%MSYS_ROOT%\ucrt64\bin"
if "%GNUMAKE%"=="" set "GNUMAKE=%MSYS_BIN%\make.exe"

if not exist "%GNUMAKE%" (
  echo [build.bat] make.exe not found: %GNUMAKE%
  echo [build.bat] Set MSYS_ROOT or GNUMAKE to the correct toolchain path.
  exit /b 1
)

set "PATH=%MSYS_BIN%;%UCRT_BIN%;%PATH%"

if "%BUILD_JOBS%"=="" set "BUILD_JOBS=%NUMBER_OF_PROCESSORS%"
if "%BUILD_JOBS%"=="" set "BUILD_JOBS=4"

set "MAKE_JOBS=-j%BUILD_JOBS%"
set "MAKE_ARGS=%*"
for %%A in (%*) do (
  set "ARG=%%~A"
  rem Respect an explicit make parallelism override from the command line.
  if /I "!ARG:~0,2!"=="-j" set "MAKE_JOBS="
  if /I "!ARG:~0,5!"=="JOBS=" set "MAKE_JOBS=-j!ARG:~5!"
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
if /I "%~1"=="lua54" goto :LUA54
if /I "%~1"=="lua54perf" goto :LUA54PERF
if /I "%~1"=="smoke" goto :SMOKE
if /I "%~1"=="smoke54" goto :SMOKE54
if /I "%~1"=="rebuild" goto :REBUILD
goto :FORWARD

:HELP
echo Usage: build.bat [target or make args]
echo.
echo Common targets:
echo   build       Build LuaJIT only.
echo   test        Run default and Lua 5.4 C API smoke tests. This is default.
echo   default     Run the default compatibility smoke and C API smoke.
echo   lua54       Run the Lua 5.4 compatibility smoke and C API smoke.
echo   lua54perf   Run Lua 5.4 perf and memory smoke with JIT on/off.
echo   smoke       Run the default Lua smoke test only.
echo   smoke54     Run the Lua 5.4 compatibility Lua smoke test only.
echo   clean       Forward to make clean.
echo   rebuild     Run clean, then build.
echo.
echo Any other arguments are forwarded to GNU make unchanged.
echo Parallelism defaults to BUILD_JOBS or NUMBER_OF_PROCESSORS.
echo Override examples: build.bat lua54 -j8   or   set BUILD_JOBS=8
exit /b 0

:BUILD
call :SET_REST %*
set "MAKE_ARGS=all%REST_ARGS%"
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

:DEFAULT
call :SET_REST %*
call :RUN smoketest-capi-default%REST_ARGS%
exit /b !ERRORLEVEL!

:LUA54
call :SET_REST %*
call :RUN smoketest-capi-lua54compat%REST_ARGS%
exit /b !ERRORLEVEL!

:LUA54PERF
call :SET_REST %*
call :RUN smoketest-perf-lua54compat%REST_ARGS%
exit /b !ERRORLEVEL!

:TEST
call :SET_REST %*
call :RUN smoketest-capi-default%REST_ARGS%
if errorlevel 1 exit /b !ERRORLEVEL!
call :RUN smoketest-capi-lua54compat%REST_ARGS%
if errorlevel 1 exit /b !ERRORLEVEL!
call :RUN smoketest-perf-lua54compat%REST_ARGS%
exit /b !ERRORLEVEL!

:FORWARD
call :RUN %MAKE_ARGS%
exit /b !ERRORLEVEL!

:RUN
echo [build.bat] "%GNUMAKE%" %MAKE_JOBS% %*
"%GNUMAKE%" %MAKE_JOBS% %*
exit /b %ERRORLEVEL%

:SET_REST
set "REST_ARGS="
shift
:SET_REST_LOOP
if "%~1"=="" exit /b 0
set "REST_ARGS=!REST_ARGS! %~1"
shift
goto :SET_REST_LOOP
