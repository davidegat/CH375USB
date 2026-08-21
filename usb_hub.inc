; CH375USB 0.4.13 - usb_hub.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: Experimental single-hub enumeration and downstream-port handling.

%define HUB_DT_HUB               029h
%define HUB_REQ_GET_STATUS       000h
%define HUB_REQ_CLEAR_FEATURE    001h
%define HUB_REQ_SET_FEATURE      003h
%define HUB_REQ_GET_DESCRIPTOR   006h

%define HUB_PORT_CONNECTION      0
%define HUB_PORT_ENABLE          1
%define HUB_PORT_SUSPEND         2
%define HUB_PORT_OVER_CURRENT    3
%define HUB_PORT_RESET           4
%define HUB_PORT_POWER           8
%define HUB_C_PORT_CONNECTION    16
%define HUB_C_PORT_ENABLE        17
%define HUB_C_PORT_SUSPEND       18
%define HUB_C_PORT_OVER_CURRENT  19
%define HUB_C_PORT_RESET         20

%define HUB_STAT_CONNECTION      0001h
%define HUB_STAT_ENABLE          0002h
%define HUB_STAT_LOW_SPEED       0200h

hub_work_port db 0


hub_init_slot:
    push ax
    push bx
    push cx
    push dx
    push di

    mov [hub_slot],al

    mov word [xfer_seg],cs
    mov word [xfer_off],hub_buf
    mov word [ctrl_wlength],9
    mov byte [setup_buf+0],0A0h
    mov byte [setup_buf+1],HUB_REQ_GET_DESCRIPTOR
    mov byte [setup_buf+2],0
    mov byte [setup_buf+3],HUB_DT_HUB
    mov word [setup_buf+4],0
    mov word [setup_buf+6],9
    call usb_control_in_slot
    jc .fail
    cmp word [ctrl_actual],3
    jb .fail
    cmp byte [hub_buf+1],HUB_DT_HUB
    jne .fail

    mov bl,[hub_buf+2]
    cmp bl,MAX_HUB_PORTS
    jbe .ports_ok
    mov bl,MAX_HUB_PORTS
.ports_ok:
    cmp bl,0
    je .fail
    mov [hub_ports],bl
    call slot_ptr_ax
    mov [di+D_HUBPORTS],bl


    xor cx,cx
    mov cl,bl
    mov dl,1
.power_loop:
    push ax
    push cx
    push dx
    mov bl,dl
    mov cx,HUB_PORT_POWER
    call hub_set_port_feature
    pop dx
    pop cx
    pop ax
    inc dl
    loop .power_loop
    call delay_100ms


    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    mov byte [hub_slot],0FFh
    mov byte [hub_ports],0
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret


hub_set_port_feature:
    push ax
    mov byte [setup_buf+0],023h
    mov byte [setup_buf+1],HUB_REQ_SET_FEATURE
    mov [setup_buf+2],cl
    mov [setup_buf+3],ch
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],0
    call usb_control_out0_slot
    pop ax
    ret


hub_clear_port_feature:
    push ax
    mov byte [setup_buf+0],023h
    mov byte [setup_buf+1],HUB_REQ_CLEAR_FEATURE
    mov [setup_buf+2],cl
    mov [setup_buf+3],ch
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],0
    call usb_control_out0_slot
    pop ax
    ret


hub_get_port_status:
    push ax
    mov word [xfer_seg],cs
    mov word [xfer_off],hub_status
    mov word [ctrl_wlength],4
    mov byte [setup_buf+0],0A3h
    mov byte [setup_buf+1],HUB_REQ_GET_STATUS
    mov word [setup_buf+2],0
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],4
    call usb_control_in_slot
    jc .fail
    cmp word [ctrl_actual],4
    jb .fail
    pop ax
    clc
    ret
.fail:
    pop ax
    stc
    ret


hub_reset_port:
    push ax
    push bx
    push cx
    mov cx,HUB_PORT_RESET
    call hub_set_port_feature
    jc .fail
    call delay_10ms
    mov cx,30
