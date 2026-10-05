; This file is part of Soulgram,
; an unofficial fork of Telegram Desktop.
;
; For license and copyright information please follow this link:
; https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

; Installer of Soulgram, based on Telegram/build/setup.iss.
; Defines from the command line: MyAppVersion, ReleasePath, ImagesPath,
; OutputPath. See installer.cmd next to the build scripts.

#define MyAppShortName "Soulgram"
#define MyAppName "Soulgram"
#define MyAppPublisher "Soulgram"
#define MyAppURL "https://github.com/soulgram/Soulgram"
#define MyAppSupportURL "https://t.me/official_soulgram"
#define MyAppExeName "Soulgram.exe"
; The same as AppId in core/version.h.
#define MyAppId "25A2470B-0C7F-4C06-ADB2-C5349BB57314"
#define ModulesFolder "modules\x64"

[Setup]
AppId={{{#MyAppId}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppSupportURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={userappdata}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir={#OutputPath}
OutputBaseFilename=Soulgram-setup-{#MyAppVersion}
SetupIconFile={#SourcePath}..\..\..\Resources\art\icon256.ico
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
DisableStartupPrompt=yes
PrivilegesRequired=lowest
VersionInfoVersion={#MyAppVersion}.0
VersionInfoDescription={#MyAppName} Setup
CloseApplications=force
DisableDirPage=no
DisableProgramGroupPage=yes
WizardStyle=modern
WizardImageFile={#ImagesPath}\wizard_100.bmp,{#ImagesPath}\wizard_150.bmp,{#ImagesPath}\wizard_200.bmp
WizardSmallImageFile={#ImagesPath}\wizard_small_100.bmp,{#ImagesPath}\wizard_small_150.bmp,{#ImagesPath}\wizard_small_200.bmp
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "ru";      MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "ua";      MessagesFile: "compiler:Languages\Ukrainian.isl"
Name: "de";      MessagesFile: "compiler:Languages\German.isl"
Name: "es";      MessagesFile: "compiler:Languages\Spanish.isl"
Name: "fr";      MessagesFile: "compiler:Languages\French.isl"
Name: "it";      MessagesFile: "compiler:Languages\Italian.isl"
Name: "nl";      MessagesFile: "compiler:Languages\Dutch.isl"
Name: "pt_BR";   MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#ReleasePath}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#ReleasePath}\{#ModulesFolder}\d3d\d3dcompiler_47.dll"; DestDir: "{app}\{#ModulesFolder}\d3d"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppShortName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppShortName}}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#MyAppShortName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppShortName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\data"
Type: files; Name: "{app}\data_config"
Type: files; Name: "{app}\log.txt"
Type: filesandordirs; Name: "{app}\DebugLogs"
Type: filesandordirs; Name: "{app}\tupdates"
Type: filesandordirs; Name: "{app}\tdata"
Type: filesandordirs; Name: "{app}\tcache"
Type: filesandordirs; Name: "{app}\tdumps"
Type: filesandordirs; Name: "{app}\modules"
Type: filesandordirs; Name: "{app}\soulgram"
Type: dirifempty; Name: "{app}"
Type: files; Name: "{userappdata}\{#MyAppName}\data"
Type: files; Name: "{userappdata}\{#MyAppName}\data_config"
Type: files; Name: "{userappdata}\{#MyAppName}\log.txt"
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}\DebugLogs"
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}\tupdates"
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}\tdata"
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}\tcache"
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}\tdumps"
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}\modules"
Type: filesandordirs; Name: "{userappdata}\{#MyAppName}\soulgram"
Type: dirifempty; Name: "{userappdata}\{#MyAppName}"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec(ExpandConstant('{app}\{#MyAppExeName}'), '-cleanup', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;
