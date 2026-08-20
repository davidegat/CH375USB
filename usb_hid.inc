; CH375USB 0.4.11 - usb_hid.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: USB HID boot keyboard/mouse polling and DOS input bridging.

hid_init_slot:
    push ax
    push bx
    push di
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_KBD
    jz .mouse
    mov bl,[di+D_KBD_ITF]
    push ax
    call hid_set_boot_protocol
    pop ax
    push ax
    call hid_set_idle0
    pop ax
.mouse:
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_MOUSE
    jz .done
    mov bl,[di+D_MOUSE_ITF]
    push ax
    call hid_set_boot_protocol
    pop ax
    push ax
    call hid_set_idle0
    pop ax
.done:
    pop di
    pop bx
    pop ax
    clc
    ret


hid_set_boot_protocol:
    mov byte [setup_buf+0],021h
    mov byte [setup_buf+1],00Bh
    mov word [setup_buf+2],0
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],0
    call usb_control_out0_slot
    ret


hid_set_idle0:
    mov byte [setup_buf+0],021h
    mov byte [setup_buf+1],00Ah
    mov word [setup_buf+2],0
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],0
    call usb_control_out0_slot
    ret


hid_poll_keyboard_slot:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    mov [xfer_slot],al
    call usb_select_slot
    jc .done
    mov bl,[di+D_KBD_EP]
    cmp bl,0
    je .done
    mov al,[di+D_KBD_TOG]
    call ch_set_in_toggle
    mov al,bl
    mov ah,PID_IN
    call ch_issue_hid_in_fast
    jc .done
    push cs
    pop es
    mov di,usb_buf
    mov cx,64
    call ch_read_packet
    cmp ax,8
    jb .done
    mov al,[xfer_slot]
    xor ah,ah
    call slot_ptr_ax
    xor byte [di+D_KBD_TOG],040h
    mov ax,0
    mov al,[xfer_slot]
    call process_keyboard_slot
.done:
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret


hid_poll_mouse_slot:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov [xfer_slot],al
    mov si,4
.poll_again:
    mov al,[xfer_slot]
    call usb_select_slot
    jc .done
    mov bl,[di+D_MOUSE_EP]
    cmp bl,0
    je .done
    mov al,[di+D_MOUSE_TOG]
    call ch_set_in_toggle
    mov al,bl
    mov ah,PID_IN
    call ch_issue_hid_in_fast
    jc .done
    push cs
    pop es
    mov di,usb_buf
    mov cx,64
    call ch_read_packet
    cmp ax,3
    jb .done
    mov al,[xfer_slot]
    xor ah,ah
    call slot_ptr_ax
    xor byte [di+D_MOUSE_TOG],040h

    ; A boot mouse with no new event may return the same buttons and zero motion.
    mov al,[usb_buf]
    and al,7
    cmp al,[mouse_buttons]
    jne .changed
    cmp byte [usb_buf+1],0
    jne .changed
    cmp byte [usb_buf+2],0
    je .done
.changed:
    call process_mouse_report
    dec si
    jnz .poll_again
.done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret


process_keyboard_slot:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    mov [xfer_slot],al


    xor bx,bx
    mov bl,[xfer_slot]
    mov si,kbd_old_mod
    add si,bx
    mov di,bx
    shl di,1
    mov dx,di
    shl di,1
    add di,dx
    add di,kbd_old_keys


    mov al,[usb_buf]
    mov ah,[si]
    mov dl,al
    xor dl,ah

    test dl,01h
    jz .m1
    mov al,01Dh
    test byte [usb_buf],01h
    jnz .m0send
    or al,080h
.m0send:
    xor ah,ah
    call inject_kbd_scan
.m1:
    test dl,02h
    jz .m2
    mov al,02Ah
    test byte [usb_buf],02h
    jnz .m1send
    or al,080h
.m1send:
    xor ah,ah
    call inject_kbd_scan
.m2:
    test dl,04h
    jz .m4
    mov al,038h
    test byte [usb_buf],04h
    jnz .m2send
    or al,080h
.m2send:
    xor ah,ah
    call inject_kbd_scan
.m4:
    test dl,010h
    jz .m5
    mov al,01Dh
    test byte [usb_buf],010h
    jnz .m4send
    or al,080h
.m4send:
    mov ah,0E0h
    call inject_kbd_scan
