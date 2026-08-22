; CH375USB 0.5.1 - self-contained DOS/Windows INT 33h mouse core
;
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
;
; This module replaces the external CuteMouse/CTMOUSE runtime dependency.
; Architecture and API coverage are derived from studying CuteMouse 2.1 beta4
; (Copyright (c) 1997-2002 Nagy Daniel, GPL-2.0-or-later) and the documented
; Microsoft-compatible DOS INT 33h interface.  The implementation below is a
; NASM adaptation written for the CH375USB resident .SYS architecture; it does
; not embed the CuteMouse TSR loader, command-line parser, serial/UART backend,
; relocation code or asmlib macro layer.
;
; The current release keeps the validated USB + DOS PS/2 paths and adds
; a narrow Windows re-arm for the physical PS/2 BIOS callback.  Physical PS/2 setup now talks
; directly to the real BIOS (CH375USB does not proxy INT15 in internal-driver
; builds), uses the conventional 3-byte C205 initialization sequence, and keeps
; the C207 callback minimal: deposit one event and RETF.
;
; CH375USB owns the DOS INT 33h API. Text-mode cursor rendering is on-demand:
; it is activated only by INT 33h/01h from a DOS application and never shown
; merely because the driver is resident. BIOS mouse callbacks remain renderer-free.

; ---------------------------------------------------------------------------
; Initial install; optional BIOS PS/2 registration is safety-gated
; ---------------------------------------------------------------------------
mouse_internal_init:
    push ax
    push bx
    push cx
    push dx
    push ds
    push es

    push cs
    pop ds
    call mouse_internal_soft_reset
    mov byte [mouse_ps2_available],0

%if INTERNAL_MOUSE_PHYSICAL_PS2
    ; Use the real BIOS directly. Do not route these calls through
    ; bios_ps2_int15: that mirror layer is only useful with an external INT33
    ; driver, whereas our USB path already feeds mouse_core_event directly.
    call mouse_ps2_bios_prepare
    jc .ps2_done
    mov byte [mouse_ps2_available],1
.ps2_done:
%endif

    pop es
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    ret


; Prepare a conventional 3-byte BIOS PS/2 mouse in the same conservative order
; used by BIOS/PS2 mouse implementations:
;   disable/cancel -> C205 packet-size init -> C203 resolution -> probe C207 ->
;   cancel probe -> install real C207 -> enable -> C202 100 reports/s.
; CF=0 on success, CF=1 on failure.  On failure the mouse is disabled and the
; BIOS callback is cancelled so a bad partial setup cannot freeze the machine.
mouse_ps2_bios_prepare:
    push ax
    push bx
    push cx
    push dx
    push es

    ; Start from a known BIOS state. Failure here is harmless; some BIOSes
    ; report an error if the device was already disabled or no handler existed.
    mov ax,0C200h
    xor bh,bh
    int 15h
    xor bx,bx
    mov es,bx
    mov ax,0C207h
    int 15h

    ; Force the classic three-byte PS/2 packet format. This is the key setup
    ; required setup step before the first callback arrives.
    mov bh,3
    mov ax,0C205h
    int 15h
    call mouse_ps2_bios_result
    jc .fail

    ; CuteMouse's established BIOS path uses resolution code 3 (8 counts/mm).
    mov bh,3
    mov ax,0C203h
    int 15h
    call mouse_ps2_bios_result
    jc .fail

    ; Probe that C207 FAR callbacks are actually accepted, then immediately
    ; cancel the harmless RETF-only probe before installing the real handler.
    push cs
    pop es
    mov bx,internal_ps2_probe_handler
    mov ax,0C207h
    int 15h
    call mouse_ps2_bios_result
    jc .fail

    xor bx,bx
    mov es,bx
    mov ax,0C207h
    int 15h
    call mouse_ps2_bios_result
    jc .fail

    ; Install the real callback. Retry once only for BIOS status 04h (RESEND),
    ; which the IBM interface explicitly permits the caller to retry.
    push cs
    pop es
    mov bx,internal_ps2_handler
    mov ax,0C207h
    int 15h
    jnc .handler_status
    cmp ah,04h
    jne .fail
    mov ax,0C207h
    int 15h
.handler_status:
    call mouse_ps2_bios_result
    jc .fail

    ; Enable delivery only after the callback is in place.
    mov bh,1
    mov ax,0C200h
    int 15h
    call mouse_ps2_bios_result
    jc .fail

    ; Match CuteMouse's normal 100 reports/s default. If a BIOS rejects rate
    ; selection after enabling, keep the already-working mouse rather than
    ; tearing it back down: the rate is performance tuning, not correctness.
    mov bh,5
    mov ax,0C202h
    int 15h

    clc
    jmp .done

