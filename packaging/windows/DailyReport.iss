#define AppName "DailyReport"
#define AppPublisher "DailyReport"
#define AppExeName "DailyReport.exe"

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\DailyReport-1.0.0-windows-x64"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif
#ifndef SetupIcon
  #define SetupIcon "dailyreport.ico"
#endif

[Setup]
AppId={{952998A6-DD39-4653-971C-F18DB0CFF239}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\DailyReport
DefaultGroupName=DailyReport
AllowNoIcons=yes
DisableProgramGroupPage=auto
DisableDirPage=no
UsePreviousAppDir=yes
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=DailyReport-Setup-{#AppVersion}
SetupIconFile={#SetupIcon}
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}
AppMutex=DailyReport_SingleInstance
CloseApplications=yes
RestartApplications=no
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
WizardResizable=no
SetupLogging=yes
VersionInfoVersion={#AppVersion}.0
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoDescription=DailyReport Installer

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\DailyReport"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\DailyReport"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

