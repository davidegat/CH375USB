; CH375USB 0.5.0 - bios_mouse.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
;
; Virtual PS/2 pointing-device BIOS interface for USB HID mice.
;
; This is an original NASM implementation based on the public IBM/AMI
; INT 15h AH=C2h interface specification.  It intentionally does not contain
; or translate code from Bret Johnson's copyrighted freeware USBMOUSE driver.
;
; Design:
;   USB HID boot-mouse report -> INT 15h/C207 registered FAR callback
;   CuteMouse (or another BIOS-PS/2-aware DOS mouse driver) owns INT 33h.
;
; Conservative build: Windows enhanced mode chains C2xx to the real BIOS.
; WIN_MOUSE_INT33_BRIDGE build: keep this virtual BIOS interface alive so a
; DOS INT33 driver (e.g. CuteMouse) can continue feeding a Win16 mouse driver.
; Neither mode stuffs AUX bytes into the physical 8042 output buffer.

%define BIOS_PS2_ERR_INVALID_FUNCTION 01h
%define BIOS_PS2_ERR_INVALID_INPUT    02h
%define BIOS_PS2_ERR_INTERFACE        03h
%define BIOS_PS2_ERR_RESEND           04h
%define BIOS_PS2_ERR_NO_HANDLER       05h

; ---------------------------------------------------------------------------
; INT 15h AH=C2h hook
; ---------------------------------------------------------------------------
; We virtualize the standard subfunctions 00h..07h while DOS owns the real-mode
; environment.  All other INT 15h traffic, and all C2xx traffic while Windows
; enhanced mode is active, is passed to the original BIOS.
;
; On entry to an interrupt handler, after PUSH BP / MOV BP,SP:
;   [SS:BP+2] = return IP
;   [SS:BP+4] = return CS
;   [SS:BP+6] = saved FLAGS
; so carry can be returned by editing the saved FLAGS word before IRET.
bios_ps2_int15:
    cmp ah,0C2h
    jne .chain
%if WIN_MOUSE_INT33_BRIDGE = 0
    cmp byte [cs:windows_active],1
    je .chain
%endif
    cmp al,07h
    ja .chain

    push bp
    mov bp,sp

    ; Preserve the caller's original C2xx request while the real BIOS runs.
    ; These saved registers also let us retry the same request against the
    ; virtual USB backend if the physical BIOS reports an error.
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    ; Mirror CuteMouse's configuration into our USB side, but do not consume
    ; the request.  The physical BIOS remains the primary PS/2 owner.  This is
    ; what lets a real PS/2 mouse and the CH375 USB mouse share one INT 33h
    ; driver instead of CH375USB replacing the physical device.
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
    call bios_mouse_refresh_usb_available
    call bios_ps2_mirror_request
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax

    ; Give the real BIOS the request first.  PUSHF + FAR CALL is the standard
    ; way to invoke a chained interrupt handler that returns with IRET.
    pushf
    call far [cs:old_int15_off]
    jnc .real_ok

    ; If no USB mouse exists, preserve the physical BIOS failure exactly.
    ; When a USB mouse is present, however, retry the original request against
    ; our virtual backend so USB-only systems still work.
    cmp byte [cs:bios_mouse_usb_available],1
    jne .real_error

    ; Restore the original request registers saved before the BIOS call.
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax

    push ds
    push si
    push di
    push cs
    pop ds
    call bios_ps2_dispatch
    jc .virtual_error
    and word [ss:bp+6],0FFFEh
    jmp .virtual_return
.virtual_error:
    or word [ss:bp+6],0001h
.virtual_return:
    pop di
    pop si
    pop ds
    pop bp
    iret

.real_ok:
    ; Keep all result registers returned by the physical BIOS; only discard
    ; our saved copy of the input request and reflect CF=0 to the caller.
    mov sp,bp
    and word [ss:bp+6],0FFFEh
    pop bp
    iret

.real_error:
    ; Same for a real-BIOS failure when there is no USB mouse to fall back to.
    mov sp,bp
    or word [ss:bp+6],0001h
    pop bp
    iret

.chain:
    jmp far [cs:old_int15_off]


; Refresh whether at least one enumerated CH375 device exposes a mouse HID
; interface.  DS must address the resident driver segment.  All registers used
; here are preserved because this helper runs while proxying BIOS calls.
bios_mouse_refresh_usb_available:
    push ax
    push cx
    push di

    mov byte [bios_mouse_usb_available],0
    mov cx,MAX_DEVICES
    mov di,devices
