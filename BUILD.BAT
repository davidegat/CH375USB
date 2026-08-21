@ECHO OFF
REM CH375USB 0.5.0 native DOS build
REM Builds BOTH CH375USB.SYS and CH375MOU.DRV.
REM Author: Davide "gat" - https://github.com/davidegat
REM SPDX-License-Identifier: GPL-3.0-or-later
REM
REM Required DOS-host tools and fixed install paths:
REM   NASM       C:\NASM\NASM.EXE
REM   OpenWatcom C:\OPENWAT\BINW\WASM.EXE
REM              C:\OPENWAT\BINW\WLINK.EXE
REM
REM Open Watcom V2 DOS/Win16 installer:
REM   https://github.com/open-watcom/open-watcom-v2/releases
REM NASM native-DOS builds (recommended 2.16.03):
REM   https://www.nasm.us/pub/nasm/releasebuilds/2.16.03/dos/

SET NASMROOT=C:\NASM
SET OWROOT=C:\OPENWAT
SET NASM=%NASMROOT%\NASM.EXE
SET WASM=%OWROOT%\BINW\WASM.EXE
SET WLINK=%OWROOT%\BINW\WLINK.EXE

ECHO.
ECHO ============================================================
ECHO  CH375USB / CH375MOU DOS BUILD
ECHO ============================================================
ECHO.
ECHO NASM  : %NASM%
ECHO WASM  : %WASM%
ECHO WLINK : %WLINK%
ECHO.

IF NOT EXIST %NASM% GOTO NO_NASM
IF NOT EXIST %WASM% GOTO NO_WATCOM
IF NOT EXIST %WLINK% GOTO NO_WATCOM

REM Open Watcom runtime/search environment. BINW is the DOS-host tool dir.
SET WATCOM=%OWROOT%
SET EDPATH=%OWROOT%\EDDAT
SET INCLUDE=%OWROOT%\H
SET PATH=%NASMROOT%;%OWROOT%\BINW;%PATH%
IF EXIST %OWROOT%\BINW\WLINK.LNK SET WLINK_LNK=%OWROOT%\BINW\WLINK.LNK

REM Remove stale local build products first.
IF EXIST CH375USB.SYS DEL CH375USB.SYS
IF EXIST CH375W.SYS DEL CH375W.SYS
IF EXIST CH375MOU.DRV DEL CH375MOU.DRV
IF EXIST CH375MOU.MAP DEL CH375MOU.MAP
IF EXIST CH375MOU.OBJ DEL CH375MOU.OBJ
IF EXIST CH375MOU.LOG DEL CH375MOU.LOG

ECHO [1/3] Building CH375USB.SYS with NASM...
%NASM% -f bin CH375USB.ASM -o CH375USB.SYS
IF ERRORLEVEL 1 GOTO SYS_ERROR
IF NOT EXIST CH375USB.SYS GOTO SYS_ERROR

ECHO [2/3] Assembling CH375MOU.OBJ with Open Watcom WASM...
REM Keep these options in sync with build.sh:
REM   -q       quiet
REM   -2       80286 instruction mode
REM   -fo=...  OMF object output
%WASM% -q -2 -fo=CH375MOU.OBJ CH375MOU.ASM
IF ERRORLEVEL 1 GOTO ASM_ERROR
IF NOT EXIST CH375MOU.OBJ GOTO ASM_ERROR

ECHO [3/3] Linking CH375MOU.DRV as a Win16 NE DLL/driver...
REM CH375MOU.LNK supplies:
REM   FORMAT WINDOWS DLL INITGLOBAL
REM   OPTION NODEFAULTLIBS
REM   OPTION START=Initialize
REM   OPTION ONEAUTODATA
REM   fixed code/data segments and the mouse.drv exports.
REM WLINK.LNK must be reachable through BINW/PATH (and WLINK_LNK above)
REM so Open Watcom's Windows system definitions are available.
%WLINK% @CH375MOU.LNK > CH375MOU.LOG
IF ERRORLEVEL 1 GOTO LINK_ERROR
IF NOT EXIST CH375MOU.DRV GOTO LINK_ERROR

REM Linux build.sh refuses linker warnings because WLINK can sometimes emit
REM an output file even after a bad system/link configuration. Mirror that
REM safety check under DOS as far as COMMAND.COM allows.
FIND "Warning!" CH375MOU.LOG > NUL
IF ERRORLEVEL 1 GOTO LINK_OK
GOTO LINK_WARNING

:LINK_OK
IF EXIST CH375MOU.LOG TYPE CH375MOU.LOG
IF EXIST CH375MOU.LOG DEL CH375MOU.LOG
IF EXIST CH375MOU.OBJ DEL CH375MOU.OBJ
ECHO.
ECHO ============================================================
ECHO  BUILD SUCCESSFUL
ECHO ============================================================
ECHO Built:
ECHO   CH375USB.SYS
ECHO   CH375MOU.DRV
ECHO   CH375MOU.MAP
ECHO.
GOTO END

:NO_NASM
ECHO ERROR: NASM was not found at C:\NASM\NASM.EXE
ECHO Install/extract the native DOS NASM package into C:\NASM.
GOTO FAIL

:NO_WATCOM
ECHO ERROR: Open Watcom DOS tools were not found under C:\OPENWAT\BINW.
ECHO Install Open Watcom V2 C/C++ for DOS/Win16 into C:\OPENWAT.
GOTO FAIL

:SYS_ERROR
ECHO ERROR: NASM failed while building CH375USB.SYS.
GOTO FAIL

:ASM_ERROR
ECHO ERROR: WASM failed while assembling CH375MOU.ASM.
GOTO FAIL

:LINK_WARNING
ECHO.
ECHO ERROR: WLINK emitted one or more warnings.
IF EXIST CH375MOU.LOG TYPE CH375MOU.LOG
ECHO Refusing to accept CH375MOU.DRV as a clean build.
GOTO FAIL

:LINK_ERROR
ECHO.
ECHO ERROR: WLINK failed while building CH375MOU.DRV.
IF EXIST CH375MOU.LOG TYPE CH375MOU.LOG
GOTO FAIL

:FAIL
IF EXIST CH375MOU.OBJ DEL CH375MOU.OBJ
ECHO.
ECHO Build failed.
GOTO END

:END
