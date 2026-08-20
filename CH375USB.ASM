; CH375USB.SYS 0.4.11 - DOS USB host driver for CH375 ISA at I/O 0260h
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
;
; Provides DOS USB mass-storage, HID keyboard and HID mouse support with hotplug.
; Windows 95 transition handling suspends the DOS HID bridge while Windows runs.
; Target: 16-bit real mode on 386+ PCs with CH375 ISA parallel I/O at 0260h/0261h.

BITS 16
CPU 386
ORG 0

%define DRIVER_VERSION      '0.4.11'
%define IO_BASE             0260h
%define DATA_PORT           IO_BASE
%define CMD_PORT            (IO_BASE+1)
%define ENABLE_WRITES       1
%define SYNC_AFTER_WRITE    1
%define MAX_HUB_PORTS       4
%define MAX_DEVICES         5
%define DEV_SIZE            24


%include "ch375_defs.inc"


%define CAP_HUB             01h
%define CAP_MSC             02h
%define CAP_KBD             04h
%define CAP_MOUSE           08h

%define STORAGE_NONE        0
%define STORAGE_NATIVE      1
%define STORAGE_BOT         2


%define D_USED              0
%define D_ADDR              1
%define D_PARENT            2
%define D_PORT              3
%define D_FLAGS             4
%define D_CAPS              5
%define D_CFG               6
%define D_EP0               7
%define D_HUBPORTS          8
%define D_KBD_EP            9
%define D_KBD_TOG           10
%define D_MOUSE_EP          11
%define D_MOUSE_TOG         12
%define D_MSC_IN            13
%define D_MSC_OUT           14
%define D_MSC_ITF           15
%define D_MSC_IN_TOG        16
%define D_MSC_OUT_TOG       17
%define D_MSC_LUN           18
%define D_KBD_ITF           19
%define D_MOUSE_ITF         20
%define D_RESERVED2         21
%define D_RESERVED3         22
%define D_RESERVED4         23

%define FLAG_LOW_SPEED      01h


%define ST_DONE             0100h
%define ST_ERROR            8000h
%define ERR_WRITE_PROTECT   0
%define ERR_UNKNOWN_UNIT    1
%define ERR_DRIVE_NOT_READY 2
%define ERR_UNKNOWN_COMMAND 3
%define ERR_CRC             4
%define ERR_BAD_REQUEST     5
%define ERR_SEEK            6
%define ERR_UNKNOWN_MEDIA   7
%define ERR_SECTOR_NOT_FOUND 8
%define ERR_WRITE_FAULT     10
%define ERR_READ_FAULT      11
%define ERR_GENERAL         12


; DOS reserves one removable block-device letter even when no storage media is present.
device_header:
    dd 0FFFFFFFFh
    dw 0000h
    dw strategy
    dw interrupt
    db 1,0,0,0,0,0,0,0

request_off     dw 0
request_seg     dw 0


; Saved interrupt vectors and runtime flags for the DOS HID bridge.
; The INT 33h shim is installed only when a conventional DOS mouse driver exists.
base_int33_off  dw 0
base_int33_seg  dw 0
old_int33_off   dw 0
old_int33_seg   dw 0
mouse_int33_hooked db 0
old_int1c_off   dw 0
old_int1c_seg   dw 0
old_int28_off   dw 0
old_int28_seg   dw 0
old_int2f_off   dw 0
old_int2f_seg   dw 0
windows_active  db 0


usb_present     db 0
usb_busy        db 0
root_event      db 1
maintenance_due db 1
maintenance_div db 0
hub_scan_div    db 0
current_slot    db 0
current_addr    db 0
parse_itf       db 0
enum_addr       db 0
last_status     db 0
enum_fail_stage db 0
root_connect_seen db 0
root_link_state db 0
root_retry_ticks db 0
maint_tick       db 0
next_address    db 1


storage_slot    db 0FFh
storage_mode    db STORAGE_NONE
native_last_status db 0
native_probe20     db 0
native_timeout_kind db 0
native_fail_stage db 0
media_ready     db 0
media_changed   db 1
part_lba        dd 0
msc_tag         dd 1
msc_last_lba    dd 0
msc_block_size  dd 512
write_verify    db 0