.scan:
    cmp byte [di+D_USED],0
    je .next
    test byte [di+D_CAPS],CAP_MOUSE
    jz .next
    mov byte [bios_mouse_usb_available],1
    jmp .done
.next:
    add di,DEV_SIZE
    loop .scan
.done:
    pop di
    pop cx
    pop ax
    ret


; Mirror state-changing C2xx requests so USB events can be delivered through
; the same callback that CuteMouse registered with the physical BIOS.  This
; function deliberately does not produce a BIOS return value; the real BIOS is
; called afterwards and remains authoritative whenever it succeeds.
; DS must address the resident driver segment.  ES is the caller's original ES.
bios_ps2_mirror_request:
    cmp al,00h
    je .enable_disable
    cmp al,01h
    je .reset
    cmp al,02h
    je .sample
    cmp al,03h
    je .resolution
    cmp al,05h
    je .initialize
    cmp al,06h
    je .extended
    cmp al,07h
    je .set_handler
    ret

.enable_disable:
    cmp bh,01h
    ja .done
    or bh,bh
    jz .disable
    mov si,[bios_mouse_cb_off]
    or si,[bios_mouse_cb_seg]
    jz .done
    mov byte [bios_mouse_enabled],1
    ret
.disable:
    mov byte [bios_mouse_enabled],0
    ret

.reset:
    mov byte [bios_mouse_enabled],0
    mov byte [bios_mouse_sample_code],5
    mov byte [bios_mouse_resolution],2
    mov byte [bios_mouse_scaling2],0
    mov byte [bios_mouse_last_buttons],0
    ret

.sample:
    cmp bh,6
    ja .done
    mov [bios_mouse_sample_code],bh
    ret

.resolution:
    cmp bh,3
    ja .done
    mov [bios_mouse_resolution],bh
    ret

.initialize:
    cmp bh,1
    jb .done
    cmp bh,8
    ja .done
    mov [bios_mouse_packet_size],bh
    mov byte [bios_mouse_enabled],0
    mov byte [bios_mouse_sample_code],5
    mov byte [bios_mouse_resolution],2
    mov byte [bios_mouse_scaling2],0
    mov byte [bios_mouse_last_buttons],0
    ret

.extended:
    cmp bh,1
    je .scale_1
    cmp bh,2
    je .scale_2
    ret
.scale_1:
    mov byte [bios_mouse_scaling2],0
    ret
.scale_2:
    mov byte [bios_mouse_scaling2],1
    ret

.set_handler:
    mov [bios_mouse_cb_off],bx
    mov si,es
    mov [bios_mouse_cb_seg],si
    or si,bx
    jnz .done
    mov byte [bios_mouse_enabled],0
.done:
    ret


bios_ps2_dispatch:
    cmp al,00h
    je .enable_disable
    cmp al,01h
    je .reset
    cmp al,02h
    je .sample
    cmp al,03h
    je .resolution
    cmp al,04h
    je .get_id
    cmp al,05h
    je .initialize
    cmp al,06h
    je .extended
    cmp al,07h
    je .set_handler
    mov ah,BIOS_PS2_ERR_INVALID_FUNCTION
    stc
    ret

.enable_disable:
    cmp bh,01h
    ja .bad_input
    or bh,bh
    jz .disable

    mov si,[bios_mouse_cb_off]
    or si,[bios_mouse_cb_seg]
    jz .no_handler
    mov byte [bios_mouse_enabled],1
    xor ah,ah
    clc
    ret
.disable:
    mov byte [bios_mouse_enabled],0
    xor ah,ah
    clc
    ret

.reset:
    ; Reset keeps the configured packet size but restores all other defaults.
    mov byte [bios_mouse_enabled],0
    mov byte [bios_mouse_sample_code],5       ; 100 reports/s
    mov byte [bios_mouse_resolution],2        ; 4 counts/mm
    mov byte [bios_mouse_scaling2],0          ; 1:1
    mov byte [bios_mouse_last_buttons],0
    xor ah,ah
    xor bh,bh                                  ; standard PS/2 device ID 00h
    mov bl,0AAh                                ; conventional mouse BAT/type byte
    clc
    ret

.sample:
    cmp bh,6
    ja .bad_input
    mov [bios_mouse_sample_code],bh
    xor ah,ah
    clc
    ret