.m5:
    test dl,020h
    jz .m6
    mov al,036h
    test byte [usb_buf],020h
    jnz .m5send
    or al,080h
.m5send:
    xor ah,ah
    call inject_kbd_scan
.m6:
    test dl,040h
    jz .mods_done
    mov al,038h
    test byte [usb_buf],040h
    jnz .m6send
    or al,080h
.m6send:
    mov ah,0E0h
    call inject_kbd_scan
.mods_done:
    mov al,[usb_buf]
    mov [si],al


    push di
    mov cx,6
.rel_loop:
    mov al,[di]
    or al,al
    jz .rel_next
    call key_in_new_report
    jc .rel_next
    call usage_to_set1
    or al,al
    jz .rel_next
    or al,080h
    call inject_kbd_scan
.rel_next:
    inc di
    loop .rel_loop
    pop di


    mov si,usb_buf+2
    mov cx,6
.press_loop:
    mov al,[si]
    or al,al
    jz .press_next
    push di
    call key_in_old_at_di
    pop di
    jc .press_next
    call usage_to_set1
    or al,al
    jz .press_next
    call inject_kbd_scan
.press_next:
    inc si
    loop .press_loop


    push es
    push cs
    pop es
    mov si,usb_buf+2
    mov cx,6
    cld
    rep movsb
    pop es

    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret


key_in_new_report:
    push ax
    push cx
    push si
    mov ah,al
    mov si,usb_buf+2
    mov cx,6
.loop:
    cmp ah,[si]
    je .yes
    inc si
    loop .loop
    clc
    jmp .out
.yes:
    stc
.out:
    pop si
    pop cx
    pop ax
    ret


key_in_old_at_di:
    push ax
    push cx
    push si
    mov ah,al
    mov si,di
    mov cx,6
.loop:
    cmp ah,[si]
    je .yes
    inc si
    loop .loop
    clc
    jmp .out
.yes:
    stc
.out:
    pop si
    pop cx
    pop ax
    ret


usage_to_set1:
    xor ah,ah
    cmp al,04h
    jb .zero
    cmp al,1Dh
    jbe .letters
    cmp al,1Eh
    jb .special
    cmp al,27h
    jbe .numbers
.special:
    push bx
    push cx
    push si
    mov si,keymap_special
    mov cx,keymap_special_count
.loop:
    cmp al,[si]
    je .found
    add si,3
    loop .loop
    pop si
    pop cx
    pop bx
    xor ax,ax
    ret
.found:
    mov al,[si+1]
    mov ah,[si+2]
    pop si
    pop cx
    pop bx
    ret
.letters:
    sub al,04h
    xor ah,ah
    mov bx,ax
    mov al,[keymap_letters+bx]
    xor ah,ah
    ret
.numbers:
    sub al,1Eh
    xor ah,ah
    mov bx,ax
    mov al,[keymap_numbers+bx]
    xor ah,ah
    ret
.zero:
    xor ax,ax
    ret

keymap_letters:
    db 01Eh,030h,02Eh,020h,012h,021h,022h,023h
    db 017h,024h,025h,026h,032h,031h,018h,019h
    db 010h,013h,01Fh,014h,016h,02Fh,011h,02Dh
    db 015h,02Ch
keymap_numbers:
    db 002h,003h,004h,005h,006h,007h,008h,009h,00Ah,00Bh
keymap_special:
    db 028h,01Ch,000h, 029h,001h,000h, 02Ah,00Eh,000h
    db 02Bh,00Fh,000h, 02Ch,039h,000h, 02Dh,00Ch,000h
    db 02Eh,00Dh,000h, 02Fh,01Ah,000h, 030h,01Bh,000h
    db 031h,02Bh,000h, 033h,027h,000h, 034h,028h,000h
    db 035h,029h,000h, 036h,033h,000h, 037h,034h,000h
    db 038h,035h,000h, 039h,03Ah,000h
    db 03Ah,03Bh,000h, 03Bh,03Ch,000h, 03Ch,03Dh,000h
    db 03Dh,03Eh,000h, 03Eh,03Fh,000h, 03Fh,040h,000h
    db 040h,041h,000h, 041h,042h,000h, 042h,043h,000h
    db 043h,044h,000h, 044h,057h,000h, 045h,058h,000h
    db 049h,052h,0E0h, 04Ah,047h,0E0h, 04Bh,049h,0E0h
    db 04Ch,053h,0E0h, 04Dh,04Fh,0E0h, 04Eh,051h,0E0h
    db 04Fh,04Dh,0E0h, 050h,04Bh,0E0h, 051h,050h,0E0h
    db 052h,048h,0E0h