usb_control_wait db 0
mouse_pending       db 0
mouse_buttons       db 0
mouse_event_pending dw 0
mouse_accum_dx      dw 0
mouse_accum_dy      dw 0
mouse_motion_x      dw 0
mouse_motion_y      dw 0
mouse_press_l       dw 0
mouse_press_r       dw 0
mouse_press_m       dw 0
mouse_release_l     dw 0
mouse_release_r     dw 0
mouse_release_m     dw 0
mouse_press_l_x     dw 0
mouse_press_l_y     dw 0
mouse_press_r_x     dw 0
mouse_press_r_y     dw 0
mouse_press_m_x     dw 0
mouse_press_m_y     dw 0
mouse_release_l_x   dw 0
mouse_release_l_y   dw 0
mouse_release_r_x   dw 0
mouse_release_r_y   dw 0
mouse_release_m_x   dw 0
mouse_release_m_y   dw 0
mouse_x            dw 320
mouse_y            dw 100
mouse_min_x        dw 0
mouse_max_x        dw 639
mouse_min_y        dw 0
mouse_max_y        dw 199
mouse_cb_mask      dw 0
mouse_cb_off       dw 0
mouse_cb_seg       dw 0

usb_speed_mode   db 0

devices:
    times MAX_DEVICES*DEV_SIZE db 0


kbd_old_mod:
    times MAX_DEVICES db 0
kbd_old_keys:
    times MAX_DEVICES*6 db 0


bpb:
    dw 512
    db 1
    dw 1
    db 2
    dw 512
    dw 4096
    db 0F8h
    dw 12
    dw 32
    dw 64
    dd 0
    dd 0
bpb_ptr_array:
    dw bpb,0


setup_buf       times 8 db 0
usb_buf         times 64 db 0
cfg_buf         times 256 db 0
hub_buf         times 16 db 0
sector_buf      times 512 db 0
verify_buf      times 512 db 0
cbw_buf         times 31 db 0
csw_buf         times 13 db 0
sense_buf       times 18 db 0
capacity_buf    times 8 db 0


xfer_slot       db 0
xfer_ep         db 0
xfer_toggle     db 0
xfer_len        dw 0
xfer_done       dw 0
xfer_seg        dw 0
xfer_off        dw 0
ctrl_wlength    dw 0
ctrl_actual     dw 0


hub_slot        db 0FFh
hub_ports       db 0
hub_status      times 4 db 0


msg_banner      db 13,10,'CH375USB 0.4.11 - DOS USB host driver for CH375 ISA',13,10,'$'
msg_desc        db 'Storage / HID keyboard-mouse / hotplug | I/O 0260h',13,10,'$'
msg_author      db 'Author: Davide (gat) | License: GPL-3.0-or-later',13,10,'$'
msg_chipok      db 'Controller: CH375 detected.',13,10,'$'
msg_nochip      db 'Controller: CH375 not detected; driver inactive.',13,10,'$'
msg_hub         db 'Hub: detected (experimental support).',13,10,'$'
msg_nohub       db 'Hub: not detected.',13,10,'$'
msg_storage     db 'Storage: ready (read/write).',13,10,'$'
msg_nostorage   db 'Storage: no media; DOS drive remains reserved.',13,10,'$'
msg_kbd         db 'Keyboard: ready.',13,10,'$'
msg_nokbd       db 'Keyboard: not detected.',13,10,'$'
msg_mouse       db 'Mouse: ready.',13,10,'$'
msg_nomouse     db 'Mouse: not detected.',13,10,'$'
msg_hotplug     db 'Hotplug: enabled.',13,10,'$'
strategy:
    mov [cs:request_off],bx
    mov [cs:request_seg],es
    retf

interrupt:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    push cs
    pop ds
    les bx,[request_off]
    mov al,[es:bx+2]
    cmp al,0
    je .init
    cmp al,1
    je .media
    cmp al,2
    je .bpb
    cmp al,4
    je .read
    cmp al,8
    je .write
    cmp al,9
    je .write
    cmp al,13
    je .ok
    cmp al,14
    je .ok
    mov word [es:bx+3],ST_DONE|ST_ERROR|ERR_UNKNOWN_COMMAND
    jmp .exit
.init:
    call drv_init
    jmp .exit
.media:
    call drv_media_check
    jmp .exit
.bpb:
    call drv_build_bpb
    jmp .exit
.read:
    call drv_read
    jmp .exit
.write:
    call drv_write
    jmp .exit
.ok:
    mov word [es:bx+3],ST_DONE
.exit:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    retf


