; CH375USB 0.4.11 - ch375_native.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: Root USB mass-storage support through the CH375 native DISK_* commands.

native_probe_cmd0a20:
    push dx
    mov al,CMD_AUX_0A
    call ch_cmd
    mov al,020h
    call ch_data_out
    call ch_data_in
    mov [native_probe20],al
    pop dx
    ret


native_wait_cmd_status:
    push cx
    mov cx,4
.retry:
    call ch_wait_status_native
    jc .timeout
    mov [native_last_status],al
    cmp al,USB_INT_CONNECT
    jne .not_connect
    mov byte [root_connect_seen],1
    loop .retry
    jmp .timeout
.not_connect:
    cmp al,USB_INT_DISCONNECT
    jne .done
    mov byte [root_connect_seen],0
    loop .retry
    jmp .timeout
.done:
    pop cx
    clc
    ret
.timeout:
    mov byte [native_last_status],0FFh
    pop cx
    stc
    ret


native_storage_try:
    push ax
    push bx
    push cx
    push dx
    push di

    mov byte [storage_mode],STORAGE_NONE
    mov byte [storage_slot],0FFh
    mov byte [media_ready],0
    mov byte [native_last_status],0
    mov byte [native_timeout_kind],0
    mov byte [enum_fail_stage],10h
    mov byte [native_fail_stage],10h


    mov al,CMD_SET_USB_MODE
    call ch_cmd
    mov al,6
    call ch_data_out


    mov byte [enum_fail_stage],11h
    mov byte [native_fail_stage],11h
    call ch_wait_connect_quick
    jc .fail
    mov [native_last_status],al
    cmp al,USB_INT_CONNECT
    jne .fail
    mov byte [root_connect_seen],1


    call native_short_settle


    mov byte [enum_fail_stage],12h
    mov byte [native_fail_stage],12h
    mov al,CMD_DISK_INIT
    call ch_cmd
    call native_short_settle
    call ch_wait_status_native
    jc .fail
    mov [native_last_status],al
    cmp al,USB_INT_SUCCESS
    jne .fail


    mov byte [enum_fail_stage],13h
    mov byte [native_fail_stage],13h
    mov cx,5
.size_retry:
    mov al,CMD_DISK_SIZE
    call ch_cmd
    call ch_wait_status_native
    jc .size_recover
    mov [native_last_status],al
    cmp al,USB_INT_SUCCESS
    je .size_ok
.size_recover:
    mov al,CMD_DISK_SENSE
    call ch_cmd
    call native_short_settle
    loop .size_retry
    jmp .fail

.size_ok:


    mov byte [enum_fail_stage],14h
    mov byte [native_fail_stage],14h
    mov cx,5
.ready_retry:
    mov al,CMD_DISK_READY
    call ch_cmd
    call ch_wait_status_native
    jc .ready_recover
    mov [native_last_status],al
    cmp al,USB_INT_SUCCESS
    je .ready_ok
.ready_recover:
    mov al,CMD_DISK_SENSE
    call ch_cmd
    call native_short_settle
    loop .ready_retry
    jmp .fail

.ready_ok:
    xor ax,ax
    call clear_slot_ax
    xor ax,ax
    call slot_ptr_ax
    mov byte [di+D_USED],1
    mov byte [di+D_ADDR],0
    mov byte [di+D_PARENT],0FFh
    mov byte [di+D_PORT],0
    mov byte [di+D_CAPS],CAP_MSC
    mov byte [storage_slot],0
    mov byte [storage_mode],STORAGE_NATIVE


    mov byte [enum_fail_stage],15h
    mov byte [native_fail_stage],15h
    call storage_mount_volume
    jc .mount_fail

    mov byte [media_ready],1
    mov byte [media_changed],1
    mov byte [enum_fail_stage],0
    mov byte [native_fail_stage],0
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

.mount_fail:
    mov byte [media_ready],0
    mov byte [media_changed],1
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE
.fail:

    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret


native_short_settle:
    push ax
    push cx
    mov cx,250
.loop:
    in al,020h
    loop .loop
    pop cx
    pop ax
    ret


native_disk_ready_check:
    push ax
    push cx
    mov cx,3
.retry:
    mov al,CMD_DISK_READY
    call ch_cmd
    call ch_wait_status_native
    jc .sense
    mov [native_last_status],al
    cmp al,USB_INT_SUCCESS
    je .ok
.sense:
    mov al,CMD_DISK_SENSE
    call ch_cmd
    call delay_10ms
    loop .retry
    pop cx
    pop ax
    stc
    ret
.ok:
    pop cx
    pop ax
    clc
    ret


native_advance_seg_on_wrap_es:

    push ax
    mov ax,es
    add ax,1000h
    mov es,ax
    pop ax
    ret

native_advance_seg_on_wrap_ds:

    push ax
    mov ax,ds
    add ax,1000h
    mov ds,ax
    pop ax
    ret