keymap_special_count equ ($-keymap_special)/3


inject_kbd_scan:
    push ax
    cmp ah,0
    je .scan
    mov al,ah
    call i8042_kbd_byte
    pop ax
    push ax
.scan:
    call i8042_kbd_byte
    pop ax
    ret


i8042_wait_input_empty:
    push ax
    push cx
    mov cx,0FFFFh
.loop:
    in al,064h
    test al,02h
    jz .ok
    loop .loop
.ok:
    pop cx
    pop ax
    ret

i8042_kbd_byte:
    push ax
    call i8042_wait_input_empty
    mov al,0D2h
    out 064h,al
    call i8042_wait_input_empty
    pop ax
    out 060h,al
    ret


process_mouse_report:
    push ax
    push bx
    push dx

    mov bl,[mouse_buttons]
    mov al,[usb_buf]
    and al,7
    mov bh,al
    xor bl,bh
    mov [mouse_buttons],bh

    xor dx,dx

    ; Movement event and counters.
    mov al,[usb_buf+1]
    cbw
    add [mouse_accum_dx],ax
    add [mouse_motion_x],ax
    or ax,ax
    jz .dy
    or dx,0001h
.dy:
    mov al,[usb_buf+2]
    cbw
    add [mouse_accum_dy],ax
    add [mouse_motion_y],ax
    or ax,ax
    jz .buttons
    or dx,0001h

.buttons:
    test bl,01h
    jz .right
    test bh,01h
    jz .left_release
    inc word [mouse_press_l]
    or dx,0002h
    jmp .right
.left_release:
    inc word [mouse_release_l]
    or dx,0004h

.right:
    test bl,02h
    jz .middle
    test bh,02h
    jz .right_release
    inc word [mouse_press_r]
    or dx,0008h
    jmp .middle
.right_release:
    inc word [mouse_release_r]
    or dx,0010h

.middle:
    test bl,04h
    jz .finish
    test bh,04h
    jz .middle_release
    inc word [mouse_press_m]
    or dx,0020h
    jmp .finish
.middle_release:
    inc word [mouse_release_m]
    or dx,0040h

.finish:
    or [mouse_event_pending],dx
    or dx,dx
    jz .done
    mov byte [mouse_pending],1
.done:
    pop dx
    pop bx
    pop ax
    ret


; CTMOUSE/MOUSE.COM is loaded after CONFIG.SYS. Once INT 33h changes from the
; boot-time vector, chain it behind our software mouse shim.
mouse_try_hook_int33:
    push ax
    push bx
    push dx
    push es

    xor ax,ax
    mov es,ax
    mov ax,[es:33h*4]
    mov bx,[es:33h*4+2]

    mov dx,cs
    cmp bx,dx
    jne .not_ours
    cmp ax,mouse_int33
    je .done
.not_ours:
    cmp ax,[base_int33_off]
    jne .install
    cmp bx,[base_int33_seg]
    je .done
.install:
    mov [old_int33_off],ax
    mov [old_int33_seg],bx
    pushf
    cli
    mov word [es:33h*4],mouse_int33
    mov [es:33h*4+2],cs
    popf
    mov byte [mouse_int33_hooked],1
.done:
    pop es
    pop dx
    pop bx
    pop ax
    ret


; INT 33h compatibility shim. The existing DOS mouse driver remains responsible
; for cursor drawing; CH375USB supplies USB-derived position/buttons/mickeys.
mouse_int33:
    cmp byte [cs:windows_active],1
    je .pass
    cmp ax,0000h
    je .reset
    cmp ax,0003h
    je .get_pos
    cmp ax,0004h
    je .set_pos
    cmp ax,0005h
    je .get_press
    cmp ax,0006h
    je .get_release
    cmp ax,0007h
    je .set_hrange
    cmp ax,0008h
    je .set_vrange
    cmp ax,000Bh
    je .get_motion
    cmp ax,000Ch
    je .set_callback
.pass:
    jmp far [cs:old_int33_off]

.reset:
    pushf
    call far [cs:old_int33_off]
    push ax
    mov word [cs:mouse_x],320
    mov word [cs:mouse_y],100
    mov word [cs:mouse_min_x],0
    mov word [cs:mouse_max_x],639
    mov word [cs:mouse_min_y],0
    mov word [cs:mouse_max_y],199
    mov word [cs:mouse_accum_dx],0
    mov word [cs:mouse_accum_dy],0
    mov word [cs:mouse_motion_x],0
    mov word [cs:mouse_motion_y],0
    mov byte [cs:mouse_buttons],0
    mov word [cs:mouse_event_pending],0
    mov word [cs:mouse_press_l],0
    mov word [cs:mouse_press_r],0
    mov word [cs:mouse_press_m],0
    mov word [cs:mouse_release_l],0
    mov word [cs:mouse_release_r],0
    mov word [cs:mouse_release_m],0
    mov word [cs:mouse_cb_mask],0
    mov word [cs:mouse_cb_off],0
    mov word [cs:mouse_cb_seg],0
    pop ax
    iret

.get_pos:
    call mouse_poll_on_demand
    xor bx,bx
    mov bl,[cs:mouse_buttons]
    mov cx,[cs:mouse_x]
    mov dx,[cs:mouse_y]
    iret

.get_press:
    call mouse_poll_on_demand
    cmp bx,0
    je .press_left
    cmp bx,1
    je .press_right
    cmp bx,2
    je .press_middle
    jmp .pass
.press_left:
    mov bx,[cs:mouse_press_l]
    mov word [cs:mouse_press_l],0
    mov cx,[cs:mouse_press_l_x]
    mov dx,[cs:mouse_press_l_y]
    jmp .button_status
.press_right:
    mov bx,[cs:mouse_press_r]
    mov word [cs:mouse_press_r],0
    mov cx,[cs:mouse_press_r_x]
    mov dx,[cs:mouse_press_r_y]
    jmp .button_status
.press_middle:
    mov bx,[cs:mouse_press_m]
    mov word [cs:mouse_press_m],0
    mov cx,[cs:mouse_press_m_x]
    mov dx,[cs:mouse_press_m_y]
.button_status:
    xor ax,ax
    mov al,[cs:mouse_buttons]
    iret

.get_release:
    call mouse_poll_on_demand
    cmp bx,0
    je .release_left
    cmp bx,1
    je .release_right
    cmp bx,2
    je .release_middle
    jmp .pass
.release_left:
    mov bx,[cs:mouse_release_l]
    mov word [cs:mouse_release_l],0
    mov cx,[cs:mouse_release_l_x]
    mov dx,[cs:mouse_release_l_y]
    jmp .button_status
.release_right:
    mov bx,[cs:mouse_release_r]
    mov word [cs:mouse_release_r],0
    mov cx,[cs:mouse_release_r_x]
    mov dx,[cs:mouse_release_r_y]
    jmp .button_status
.release_middle:
    mov bx,[cs:mouse_release_m]
    mov word [cs:mouse_release_m],0
    mov cx,[cs:mouse_release_m_x]
    mov dx,[cs:mouse_release_m_y]
    jmp .button_status

.set_pos:
    mov [cs:mouse_x],cx
    mov [cs:mouse_y],dx
    jmp far [cs:old_int33_off]

.set_hrange:
    mov [cs:mouse_min_x],cx
    mov [cs:mouse_max_x],dx
    jmp far [cs:old_int33_off]

.set_vrange:
    mov [cs:mouse_min_y],cx
    mov [cs:mouse_max_y],dx
    jmp far [cs:old_int33_off]

.get_motion:
    call mouse_poll_on_demand
    mov cx,[cs:mouse_motion_x]
    mov dx,[cs:mouse_motion_y]
    mov word [cs:mouse_motion_x],0
    mov word [cs:mouse_motion_y],0
    iret

.set_callback:
    mov [cs:mouse_cb_mask],cx
    mov [cs:mouse_cb_off],dx
    mov [cs:mouse_cb_seg],es
    iret


mouse_clear_runtime_state:
    mov byte [mouse_pending],0
    mov byte [mouse_buttons],0
    mov word [mouse_event_pending],0
    mov word [mouse_accum_dx],0
    mov word [mouse_accum_dy],0
    mov word [mouse_motion_x],0
    mov word [mouse_motion_y],0
    mov word [mouse_press_l],0
    mov word [mouse_press_r],0
    mov word [mouse_press_m],0
    mov word [mouse_release_l],0
    mov word [mouse_release_r],0
    mov word [mouse_release_m],0
    mov word [mouse_cb_mask],0
    mov word [mouse_cb_off],0
    mov word [mouse_cb_seg],0
    ret


