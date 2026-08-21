@ECHO OFF
REM CH375USB 0.5.0 unified SYS build
REM Author: Davide "gat" - https://github.com/davidegat
REM SPDX-License-Identifier: GPL-3.0-or-later

NASM -f bin CH375USB.ASM -o CH375USB.SYS
IF ERRORLEVEL 1 GOTO ERR
ECHO Built unified CH375USB.SYS
GOTO END
:ERR
ECHO Build failed.
:END
