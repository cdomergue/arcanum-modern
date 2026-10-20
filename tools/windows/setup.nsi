Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

Name "Arcanum Renaissance"
OutFile "ArcanumModern-windows-x64-setup.exe"
RequestExecutionLevel user
ManifestDPIAware true
ManifestSupportedOS Win10
SetCompressor /SOLID lzma
InstallDir "$LOCALAPPDATA\Arcanum"
InstallDirRegKey HKCU "Software\ArcanumModern" "InstallDir"
ShowInstDetails show
ShowUninstDetails show
Var GameIcon

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "Install Arcanum Renaissance into your existing Arcanum 1.0.7.4 game folder.$\r$\n$\r$\nThe setup includes the engine and interface resources. You must supply your own game installation.$\r$\n$\r$\nClose the game before continuing. Your original executable, saved games and preferences are kept."
!insertmacro MUI_PAGE_WELCOME
!define MUI_DIRECTORYPAGE_TEXT_TOP "Choose your existing Arcanum 1.0.7.4 game folder (the folder containing arcanum1.dat)."
!define MUI_DIRECTORYPAGE_VERIFYONLEAVE
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE CheckGameDirectory
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TEXT "Arcanum Renaissance is installed. Use the desktop or Start menu shortcut to play."
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Arcanum Renaissance requires 64-bit Windows 10 or later." /SD IDOK
    SetErrorLevel 2
    Quit
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "Arcanum Renaissance requires Windows 10 or later." /SD IDOK
    SetErrorLevel 2
    Quit
  ${EndIf}
  SetShellVarContext current
FunctionEnd

Function CheckGameDirectory
  IfFileExists "$INSTDIR\arcanum1.dat" 0 missing
  IfFileExists "$INSTDIR\arcanum2.dat" 0 missing
  IfFileExists "$INSTDIR\arcanum3.dat" 0 missing
  IfFileExists "$INSTDIR\tig.dat" 0 missing
  IfFileExists "$INSTDIR\modules\Arcanum.dat" 0 missing
  ClearErrors
  GetTempFileName $0 "$INSTDIR"
  IfErrors unwritable
  Delete "$0"
  Return
missing:
  MessageBox MB_OK|MB_ICONSTOP "Select an existing Arcanum game folder containing arcanum1.dat, arcanum2.dat, arcanum3.dat, tig.dat and modules\Arcanum.dat." /SD IDOK
  SetErrorLevel 2
  Abort
unwritable:
  MessageBox MB_OK|MB_ICONSTOP "This game folder is not writable. Copy your Arcanum installation to a folder you can write to, then select that copy." /SD IDOK
  SetErrorLevel 2
  Abort
FunctionEnd

Section "Arcanum Renaissance"
  Call CheckGameDirectory
  SetOverwrite on
  ClearErrors
  !include "install.nsh"
  IfErrors failed
  WriteUninstaller "$INSTDIR\Uninstall Arcanum Modern.exe"
  IfErrors failed
  SetOutPath "$INSTDIR"
  StrCpy $GameIcon "$INSTDIR\arcanum-modern.exe"
  ${If} ${FileExists} "$INSTDIR\Arcanum.exe"
    StrCpy $GameIcon "$INSTDIR\Arcanum.exe"
  ${EndIf}
  ReadRegStr $0 HKCU "Software\ArcanumModern" "InstallDir"
  ${If} $0 == $INSTDIR
    Delete "$DESKTOP\Arcanum Modern.lnk"
    Delete "$SMPROGRAMS\Arcanum Modern\Arcanum Modern.lnk"
    Delete "$SMPROGRAMS\Arcanum Modern\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Arcanum Modern"
  ${EndIf}
  ClearErrors
  CreateShortcut "$DESKTOP\Arcanum Renaissance.lnk" "$INSTDIR\arcanum-modern.exe" "-geometry=1280x720" "$GameIcon" 0
  CreateDirectory "$SMPROGRAMS\Arcanum Renaissance"
  CreateShortcut "$SMPROGRAMS\Arcanum Renaissance\Arcanum Renaissance.lnk" "$INSTDIR\arcanum-modern.exe" "-geometry=1280x720" "$GameIcon" 0
  CreateShortcut "$SMPROGRAMS\Arcanum Renaissance\Uninstall.lnk" "$INSTDIR\Uninstall Arcanum Modern.exe"
  WriteRegStr HKCU "Software\ArcanumModern" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ArcanumModern" "DisplayName" "Arcanum Renaissance"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ArcanumModern" "UninstallString" '$\"$INSTDIR\Uninstall Arcanum Modern.exe$\"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ArcanumModern" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ArcanumModern" "DisplayIcon" "$GameIcon"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ArcanumModern" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ArcanumModern" "NoRepair" 1
  IfErrors failed
  SetErrorLevel 0
  Goto done
failed:
  MessageBox MB_OK|MB_ICONSTOP "Installation could not finish. Close Arcanum Renaissance, check that the game folder is writable, then run setup again." /SD IDOK
  SetErrorLevel 2
  Abort
done:
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  !include "uninstall.nsh"
  Delete "$INSTDIR\Uninstall Arcanum Modern.exe"
  ReadRegStr $0 HKCU "Software\ArcanumModern" "InstallDir"
  ${If} $0 == $INSTDIR
    Delete "$DESKTOP\Arcanum Renaissance.lnk"
    Delete "$SMPROGRAMS\Arcanum Renaissance\Arcanum Renaissance.lnk"
    Delete "$SMPROGRAMS\Arcanum Renaissance\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Arcanum Renaissance"
    DeleteRegKey HKCU "Software\ArcanumModern"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ArcanumModern"
  ${EndIf}
SectionEnd