drv_init:
    mov ax,cs
    mov [bpb_ptr_array+2],ax


    mov byte [es:bx+13],1
    mov word [es:bx+14],resident_end
    mov [es:bx+16],cs
    mov word [es:bx+18],bpb_ptr_array
    mov [es:bx+20],cs

    mov dx,msg_banner
    mov ah,09h
    int 21h
    mov dx,msg_desc
    int 21h
    mov dx,msg_author
    int 21h

    call ch_probe
    jc .nochip
    mov byte [usb_present],1
    mov dx,msg_chipok
    mov ah,09h
    int 21h


    call clear_all_slots
    call native_storage_try
    jnc .boot_native_ok


    cmp byte [root_connect_seen],1
    jne .boot_no_device


    call generic_root_enumerate
    jnc .boot_generic_ok


    mov byte [root_event],0
    mov byte [maintenance_due],0
    mov byte [root_link_state],1
    mov byte [root_retry_ticks],36
    jmp .boot_native_done

.boot_no_device:
    mov byte [root_event],0
    mov byte [maintenance_due],0
    mov byte [root_link_state],0
    mov byte [root_retry_ticks],0
    jmp .boot_native_done

.boot_generic_ok:
    mov byte [root_link_state],1
    mov byte [root_event],0
    mov byte [maintenance_due],0
    jmp .boot_native_done

.boot_native_ok:
    mov byte [root_link_state],1
    mov byte [root_event],0
    mov byte [maintenance_due],0

.boot_native_done:
    call print_boot_summary

    ; Install the lightweight timer/idle/Windows-transition hooks after USB init.
    ; INT 33h itself is hooked later, once a real DOS mouse driver is present.
    cli
    xor ax,ax
    mov es,ax

    mov ax,[es:33h*4]
    mov [base_int33_off],ax
    mov ax,[es:33h*4+2]
    mov [base_int33_seg],ax

    mov ax,[es:1Ch*4]
    mov [old_int1c_off],ax
    mov ax,[es:1Ch*4+2]
    mov [old_int1c_seg],ax
    mov word [es:1Ch*4],timer_1c
    mov [es:1Ch*4+2],cs

    mov ax,[es:28h*4]
    mov [old_int28_off],ax
    mov ax,[es:28h*4+2]
    mov [old_int28_seg],ax
    mov word [es:28h*4],idle_28
    mov [es:28h*4+2],cs

    mov ax,[es:2Fh*4]
    mov [old_int2f_off],ax
    mov ax,[es:2Fh*4+2]
    mov [old_int2f_seg],ax
    mov word [es:2Fh*4],int2f_handler
    mov [es:2Fh*4+2],cs
    sti
    jmp .done

.nochip:
    mov dx,msg_nochip
    mov ah,09h
    int 21h
.done:
    les bx,[request_off]
    mov word [es:bx+3],ST_DONE
    ret

; No USB enumeration is allowed from block-device requests; absent media must return immediately.
drv_media_check:


.ready:
    mov al,[media_changed]
    and al,1
    mov [es:bx+14],al
    mov byte [media_changed],0
    mov word [es:bx+3],ST_DONE
    ret

drv_build_bpb:
    cmp byte [media_ready],1
    jne .nr
.ok:
    mov word [es:bx+18],bpb
    mov [es:bx+20],cs
    mov word [es:bx+3],ST_DONE
    ret
.nr:
    mov word [es:bx+3],ST_DONE|ST_ERROR|ERR_DRIVE_NOT_READY
    ret


drv_read:
    cmp byte [media_ready],1
    jne .nr
.have_media:
    mov byte [usb_busy],1
    mov cx,[es:bx+18]
    jcxz .done_ok


    mov ax,[es:bx+14]
    mov [xfer_off],ax
    mov ax,[es:bx+16]
    mov [xfer_seg],ax


    mov ax,[es:bx+20]
    cmp ax,0FFFFh
    jne .small
    mov ax,[es:bx+26]
    mov dx,[es:bx+28]
    jmp .have_lba
.small:
    xor dx,dx
.have_lba:
    add ax,[part_lba]
    adc dx,[part_lba+2]

.loop:
    push ax
    push dx
    push cx
    call storage_read_sector_far
    pop cx
    pop dx
    pop ax
    jc .fail
    inc ax
    jnz .lba_ok
    inc dx
.lba_ok:
    add word [xfer_off],512
    jnc .ptr_ok
    add word [xfer_seg],1000h
.ptr_ok:
    loop .loop
.done_ok:
    mov byte [usb_busy],0
    les bx,[request_off]
    mov word [es:bx+3],ST_DONE
    ret
.fail:
    mov byte [usb_busy],0
    call storage_fail_soft
    les bx,[request_off]
    mov word [es:bx+3],ST_DONE|ST_ERROR|ERR_READ_FAULT
    ret
.nr:
    mov word [es:bx+3],ST_DONE|ST_ERROR|ERR_DRIVE_NOT_READY
    ret

drv_write:
%if ENABLE_WRITES = 0
    mov word [es:bx+3],ST_DONE|ST_ERROR|ERR_WRITE_PROTECT
    ret
%else
    cmp byte [media_ready],1
    jne .nr
.have_media:
    mov byte [usb_busy],1
    mov byte [write_verify],0
    cmp byte [es:bx+2],9
    jne .wv_set
    mov byte [write_verify],1
.wv_set:
    mov cx,[es:bx+18]
    jcxz .done_ok

    mov ax,[es:bx+14]
    mov [xfer_off],ax
    mov ax,[es:bx+16]
    mov [xfer_seg],ax

    mov ax,[es:bx+20]
    cmp ax,0FFFFh
    jne .small
    mov ax,[es:bx+26]
    mov dx,[es:bx+28]
    jmp .have_lba
.small:
    xor dx,dx
.have_lba:
    add ax,[part_lba]
    adc dx,[part_lba+2]

.loop:
    push ax
    push dx
    push cx
    call storage_write_sector_far
    jc .write_call_fail
    cmp byte [write_verify],1
    jne .write_verified
    call storage_verify_sector_far
    jc .write_call_fail
.write_verified:
    pop cx
    pop dx
    pop ax
    jmp .write_ok
.write_call_fail:
    pop cx
    pop dx
    pop ax
    jmp .fail
.write_ok:
    inc ax
    jnz .lba_ok
    inc dx
.lba_ok:
    add word [xfer_off],512
    jnc .ptr_ok
    add word [xfer_seg],1000h
.ptr_ok:
    loop .loop
%if SYNC_AFTER_WRITE = 1
    call storage_sync_cache


%endif
.done_ok:
    mov byte [usb_busy],0
    les bx,[request_off]
    mov word [es:bx+3],ST_DONE
    ret
.fail:
    mov byte [usb_busy],0
    call storage_fail_soft
    les bx,[request_off]
    mov word [es:bx+3],ST_DONE|ST_ERROR|ERR_WRITE_FAULT
    ret
.nr:
    mov word [es:bx+3],ST_DONE|ST_ERROR|ERR_DRIVE_NOT_READY
    ret
%endif

storage_fail_soft:


    mov byte [maintenance_due],1
    ret

print_boot_summary:
    cmp byte [hub_slot],0FFh
    je .hub_none
    mov dx,msg_hub
    mov ah,09h
    int 21h
    jmp .storage
.hub_none:
    mov dx,msg_nohub
    mov ah,09h
    int 21h

.storage:
    cmp byte [storage_slot],0FFh
    je .storage_none
    mov dx,msg_storage
    mov ah,09h
    int 21h
    jmp .scan_hid
.storage_none:
    mov dx,msg_nostorage
    mov ah,09h
    int 21h

.scan_hid:
    xor si,si
    xor bx,bx
.scan:
    cmp si,MAX_DEVICES
    jae .scanned
    mov ax,si
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_KBD
    jz .no_kbd_here
    mov bh,1
.no_kbd_here:
    test byte [di+D_CAPS],CAP_MOUSE
    jz .no_mouse_here
    mov bl,1
.no_mouse_here:
    inc si
    jmp .scan

.scanned:
    cmp bh,0
    je .kbd_none
    mov dx,msg_kbd
    mov ah,09h
    int 21h
    jmp .mouse
.kbd_none:
    mov dx,msg_nokbd
    mov ah,09h
    int 21h

.mouse:
    cmp bl,0
    je .mouse_none
    mov dx,msg_mouse
    mov ah,09h
    int 21h
    jmp .done
.mouse_none:
    mov dx,msg_nomouse
    mov ah,09h
    int 21h
.done:
    mov dx,msg_hotplug
    mov ah,09h
    int 21h
    ret

timer_1c:
    pushf
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push cs
    pop ds

    cmp byte [windows_active],1
    je .chain
    cmp byte [usb_present],1
    jne .chain
    cmp byte [usb_busy],0
    jne .chain


    cmp byte [root_retry_ticks],0
    je .retry_ok
    dec byte [root_retry_ticks]
.retry_ok:
    inc byte [maint_tick]
    cmp byte [maint_tick],18
    jb .sample_root
    mov byte [maint_tick],0


    cmp byte [storage_slot],0FFh
    jne .schedule_maint
    cmp byte [hub_slot],0FFh
    je .sample_root
.schedule_maint:
    mov byte [maintenance_due],1

.sample_root:

    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jnz .hid
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    cmp al,USB_INT_CONNECT
    je .evt_connect
    cmp al,USB_INT_DISCONNECT
    je .evt_disconnect
    jmp .hid

.evt_connect:
    cmp byte [root_link_state],1
    je .hid
    cmp byte [root_retry_ticks],0
    jne .hid
    mov byte [root_link_state],1
    mov byte [root_event],1
    mov byte [maintenance_due],1
    mov byte [root_retry_ticks],18
    jmp .chain

.evt_disconnect:
    cmp byte [root_link_state],0
    je .hid
    mov byte [root_link_state],0
    mov byte [root_event],1
    mov byte [maintenance_due],1
    mov byte [root_retry_ticks],9
    call mark_all_detached_fast
    jmp .chain

.hid:

    mov byte [usb_busy],1
    xor si,si
.hloop:
    cmp si,MAX_DEVICES
    jae .hdone
    mov ax,si
    call slot_ptr_ax
    cmp byte [di+D_USED],1
    jne .hnext
    test byte [di+D_CAPS],CAP_KBD
    jz .hmouse
    mov ax,si
    call hid_poll_keyboard_slot
.hmouse:
    test byte [di+D_CAPS],CAP_MOUSE
    jz .hnext
    mov ax,si
    call hid_poll_mouse_slot
.hnext:
    inc si
    jmp .hloop
.hdone:
    mov byte [usb_busy],0

    ; Mouse state must advance even while a DOS application never calls INT 28h.
    ; The USB report has already been read; only the software INT 33h bridge runs here.
    cmp byte [mouse_pending],1
    jne .chain
    call mouse_try_hook_int33
    cmp byte [mouse_int33_hooked],1
    jne .chain
    call mouse_apply_pending

.chain:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    jmp far [cs:old_int1c_off]

idle_28:
    pushf
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push cs
    pop ds

    cmp byte [windows_active],1
    je .chain

    ; Hook INT 33h independently of mouse activity. Applications may register
    ; their button callback before the first USB mouse report arrives.
    call mouse_try_hook_int33

.usb:
    cmp byte [usb_present],1
    jne .chain
    cmp byte [usb_busy],0
    jne .chain

    cmp byte [root_event],1
    je .doit
    cmp byte [maintenance_due],1
    jne .chain
.doit:
    sti
    call usb_maintenance
    cli
.chain:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    jmp far [cs:old_int28_off]


; Windows 3.x/95 enhanced mode broadcasts INT 2Fh AX=1605h on entry and
; AX=1606h on exit. Suspend the real-mode HID bridge while protected-mode
; VKD/VMOUSE own keyboard and mouse services.
int2f_handler:
    cmp ax,1605h
    je .win_init
    cmp ax,1606h
    je .win_exit
    jmp far [cs:old_int2f_off]

.win_init:
    pushf
    call far [cs:old_int2f_off]
    pushf
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push cs
    pop ds

    mov byte [windows_active],1
    call mouse_clear_runtime_state

    ; If CH375USB is currently at the top of INT 33h, remove only our shim.
    xor ax,ax
    mov es,ax
    mov ax,[es:33h*4]
    cmp ax,mouse_int33
    jne .init_done
    mov ax,[es:33h*4+2]
    mov dx,cs
    cmp ax,dx
    jne .init_done
    cli
    mov ax,[old_int33_off]
    mov [es:33h*4],ax
    mov ax,[old_int33_seg]
    mov [es:33h*4+2],ax
.init_done:
    mov byte [mouse_int33_hooked],0
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    iret

.win_exit:
    pushf
    call far [cs:old_int2f_off]
    pushf
    push ax
    push ds
    push cs
    pop ds
    mov byte [windows_active],0
    mov byte [mouse_int33_hooked],0
    call mouse_clear_runtime_state
    pop ds
    pop ax
    popf
    iret


%include "ch375_hw.inc"
%include "ch375_native.inc"
%include "usb_core.inc"
%include "usb_hub.inc"
%include "usb_hid.inc"
%include "usb_msc.inc"
%include "usb_maint.inc"

resident_end:
