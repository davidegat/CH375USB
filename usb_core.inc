; CH375USB 0.4.12-exp1 - usb_core.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: Generic USB control transfers, addressing, descriptors and device slots.

slot_ptr_ax:
    push ax
    push bx
    xor ah,ah
    mov bx,ax
    shl bx,3
    mov di,bx
    shl di,1
    add di,bx
    add di,devices
    pop bx
    pop ax
    ret


clear_slot_ax:
    push ax
    push cx
    push es
    call slot_ptr_ax
    push cs
    pop es
    xor ax,ax
    mov cx,DEV_SIZE
    cld
    rep stosb
    pop es
    pop cx
    pop ax
    ret


clear_all_slots:
    push ax
    push cx
    push di
    push es
    push cs
    pop es
    mov di,devices
    xor ax,ax
    mov cx,MAX_DEVICES*DEV_SIZE
    cld
    rep stosb
    mov di,kbd_old_mod
    mov cx,MAX_DEVICES
    rep stosb
    mov di,kbd_old_keys
    mov cx,MAX_DEVICES*6
    rep stosb
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE
    mov byte [hub_slot],0FFh
    mov byte [hub_ports],0
    mov byte [media_ready],0
    mov byte [media_changed],1
    mov byte [next_address],1
    pop es
    pop di
    pop cx
    pop ax
    ret


mark_all_detached_fast:
    push ax
    push cx
    push di
    push es
    push cs
    pop es
    mov di,devices
    xor ax,ax
    mov cx,MAX_DEVICES*DEV_SIZE
    cld
    rep stosb
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE
    mov byte [hub_slot],0FFh
    mov byte [hub_ports],0
    mov byte [media_ready],0
    mov byte [media_changed],1
    mov byte [maintenance_due],1
    pop es
    pop di
    pop cx
    pop ax
    ret


usb_select_slot:
    cmp ax,MAX_DEVICES
    jae .bad
    call slot_ptr_ax
    cmp byte [di+D_USED],1
    jne .bad
    mov [current_slot],al
    mov al,[di+D_ADDR]
    call ch_select_addr
    clc
    ret
.bad:
    stc
    ret


usb_control_in_slot:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov byte [usb_control_wait],1
    call usb_select_slot
    jc .fail


    mov al,TOG_DATA0
    call ch_set_out_toggle
    mov si,setup_buf
    mov cx,8
    call ch_write_packet
    xor al,al
    mov ah,PID_SETUP
    mov cx,8
    call ch_issue_token
    jc .fail

    mov word [ctrl_actual],0
    mov byte [xfer_toggle],TOG_DATA1

.data_loop:
    mov ax,[ctrl_actual]
    cmp ax,[ctrl_wlength]
    jae .status

    mov al,[xfer_toggle]
    call ch_set_in_toggle
    xor al,al
    mov ah,PID_IN
    mov cx,64
    call ch_issue_token
    jc .fail


    push cs
    pop es
    mov di,usb_buf
    mov cx,64
    call ch_read_packet
    mov bx,ax
    cmp bx,0
    je .status


    mov dx,[ctrl_wlength]
    sub dx,[ctrl_actual]
    cmp bx,dx
    jbe .len_ok
    mov bx,dx
.len_ok:
    push ds
    push es
    push si
    push di
    push cx
    push cs
    pop ds
    mov si,usb_buf
    mov es,[xfer_seg]
    mov di,[xfer_off]
    add di,[ctrl_actual]
    mov cx,bx
    cld
    rep movsb
    pop cx
    pop di
    pop si
    pop es
    pop ds
    add [ctrl_actual],bx


    xor byte [xfer_toggle],040h
    cmp bx,64
    je .data_loop
    jmp .status

.status:

    mov al,TOG_DATA1
    call ch_set_out_toggle
    mov si,setup_buf
    xor cx,cx
    call ch_write_packet
    xor al,al
    mov ah,PID_OUT
    mov cx,32
    call ch_issue_token
    jc .fail
    mov byte [usb_control_wait],0
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    mov byte [usb_control_wait],0
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

usb_control_out0_slot:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov byte [usb_control_wait],1
    call usb_select_slot
    jc .fail
    mov al,TOG_DATA0
    call ch_set_out_toggle
    mov si,setup_buf
    mov cx,8
    call ch_write_packet
    xor al,al
    mov ah,PID_SETUP
    mov cx,32
    call ch_issue_token
    jc .fail


    mov al,TOG_DATA1
    call ch_set_in_toggle
    xor al,al
    mov ah,PID_IN
    mov cx,32
    call ch_issue_token
    jc .fail
    push cs
    pop es
    mov di,usb_buf
    mov cx,64
    call ch_read_packet

    mov byte [usb_control_wait],0
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    ; Keep later transfers from inheriting the long control-wait mode after
    ; a failed zero-data control request.
    mov byte [usb_control_wait],0
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret


usb_get_descriptor:
    push ax
    push bx
    push cx
    mov byte [setup_buf+0],080h
    mov byte [setup_buf+1],006h
    mov byte [setup_buf+2],bl
    mov byte [setup_buf+3],bh
    mov word [setup_buf+4],0
    mov word [setup_buf+6],cx
    mov [ctrl_wlength],cx
    call usb_control_in_slot
    pop cx
    pop bx
    pop ax
    ret


usb_set_configuration:
    push ax
    mov byte [setup_buf+0],000h
    mov byte [setup_buf+1],009h
    mov byte [setup_buf+2],bl
    mov byte [setup_buf+3],0
    mov word [setup_buf+4],0
    mov word [setup_buf+6],0
    call usb_control_out0_slot
    pop ax
    ret


usb_clear_halt:
    push ax
    mov byte [setup_buf+0],002h
    mov byte [setup_buf+1],001h
    mov word [setup_buf+2],0
    mov byte [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],0
    call usb_control_out0_slot
    pop ax
    ret


usb_assign_address0:
    push ax
    mov al,0
    call ch_select_addr
    mov al,CMD_SET_ADDRESS
    call ch_cmd
    mov al,bl
    call ch_data_out
    call ch_wait_status_control
    jc .fail
    cmp al,USB_INT_SUCCESS
    jne .fail
    mov al,bl
    call ch_select_addr
    call delay_10ms
    pop ax
    clc
    ret
.fail:
    pop ax
    stc
    ret


usb_enumerate_addr0:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov [xfer_slot],al
    call slot_ptr_ax
    mov byte [di+D_USED],1
    mov byte [di+D_ADDR],0
    mov byte [di+D_EP0],8


    mov byte [enum_fail_stage],03h
    call ch_get_device_descriptor0
    jc .fail


    mov al,[usb_buf+7]
    mov [di+D_EP0],al


    mov byte [enum_fail_stage],04h
    mov bl,[next_address]
    mov [enum_addr],bl
    cmp bl,127
    jae .fail
    call usb_assign_address0
    jc .fail
    mov al,[xfer_slot]
    xor ah,ah
    call slot_ptr_ax
    mov bl,[enum_addr]
    mov [di+D_ADDR],bl
    inc byte [next_address]


    mov word [xfer_seg],cs
    mov word [xfer_off],cfg_buf
    mov bh,2
    xor bl,bl
    mov byte [enum_fail_stage],05h
    mov cx,9
    mov ax,0
    mov al,[xfer_slot]
    call usb_get_descriptor
    jc .fail
    cmp word [ctrl_actual],9
    jb .fail
    cmp byte [cfg_buf+1],2
    jne .fail
    mov cx,[cfg_buf+2]
    cmp cx,9
    jb .fail
    cmp cx,256
    jbe .cfg_len_ok
    mov cx,256
.cfg_len_ok:
    mov byte [enum_fail_stage],06h
    mov bh,2
    xor bl,bl
    mov ax,0
    mov al,[xfer_slot]
    call usb_get_descriptor
    jc .fail
    cmp word [ctrl_actual],9
    jb .fail

    mov al,[xfer_slot]
    xor ah,ah
    call slot_ptr_ax
    mov al,[cfg_buf+5]
    mov [di+D_CFG],al


    mov byte [enum_fail_stage],07h
    mov ax,0
    mov al,[xfer_slot]
    call usb_parse_config
    jc .fail


    mov byte [enum_fail_stage],08h
    mov ax,0
    mov al,[xfer_slot]
    call slot_ptr_ax
    mov bl,[di+D_CFG]
    mov ax,0
    mov al,[xfer_slot]
    call usb_set_configuration
    jc .fail

    ; Keep normal idle NAKs visible to software after enumeration.  This is
    ; the documented SET_RETRY policy used by the fast HID/BOT polling paths.
    mov al,CH375_RETRY_FAST
    call ch_set_retry

    mov byte [enum_fail_stage],09h
    mov ax,0
    mov al,[xfer_slot]
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_HUB
    jz .nohub
    cmp byte [xfer_slot],0
    jne .fail
    mov ax,0
    mov al,[xfer_slot]
    call hub_init_slot
    jc .fail
.nohub:
    mov ax,0
    mov al,[xfer_slot]
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_KBD|CAP_MOUSE
    jz .nohid
    mov ax,0
    mov al,[xfer_slot]
    call hid_init_slot
.nohid:
    mov ax,0
    mov al,[xfer_slot]
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_MSC
    jz .success
    mov ax,0
    mov al,[xfer_slot]
    call msc_init_slot

.success:
    mov byte [enum_fail_stage],00h
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    mov ax,0
    mov al,[xfer_slot]
    call clear_slot_ax
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret


usb_parse_config:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    call slot_ptr_ax
    mov byte [di+D_CAPS],0
    mov byte [di+D_KBD_EP],0
    mov byte [di+D_MOUSE_EP],0
    mov byte [di+D_MSC_IN],0
    mov byte [di+D_MSC_OUT],0
    mov byte [di+D_KBD_TOG],TOG_DATA0
    mov byte [di+D_MOUSE_TOG],TOG_DATA0
    mov byte [di+D_MSC_IN_TOG],TOG_DATA0
    mov byte [di+D_MSC_OUT_TOG],TOG_DATA0

    mov cx,[ctrl_actual]
    cmp cx,256
    jbe .len_ok
    mov cx,256
.len_ok:
    xor si,si
    xor bx,bx
.parse:
    cmp si,cx
    jae .done
    mov al,[cfg_buf+si]
    cmp al,2
    jb .bad
    mov dl,[cfg_buf+si+1]
    cmp dl,4
    je .iface
    cmp dl,5
    je .ep
.advance:
    xor ah,ah
    add si,ax
    jmp .parse

.iface:
    cmp al,9
    jb .advance
    mov bl,[cfg_buf+si+5]
    mov bh,[cfg_buf+si+7]
    mov dl,[cfg_buf+si+2]
    mov [parse_itf],dl
    cmp bl,09h
    jne .not_hub
    or byte [di+D_CAPS],CAP_HUB
    jmp .advance
.not_hub:
    cmp bl,08h
    jne .not_msc
    cmp byte [cfg_buf+si+6],006h
    jne .advance
    cmp byte [cfg_buf+si+7],050h
    jne .advance
    or byte [di+D_CAPS],CAP_MSC
    mov dl,[cfg_buf+si+2]
    mov [di+D_MSC_ITF],dl
    jmp .advance
.not_msc:
    cmp bl,03h
    jne .advance
    cmp byte [cfg_buf+si+6],1
    jne .advance
    cmp bh,1
    jne .maybe_mouse
    or byte [di+D_CAPS],CAP_KBD
    mov dl,[parse_itf]
    mov [di+D_KBD_ITF],dl
    jmp .advance
.maybe_mouse:
    cmp bh,2
    jne .advance
    or byte [di+D_CAPS],CAP_MOUSE
    mov dl,[parse_itf]
    mov [di+D_MOUSE_ITF],dl
    jmp .advance

.ep:
    cmp al,7
    jb .advance
    mov dl,[cfg_buf+si+2]
    mov dh,[cfg_buf+si+3]
    and dh,3
    cmp bl,08h
    jne .hid_ep
    cmp dh,2
    jne .advance
    test dl,080h
    jz .msc_out
    and dl,0Fh
    mov [di+D_MSC_IN],dl
    jmp .advance
.msc_out:
    and dl,0Fh
    mov [di+D_MSC_OUT],dl
    jmp .advance
.hid_ep:
    cmp bl,03h
    jne .advance
    cmp dh,3
    jne .advance
    test dl,080h
    jz .advance
    and dl,0Fh
    cmp bh,1
    jne .mouse_ep
    mov [di+D_KBD_EP],dl
    jmp .advance
.mouse_ep:
    cmp bh,2
    jne .advance
    mov [di+D_MOUSE_EP],dl
    jmp .advance

.done:

    test byte [di+D_CAPS],CAP_MSC
    jz .nomsc_check
    cmp byte [di+D_MSC_IN],0
    je .drop_msc
    cmp byte [di+D_MSC_OUT],0
    jne .nomsc_check
.drop_msc:
    and byte [di+D_CAPS],0FDh
.nomsc_check:
    test byte [di+D_CAPS],CAP_KBD
    jz .nokbd_check
    cmp byte [di+D_KBD_EP],0
    jne .nokbd_check
    and byte [di+D_CAPS],0FBh
.nokbd_check:
    test byte [di+D_CAPS],CAP_MOUSE
    jz .caps_done
    cmp byte [di+D_MOUSE_EP],0
    jne .caps_done
    and byte [di+D_CAPS],0F7h
.caps_done:
    cmp byte [di+D_CAPS],0
    je .bad
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.bad:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret
