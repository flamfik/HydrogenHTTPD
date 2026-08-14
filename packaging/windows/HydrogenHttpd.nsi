!include "MUI2.nsh"
!include "x64.nsh"

!ifndef STAGE_DIR
  !error "STAGE_DIR must point to the CMake install staging directory."
!endif

!ifndef OUTPUT_DIR
  !define OUTPUT_DIR "."
!endif

!define PRODUCT_NAME "HydrogenHttpd"
!define PRODUCT_VERSION "1.9.0"
!define COMPANY_NAME "HydrogenHttpd Project"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\HydrogenHttpd"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${OUTPUT_DIR}\HydrogenHttpd-${PRODUCT_VERSION}-Windows-x64-Setup.exe"
InstallDir "$PROGRAMFILES64\HydrogenHttpd"
InstallDirRegKey HKLM "${UNINSTALL_KEY}" "InstallLocation"
RequestExecutionLevel admin
Unicode true
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE_DIR}\share\doc\hydrogenhttpd\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Polish"

Section "HydrogenHttpd Server" SecMain
    SetShellVarContext all

    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "HydrogenHttpd v1.9.0 requires 64-bit Windows."
        Abort
    ${EndIf}

    SetOutPath "$INSTDIR"
    File /r "${STAGE_DIR}\*.*"

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "HydrogenHttpd"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "${COMPANY_NAME}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1

    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\share\hydrogenhttpd\windows\install-service.ps1" -InstallRoot "$INSTDIR"'
    Pop $0
    ${If} $0 != 0
        MessageBox MB_ICONSTOP "Service installation failed with exit code $0. Review the installer log."
        Abort
    ${EndIf}
SectionEnd

Section "Uninstall"
    SetShellVarContext all

    nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$INSTDIR\share\hydrogenhttpd\windows\uninstall-service.ps1" -InstallRoot "$INSTDIR"'
    Pop $0

    DeleteRegKey HKLM "${UNINSTALL_KEY}"
    RMDir /r "$INSTDIR"

    MessageBox MB_ICONINFORMATION "HydrogenHttpd was removed. Runtime data in $PROGRAMDATA\HydrogenHttpd was preserved."
SectionEnd
