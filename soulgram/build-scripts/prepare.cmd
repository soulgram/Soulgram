@echo off
rem Downloads and builds all third-party libraries (Qt, FFmpeg, WebRTC, ...).
rem The first run takes a few hours. Re-run after merging a new upstream version.
setlocal
call "%~dp0soulgram_env.cmd" || exit /b 1
cd /d "%SOULGRAM_ROOT%"
call "%SOULGRAM_ROOT%\tdesktop\Telegram\build\prepare\win.bat" silent %*
exit /b %errorlevel%