; Applications such as EDIT query INT 33h much faster than the BIOS 18.2 Hz
; timer. Polling here makes movement and button state immediately responsive.
; The normal timer poll remains as a fallback for callback-driven programs.
mouse_poll_on_demand:
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

    cmp byte [usb_present],1
    jne .done
    cmp byte [usb_busy],0
    jne .apply

    mov byte [usb_busy],1
    xor si,si
.scan:
    cmp si,MAX_DEVICES
    jae .polled
    mov ax,si
    call slot_ptr_ax
    cmp byte [di+D_USED],1
    jne .next
    test byte [di+D_CAPS],CAP_MOUSE
    jz .next
    mov ax,si
    call hid_poll_mouse_slot
.next:
    inc si
    jmp .scan

.polled:
    mov byte [usb_busy],0

.apply:
    cmp byte [mouse_pending],1
    jne .done
    cmp byte [mouse_int33_hooked],1
    jne .done
    call mouse_apply_pending

.done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret


mouse_apply_pending:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov byte [mouse_pending],0

    mov ax,[mouse_accum_dx]
    mov word [mouse_accum_dx],0
    mov bx,[mouse_x]
    add bx,ax
    cmp bx,[mouse_min_x]
    jge .x_hi
    mov bx,[mouse_min_x]
.x_hi:
    cmp bx,[mouse_max_x]
    jle .x_store
    mov bx,[mouse_max_x]
.x_store:
    mov [mouse_x],bx

    mov ax,[mouse_accum_dy]
    mov word [mouse_accum_dy],0
    mov bx,[mouse_y]
    add bx,ax
    cmp bx,[mouse_min_y]
    jge .y_hi
    mov bx,[mouse_min_y]
.y_hi:
    cmp bx,[mouse_max_y]
    jle .y_store
    mov bx,[mouse_max_y]
.y_store:
    mov [mouse_y],bx

    ; Ask the installed DOS mouse driver to redraw its cursor at our position.
    mov ax,0004h
    mov cx,[mouse_x]
    mov dx,[mouse_y]
    pushf
    call far [old_int33_off]

    ; Button edges were captured when each HID report arrived, so a short
    ; click cannot disappear merely because press and release occurred in one batch.
    mov si,[mouse_event_pending]
    mov word [mouse_event_pending],0

    test si,0002h
    jz .no_lp
    mov ax,[mouse_x]
    mov [mouse_press_l_x],ax
    mov ax,[mouse_y]
    mov [mouse_press_l_y],ax
.no_lp:
    test si,0004h
    jz .no_lr
    mov ax,[mouse_x]
    mov [mouse_release_l_x],ax
    mov ax,[mouse_y]
    mov [mouse_release_l_y],ax
.no_lr:
    test si,0008h
    jz .no_rp
    mov ax,[mouse_x]
    mov [mouse_press_r_x],ax
    mov ax,[mouse_y]
    mov [mouse_press_r_y],ax
.no_rp:
    test si,0010h
    jz .no_rr
    mov ax,[mouse_x]
    mov [mouse_release_r_x],ax
    mov ax,[mouse_y]
    mov [mouse_release_r_y],ax
.no_rr:
    test si,0020h
    jz .no_mp
    mov ax,[mouse_x]
    mov [mouse_press_m_x],ax
    mov ax,[mouse_y]
    mov [mouse_press_m_y],ax
.no_mp:
    test si,0040h
    jz .events_done
    mov ax,[mouse_x]
    mov [mouse_release_m_x],ax
    mov ax,[mouse_y]
    mov [mouse_release_m_y],ax
.events_done:

    mov ax,[mouse_cb_mask]
    test ax,si
    jz .done
    mov ax,[mouse_cb_seg]
    or ax,[mouse_cb_off]
    jz .done

    mov ax,si
    xor bx,bx
    mov bl,[mouse_buttons]
    mov cx,[mouse_x]
    mov dx,[mouse_y]
    mov si,[mouse_motion_x]
    mov di,[mouse_motion_y]
    call far [mouse_cb_off]

.done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
