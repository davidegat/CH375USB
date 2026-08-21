; CH375USB 0.4.13 - ch375_defs.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: CH375 command, status, USB PID and protocol constants.

%define CMD_SET_USB_SPEED   004h
%define CMD_CHECK_EXIST     006h
%define CMD_GET_DEV_RATE    00Ah
%define CMD_AUX_0A          CMD_GET_DEV_RATE     ; legacy alias used by old probe code
%define CMD_SET_RETRY       00Bh
%define CMD_SET_USB_ADDR    013h
%define CMD_SET_USB_MODE    015h
%define CMD_ABORT_NAK       017h
%define CMD_SET_ENDP6       01Ch
%define CMD_SET_ENDP7       01Dh
%define CMD_GET_STATUS      022h
%define CMD_RD_USB_DATA0    027h
%define CMD_RD_USB_DATA     028h
%define CMD_WR_USB_DATA7    02Bh
%define CMD_CLR_STALL       041h
%define CMD_SET_ADDRESS     045h
%define CMD_GET_DESCR       046h
%define CMD_SET_CONFIG      049h
%define CMD_ISSUE_TKN_X     04Eh
%define CMD_ISSUE_TOKEN     04Fh


%define CMD_DISK_INIT       051h
%define CMD_DISK_RESET      052h
%define CMD_DISK_SIZE       053h
%define CMD_DISK_READ       054h
%define CMD_DISK_RD_GO      055h
%define CMD_DISK_WRITE      056h
%define CMD_DISK_WR_GO      057h
%define CMD_DISK_INQUIRY    058h
%define CMD_DISK_READY      059h
%define CMD_DISK_SENSE      05Ah
%define CMD_DISK_MAX_LUN    05Dh

%define CMD_RET_SUCCESS     051h
%define CMD_RET_ABORT       05Fh

%define USB_INT_SUCCESS     014h
%define USB_INT_CONNECT     015h
%define USB_INT_DISCONNECT  016h
%define USB_INT_BUF_OVER    017h
%define USB_INT_DISK_READ   01Dh
%define USB_INT_DISK_WRITE  01Eh
%define USB_INT_DISK_ERR    01Fh

; CH375 Datasheet II host-transaction failure decoding.
; Bit 4 is the IN synchronization flag, so mask it when comparing returned PID.
%define USB_ERR_PID_MASK     02Fh
%define USB_ERR_NAK          02Ah
%define USB_ERR_STALL        02Eh

%define PID_IN              009h
%define PID_OUT             001h
%define PID_SETUP           00Dh
%define PID_PRE             00Ch


%define TOG_DATA0           080h
%define TOG_DATA1           0C0h

%define USB_SPEED_FULL       000h
%define USB_SPEED_LOW        002h
%define CH375_RETRY_MAGIC    025h
%define CH375_RETRY_BOUNDED  005h   ; NAK returned immediately, 5 retries after timeout
%define CH375_RETRY_FAST     000h   ; NAK returned immediately, no timeout retry
