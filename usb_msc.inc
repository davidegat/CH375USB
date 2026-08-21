; CH375USB 0.4.13 - usb_msc.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: Generic USB Mass Storage BOT/SCSI fallback path.

%define CBW_SIGNATURE       043425355h
%define CSW_SIGNATURE       053425355h

%define SCSI_TEST_UNIT_READY 000h
%define SCSI_REQUEST_SENSE   003h
%define SCSI_INQUIRY         012h
%define SCSI_READ_CAPACITY10 025h
%define SCSI_READ10          028h
%define SCSI_WRITE10         02Ah
%define SCSI_SYNCHRONIZE10   035h

msc_active_tag  dd 0
msc_lba_lo     dw 0
msc_lba_hi     dw 0
msc_tmp_slot   db 0
msc_data_dir   db 0
msc_data_len   dw 0
msc_data_len_hi dw 0


far_to_usb_buf:

    push ax
    push cx
    push si
    push di
    push ds
    push es
    mov ax,[xfer_seg]
    mov ds,ax
    mov si,[xfer_off]
    push cs
    pop es
    mov di,usb_buf
    cld
    rep movsb
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    ret

usb_buf_to_far:

    push ax
    push cx
    push si
    push di
    push ds
    push es
    push cs
    pop ds
    mov si,usb_buf
    mov ax,[xfer_seg]
    mov es,ax
    mov di,[xfer_off]
    cld
    rep movsb
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    ret

advance_xfer_ptr_cx:
    add [xfer_off],cx
    jnc .done
    add word [xfer_seg],1000h
.done:
    ret


msc_bulk_out_far:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov [msc_tmp_slot],al
    mov dx,cx
.loop:
    or dx,dx
    jz .ok
    mov cx,64
    cmp dx,cx
    jae .chunk
    mov cx,dx
.chunk:
    push cx
    call far_to_usb_buf
    pop cx

    xor ax,ax
    mov al,[msc_tmp_slot]
    call usb_select_slot
    jc .fail
    mov bl,[di+D_MSC_OUT]
    mov al,[di+D_MSC_OUT_TOG]
    call ch_set_out_toggle
    mov si,usb_buf
    push cx
    call ch_write_packet
    pop cx
    mov al,bl
    mov ah,PID_OUT
    push cx
    mov cx,128
    call ch_issue_token
    pop cx
    jc .fail

    xor ax,ax
    mov al,[msc_tmp_slot]
    call slot_ptr_ax
    xor byte [di+D_MSC_OUT_TOG],040h
    call advance_xfer_ptr_cx
    sub dx,cx
    jmp .loop
.ok:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

msc_bulk_in_far:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov [msc_tmp_slot],al
    mov dx,cx
.loop:
    or dx,dx
    jz .ok
    xor ax,ax
    mov al,[msc_tmp_slot]
    call usb_select_slot
    jc .fail
    mov bl,[di+D_MSC_IN]
    mov al,[di+D_MSC_IN_TOG]
    call ch_set_in_toggle
    mov al,bl
    mov ah,PID_IN
    mov cx,128
    call ch_issue_token
    jc .fail

    push cs
    pop es
    mov di,usb_buf
    mov cx,64
    call ch_read_packet
    or ax,ax
    jz .fail
    cmp ax,dx
    jbe .not_over
    mov ax,dx
.not_over:
    mov cx,ax
    push cx
    call usb_buf_to_far
    pop cx
    call advance_xfer_ptr_cx
    sub dx,cx

    xor ax,ax
    mov al,[msc_tmp_slot]
    call slot_ptr_ax
    xor byte [di+D_MSC_IN_TOG],040h

    cmp cx,64
    je .loop
    or dx,dx
    jnz .fail
    jmp .ok
.ok:
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
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret


msc_zero_cbw:
    push ax
    push cx
    push di
    push es
    push cs
    pop es
    mov di,cbw_buf
    xor ax,ax
    mov cx,31
    cld
    rep stosb
    pop es
    pop di
    pop cx
    pop ax
    ret


msc_prepare_cbw:
    push ax
    push di
    call msc_zero_cbw
    mov dword [cbw_buf+0],CBW_SIGNATURE
    mov ax,[msc_tag]
    mov [cbw_buf+4],ax
    mov ax,[msc_tag+2]
    mov [cbw_buf+6],ax
    mov ax,[msc_tag]
    mov [msc_active_tag],ax
    mov ax,[msc_tag+2]
    mov [msc_active_tag+2],ax
    inc dword [msc_tag]
    mov [cbw_buf+8],cx
    mov [cbw_buf+10],dx
    mov [cbw_buf+12],bl
    mov [cbw_buf+14],bh
    pop di
    pop ax
    push ax
    push di
    call slot_ptr_ax
    mov al,[di+D_MSC_LUN]
    and al,0Fh
    mov [cbw_buf+13],al
    pop di
    pop ax
    ret


msc_send_cbw:
    push ax
    push bx
    mov bx,[xfer_seg]
    push bx
    mov bx,[xfer_off]
    push bx
    mov word [xfer_seg],cs
    mov word [xfer_off],cbw_buf
    mov cx,31
    call msc_bulk_out_far
    pop bx
    mov [xfer_off],bx
    pop bx
    mov [xfer_seg],bx
    pop bx
    pop ax
    ret


msc_read_csw:
    push ax
    push bx
    mov bx,[xfer_seg]
    push bx
    mov bx,[xfer_off]
    push bx
    mov word [xfer_seg],cs
    mov word [xfer_off],csw_buf
    mov cx,13
    call msc_bulk_in_far
    pop bx
    mov [xfer_off],bx
    pop bx
    mov [xfer_seg],bx
    jc .fail
    cmp dword [csw_buf+0],CSW_SIGNATURE
    jne .fail
    mov bx,[csw_buf+4]
    cmp bx,[msc_active_tag]
    jne .fail
    mov bx,[csw_buf+6]
    cmp bx,[msc_active_tag+2]
    jne .fail
    cmp byte [csw_buf+12],0
    jne .fail
    pop bx
    pop ax
    clc
    ret
.fail:
    pop bx
    pop ax
    stc
    ret


msc_recover_slot:
    push ax
    push bx
    push di

    call slot_ptr_ax
    mov bl,[di+D_MSC_ITF]
    mov byte [setup_buf+0],021h
    mov byte [setup_buf+1],0FFh
    mov word [setup_buf+2],0
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],0
    push ax
    call usb_control_out0_slot
    pop ax


    call slot_ptr_ax
    mov bl,[di+D_MSC_OUT]
    push ax
    call usb_clear_halt
    pop ax


    call slot_ptr_ax
    mov bl,[di+D_MSC_IN]
    or bl,080h
    push ax
    call usb_clear_halt
    pop ax

    call slot_ptr_ax
    mov byte [di+D_MSC_IN_TOG],TOG_DATA0
    mov byte [di+D_MSC_OUT_TOG],TOG_DATA0
    call delay_10ms

    pop di
    pop bx
    pop ax
    ret


msc_exec_nodata:
    push ax
    call msc_send_cbw
    pop ax
    jc .recover_fail
    push ax
    call msc_read_csw
    pop ax
    jc .recover_fail
    clc
    ret
.recover_fail:
    push ax
    call msc_recover_slot
    pop ax
    stc
    ret


msc_exec_in:
    push ax
    push cx
    call msc_send_cbw
    pop cx
    pop ax
    jc .fail_recover
    push ax
    push cx
    call msc_bulk_in_far
    pop cx
    pop ax
    jc .fail_recover
    push ax
    call msc_read_csw
    pop ax
    jc .fail_recover
    clc
    ret
.fail_recover:
    push ax
    call msc_recover_slot
    pop ax
    stc
    ret


msc_exec_out:
    push ax
    push cx
    call msc_send_cbw
    pop cx
    pop ax
    jc .fail_recover
    push ax
    push cx
    call msc_bulk_out_far
    pop cx
    pop ax
    jc .fail_recover
    push ax
    call msc_read_csw
    pop ax
    jc .fail_recover
    clc
    ret
.fail_recover:
    push ax
    call msc_recover_slot
    pop ax
    stc
    ret


msc_init_slot:
    push ax
    push bx
    push cx
    push dx
    push di

    mov [msc_tmp_slot],al
    call slot_ptr_ax
    mov byte [di+D_MSC_LUN],0
    mov byte [di+D_MSC_IN_TOG],TOG_DATA0
    mov byte [di+D_MSC_OUT_TOG],TOG_DATA0


    mov word [xfer_seg],cs
    mov word [xfer_off],usb_buf
    mov word [ctrl_wlength],1
    mov byte [setup_buf+0],0A1h
    mov byte [setup_buf+1],0FEh
    mov word [setup_buf+2],0
    mov bl,[di+D_MSC_ITF]
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],1
    mov ax,0
    mov al,[msc_tmp_slot]
    call usb_control_in_slot
    jc .lun0
    cmp word [ctrl_actual],1
    jb .lun0


    mov ax,0
    mov al,[msc_tmp_slot]
    call slot_ptr_ax
    mov byte [di+D_MSC_LUN],0
.lun0:


    mov ax,0
    mov al,[msc_tmp_slot]
    call msc_recover_slot


    mov cx,6
.tur_loop:
    push cx
    mov ax,0
    mov al,[msc_tmp_slot]
    call msc_test_unit_ready_slot
    pop cx
    jnc .tur_ok
    call delay_100ms
    loop .tur_loop
    jmp .fail
.tur_ok:

    mov ax,0
    mov al,[msc_tmp_slot]
    call msc_read_capacity_slot
    jc .fail
    cmp dword [msc_block_size],512
    jne .fail


    cmp byte [storage_slot],0FFh
    jne .ok
    mov al,[msc_tmp_slot]
    mov [storage_slot],al
    mov byte [storage_mode],STORAGE_BOT
    call storage_mount_volume
    jc .unclaim
    mov byte [media_ready],1
    mov byte [media_changed],1
    jmp .ok
.unclaim:
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE
    mov byte [media_ready],0
.fail:
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret
.ok:
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret


msc_test_unit_ready_slot:
    push ax
    push bx
    push cx
    push dx
    mov bl,0
    mov bh,6
    xor cx,cx
    xor dx,dx
    call msc_prepare_cbw
    mov byte [cbw_buf+15],SCSI_TEST_UNIT_READY
    call msc_exec_nodata
    pop dx
    pop cx
    pop bx
    pop ax
    ret


msc_read_capacity_slot:
    push ax
    push bx
    push cx
    push dx
    mov bl,080h
    mov bh,10
    mov cx,8
    xor dx,dx
    call msc_prepare_cbw
    mov byte [cbw_buf+15],SCSI_READ_CAPACITY10
    mov word [xfer_seg],cs
    mov word [xfer_off],capacity_buf
    mov cx,8
    call msc_exec_in
    jc .fail


    mov al,[capacity_buf+3]
    mov ah,[capacity_buf+2]
    mov [msc_last_lba],ax
    mov al,[capacity_buf+1]
    mov ah,[capacity_buf+0]
    mov [msc_last_lba+2],ax

    mov al,[capacity_buf+7]
    mov ah,[capacity_buf+6]
    mov [msc_block_size],ax
    mov al,[capacity_buf+5]
    mov ah,[capacity_buf+4]
    mov [msc_block_size+2],ax
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret


msc_read_sector_far:
    push ax
    push bx
    push cx
    push dx
    mov [msc_lba_lo],ax
    mov [msc_lba_hi],dx
    cmp byte [storage_slot],0FFh
    je .fail
    xor ax,ax
    mov al,[storage_slot]
    mov bl,080h
    mov bh,10
    mov cx,512
    xor dx,dx
    call msc_prepare_cbw
    mov byte [cbw_buf+15],SCSI_READ10
    mov ax,[msc_lba_hi]
    mov [cbw_buf+17],ah
    mov [cbw_buf+18],al
    mov ax,[msc_lba_lo]
    mov [cbw_buf+19],ah
    mov [cbw_buf+20],al
    mov byte [cbw_buf+22],0
    mov byte [cbw_buf+23],1
    push word [xfer_seg]
    push word [xfer_off]
    xor ax,ax
    mov al,[storage_slot]
    mov cx,512
    call msc_exec_in
    pop word [xfer_off]
    pop word [xfer_seg]
    jc .fail
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.fail:
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

msc_write_sector_far:
    push ax
    push bx
    push cx
    push dx
%if ENABLE_WRITES = 0
    stc
    jmp .out
%else
    mov [msc_lba_lo],ax
    mov [msc_lba_hi],dx
    cmp byte [storage_slot],0FFh
    je .fail
    xor ax,ax
    mov al,[storage_slot]
    mov bl,000h
    mov bh,10
    mov cx,512
    xor dx,dx
    call msc_prepare_cbw
    mov byte [cbw_buf+15],SCSI_WRITE10
    mov ax,[msc_lba_hi]
    mov [cbw_buf+17],ah
    mov [cbw_buf+18],al
    mov ax,[msc_lba_lo]
    mov [cbw_buf+19],ah
    mov [cbw_buf+20],al
    mov byte [cbw_buf+22],0
    mov byte [cbw_buf+23],1
    push word [xfer_seg]
    push word [xfer_off]
    xor ax,ax
    mov al,[storage_slot]
    mov cx,512
    call msc_exec_out
    pop word [xfer_off]
    pop word [xfer_seg]
    jc .fail
    clc
    jmp .out
.fail:
    stc
%endif
.out:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

msc_sync_cache:
    push ax
    push bx
    push cx
    push dx
    cmp byte [storage_slot],0FFh
    je .fail
    xor ax,ax
    mov al,[storage_slot]
    mov bl,0
    mov bh,10
    xor cx,cx
    xor dx,dx
    call msc_prepare_cbw
    mov byte [cbw_buf+15],SCSI_SYNCHRONIZE10
    xor ax,ax
    mov al,[storage_slot]
    call msc_exec_nodata
    jmp .out
.fail:
    stc
.out:
    pop dx
    pop cx
    pop bx
    pop ax
    ret


storage_mount_volume:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    mov word [xfer_seg],cs
    mov word [xfer_off],sector_buf
    xor ax,ax
    xor dx,dx
    call storage_read_sector_far
    jc .fail
    cmp word [sector_buf+510],0AA55h
    jne .fail


    cmp word [sector_buf+11],512
    jne .mbr
    mov al,[sector_buf+21]
    cmp al,0F0h
    jb .mbr
    mov dword [part_lba],0
    call copy_bpb_from_sector
    clc
    jmp .out

.mbr:
    mov si,sector_buf+446
    mov cx,4
.find:
    mov al,[si+4]
    cmp al,01h
    je .found
    cmp al,04h
    je .found
    cmp al,06h
    je .found
    cmp al,0Eh
    je .found
    add si,16
    loop .find
    jmp .fail
.found:
    mov ax,[si+8]
    mov dx,[si+10]
    mov [part_lba],ax
    mov [part_lba+2],dx
    mov word [xfer_seg],cs
    mov word [xfer_off],sector_buf
    call storage_read_sector_far
    jc .fail
    cmp word [sector_buf+510],0AA55h
    jne .fail
    cmp word [sector_buf+11],512
    jne .fail
    call copy_bpb_from_sector
    clc
    jmp .out
.fail:
    stc
.out:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

copy_bpb_from_sector:
    push ax
    push cx
    push si
    push di
    push ds
    push es
    push cs
    pop ds
    push cs
    pop es
    mov si,sector_buf+11
    mov di,bpb
    mov cx,25
    cld
    rep movsb
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    ret


msc_verify_sector_far:
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
    call msc_read_sector_far
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