.fail:
    ; Best-effort rollback. Never leave a resident callback registered after a
    ; failed initialization sequence.
    mov ax,0C200h
    xor bh,bh
    int 15h
    xor bx,bx
    mov es,bx
    mov ax,0C207h
    int 15h
    stc
.done:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Normalize the BIOS convention used by these services: both CF and AH are
; checked because firmware exists which reports failure in only one of them.
mouse_ps2_bios_result:
    jc .bad
    test ah,ah
    jnz .bad
    clc
    ret
.bad:
    stc
    ret

; Re-arm only the already-probed physical PS/2 backend.
;
; Windows 95 enhanced-mode startup may reset/replace the BIOS C207 callback
; after the DOS CONFIG.SYS phase.  CH375MOU.DRV later registers its INT33/0Ch
; Windows event callback; that is the safe point to restore our physical PS/2
; callback.  Do NOT redo C205 packet initialization or resolution here: the
; DOS path has already proved the device, and touching the packet format during
; Windows startup is unnecessarily invasive.
;
; CF=0 if C207 + enable succeeded, CF=1 otherwise.  All general registers and
; segment registers are preserved so this is safe from the INT33/0Ch setup path.
mouse_ps2_bios_rearm:
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
    push cs
    pop es
    mov bx,internal_ps2_handler
    mov ax,0C207h
    int 15h
    call mouse_ps2_bios_result
    jc .fail

    mov bh,1
    mov ax,0C200h
    int 15h
    call mouse_ps2_bios_result
    jc .fail

    ; Rate is only tuning.  As at boot, do not fail a working re-arm merely
    ; because this particular BIOS/Windows V86 layer rejects C202.
    mov bh,5
    mov ax,0C202h
    int 15h
    clc
    jmp .done
.fail:
    stc
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

; RETF-only probe used during setup, matching the long-established CuteMouse
; presence test. The BIOS owns and removes the four parameter words.
internal_ps2_probe_handler:
    retf


; ---------------------------------------------------------------------------
; Direct CH375 USB HID -> internal INT33 feed (validated path)
;
; This path intentionally bypasses INT15/C207 entirely.  It is the safe
; isolation path after the physical PS/2 callback froze the Pocket386 on the
; first packet.  usb_buf is a boot-protocol report: buttons, signed X, signed Y.
; ---------------------------------------------------------------------------
mouse_internal_feed_hid:
    push ax
    push bx
    push dx
    push ds

    push cs
    pop ds

    xor bx,bx
    mov bl,[usb_buf]
    and bl,07h
    mov [mouse_hid_last_buttons],bl
    ; Both backends feed one INT33 device. Preserve a button held on the other
    ; mouse instead of treating each report as the complete global state.
    or bl,[mouse_ps2_last_buttons]

    mov al,[usb_buf+1]
    cbw
    push ax                         ; save signed X
    mov al,[usb_buf+2]
    cbw
    mov dx,ax                       ; signed HID Y is already positive-down
    pop ax                          ; AX = signed X

    call mouse_core_event

    pop ds
    pop dx
    pop bx
    pop ax
    ret

; If the CH375 USB mouse disappears while a button is held, generate the
; corresponding INT33 release transitions directly.
mouse_internal_release_all:
    push ax
    push bx
    push dx
    push ds
    push cs
    pop ds

    mov byte [mouse_hid_last_buttons],0
    mov bl,[mouse_ps2_last_buttons]
    cmp bl,[mouse_buttons]
    je .done
    xor ax,ax
    xor dx,dx
    call mouse_core_event
.done:
    pop ds
    pop dx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; BIOS INT15/C207 PS/2 packet callback
;
; Standard callback stack on entry (after PUSH BP / MOV BP,SP):
;   [SS:BP+2]  return IP
;   [SS:BP+4]  return CS
;   [SS:BP+6]  Z/reserved word
;   [SS:BP+8]  Y byte in low byte
;   [SS:BP+10] X byte in low byte
;   [SS:BP+12] status/buttons byte in low byte
;
; BIOS callback status uses bit0=left, bit1=right, bit2=middle, with X/Y
; sign bits in 4/5. Positive PS/2 Y is up while INT33 screen Y is down.
; ---------------------------------------------------------------------------
internal_ps2_handler:
    ; This intentionally follows the classic CuteMouse C207 stack decoder.
    ; The BIOS FAR-calls us with four parameter WORDs below the return address.
    ; We save DS/ES + PUSHA (20 bytes), then BP points at that saved frame:
    ;   BP+24 = Z/reserved, BP+26 = Y, BP+28 = X, BP+30 = status.
    ; No INT33 callback, USB access, BIOS call, DOS call or video work happens
    ; here.  We only accumulate one normalized event and RETF immediately.
    cld
    push ds
    push es
    pusha
    push cs
    pop ds
    mov bp,sp

    ; CuteMouse sign-extension trick: status bit 5 signs Y and bit 4 signs X.
    mov al,[ss:bp+30]             ; status/buttons
    mov bl,al                     ; keep status in BL
    shl al,3                      ; CF <- original Y sign; bit7 <- X sign
    sbb ch,ch                     ; CH = 00/FF for signed Y
    cbw                           ; AH = 00/FF for signed X
    mov al,[ss:bp+28]             ; AX = signed X movement
    mov cl,[ss:bp+26]             ; CX = signed Y movement
    xchg bx,ax                    ; BX = signed X, AL = original status
    neg cx                        ; PS/2 positive-up -> INT33 positive-down

    add [mouse_ps2_pending_dx],bx
    add [mouse_ps2_pending_dy],cx

    ; BIOS C207 status uses the normal DOS ordering here:
    ; bit0=left, bit1=right, bit2=middle.
    and al,07h
    mov [mouse_ps2_pending_buttons],al
    mov byte [mouse_ps2_pending],1

    popa
    pop es
    pop ds
    retf

; Drain one accumulated physical BIOS PS/2 event outside the C207 callback.
; CH375USB drains this from INT33/INT1C/INT70, outside the BIOS callback.
; Registers are preserved.  CLI is held only while taking/clearing the snapshot.
mouse_internal_drain_ps2:
    push ax
    push bx
    push dx
    push ds
    push cs
    pop ds
    pushf
    cli
    cmp byte [mouse_ps2_pending],1
    jne .none
    mov ax,[mouse_ps2_pending_dx]
    mov dx,[mouse_ps2_pending_dy]
    xor bx,bx
    mov bl,[mouse_ps2_pending_buttons]
    mov [mouse_ps2_last_buttons],bl
    or bl,[mouse_hid_last_buttons]
    mov word [mouse_ps2_pending_dx],0
    mov word [mouse_ps2_pending_dy],0
    mov byte [mouse_ps2_pending],0
    popf
    call mouse_core_event
    jmp .done
.none:
    popf
.done:
    pop ds
    pop dx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; Feed one normalized relative event into the internal INT33 state machine.
; In: AX=signed horizontal mickeys, DX=signed vertical mickeys (positive down),
;     BL=buttons L/R/M in bits 0..2.
; ---------------------------------------------------------------------------
mouse_core_event:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es

    cmp byte [mouse_driver_enabled],1
    jne .done

    mov si,ax                     ; current raw horizontal mickeys for callback
    mov di,dx                     ; current raw vertical mickeys for callback

    ; Preserve raw motion counters for INT33/0Bh.
    add [mouse_motion_x],ax
    add [mouse_motion_y],dx

    ; Build standard callback/event mask and detect button transitions.
    xor bp,bp
    or ax,ax
    jz .check_dy
    or bp,0001h
.check_dy:
    or dx,dx
    jz .buttons
    or bp,0001h

.buttons:
    mov al,[mouse_buttons]
    mov ah,bl
    and ah,07h
    mov [mouse_buttons],ah
    xor al,ah                     ; AL = changed bits

    test al,01h
    jz .right
    test ah,01h
    jz .left_up
    inc word [mouse_press_l]
    or bp,0002h
    jmp .right
.left_up:
    inc word [mouse_release_l]
    or bp,0004h

.right:
    test al,02h
    jz .middle
    test ah,02h
    jz .right_up
    inc word [mouse_press_r]
    or bp,0008h
    jmp .middle
.right_up:
    inc word [mouse_release_r]
    or bp,0010h

.middle:
    test al,04h
    jz .move
    test ah,04h
    jz .middle_up
    inc word [mouse_press_m]
    or bp,0020h
    jmp .move
.middle_up:
    inc word [mouse_release_m]
    or bp,0040h

.move:
    ; Keep fractional mickeys so slow motion is not lost when ratio > 8.
    mov ax,si
    mov cx,[mouse_ratio_x]
    call mouse_scale_with_fraction_x
    add [mouse_x],ax

    mov ax,di
    mov cx,[mouse_ratio_y]
    call mouse_scale_with_fraction_y
    add [mouse_y],ax

    call mouse_clamp_position
    call mouse_text_cursor_refresh

    ; Store position associated with each press/release transition.
    test bp,0002h
    jz .no_lp
    mov ax,[mouse_x]
    mov [mouse_press_l_x],ax
    mov ax,[mouse_y]
    mov [mouse_press_l_y],ax
.no_lp:
    test bp,0004h
    jz .no_lr
    mov ax,[mouse_x]
    mov [mouse_release_l_x],ax
    mov ax,[mouse_y]
    mov [mouse_release_l_y],ax
.no_lr:
    test bp,0008h
    jz .no_rp
    mov ax,[mouse_x]
    mov [mouse_press_r_x],ax
    mov ax,[mouse_y]
    mov [mouse_press_r_y],ax
.no_rp:
    test bp,0010h
    jz .no_rr
    mov ax,[mouse_x]
    mov [mouse_release_r_x],ax
    mov ax,[mouse_y]
    mov [mouse_release_r_y],ax
.no_rr:
    test bp,0020h
    jz .no_mp
    mov ax,[mouse_x]
    mov [mouse_press_m_x],ax
    mov ax,[mouse_y]
    mov [mouse_press_m_y],ax
.no_mp:
    test bp,0040h
    jz .callbacks
    mov ax,[mouse_x]
    mov [mouse_release_m_x],ax
    mov ax,[mouse_y]
    mov [mouse_release_m_y],ax

.callbacks:
    ; Deliver normal INT33 function 0Ch callback events.  CH375MOU.DRV uses
    ; exactly this path under Windows 95.
    mov ax,[mouse_cb_mask]
    test ax,bp
    jz .done
    mov ax,[mouse_cb_off]
    or ax,[mouse_cb_seg]
    jz .done
    cmp byte [mouse_callback_busy],0
    jne .done

    mov byte [mouse_callback_busy],1
    mov ax,bp
    xor bx,bx
    mov bl,[mouse_buttons]
    mov cx,[mouse_x]
    mov dx,[mouse_y]
    ; SI/DI already contain current raw mickeys. Preserve our resident DS even
    ; if the application/Win16 callback changes segment registers.
    push ds
    call far [mouse_cb_off]
    pop ds
    mov byte [mouse_callback_busy],0

.done:
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; AX signed raw mickeys, CX mickeys-per-8-pixels, returns AX signed pixels.
mouse_scale_with_fraction_x:
    push bx
    push dx
    mov bx,cx
    or bx,bx
    jnz .ratio_ok
    mov bx,8
.ratio_ok:
    shl ax,1
    shl ax,1
    shl ax,1
    add ax,[mouse_accum_dx]
    cwd
    idiv bx
    mov [mouse_accum_dx],dx
    pop dx
    pop bx
    ret

mouse_scale_with_fraction_y:
    push bx
    push dx
    mov bx,cx
    or bx,bx
    jnz .ratio_ok
    mov bx,16
.ratio_ok:
    shl ax,1
    shl ax,1
    shl ax,1
    add ax,[mouse_accum_dy]
    cwd
    idiv bx
    mov [mouse_accum_dy],dx
    pop dx
    pop bx
    ret

mouse_clamp_position:
    push ax
    push ds
    push cs
    pop ds
    mov ax,[mouse_x]
    cmp ax,[mouse_min_x]
    jge .x_hi
    mov ax,[mouse_min_x]
.x_hi:
    cmp ax,[mouse_max_x]
    jle .x_store
    mov ax,[mouse_max_x]
.x_store:
    mov [mouse_x],ax

    mov ax,[mouse_y]
    cmp ax,[mouse_min_y]
    jge .y_hi
    mov ax,[mouse_min_y]
.y_hi:
    cmp ax,[mouse_max_y]
    jle .y_store
    mov ax,[mouse_max_y]
.y_store:
    mov [mouse_y],ax
    pop ds
    pop ax
    ret

; ---------------------------------------------------------------------------
; DOS text-mode cursor renderer
; ---------------------------------------------------------------------------
; A DOS mouse driver is expected to draw the pointer when an application calls
; INT 33h/01h.  We deliberately do NOT draw a cursor merely because the driver
; is resident.  Rendering is limited to BIOS text modes 0,1,2,3 and 7 and uses
; only the BIOS Data Area plus direct text VRAM, so no INT 10h call is made from
; timer/INT33 paths.  Graphics applications remain responsible for their own
; pointer until a graphics renderer is added.
;
; Software text cursor semantics follow INT 33h/0Ah: screen mask in CX and
; cursor mask in DX.  Default 77FFh/7700h reverses the character attribute.

