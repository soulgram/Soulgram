@echo off
rem ==================================================================
rem  Soulgram build environment.
rem  Initializes MSVC 14.44 (x64) and keeps every temp/cache folder
rem  inside the build folder, so nothing is written to drive C:.
rem  Used by prepare.cmd, configure.cmd and build.cmd.
rem ==================================================================

set "SOULGRAM_ROOT=%~dp0"
if "%SOULGRAM_ROOT:~-1%"=="\" set "SOULGRAM_ROOT=%SOULGRAM_ROOT:~0,-1%"

set "TEMP=%SOULGRAM_ROOT%\tmp"
set "TMP=%SOULGRAM_ROOT%\tmp"
set "PIP_CACHE_DIR=%SOULGRAM_ROOT%\tmp\pip-cache"
set "NUGET_PACKAGES=%SOULGRAM_ROOT%\tmp\nuget-packages"
set "VSLANG=1033"
if not exist "%TEMP%" mkdir "%TEMP%"

set "SOULGRAM_VS=F:\VS\18\Community"
if not exist "%SOULGRAM_VS%\VC\Auxiliary\Build\vcvars64.bat" (
    echo [soulgram] Visual Studio not found in %SOULGRAM_VS%
    exit /b 1
)

rem vcvars needs vswhere.exe from the Visual Studio Installer folder.
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%SOULGRAM_VS%\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul
if not "%Platform%"=="x64" (
    echo [soulgram] Could not initialize the x64 MSVC 14.44 environment.
    exit /b 1
)
echo [soulgram] Environment ready: MSVC %VCToolsVersion%, temp in %TEMP%
exit /b 0
