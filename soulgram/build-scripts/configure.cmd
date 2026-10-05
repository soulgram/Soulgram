@echo off
rem Generates the Visual Studio solution in tdesktop\out.
rem Reads api_id / api_hash from api_credentials.cmd (never committed to git).
rem Without that file the TEST-ONLY credentials are used: the app builds,
rem but logging in will most likely fail.
setlocal
call "%~dp0soulgram_env.cmd" || exit /b 1

set "SOULGRAM_API_ARGS=-D TDESKTOP_API_TEST=ON"
if not exist "%SOULGRAM_ROOT%\api_credentials.cmd" goto configure

call "%SOULGRAM_ROOT%\api_credentials.cmd"
if "%SOULGRAM_API_ID%"=="" goto nocreds
if "%SOULGRAM_API_ID%"=="0" goto nocreds
if "%SOULGRAM_API_HASH%"=="" goto nocreds
set "SOULGRAM_API_ARGS=-D TDESKTOP_API_TEST=OFF -D TDESKTOP_API_ID=%SOULGRAM_API_ID% -D TDESKTOP_API_HASH=%SOULGRAM_API_HASH%"
echo [soulgram] Using api_id from api_credentials.cmd
goto configure

:nocreds
echo [soulgram] api_credentials.cmd is not filled in.
exit /b 1

:configure
if "%SOULGRAM_API_ARGS%"=="-D TDESKTOP_API_TEST=ON" echo [soulgram] WARNING: no api_credentials.cmd, using TEST-ONLY credentials.
cd /d "%SOULGRAM_ROOT%\tdesktop\Telegram"
call "%SOULGRAM_ROOT%\tdesktop\Telegram\configure.bat" x64 %SOULGRAM_API_ARGS% %*
exit /b %errorlevel%
