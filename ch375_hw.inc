; CH375USB 0.4.12-exp1 - ch375_hw.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: Low-level CH375 parallel-I/O access, delays, probing and bounded waits.

ch_delay:
    push cx
    mov cx,14
.d:
    loop .d
    pop cx
    ret


ch_cmd_delay:
    push cx
    mov cx,96
.d:
    loop .d
    pop cx
    ret

ch_cmd:
    push dx
    mov dx,CMD_PORT
    out dx,al
    call ch_cmd_delay
    pop dx
    ret

ch_data_out:
    push dx
    mov dx,DATA_PORT
    out dx,al
    call ch_delay
    pop dx
    ret

ch_data_in:
    push dx
    mov dx,DATA_PORT
    in al,dx
    call ch_delay
    pop dx
    ret

ch_probe:
    mov al,CMD_CHECK_EXIST
    call ch_cmd
    mov al,055h
    call ch_data_out
    call ch_data_in
    cmp al,0AAh
    jne .bad
    clc
    ret
.bad:
    stc
    ret


ch_wait_status_control:


    push bx
    push cx
    push dx
    push di
    push es

    pushf
    pop ax
    test ax,0200h
    jz .direct_fallback

    mov ax,0040h
    mov es,ax
    mov bx,[es:006Ch]

.poll_irq:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jz .pending

    mov ax,[es:006Ch]
    sub ax,bx
    cmp ax,10
    jb .poll_irq
    jmp .direct_fallback

.pending:
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [cs:last_status],al

    cmp al,USB_INT_CONNECT
    jne .not_connect
    mov byte [cs:root_connect_seen],1

    jmp .poll_irq

.not_connect:
    cmp al,USB_INT_DISCONNECT
    jne .done
    mov byte [cs:root_connect_seen],0
    jmp .abort

.direct_fallback:


    mov di,6
.direct_again:
    call delay_10ms
    ; Datasheet I: GET_STATUS is meaningful after INT# is asserted.  In
    ; parallel mode command-port bit 7 mirrors INT# (0 = pending), so do not
    ; consume status blindly while interrupts/ticks are unavailable.
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jnz .direct_no_event
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [cs:last_status],al

    cmp al,USB_INT_CONNECT
    jne .direct_not_connect
    mov byte [cs:root_connect_seen],1
    dec di
    jnz .direct_again
    jmp .timeout

.direct_not_connect:
    cmp al,USB_INT_DISCONNECT
    jne .done
    mov byte [cs:root_connect_seen],0
    jmp .abort

.direct_no_event:
    dec di
    jnz .direct_again
    jmp .timeout

.done:
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    clc
    ret

.abort:
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    stc
    ret

.timeout:
    mov al,0FFh
    mov [cs:last_status],al
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    stc
    ret

delay_10ms:
    push cx
    push dx
    mov dx,2
.outer:
    mov cx,0FFFFh
.inner:
    loop .inner
    dec dx
    jnz .outer
    pop dx
    pop cx
    ret

delay_100ms:
    push cx
    mov cx,10
.loop:
    call delay_10ms
    loop .loop
    ret


ch_set_mode:
    push bx
    push cx
    mov bl,al
    mov al,CMD_SET_USB_MODE
    call ch_cmd
    mov al,bl
    call ch_data_out

    mov cx,100
.poll:
    call ch_data_in
    cmp al,CMD_RET_SUCCESS
    je .ok
    cmp al,CMD_RET_ABORT
    je .bad
    loop .poll
.bad:
    stc
    pop cx
    pop bx
    ret
.ok:
    clc
    pop cx
    pop bx
    ret

ch_irq_pending:
    push dx
    mov dx,CMD_PORT
    in al,dx
    pop dx
    test al,080h
    jnz .no
    stc
    ret
.no:
    clc
    ret


ch_wait_status:
    push cx
    push dx
    mov cx,08000h
.wait:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jz .pending
    loop .wait
    mov al,0FFh
    mov [last_status],al
    pop dx
    pop cx
    stc
    ret
.pending:
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al
    pop dx
    pop cx
    clc
    ret


ch_wait_status_native:
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov byte [cs:native_timeout_kind],0


    pushf
    pop ax
    test ax,0200h
    jz .fallback

    mov ax,0040h
    mov es,ax
    mov bx,[es:006Ch]