mouse_text_cursor_erase:
    pushf
    cli
    push ax
    push di
    push es
    cmp byte [cs:mouse_text_cursor_drawn],1
    jne .done
    mov ax,[cs:mouse_text_cursor_seg]
    or ax,ax
    jz .clear
    mov es,ax
    mov di,[cs:mouse_text_cursor_off]
    mov ax,[cs:mouse_text_cursor_saved]
    mov [es:di],ax
.clear:
    mov byte [cs:mouse_text_cursor_drawn],0
.done:
    pop es
    pop di
    pop ax
    popf
    ret

mouse_text_cursor_refresh:
    pushf
    cli
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es

    ; Restore the old cell before calculating the new one. This also handles
    ; hide/disable/mode changes without leaving reverse-video debris behind.
    call mouse_text_cursor_erase

    cmp byte [cs:windows_active],0
    jne .done
    cmp byte [cs:mouse_driver_enabled],1
    jne .done
    cmp word [cs:mouse_hide_count],0
    jne .done

    ; Read current text geometry from the BIOS Data Area.
    mov ax,0040h
    mov es,ax
    xor ax,ax
    mov al,[es:0049h]             ; current video mode
    cmp al,7
    je .mono
    cmp al,3
    ja .done                      ; graphics/unsupported mode: draw nothing
    mov bp,0B800h
    jmp .geometry
.mono:
    mov bp,0B000h

.geometry:
    mov cx,[es:004Ah]             ; columns (normally 40 or 80)
    or cx,cx
    jz .done
    mov si,[es:004Eh]             ; active page start offset in video RAM

    ; X coordinates use the conventional 0..639 virtual range.  80-column
    ; modes therefore use 8 virtual pixels/cell; 40-column modes use 16.
    mov ax,[cs:mouse_x]
    cmp cx,40
    jbe .x40
    shr ax,1
    shr ax,1
    shr ax,1                      ; /8
    jmp .x_ready
.x40:
    shr ax,1
    shr ax,1
    shr ax,1
    shr ax,1                      ; /16
.x_ready:
    cmp ax,cx
    jb .x_ok
    mov ax,cx
    dec ax
.x_ok:
    mov bx,ax                     ; BX = column

    ; The standard DOS mouse virtual Y range is 0..199 in text modes, giving
    ; 8 virtual pixels per row for the common 25-line modes used by EDIT.
    mov ax,[cs:mouse_y]
    shr ax,1
    shr ax,1
    shr ax,1                      ; /8 -> row

    ; EGA/VGA BIOSes expose rows-1 at BDA 40:84. Use it when sane; otherwise
    ; fall back to 25 rows for CGA/older BIOS compatibility.
    xor dx,dx
    mov dl,[es:0084h]
    cmp dl,24
    jb .rows25
    cmp dl,59
    ja .rows25
    inc dx
    jmp .rows_ready
.rows25:
    mov dx,25
.rows_ready:
    cmp ax,dx
    jb .row_ok
    mov ax,dx
    dec ax
.row_ok:

    ; offset = active_page + 2 * (row * columns + column)
    mul cx                         ; DX:AX = row * columns (small, DX=0)
    add ax,bx
    shl ax,1
    add ax,si
    mov di,ax

    mov es,bp
    mov ax,[es:di]
    mov [cs:mouse_text_cursor_saved],ax
    mov [cs:mouse_text_cursor_seg],bp
    mov [cs:mouse_text_cursor_off],di

    cmp word [cs:mouse_user_text_type],0
    jne .default_mask
    and ax,[cs:mouse_user_text_scr]
    xor ax,[cs:mouse_user_text_cur]
    jmp .write
.default_mask:
    ; Hardware-cursor requests (BX=1 in INT33/0Ah) carry scan-line values in
    ; CX/DX, not AND/XOR masks. Until a CRTC cursor backend is added, render a
    ; visible software reverse-video cell instead of misusing those values.
    and ax,077FFh
    xor ax,07700h
.write:
    mov [es:di],ax
    mov byte [cs:mouse_text_cursor_drawn],1

.done:
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    popf
    ret

; ---------------------------------------------------------------------------
; INT 33h API
; ---------------------------------------------------------------------------
mouse_int33:
    ; Under DOS, polling here gives low-latency USB response even between timer
    ; ticks. Under Windows the high-rate RTC/timer path owns USB polling.
    cmp byte [cs:windows_active],1
    je .dispatch
    push ax
    call mouse_poll_on_demand
    call mouse_internal_drain_ps2
    pop ax

.dispatch:
    cmp ax,0000h
    je .reset
    cmp ax,0001h
    je .show
    cmp ax,0002h
    je .hide
    cmp ax,0003h
    je .status
    cmp ax,0004h
    je .setpos
    cmp ax,0005h
    je .press
    cmp ax,0006h
    je .release
    cmp ax,0007h
    je .hrange
    cmp ax,0008h
    je .vrange
    cmp ax,0009h
    je .gfxcursor
    cmp ax,000Ah
    je .textcursor
    cmp ax,000Bh
    je .motion
    cmp ax,000Ch
    je .callback
    cmp ax,000Fh
    je .ratio
    cmp ax,0011h
    je .wheel_api
    cmp ax,0013h
    je .threshold
    cmp ax,0014h
    je .exchange_cb
    cmp ax,0015h
    je .storage_req
    cmp ax,001Ah
    je .set_sensitivity
    cmp ax,001Bh
    je .get_sensitivity
    cmp ax,001Eh
    je .get_page
    cmp ax,001Fh
    je .disable
    cmp ax,0020h
    je .enable
    cmp ax,0021h
    je .soft_reset
    cmp ax,0024h
    je .version
    cmp ax,0026h
    je .maxcoords
    cmp ax,0031h
    je .getrange
    cmp ax,004Dh
    je .copyright

    ; Preserve a pre-existing provider only when the saved vector is non-null.
    ; CONFIG.SYS normally runs before any TSR mouse driver, so 0000:0000 is a
    ; perfectly possible saved value and must never be jumped to.
    cmp word [cs:old_int33_seg],0
    jne .chain_old
    cmp word [cs:old_int33_off],0
    je .iret
.chain_old:
    jmp far [cs:old_int33_off]

.reset:
    call mouse_internal_soft_reset
    mov ax,0FFFFh
    mov bx,3
    iret

.show:
    cmp word [cs:mouse_hide_count],0
    je .iret
    dec word [cs:mouse_hide_count]
    cmp word [cs:mouse_hide_count],0
    jne .iret
    call mouse_text_cursor_refresh
    iret

.hide:
    cmp word [cs:mouse_hide_count],0
    jne .hide_count
    call mouse_text_cursor_erase
.hide_count:
    inc word [cs:mouse_hide_count]
    iret

.status:
    xor bx,bx
    mov bl,[cs:mouse_buttons]
    mov cx,[cs:mouse_x]
    mov dx,[cs:mouse_y]
    iret

.setpos:
    mov [cs:mouse_x],cx
    mov [cs:mouse_y],dx
    call mouse_clamp_position
    call mouse_text_cursor_refresh
    iret

.press:
    cmp bx,0
    je .press_l
    cmp bx,1
    je .press_r
    cmp bx,2
    je .press_m
    xor bx,bx
    jmp .button_common
.press_l:
    mov bx,[cs:mouse_press_l]
    mov word [cs:mouse_press_l],0
    mov cx,[cs:mouse_press_l_x]
    mov dx,[cs:mouse_press_l_y]
    jmp .button_common
.press_r:
    mov bx,[cs:mouse_press_r]
    mov word [cs:mouse_press_r],0
    mov cx,[cs:mouse_press_r_x]
    mov dx,[cs:mouse_press_r_y]
    jmp .button_common
.press_m:
    mov bx,[cs:mouse_press_m]
    mov word [cs:mouse_press_m],0
    mov cx,[cs:mouse_press_m_x]
    mov dx,[cs:mouse_press_m_y]
.button_common:
    xor ax,ax
    mov al,[cs:mouse_buttons]
    iret

.release:
    cmp bx,0
    je .release_l
    cmp bx,1
    je .release_r
    cmp bx,2
    je .release_m
    xor bx,bx
    jmp .release_common
.release_l:
    mov bx,[cs:mouse_release_l]
    mov word [cs:mouse_release_l],0
    mov cx,[cs:mouse_release_l_x]
    mov dx,[cs:mouse_release_l_y]
    jmp .release_common
.release_r:
    mov bx,[cs:mouse_release_r]
    mov word [cs:mouse_release_r],0
    mov cx,[cs:mouse_release_r_x]
    mov dx,[cs:mouse_release_r_y]
    jmp .release_common
