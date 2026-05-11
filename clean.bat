@echo off
setlocal EnableExtensions

rem Formal clean entry point. Keep all toolchain detection, PATH setup and
rem make job policy in build.bat so clean/build/test stay consistent.
call "%~dp0build.bat" clean %*
exit /b %ERRORLEVEL%