.resolution:
    cmp bh,3
    ja .bad_input
    mov [bios_mouse_resolution],bh
    xor ah,ah
    clc
    ret

.get_id:
    xor bh,bh                                  ; standard PS/2 mouse, no wheel yet
    xor ah,ah
    clc
    ret

.initialize:
    ; The BIOS specification allows packet sizes 1..8.  The current USB HID
    ; bridge emits the standard three-byte PS/2 data model, but accepting the
    ; documented range makes initialization tolerant of mouse-driver probing.
    cmp bh,1
    jb .bad_input
    cmp bh,8
    ja .bad_input
    mov [bios_mouse_packet_size],bh
    mov byte [bios_mouse_enabled],0
    mov byte [bios_mouse_sample_code],5
    mov byte [bios_mouse_resolution],2
    mov byte [bios_mouse_scaling2],0
    mov byte [bios_mouse_last_buttons],0
    xor ah,ah
    clc
    ret

.extended:
    cmp bh,2
    ja .bad_input
    cmp bh,0
    je .get_status
    cmp bh,1
    je .scale_1
    mov byte [bios_mouse_scaling2],1
    xor ah,ah
    clc
    ret
.scale_1:
    mov byte [bios_mouse_scaling2],0
    xor ah,ah
    clc
    ret

.get_status:
    ; C206/BH=0 status has a different button layout from the callback status:
    ; BL bit0 = right, bit2 = left, bit4 = 2:1 scaling, bit5 = enabled.
    xor bx,bx
    mov al,[bios_mouse_last_buttons]
    test al,02h
    jz .status_left
    or bl,01h
.status_left:
    test al,01h
    jz .status_scale
    or bl,04h
.status_scale:
    cmp byte [bios_mouse_scaling2],0
    je .status_enabled
    or bl,10h
.status_enabled:
    cmp byte [bios_mouse_enabled],0
    je .status_resolution
    or bl,20h
.status_resolution:
    mov cl,[bios_mouse_resolution]
    xor si,si
    mov al,[bios_mouse_sample_code]
    mov si,ax
    and si,00FFh
    mov dl,[cs:bios_mouse_sample_hz+si]
    xor ah,ah
    clc
    ret

.set_handler:
    ; Standard interface: ES:BX = FAR callback, 0000:0000 cancels it.
    mov [bios_mouse_cb_off],bx
    mov si,es
    mov [bios_mouse_cb_seg],si
    or si,bx
    jnz .handler_set
    mov byte [bios_mouse_enabled],0
.handler_set:
    xor ah,ah
    clc
    ret

.bad_input:
    mov ah,BIOS_PS2_ERR_INVALID_INPUT
    stc
    ret
.no_handler:
    mov ah,BIOS_PS2_ERR_NO_HANDLER
    stc
    ret


bios_mouse_sample_hz:
    db 10,20,40,60,80,100,200


; ---------------------------------------------------------------------------

; Apply the PS/2 resolution/scaling state mirrored from CuteMouse to a signed
; USB HID boot-mouse delta.  USB boot reports carry device-native signed counts;
; treating 4 counts/mm (PS/2 resolution code 2) as the neutral baseline lets the
; virtual device honor the same 1/2/4/8-count/mm request as the physical mouse.
; AL = signed delta, returns AL signed/clamped to -127..127.  DS must be resident.
bios_mouse_scale_axis:
    push bx
    cbw

    mov bl,[bios_mouse_resolution]
    cmp bl,2
    je .resolution_done
    ja .resolution_double
    cmp bl,1
    je .resolution_half
    ; code 0: 1 count/mm relative to neutral code 2 (4 count/mm)
    sar ax,1
.resolution_half:
    ; code 1: 2 count/mm
    sar ax,1
    jmp .resolution_done
.resolution_double:
    ; code 3: 8 count/mm
    shl ax,1

.resolution_done:
    cmp byte [bios_mouse_scaling2],0
    je .clamp

    ; Standard PS/2 non-linear 2:1 scaling: 0->0, 1->1, 2->1, 3->3,
    ; 4->6, 5->9, >5->2*n. Apply symmetrically to negative motion.
    xor bh,bh
    test ax,ax
    jns .scale_mag
    neg ax
    mov bh,1
.scale_mag:
    cmp ax,1
    jbe .scale_sign
    cmp ax,2
    jne .scale_3
    mov ax,1
    jmp .scale_sign