.release_m:
    mov bx,[cs:mouse_release_m]
    mov word [cs:mouse_release_m],0
    mov cx,[cs:mouse_release_m_x]
    mov dx,[cs:mouse_release_m_y]
.release_common:
    xor ax,ax
    mov al,[cs:mouse_buttons]
    iret

.hrange:
    cmp cx,dx
    jbe .hr_ok
    xchg cx,dx
.hr_ok:
    mov [cs:mouse_min_x],cx
    mov [cs:mouse_max_x],dx
    call mouse_clamp_position
    call mouse_text_cursor_refresh
    iret

.vrange:
    cmp cx,dx
    jbe .vr_ok
    xchg cx,dx
.vr_ok:
    mov [cs:mouse_min_y],cx
    mov [cs:mouse_max_y],dx
    call mouse_clamp_position
    call mouse_text_cursor_refresh
    iret

.gfxcursor:
    ; Keep hotspot metadata for API compatibility; graphics cursor masks are not rendered yet.
    mov [cs:mouse_user_gfx_hot_x],bx
    mov [cs:mouse_user_gfx_hot_y],cx
    iret

.textcursor:
    call mouse_text_cursor_erase
    mov [cs:mouse_user_text_type],bx
    mov [cs:mouse_user_text_scr],cx
    mov [cs:mouse_user_text_cur],dx
    call mouse_text_cursor_refresh
    iret

.motion:
    mov cx,[cs:mouse_motion_x]
    mov dx,[cs:mouse_motion_y]
    mov word [cs:mouse_motion_x],0
    mov word [cs:mouse_motion_y],0
    iret

.callback:
    mov [cs:mouse_cb_mask],cx
    mov [cs:mouse_cb_off],dx
    mov [cs:mouse_cb_seg],es
%if INTERNAL_MOUSE_PHYSICAL_PS2
    ; CH375MOU.DRV installs a non-zero INT33 callback only after Windows
    ; is up.  Windows may have discarded the BIOS C207 callback during enhanced
    ; mode initialization; re-arm the physical PS/2 backend at that point so it also works
    ; in Windows while USB remained fine.  Re-arm the physical backend here,
    ; after Windows has finished its transition and after our event sink exists.
    cmp byte [cs:windows_active],1
    jne .callback_done
    or cx,cx
    jz .callback_done
    cmp byte [cs:mouse_ps2_available],1
    jne .callback_done
    call mouse_ps2_bios_rearm
.callback_done:
%endif
    iret

.ratio:
    or cx,cx
    jnz .rx_ok
    mov cx,1
.rx_ok:
    or dx,dx
    jnz .ry_ok
    mov dx,1
.ry_ok:
    mov [cs:mouse_ratio_x],cx
    mov [cs:mouse_ratio_y],dx
    mov word [cs:mouse_accum_dx],0
    mov word [cs:mouse_accum_dy],0
    iret

.wheel_api:
    mov ax,574Dh                  ; 'WM' capability signature used by CuteMouse
    xor bx,bx
    xor cx,cx                     ; no wheel support yet
    iret

.threshold:
    mov [cs:mouse_double_threshold],dx
    iret

.exchange_cb:
    ; Exchange mask and handler, matching INT33/14h convention.
    push ax
    push bx
    mov ax,[cs:mouse_cb_mask]
    mov [cs:mouse_cb_mask],cx
    mov cx,ax
    mov ax,[cs:mouse_cb_off]
    mov [cs:mouse_cb_off],dx
    mov dx,ax
    mov bx,es
    mov ax,[cs:mouse_cb_seg]
    mov [cs:mouse_cb_seg],bx
    mov es,ax
    pop bx
    pop ax
    iret

.storage_req:
    ; Save/restore functions 16h/17h are deliberately not claimed yet.
    ; Report zero storage so callers know there is no private state image.
    xor bx,bx
    iret

.set_sensitivity:
    mov [cs:mouse_sensitivity_x],bx
    mov [cs:mouse_sensitivity_y],cx
    mov [cs:mouse_sensitivity_thr],dx
    iret

.get_sensitivity:
    mov bx,[cs:mouse_sensitivity_x]
    mov cx,[cs:mouse_sensitivity_y]
    mov dx,[cs:mouse_sensitivity_thr]
    iret

.get_page:
    push es
    mov ax,0040h
    mov es,ax
    xor bx,bx
    mov bl,[es:0062h]
    pop es
    iret