.tick_wait:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jz .pending


    in al,020h

    mov ax,[es:006Ch]
    sub ax,bx
    cmp ax,36
    jb .tick_wait

    mov byte [cs:native_timeout_kind],1
    jmp .timeout

.fallback:


    mov di,64
.fb_outer:
    mov cx,0FFFFh
.fb_inner:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jz .pending
    in al,020h
    loop .fb_inner
    dec di
    jnz .fb_outer
    mov byte [cs:native_timeout_kind],2

.timeout:
    mov al,0FFh
    mov [cs:last_status],al
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    stc
    ret

.pending:
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [cs:last_status],al
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    clc
    ret


ch_drain_events:
    push ax
    push cx
    push dx
    mov cx,8
.loop:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jnz .done
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al
    loop .loop
.done:
    pop dx
    pop cx
    pop ax
    ret


ch_delay_one_tick:
    push ax
    push bx
    push cx
    push dx
    push es

    pushf
    pop ax
    test ax,0200h
    jz .fallback

    mov ax,0040h
    mov es,ax
    mov bx,[es:006Ch]
.tick:
    mov ax,[es:006Ch]
    cmp ax,bx
    jne .done
    in al,020h
    jmp .tick

.fallback:
    mov dx,32
.fb_outer:
    mov cx,0FFFFh
.fb_inner:
    in al,020h
    loop .fb_inner
    dec dx
    jnz .fb_outer
.done:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret


ch_wait_connect_quick:
    push bx
    push cx
    push dx
    push es

    pushf
    pop ax
    test ax,0200h
    jz .fallback

    mov ax,0040h
    mov es,ax
    mov bx,[es:006Ch]
.tick:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jnz .timecheck
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al
    cmp al,USB_INT_CONNECT
    je .ok
    cmp al,USB_INT_DISCONNECT
    je .notyet
.timecheck:
    mov ax,[es:006Ch]
    sub ax,bx
    cmp ax,4
    jb .tick
    jmp .timeout

.notyet:
    jmp .timecheck

.fallback:
    mov cx,0FFFFh
.fb:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jz .fbstatus
    loop .fb
    jmp .timeout
.fbstatus:
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al
    cmp al,USB_INT_CONNECT
    jne .timeout
.ok:
    mov byte [root_connect_seen],1
    pop es
    pop dx
    pop cx
    pop bx
    clc
    ret
.timeout:
    mov byte [root_connect_seen],0
    mov al,0FFh
    mov [last_status],al
    pop es
    pop dx
    pop cx
    pop bx
    stc
    ret

ch_wait_connect_after_reset:
    push ax
    push bx
    push cx
    push dx
    push es

    pushf
    pop ax
    test ax,0200h
    jz .fallback

    mov ax,0040h
    mov es,ax
    mov bx,[es:006Ch]
.tick:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jnz .timecheck
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al
    cmp al,USB_INT_CONNECT
    je .connected
    cmp al,USB_INT_DISCONNECT
    je .timecheck
.timecheck:
    mov ax,[es:006Ch]
    sub ax,bx
    cmp ax,36
    jb .tick
    jmp .timeout

.fallback:
    mov bx,48
.fb_outer:
    mov cx,0FFFFh
.fb_inner:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jz .fb_status
    in al,020h
    loop .fb_inner
    dec bx
    jnz .fb_outer
    jmp .timeout
.fb_status:
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al
    cmp al,USB_INT_CONNECT
    je .connected
    cmp al,USB_INT_DISCONNECT
    je .fallback
    jmp .fallback

.connected:
    mov byte [root_connect_seen],1
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

.timeout:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret


ch_collect_root_events:
    push ax
    push cx
    push dx
    mov cx,8
.loop:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jnz .done
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al
    cmp al,USB_INT_CONNECT
    jne .not_conn
    mov byte [root_connect_seen],1
    jmp .next
.not_conn:
    cmp al,USB_INT_DISCONNECT
    jne .next
    mov byte [root_connect_seen],0
.next:
    loop .loop
.done:
    pop dx
    pop cx
    pop ax
    ret


