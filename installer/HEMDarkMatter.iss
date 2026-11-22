; =====================================================================================================
;  HEM Dark Matter Series - Windows installer (Inno Setup 6)
;  Built by CI:  ISCC /DAppVersion=x.y.z /DBundle=<HEM Dark Matter.vst3> /DBinDir=<build dir>
;                     /DVst3License=<VST3 SDK LICENSE.txt> HEMDarkMatter.iss
;  Installs the plug-in bundle (all 13 plug-ins) into Common Files\VST3, plus the self-tests and the
;  how-to-test guide. AppId is permanent: never change it, or upgrades will install side by side.
;  User presets live in %APPDATA%\HEM\Dark Matter: this installer never touches that folder, so saved
;  presets survive upgrades and uninstall.
; =====================================================================================================
#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif

[Setup]
AppId={{A478C897-F0C5-401B-A21C-FABA6BD3B9A0}
AppName=HEM Dark Matter Series
AppVersion={#AppVersion}
AppVerName=HEM Dark Matter Series {#AppVersion}
AppPublisher=Hybrid Elite Mafia
AppPublisherURL=https://github.com/The-Hybrid-Elite-Mafia/HEM-Dark-Matter-Series
DefaultDirName={autopf}\Hybrid Elite Mafia\HEM Dark Matter
DefaultGroupName=HEM Dark Matter
DisableProgramGroupPage=yes
OutputBaseFilename=HEM-Dark-Matter-Setup-{#AppVersion}
OutputDir=output
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
PrivilegesRequired=admin
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=HEM Dark Matter Series
LicenseFile=..\LICENSE

[Files]
Source: "{#Bundle}\*";                         DestDir: "{commoncf64}\VST3\HEM Dark Matter.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BinDir}\Release\dm_tests.exe";        DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#BinDir}\plugin\Release\dm_vst3_hosttest.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "HOW-TO-TEST.txt";                      DestDir: "{app}"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES.txt";              DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE";                           DestDir: "{app}\licenses"; DestName: "HEM-Dark-Matter-GPLv3.txt"; Flags: ignoreversion
Source: "{#Vst3License}";                       DestDir: "{app}\licenses"; DestName: "Steinberg-VST3-SDK-LICENSE.txt"; Flags: ignoreversion
Source: "..\assets\fonts\OFL.txt";              DestDir: "{app}\licenses"; DestName: "Barlow-OFL.txt"; Flags: ignoreversion
Source: "..\third_party\hc_ui\external\stb\LICENSE"; DestDir: "{app}\licenses"; DestName: "stb-LICENSE.txt"; Flags: ignoreversion

[Dirs]
Name: "{userappdata}\HEM\Dark Matter"; Flags: uninsneveruninstall

[Icons]
; cmd /k keeps the window open so the results can be read.
Name: "{group}\Self-Test 1 - Engines";            Filename: "{cmd}"; Parameters: "/k """"{app}\bin\dm_tests.exe"""""; WorkingDir: "{app}\bin"
Name: "{group}\Self-Test 2 - Installed Plug-ins";  Filename: "{cmd}"; Parameters: "/k """"{app}\bin\dm_vst3_hosttest.exe"" ""{commoncf64}\VST3\HEM Dark Matter.vst3"""""; WorkingDir: "{app}\bin"
Name: "{group}\My Presets Folder";                Filename: "{userappdata}\HEM\Dark Matter"
Name: "{group}\How to test";                      Filename: "{app}\HOW-TO-TEST.txt"
Name: "{group}\Uninstall HEM Dark Matter";        Filename: "{uninstallexe}"

[Run]
Filename: "{app}\HOW-TO-TEST.txt"; Description: "Show how to test"; Flags: postinstall shellexec skipifsilent
