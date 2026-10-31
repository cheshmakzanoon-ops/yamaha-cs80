; Inno Setup script for the Windows VST3 and Standalone. Built by
; .github/workflows/release.yml from an existing Release build:
;   iscc /DAppVersion=1.0.0 /DBuildDir=<build>\CS80Reference_artefacts\Release /DOutDir=<out> cs80-reference.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\CS80Reference_artefacts\Release"
#endif
#ifndef JuceLicense
  #define JuceLicense "..\..\build\_deps\juce-src\LICENSE.md"
#endif
#ifndef OutDir
  #define OutDir "..\..\packages"
#endif

[Setup]
AppId={{ADABA3BA-3323-4A5C-938E-6DFA14FBD310}
AppName=CS-80 Reference
AppVersion={#AppVersion}
AppPublisher=Reference Instruments
DefaultDirName={autopf}\CS-80 Reference
DefaultGroupName=CS-80 Reference
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutDir}
OutputBaseFilename=CS-80-Reference-{#AppVersion}-Windows-x64-setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
UninstallDisplayName=CS-80 Reference
LicenseFile=..\..\LICENSE

[Types]
Name: "full"; Description: "VST3 plug-in and Standalone"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
Source: "{#BuildDir}\VST3\CS-80 Reference.vst3\*"; DestDir: "{commoncf64}\VST3\CS-80 Reference.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildDir}\Standalone\CS-80 Reference.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}\licenses"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "{#JuceLicense}"; DestDir: "{app}\licenses"; DestName: "JUCE-LICENSE.md"; Flags: ignoreversion

[Icons]
Name: "{group}\CS-80 Reference"; Filename: "{app}\CS-80 Reference.exe"; Components: standalone