; Datasheet II helpers -------------------------------------------------------
; SET_RETRY takes the fixed marker 25h followed by the retry policy byte.
; AL = retry policy.  Bit 7 clear makes a normal USB NAK visible to software
; instead of allowing the CH375 default infinite NAK retry.
ch_set_retry:
    push ax
    mov ah,al
    mov al,CMD_SET_RETRY
    call ch_cmd
    mov al,CH375_RETRY_MAGIC
    call ch_data_out
    mov al,ah
    call ch_data_out
    pop ax
    ret

; AL = USB_SPEED_FULL (00h) or USB_SPEED_LOW (02h).
ch_set_usb_speed:
    push ax
    mov ah,al
    mov al,CMD_SET_USB_SPEED
    call ch_cmd
    mov al,ah
    call ch_data_out
    pop ax
    ret

; GET_DEV_RATE is valid only in host mode 5.  Bit 4 of the result indicates
; a 1.5-Mbit/s low-speed device; otherwise the device is full-speed.
ch_detect_root_speed:
    push ax
    mov byte [usb_speed_mode],0
    mov al,5
    call ch_set_mode
    jc .unknown
    call ch_collect_root_events
    mov al,CMD_GET_DEV_RATE
    call ch_cmd
    mov al,007h
    call ch_data_out
    call ch_data_in
    test al,010h
    jz .known
    mov byte [usb_speed_mode],1
.known:
    mov al,[usb_speed_mode]
    mov [usb_detected_speed],al
    pop ax
    clc
    ret
.unknown:
    mov byte [usb_speed_mode],0
    mov byte [usb_detected_speed],0
    pop ax
    stc
    ret

; SET_USB_MODE restores full-speed operation.  Re-apply low speed after the
; final switch to mode 6 when GET_DEV_RATE identified a low-speed root device.
ch_apply_root_speed:
    cmp byte [usb_speed_mode],1
    jne .done
    push ax
    mov al,USB_SPEED_LOW
    call ch_set_usb_speed
    pop ax
.done:
    ret

; Mode 7 asserts USB bus reset and must always be released by returning to mode 6.
ch_root_reset:
    mov byte [root_connect_seen],0

    ; Datasheet I recommended host sequence: mode 5 while quiescent, mode 7
    ; to assert bus reset, then mode 6 to release reset and generate SOF.
    mov al,5
    call ch_set_mode
    jc .fail
    call ch_collect_root_events

    mov al,7
    call ch_set_mode
    jc .fail
    call ch_delay_one_tick

    mov al,6
    call ch_set_mode
    jc .fail
    ; Datasheet II: SET_USB_MODE restores 12-Mbit/s full speed, therefore a
    ; low-speed selection has to be applied *after* entering mode 6.
    call ch_apply_root_speed
    call ch_delay_one_tick

    call ch_wait_connect_after_reset
    jc .fail

    xor al,al
    call ch_select_addr
    mov al,CH375_RETRY_BOUNDED
    call ch_set_retry
    clc
    ret

.fail:
    ; Never leave the bus in reset, even if a transition fails.
    mov al,6
    call ch_set_mode
    stc
    ret


ch_get_device_descriptor0:
    push bx
    push cx
    push di
    push es
    xor al,al
    call ch_select_addr
    mov al,CMD_GET_DESCR
    call ch_cmd
    mov al,1
    call ch_data_out
    call ch_wait_status_control
    jc .fail
    cmp al,USB_INT_SUCCESS
    jne .fail
    push cs
    pop es
    mov di,usb_buf
    mov cx,18
    call ch_read_packet
    cmp ax,8
    jb .fail
    pop es
    pop di
    pop cx
    pop bx
    clc
    ret
.fail:
    pop es
    pop di
    pop cx
    pop bx
    stc
    ret


; Compatibility fallback retained from the known-good 0.4.11 path.
; This old CH375B low-speed sequence is NOT the documented Datasheet-II
; SET_RETRY format (which requires marker 25h); use it only after the
; documented GET_DEV_RATE/SET_USB_SPEED path fails.
ch_set_low_speed_legacy:
    push ax
    mov al,00Bh
    call ch_cmd
    mov al,017h
    call ch_data_out
    mov al,0D8h
    call ch_data_out
    call delay_10ms
    mov byte [usb_speed_mode],1
    pop ax
    ret

ch_set_full_speed_state:
    mov byte [usb_speed_mode],0
    ret