.scale_3:
    cmp ax,3
    je .scale_sign
    cmp ax,4
    jne .scale_5
    mov ax,6
    jmp .scale_sign
.scale_5:
    cmp ax,5
    jne .scale_large
    mov ax,9
    jmp .scale_sign
.scale_large:
    shl ax,1
.scale_sign:
    or bh,bh
    jz .clamp
    neg ax

.clamp:
    cmp ax,127
    jle .clamp_low
    mov ax,127
    jmp .done
.clamp_low:
    cmp ax,-127
    jge .done
    mov ax,-127
.done:
    pop bx
    ret

; USB HID -> BIOS PS/2 callback bridge
; ---------------------------------------------------------------------------
; usb_buf contains a boot-protocol mouse report:
;   byte 0 buttons (L/R/M)
;   byte 1 signed X delta
;   byte 2 signed Y delta, positive down
;
; The INT 15h/C207 callback receives four WORD parameters on the stack:
;   status, X, Y, Z(0)
; and returns with RETF without removing them.
bios_mouse_feed_report:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es

    push cs
    pop ds

    mov bl,[usb_buf]
    and bl,07h
    mov [bios_mouse_last_buttons],bl
    ; hid_poll_mouse_slot compares against mouse_buttons to suppress unchanged
    ; zero-motion reports. 0.5.0 forgot to update it in the BIOS backend, which
    ; could duplicate presses and miss a button-only release.
    mov [mouse_buttons],bl

    ; Keep current state even while disabled, but do not issue callbacks.
    cmp byte [bios_mouse_enabled],1
    jne .done
    mov ax,[bios_mouse_cb_off]
    or ax,[bios_mouse_cb_seg]
    jz .done
    cmp byte [bios_mouse_in_callback],0
    jne .done

    ; Honor the same PS/2 resolution/scaling requested by CuteMouse for both
    ; the physical mouse and our USB-backed virtual mouse.
    mov al,[usb_buf+1]
    call bios_mouse_scale_axis
    xor cx,cx
    mov cl,al

    ; HID Y is positive down; PS/2 Y is positive up.
    mov al,[usb_buf+2]
    call bios_mouse_scale_axis
    cbw
    neg ax
    cmp ax,127
    jle .y_low
    mov ax,127
    jmp .y_ready
.y_low:
    cmp ax,-127
    jge .y_ready
    mov ax,-127
.y_ready:
    xor dx,dx
    mov dl,al

    ; Standard callback status byte: bit3 is always one, bits0..2 buttons,
    ; bit4 X sign, bit5 Y sign.  Deltas are clamped so overflow bits stay clear.
    xor bh,bh
    or bl,08h
    test cl,080h
    jz .x_sign_done
    or bl,010h
.x_sign_done:
    test dl,080h
    jz .y_sign_done
    or bl,020h
.y_sign_done:

    mov byte [bios_mouse_in_callback],1

    ; Parameters are pushed in logical order; CALL FAR then places return CS:IP
    ; above them exactly as a BIOS PS/2 handler expects.
    push bx                                     ; status word
    push cx                                     ; X word, high byte 00h
    push dx                                     ; Y word, high byte 00h
    push word 0                                 ; Z/reserved
    call far [cs:bios_mouse_cb_off]
    add sp,8

    mov byte [bios_mouse_in_callback],0
.done:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret


; If a USB mouse disappears while a button is down, synthesize a zero-motion
; release so CuteMouse/games cannot retain a stuck button indefinitely.
bios_mouse_release_all:
    push ax
    push bx
    push cx
    push dx
    push ds

    push cs
    pop ds
    mov byte [mouse_buttons],0
    cmp byte [bios_mouse_last_buttons],0
    je .done
    mov byte [bios_mouse_last_buttons],0
    cmp byte [bios_mouse_enabled],1
    jne .done
    mov ax,[bios_mouse_cb_off]
    or ax,[bios_mouse_cb_seg]
    jz .done
    cmp byte [bios_mouse_in_callback],0
    jne .done

    mov byte [bios_mouse_in_callback],1
    mov bx,0008h
    xor cx,cx
    xor dx,dx
    push bx
    push cx
    push dx
    push word 0
    call far [cs:bios_mouse_cb_off]
    add sp,8
    mov byte [bios_mouse_in_callback],0
.done:
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    ret
