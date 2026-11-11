#ifndef AppVersion
  #define AppVersion "1.1.0"
#endif

#ifndef SourceExecutable
  #define SourceExecutable "..\build\windows-msvc\Release\Quickdraw.exe"
#endif

#ifndef OutputDirectory
  #define OutputDirectory "..\out"
#endif

#define AppName "Quickdraw"
#define AppPublisher "Quickdraw"
#define AppUrl "https://github.com/thatcatfromspace/quickdraw"
#define AppMutexName "Local\Quickdraw.SingleInstance.A69E7DDA-E892-4798-93E1-921CC8F2DCFA"
#define StartupKey "Software\Microsoft\Windows\CurrentVersion\Run"
#define StartupValueName "Quickdraw"

[Setup]
AppId={{BDDCF2E2-54A9-442F-A521-912FA8BFA5ED}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases/latest
AppCopyright=Copyright (c) 2026 thatcatfromspace
DefaultDirName={localappdata}\Programs\Quickdraw
DefaultGroupName=Quickdraw
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
SetupIconFile=..\resources\quickdraw.ico
UninstallDisplayIcon={app}\Quickdraw.exe
UninstallDisplayName=Quickdraw
LicenseFile=..\LICENSE
OutputDir={#OutputDirectory}
OutputBaseFilename=Quickdraw-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
DisableWelcomePage=no
AppMutex={#AppMutexName}
CloseApplications=no
RestartApplications=no
RestartIfNeededByRun=no
UsePreviousAppDir=yes
UsePreviousGroup=yes
UsePreviousTasks=yes
SetupLogging=yes
ChangesAssociations=no
ChangesEnvironment=no
ASLRCompatible=yes
DEPCompatible=yes
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription=Quickdraw Setup
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

[Tasks]
Name: "startup"; Description: "Start Quickdraw when I sign in"; GroupDescription: "Additional tasks:"; Flags: unchecked
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional tasks:"; Flags: unchecked

[Files]
Source: "{#SourceExecutable}"; DestDir: "{app}"; DestName: "Quickdraw.exe"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Quickdraw"; Filename: "{app}\Quickdraw.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Quickdraw.exe"
Name: "{autodesktop}\Quickdraw"; Filename: "{app}\Quickdraw.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Quickdraw.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Quickdraw.exe"; Description: "Launch Quickdraw"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[Code]
function HasPrefix(const Value, Prefix: String): Boolean;
begin
  Result := CompareText(Copy(Value, 1, Length(Prefix)), Prefix) = 0;
end;

function HasExplicitTaskSelection(): Boolean;
var
  Index: Integer;
  Parameter: String;
begin
  Result := False;
  for Index := 1 to ParamCount do
  begin
    Parameter := ParamStr(Index);
    if HasPrefix(Parameter, '/TASKS=') or
       HasPrefix(Parameter, '/MERGETASKS=') then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function QueryStartupEntry(var Value: String): Boolean;
begin
  Result := RegQueryStringValue(
    HKCU,
    '{#StartupKey}',
    '{#StartupValueName}',
    Value);
end;

procedure InitializeWizard();
var
  StartupValue: String;
begin
  if not HasExplicitTaskSelection() then
  begin
    if QueryStartupEntry(StartupValue) then
      WizardSelectTasks('startup')
    else
      WizardSelectTasks('!startup');
  end;
end;

procedure SynchronizeStartupTask();
var
  Command: String;
begin
  if WizardIsTaskSelected('startup') then
  begin
    Command := '"' + ExpandConstant('{app}\Quickdraw.exe') + '"';
    if not RegWriteStringValue(
      HKCU,
      '{#StartupKey}',
      '{#StartupValueName}',
      Command) then
      RaiseException('Quickdraw could not enable launch at sign-in.');
  end
  else
    RegDeleteValue(HKCU, '{#StartupKey}', '{#StartupValueName}');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    SynchronizeStartupTask();
end;

function StartupEntryTargetsInstalledApp(): Boolean;
var
  StartupValue: String;
  InstalledPath: String;
begin
  Result := False;
  if not QueryStartupEntry(StartupValue) then
    Exit;

  InstalledPath := ExpandConstant('{app}\Quickdraw.exe');
  Log(Format(
    'Quickdraw startup ownership check: value="%s" installed="%s"', [StartupValue, InstalledPath]));
  Result :=
    (CompareText(Trim(StartupValue), InstalledPath) = 0) or
    (CompareText(Trim(StartupValue), '"' + InstalledPath + '"') = 0);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if (CurUninstallStep = usUninstall) and
     StartupEntryTargetsInstalledApp() then
  begin
    if RegDeleteValue(HKCU, '{#StartupKey}', '{#StartupValueName}') then
      Log('Removed installer-owned Quickdraw startup entry.')
    else
      Log('Could not remove installer-owned Quickdraw startup entry.');
  end;
end;
