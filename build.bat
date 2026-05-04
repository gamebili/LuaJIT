@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "MSYS_ROOT=D:\p4_gl2\pristine\ruby\Ruby33-x64\msys64"
set "MSYS_BIN=%MSYS_ROOT%\usr\bin"
set "UCRT_BIN=%MSYS_ROOT%\ucrt64\bin"
if not exist "%MSYS_BIN%\make.exe" (
  echo [build.bat] make.exe not found: %MSYS_BIN%\make.exe
  exit /b 1
)

set "PATH=%MSYS_BIN%;%UCRT_BIN%;%PATH%"

if "%BUILD_JOBS%"=="" (
  set "BUILD_JOBS=%NUMBER_OF_PROCESSORS%"
)
if "%BUILD_JOBS%"=="" set "BUILD_JOBS=4"

set "MAKE_JOBS=-j%BUILD_JOBS%"
for %%A in (%*) do (
  set "ARG=%%~A"
  if /I "!ARG:~0,2!"=="-j" set "MAKE_JOBS="
)

if "%~1"=="" (
  set "MAKE_ARGS=test"
) else (
  set "MAKE_ARGS=%*"
)

if /I "%~1"=="test" if "%~2"=="" (
  echo [build.bat] "%MSYS_BIN%\make.exe" %MAKE_JOBS% smoketest-capi-default
  "%MSYS_BIN%\make.exe" %MAKE_JOBS% smoketest-capi-default
  if errorlevel 1 exit /b !ERRORLEVEL!
  echo [build.bat] "%MSYS_BIN%\make.exe" %MAKE_JOBS% smoketest-capi-lua54compat
  "%MSYS_BIN%\make.exe" %MAKE_JOBS% smoketest-capi-lua54compat
  exit /b !ERRORLEVEL!
)
if "%~1"=="" (
  echo [build.bat] "%MSYS_BIN%\make.exe" %MAKE_JOBS% smoketest-capi-default
  "%MSYS_BIN%\make.exe" %MAKE_JOBS% smoketest-capi-default
  if errorlevel 1 exit /b !ERRORLEVEL!
  echo [build.bat] "%MSYS_BIN%\make.exe" %MAKE_JOBS% smoketest-capi-lua54compat
  "%MSYS_BIN%\make.exe" %MAKE_JOBS% smoketest-capi-lua54compat
  exit /b !ERRORLEVEL!
)

echo [build.bat] "%MSYS_BIN%\make.exe" %MAKE_JOBS% %MAKE_ARGS%
"%MSYS_BIN%\make.exe" %MAKE_JOBS% %MAKE_ARGS%
exit /b %ERRORLEVEL%
