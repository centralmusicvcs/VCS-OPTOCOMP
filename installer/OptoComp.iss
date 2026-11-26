[Setup]
AppName=OptoComp
AppVersion=0.1.0
AppPublisher=YourName
DefaultDirName={autopf}\OptoComp
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=OptoComp-Windows-Setup
Compression=lzma2
SolidCompression=yes

[Files]
Source: "..\build\OptoComp_artefacts\Release\VST3\OptoComp.vst3\*"; \
    DestDir: "{commoncf64}\VST3\OptoComp.vst3"; \
    Flags: recursesubdirs ignoreversion