.disable:
    call mouse_text_cursor_erase
    mov byte [cs:mouse_driver_enabled],0
    ; Return the previous vector in ES:BX, matching the conventional 1Fh API.
    mov bx,[cs:old_int33_off]
    mov ax,[cs:old_int33_seg]
    mov es,ax
    mov ax,001Fh
    iret

.enable:
    mov byte [cs:mouse_driver_enabled],1
    call mouse_text_cursor_refresh
    mov ax,0020h
    iret

.soft_reset:
    call mouse_internal_soft_reset
    mov ax,0FFFFh
    mov bx,3
    iret

.version:
    mov ax,0024h
    mov bx,0705h                 ; compatible reference level used by CuteMouse
    mov ch,4                     ; PS/2 mouse type
    xor cl,cl                    ; BIOS PS/2, no legacy IRQ number exposed
    iret

.maxcoords:
    xor bx,bx
    cmp byte [cs:mouse_driver_enabled],1
    je .max_enabled
    inc bx
.max_enabled:
    mov cx,[cs:mouse_max_x]
    mov dx,[cs:mouse_max_y]
    iret

.getrange:
    mov ax,[cs:mouse_min_x]
    mov bx,[cs:mouse_min_y]
    mov cx,[cs:mouse_max_x]
    mov dx,[cs:mouse_max_y]
    iret

.copyright:
    push cs
    pop es
    mov di,mouse_internal_id
    iret

.iret:
    iret

; Soft reset compatible with the common Microsoft/CuteMouse defaults used by
; DOS applications.  The resident driver remains installed and PS/2 stays on.
mouse_internal_soft_reset:
    push ax
    push ds
    push cs
    pop ds
    call mouse_text_cursor_erase
    mov word [mouse_x],320
    mov word [mouse_y],100
    mov word [mouse_min_x],0
    mov word [mouse_max_x],639
    mov word [mouse_min_y],0
    mov word [mouse_max_y],199
    mov byte [mouse_buttons],0
    mov byte [mouse_hid_last_buttons],0
    mov word [mouse_motion_x],0
    mov word [mouse_motion_y],0
    mov word [mouse_accum_dx],0
    mov word [mouse_accum_dy],0
    mov word [mouse_press_l],0
    mov word [mouse_press_r],0
    mov word [mouse_press_m],0
    mov word [mouse_release_l],0
    mov word [mouse_release_r],0
    mov word [mouse_release_m],0
    mov word [mouse_cb_mask],0
    mov word [mouse_cb_off],0
    mov word [mouse_cb_seg],0
    mov word [mouse_ratio_x],8
    mov word [mouse_ratio_y],16
    mov word [mouse_double_threshold],64
    mov word [mouse_sensitivity_x],50
    mov word [mouse_sensitivity_y],50
    mov word [mouse_sensitivity_thr],64
    mov word [mouse_user_gfx_hot_x],0
    mov word [mouse_user_gfx_hot_y],0
    mov word [mouse_user_text_type],0
    mov word [mouse_user_text_scr],077FFh
    mov word [mouse_user_text_cur],07700h
    mov byte [mouse_text_cursor_drawn],0
    mov word [mouse_text_cursor_seg],0
    mov word [mouse_text_cursor_off],0
    mov word [mouse_text_cursor_saved],0
    mov word [mouse_hide_count],1
    mov byte [mouse_driver_enabled],1
    mov word [mouse_ps2_pending_dx],0
    mov word [mouse_ps2_pending_dy],0
    mov byte [mouse_ps2_pending_buttons],0
    mov byte [mouse_ps2_last_buttons],0
    mov byte [mouse_ps2_pending],0
    pop ds
    pop ax
    ret

; Used at Windows enter/exit: clear transient input/callback state without ever
; uninstalling INT33 or the physical PS/2 BIOS callback.
mouse_clear_runtime_state_keep_driver:
    call mouse_text_cursor_erase
    mov byte [mouse_pending],0
    mov byte [mouse_buttons],0
    mov byte [mouse_hid_last_buttons],0
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
    mov byte [mouse_callback_busy],0
    mov word [mouse_ps2_pending_dx],0
    mov word [mouse_ps2_pending_dy],0
    mov byte [mouse_ps2_pending_buttons],0
    mov byte [mouse_ps2_last_buttons],0
    mov byte [mouse_ps2_pending],0
    ret

mouse_internal_id:
    db 'CH375USB 0.5.1 internal INT33 core; USB direct + BIOS PS2 Win rearm',0
