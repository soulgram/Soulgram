@echo off
rem Builds Soulgram. Usage: build.cmd [Release|Debug]   (default: Release)
rem The result is tdesktop\out\<Config>\Soulgram.exe
rem
rem Each compiler process takes up to ~3 GB of memory on this code base.
rem 12 processes at once ran out of memory (commit limit ~35 GB), so the
rem number of parallel compiler processes is limited by SOULGRAM_JOBS.
setlocal
set "SOULGRAM_CONFIG=%~1"
if "%SOULGRAM_CONFIG%"=="" set "SOULGRAM_CONFIG=Release"
if "%SOULGRAM_JOBS%"=="" set "SOULGRAM_JOBS=4"
rem With a PDB the Release link needs ~25 GB of RAM and freezes the PC.
rem link.exe appends _LINK_ last, so it overrides /DEBUG from the project.
rem Set SOULGRAM_PDB=1 to get the PDB anyway.
if /i "%SOULGRAM_CONFIG%"=="Release" if not "%SOULGRAM_PDB%"=="1" set "_LINK_=/DEBUG:NONE"
call "%~dp0soulgram_env.cmd" || exit /b 1
cmake --build "%SOULGRAM_ROOT%\tdesktop\out" --config %SOULGRAM_CONFIG% --target Telegram -- /m:1 /p:CL_MPCount=%SOULGRAM_JOBS%
exit /b %errorlevel%
