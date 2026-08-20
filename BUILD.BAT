@ECHO OFF
REM CH375USB build script for DOS/Windows.
REM Author: Davide "gat" - https://github.com/davidegat
REM SPDX-License-Identifier: GPL-3.0-or-later

nasm -f bin CH375USB.ASM -o CH375USB.SYS
IF ERRORLEVEL 1 GOTO ERROR
ECHO Built CH375USB.SYS
GOTO END

:ERROR
ECHO Build failed.

:END