native_read_chunk64:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    mov ax,[xfer_seg]
    mov es,ax
    mov di,[xfer_off]
    mov dx,DATA_PORT
    mov cx,64
.loop:
    in al,dx
    stosb
    or di,di
    jnz .next
    call native_advance_seg_on_wrap_es
.next:
    loop .loop
    mov [xfer_off],di
    mov ax,es
    mov [xfer_seg],ax
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret


native_write_chunk64:
    push ax
    push bx
    push cx
    push dx
    push si
    push ds
    mov ax,[xfer_seg]
    mov ds,ax
    mov si,[xfer_off]
    mov dx,DATA_PORT
    mov cx,64
.loop:
    lodsb
    out dx,al
    or si,si
    jnz .next
    call native_advance_seg_on_wrap_ds
.next:
    loop .loop
    mov [cs:xfer_off],si
    mov ax,ds
    mov [cs:xfer_seg],ax
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret


native_read_sector_far:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es


    push word [xfer_seg]
    push word [xfer_off]

    mov bx,ax
    mov si,dx

    mov al,CMD_DISK_READ
    call ch_cmd
    mov al,bl
    call ch_data_out
    mov al,bh
    call ch_data_out
    mov ax,si
    call ch_data_out
    mov al,ah
    call ch_data_out
    mov al,1
    call ch_data_out

    mov cx,8
.chunk:
    call ch_wait_status_native
    jc .fail
    mov [native_last_status],al
    cmp al,USB_INT_DISK_READ
    jne .fail

    mov al,CMD_RD_USB_DATA
    call ch_cmd
    call ch_data_in
    cmp al,64
    jne .fail
    call native_read_chunk64

    mov al,CMD_DISK_RD_GO
    call ch_cmd
    loop .chunk

    call ch_wait_status_native
    jc .fail
    mov [native_last_status],al
    cmp al,USB_INT_SUCCESS
    jne .fail

    pop word [xfer_off]
    pop word [xfer_seg]
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

.fail:
    pop word [xfer_off]
    pop word [xfer_seg]
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret


native_write_sector_far:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

%if ENABLE_WRITES = 0
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret
%else
    push word [xfer_seg]
    push word [xfer_off]

    mov bx,ax
    mov si,dx

    mov al,CMD_DISK_WRITE
    call ch_cmd
    mov al,bl
    call ch_data_out
    mov al,bh
    call ch_data_out
    mov ax,si
    call ch_data_out
    mov al,ah
    call ch_data_out
    mov al,1
    call ch_data_out

    mov cx,8
.chunk:
    call ch_wait_status_native
    jc .fail
    mov [native_last_status],al
    cmp al,USB_INT_DISK_WRITE
    jne .fail

    mov al,CMD_WR_USB_DATA7
    call ch_cmd
    mov al,64
    call ch_data_out
    call native_write_chunk64

    mov al,CMD_DISK_WR_GO
    call ch_cmd
    loop .chunk

    call ch_wait_status_native
    jc .fail
    mov [native_last_status],al
    cmp al,USB_INT_SUCCESS
    jne .fail

    pop word [xfer_off]
    pop word [xfer_seg]
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

.fail:
    pop word [xfer_off]
    pop word [xfer_seg]
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret
%endif


storage_ready_check:
    cmp byte [storage_mode],STORAGE_NATIVE
    je native_disk_ready_check
    cmp byte [storage_mode],STORAGE_BOT
    jne .bad
    cmp byte [storage_slot],0FFh
    je .bad
    push ax
    xor ax,ax
    mov al,[storage_slot]
    call msc_test_unit_ready_slot
    pop ax
    ret
.bad:
    stc
    ret

storage_read_sector_far:
    cmp byte [storage_mode],STORAGE_NATIVE
    je native_read_sector_far
    cmp byte [storage_mode],STORAGE_BOT
    jne .bad
    jmp msc_read_sector_far
.bad:
    stc
    ret

storage_write_sector_far:
    cmp byte [storage_mode],STORAGE_NATIVE
    je native_write_sector_far
    cmp byte [storage_mode],STORAGE_BOT
    jne .bad
    jmp msc_write_sector_far
.bad:
    stc
    ret


storage_sync_cache:
    cmp byte [storage_mode],STORAGE_NATIVE
    je .native_ok
    cmp byte [storage_mode],STORAGE_BOT
    jne .bad
    jmp msc_sync_cache
.native_ok:
    clc
    ret
.bad:
    stc
    ret


storage_verify_sector_far:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov bx,[xfer_seg]
    push bx
    mov bx,[xfer_off]
    push bx

    mov word [xfer_seg],cs
    mov word [xfer_off],verify_buf
    call storage_read_sector_far
    pop bx
    mov [xfer_off],bx
    pop bx
    mov [xfer_seg],bx
    jc .fail

    mov ax,[xfer_seg]
    mov ds,ax
    mov si,[xfer_off]
    push cs
    pop es
    mov di,verify_buf
    mov cx,512
    cld
    repe cmpsb
    jne .fail

    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret
