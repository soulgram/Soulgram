@echo off
rem Builds the Soulgram installer from tdesktop\out\Release.
rem The result is installer\Soulgram-setup-<version>.exe
rem Inno Setup 7 is at D:\dev\InnoSetup7, set SOULGRAM_ISCC to use another one.
setlocal
if "%SOULGRAM_ISCC%"=="" set "SOULGRAM_ISCC=D:\dev\InnoSetup7\ISCC.exe"
set "SOULGRAM_TOOLS=%~dp0tdesktop\Telegram\SourceFiles\soulgram\tools"
set "SOULGRAM_RELEASE=%~dp0tdesktop\out\Release"
set "SOULGRAM_OUT=%~dp0installer"

if not exist "%SOULGRAM_ISCC%" (
	echo [soulgram] Inno Setup not found: %SOULGRAM_ISCC%
	exit /b 1
)
if not exist "%SOULGRAM_RELEASE%\Soulgram.exe" (
	echo [soulgram] Build Release first: %SOULGRAM_RELEASE%\Soulgram.exe
	exit /b 1
)

rem The version is SoulgramVersionStr from core/version.h.
for /f "usebackq delims=" %%v in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%SOULGRAM_TOOLS%\soulgram_version.ps1"`) do set "SOULGRAM_VERSION=%%v"
if "%SOULGRAM_VERSION%"=="" (
	echo [soulgram] Could not read the version from core/version.h
	exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%SOULGRAM_TOOLS%\make_installer_images.ps1" -OutDir "%SOULGRAM_OUT%\images" || exit /b 1

"%SOULGRAM_ISCC%" /Q "/DMyAppVersion=%SOULGRAM_VERSION%" "/DReleasePath=%SOULGRAM_RELEASE%" "/DImagesPath=%SOULGRAM_OUT%\images" "/DOutputPath=%SOULGRAM_OUT%" "%SOULGRAM_TOOLS%\soulgram_setup.iss"
exit /b %errorlevel%