ch_get_config_descriptor_builtin:
    push bx
    push cx
    push di
    push es
    mov al,CMD_GET_DESCR
    call ch_cmd
    mov al,2
    call ch_data_out
    call ch_wait_status_control
    jc .fail
    cmp al,USB_INT_SUCCESS
    jne .fail
    push cs
    pop es
    mov di,cfg_buf
    mov cx,64
    call ch_read_packet
    cmp ax,9
    jb .fail
    mov [ctrl_actual],ax
    pop es
    pop di
    pop cx
    pop bx
    clc
    ret
.fail:
    pop es
    pop di
    pop cx
    pop bx
    stc
    ret


ch_set_config_builtin:
    push ax
    mov al,CMD_SET_CONFIG
    call ch_cmd
    mov al,bl
    call ch_data_out
    call ch_wait_status_control
    jc .fail
    cmp al,USB_INT_SUCCESS
    jne .fail
    pop ax
    clc
    ret
.fail:
    pop ax
    stc
    ret

ch_select_addr:
    and al,07Fh
    mov [current_addr],al
    push ax
    mov al,CMD_SET_USB_ADDR
    call ch_cmd
    pop ax
    call ch_data_out
    ret


ch_set_in_toggle:
    push ax
    mov ah,al
    mov al,CMD_SET_ENDP6
    call ch_cmd
    mov al,ah
    call ch_data_out
    pop ax
    ret

ch_set_out_toggle:
    push ax
    mov ah,al
    mov al,CMD_SET_ENDP7
    call ch_cmd
    mov al,ah
    call ch_data_out
    pop ax
    ret


ch_write_packet:
    push ax
    push cx
    mov al,CMD_WR_USB_DATA7
    call ch_cmd
    mov al,cl
    call ch_data_out
.loop:
    jcxz .done
    lodsb
    call ch_data_out
    loop .loop
.done:
    pop cx
    pop ax
    ret


ch_read_packet:
    push bx
    push cx
    push dx
    push si
    mov bx,cx
    mov al,CMD_RD_USB_DATA
    call ch_cmd
    call ch_data_in
    xor ah,ah
    mov si,ax
    xor cx,cx
    mov cl,al
    mov dx,DATA_PORT
.loop:
    jcxz .done
    in al,dx
    call ch_delay
    cmp bx,0
    je .discard
    stosb
    dec bx
.discard:
    loop .loop
.done:
    mov ax,si
    pop si
    pop dx
    pop cx
    pop bx
    ret


; Idle HID endpoints normally NAK; this routine never waits long inside the timer ISR.
ch_issue_hid_in_fast:
    push bx
    push cx
    push dx

    mov bl,al
    mov al,CMD_ISSUE_TOKEN
    call ch_cmd
    mov al,bl
    shl al,4
    or al,PID_IN
    call ch_data_out


    mov cx,0200h
.wait:
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jz .status
    loop .wait


    mov al,CMD_ABORT_NAK
    call ch_cmd
    pop dx
    pop cx
    pop bx
    stc
    ret

.status:
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al

    cmp al,USB_INT_SUCCESS
    je .ok

    cmp al,USB_INT_DISCONNECT
    jne .fail
    mov byte [root_link_state],0
    mov byte [root_event],1
    mov byte [maintenance_due],1
    mov byte [root_retry_ticks],9

.fail:
    pop dx
    pop cx
    pop bx
    stc
    ret
.ok:
    pop dx
    pop cx
    pop bx
    clc
    ret


ch_issue_token:
    push bx
    push dx
    mov bl,al
    mov bh,ah
.retry:
    mov al,CMD_ISSUE_TOKEN
    call ch_cmd
    mov al,bl
    shl al,4
    or al,bh
    call ch_data_out
    cmp byte [cs:usb_control_wait],1
    jne .fast_wait
    call ch_wait_status_control
    jmp .wait_done
.fast_wait:
    call ch_wait_status
.wait_done:
    jc .fail
    cmp al,USB_INT_SUCCESS
    je .ok

    mov dl,al
    and dl,USB_ERR_PID_MASK
    cmp dl,USB_ERR_NAK
    jne .fail
    jcxz .fail
    dec cx
    jmp .retry
.ok:
    pop dx
    pop bx
    clc
    ret
.fail:
    pop dx
    pop bx
    stc
    ret