.wait:
    push ax
    push bx
    call hub_get_port_status
    pop bx
    pop ax
    jc .fail
    mov dx,[hub_status]
    test dx,HUB_STAT_ENABLE
    jnz .enabled
    call delay_10ms
    loop .wait
    jmp .fail
.enabled:

    push ax
    push bx
    mov cx,HUB_C_PORT_RESET
    call hub_clear_port_feature
    pop bx
    pop ax
    push ax
    push bx
    mov cx,HUB_C_PORT_CONNECTION
    call hub_clear_port_feature
    pop bx
    pop ax
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    pop cx
    pop bx
    pop ax
    stc
    ret


hub_detach_port:
    push ax
    push bx
    xor ax,ax
    mov al,bl
    cmp ax,MAX_DEVICES
    jae .done
    call slot_ptr_ax
    cmp byte [di+D_USED],1
    jne .clear

    cmp [storage_slot],al
    jne .clear
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE
    mov byte [media_ready],0
    mov byte [media_changed],1
.clear:
    call clear_slot_ax
.done:
    pop bx
    pop ax
    ret


hub_attach_port:
    push ax
    push bx
    push cx
    push dx
    push si
    push di


    cmp bl,MAX_HUB_PORTS
    ja .fail
    mov [hub_work_port],bl

    call hub_reset_port
    jc .fail
    mov dx,[hub_status]
    test dx,HUB_STAT_CONNECTION
    jz .fail
    test dx,HUB_STAT_LOW_SPEED
    jnz .low_speed


    xor ax,ax
    mov al,[hub_work_port]
    call clear_slot_ax
    xor ax,ax
    mov al,[hub_work_port]
    call slot_ptr_ax
    mov byte [di+D_PARENT],0
    mov dl,[hub_work_port]
    mov [di+D_PORT],dl
    mov byte [di+D_FLAGS],0


    xor ax,ax
    mov al,[hub_work_port]
    call usb_enumerate_addr0
    jc .fail_detach
    jmp .ok

.low_speed:


    xor ax,ax
    mov al,[hub_work_port]
    call clear_slot_ax
    xor ax,ax
    mov al,[hub_work_port]
    call slot_ptr_ax
    mov byte [di+D_USED],2
    mov byte [di+D_PARENT],0
    mov dl,[hub_work_port]
    mov [di+D_PORT],dl
    mov byte [di+D_FLAGS],FLAG_LOW_SPEED
    stc
    jmp .out

.fail_detach:
    mov bl,[hub_work_port]
    call hub_detach_port
.fail:
    stc
    jmp .out
.ok:
    clc
.out:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret


hub_scan_ports:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    cmp byte [hub_slot],0FFh
    je .done
    xor ax,ax
    mov al,[hub_slot]
    xor cx,cx
    mov cl,[hub_ports]
    mov bl,1
.loop:
    push ax
    push bx
    push cx
    call hub_get_port_status
    pop cx
    pop bx
    pop ax
    jc .next

    mov dx,[hub_status]

    push ax
    xor ax,ax
    mov al,bl
    call slot_ptr_ax
    mov si,di
    pop ax

    test dx,HUB_STAT_CONNECTION
    jz .disconnected
    cmp byte [si+D_USED],0
    jne .ack_changes
    push ax
    push bx
    push cx
    call hub_attach_port
    pop cx
    pop bx
    pop ax
    jmp .ack_changes
.disconnected:
    cmp byte [si+D_USED],0
    je .ack_changes
    push ax
    push bx
    mov bl,bl
    call hub_detach_port
    pop bx
    pop ax

.ack_changes:

    test word [hub_status+2],0001h
    jz .next
    push ax
    push bx
    push cx
    mov cx,HUB_C_PORT_CONNECTION
    call hub_clear_port_feature
    pop cx
    pop bx
    pop ax
.next:
    inc bl
    loop .loop
.done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
