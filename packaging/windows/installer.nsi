; UTF-8 NSIS 3 Unicode installer; compiled by scripts/build_windows_installer.py.
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!define APP "Flapped Ear Telemetry"
; Keep the old directory for existing-candidate detection and uninstall/reinstall.
!define INSTALL_DIRECTORY "FlappedEar Telemetry"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\FlappedEarTelemetry"
Name "${APP} (internal candidate)"
OutFile "${OUTPUT_FILE}"
RequestExecutionLevel user
SetCompressor /SOLID zlib
Var RemovalFailed
ShowInstDetails show
ShowUninstDetails show
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetShellVarContext current
  SetRegView 64
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "This candidate requires 64-bit Windows." /SD IDOK
    SetErrorLevel 2
    Abort
  ${EndIf}
  ; Fixed per-user location; deliberately ignore /D and never request elevation.
  StrCpy $INSTDIR "$LOCALAPPDATA\Programs\${INSTALL_DIRECTORY}"
  IfFileExists "$INSTDIR\Uninstall.exe" 0 ready
    MessageBox MB_OK|MB_ICONSTOP "Close ${APP} and uninstall the existing candidate from Windows Settings before installing this one. Projects and settings are preserved." /SD IDOK
    SetErrorLevel 2
    Abort
  ready:
FunctionEnd

Section "Application"
  SetShellVarContext current
  SetRegView 64
  SetOverwrite on
  !include "${PAYLOAD_INCLUDE}"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\${APP}"
  SetOutPath "$INSTDIR\bin"
  CreateShortcut "$SMPROGRAMS\${APP}\${APP}.lnk" "$INSTDIR\bin\${APP}.exe"
  CreateShortcut "$SMPROGRAMS\${APP}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${APP} (internal candidate)"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION} (${COMMIT})"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "BuildCommit" "${COMMIT}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\bin\${APP}.exe"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Function un.onInit
  SetShellVarContext current
  SetRegView 64
  ReadRegStr $0 HKCU "${UNINSTALL_KEY}" "BuildCommit"
  ${If} $0 != "${COMMIT}"
    MessageBox MB_OK|MB_ICONSTOP "This uninstaller does not match the installed candidate." /SD IDOK
    SetErrorLevel 2
    Abort
  ${EndIf}
  ; Do not derive the deletion root from where somebody moved Uninstall.exe.
  StrCpy $INSTDIR "$LOCALAPPDATA\Programs\${INSTALL_DIRECTORY}"
FunctionEnd

Section "Uninstall"
  SetShellVarContext current
  SetRegView 64
  ; Exact build-owned files only; never recursively remove the installation root.
  StrCpy $RemovalFailed 0
  !include "${REMOVE_INCLUDE}"
  StrCmp $RemovalFailed 0 removed
    MessageBox MB_OK|MB_ICONSTOP "Close ${APP} and run uninstall again." /SD IDOK
    SetErrorLevel 2
    Abort
  removed:
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$SMPROGRAMS\${APP}\${APP}.lnk"
  Delete "$SMPROGRAMS\${APP}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${APP}"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  RMDir "$INSTDIR"
SectionEnd
