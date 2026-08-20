; CH375USB 0.4.11 - ch375_defs.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: CH375 command, status, USB PID and protocol constants.

%define CMD_CHECK_EXIST     006h
%define CMD_AUX_0A          00Ah
%define CMD_SET_RETRY       00Bh
%define CMD_SET_USB_ADDR    013h
%define CMD_SET_USB_MODE    015h
%define CMD_ABORT_NAK       017h
%define CMD_SET_ENDP6       01Ch
%define CMD_SET_ENDP7       01Dh
%define CMD_GET_STATUS      022h
%define CMD_RD_USB_DATA     028h
%define CMD_WR_USB_DATA7    02Bh
%define CMD_CLR_STALL       041h
%define CMD_SET_ADDRESS     045h
%define CMD_GET_DESCR       046h
%define CMD_SET_CONFIG      049h
%define CMD_ISSUE_TOKEN     04Fh


%define CMD_DISK_INIT       051h
%define CMD_DISK_SIZE       053h
%define CMD_DISK_READ       054h
%define CMD_DISK_RD_GO      055h
%define CMD_DISK_WRITE      056h
%define CMD_DISK_WR_GO      057h
%define CMD_DISK_INQUIRY    058h
%define CMD_DISK_READY      059h
%define CMD_DISK_SENSE      05Ah

%define CMD_RET_SUCCESS     051h
%define CMD_RET_ABORT       05Fh

%define USB_INT_SUCCESS     014h
%define USB_INT_CONNECT     015h
%define USB_INT_DISCONNECT  016h
%define USB_INT_BUF_OVER    017h
%define USB_INT_DISK_READ   01Dh
%define USB_INT_DISK_WRITE  01Eh
%define USB_INT_DISK_ERR    01Fh

%define PID_IN              009h
%define PID_OUT             001h
%define PID_SETUP           00Dh
%define PID_PRE             00Ch


%define TOG_DATA0           080h
%define TOG_DATA1           0C0h
