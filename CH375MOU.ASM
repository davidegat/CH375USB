; CH375MOU.DRV - minimal Win16 mouse driver bridge for CH375USB internal INT33
; Independent implementation using the documented Windows mouse-driver ABI
; and documented DOS INT 33h mouse callback interface.
;
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
;
; Build with Open Watcom WASM + WLINK (see build.sh).
; Windows hotplug remains incremental inside the unified CH375USB.SYS;
; pointer-speed handling is normalized so Windows owns acceleration.
; The older protected-mode idle/DPMI bridge remains assembled only as inert
; compatibility code and is no longer installed by Enable().

.286

DATASEG SEGMENT WORD PUBLIC 'DATA'
; Writable Win16 state.  This MUST NOT live in a CODE segment: Windows 95
; gives code selectors execute/read attributes and writing through CS faults
; with #GP (fatal exception 0D).
event_proc      DD 0
driver_enabled  DB 0
prev_pos_x      DW 0
prev_pos_y      DW 0

; Protected-mode INT 2Fh idle hook state.  Windows documents AX=1689h as the
; kernel-idle notification and requires the ISR to live in a FIXED driver/DLL
; segment. CALLBACKS is PRELOAD FIXED in CH375MOU.LNK.
old_int2f       DD 0
int2f_hooked    DB 0
ch375_bridge    DB 0
idle_divider    DB 0
idle_call_busy  DB 0

; DPMI 0.9 real-mode call structure (52 bytes) for INT 31h AX=0300h.
; We use it to simulate a real-mode INT 2Fh so the protected-mode mouse driver
; can explicitly reach the resident CH375USB.SYS multiplex handler.
rmcs            DB 52 DUP (0)
DATASEG ENDS

DGROUP GROUP DATASEG

CALLBACKS SEGMENT WORD PUBLIC 'CODE'
ASSUME CS:CALLBACKS, DS:DGROUP

PUBLIC Initialize
PUBLIC WEP
PUBLIC Inquire
PUBLIC Enable
PUBLIC Disable
PUBLIC MouseGetIntVect


; ---------------------------------------------------------------------------
; Win16 raw DLL/driver initialization entry point.
;
; This is NOT C LibMain.  Because CH375MOU is pure assembly there is no
; Open-Watcom DLL startup stub in front of us.  The Win16 driver loader calls
; the NE entry point directly with:
;   CX    = heap size
;   DI    = module handle
;   DS    = automatic data segment
;   ES:SI = command line
; Return AX != 0 for successful initialization.
; ---------------------------------------------------------------------------
Initialize PROC FAR
    mov ax,1
    retf
Initialize ENDP

; int FAR PASCAL WEP(int)
WEP PROC FAR
    push ds
    mov ax,SEG event_proc
    mov ds,ax
    call UnhookIdleInt2F
    pop ds
    mov ax,1
    retf 2
WEP ENDP

; ---------------------------------------------------------------------------
; WORD FAR PASCAL Inquire(LPMOUSEINFO p)
;
; MOUSEINFO (Windows DDK):
;   +0 char  msExist
;   +1 char  msRelative
;   +2 short msNumButtons
;   +4 short msRate
;   +6 short msXThreshold
;   +8 short msYThreshold
;  +10 short msXRes
;  +12 short msYRes
;  =14 bytes
; ---------------------------------------------------------------------------
Inquire PROC FAR
    push bp
    mov bp,sp
    push ds
    push es
    push di

    mov ax,SEG event_proc
    mov ds,ax

    mov di,[bp+6]          ; far pointer offset
    mov es,[bp+8]          ; far pointer segment
    mov byte ptr es:[di+0],1
    mov byte ptr es:[di+1],0       ; standard relative mouse
    mov word ptr es:[di+2],3       ; three buttons
    mov word ptr es:[di+4],100     ; advertised maximum event rate
    ; Microsoft sample MOUSE.DRV uses 2/2 here.  Zero explicitly disables
    ; Windows mouse acceleration, which made the Win95 Motion slider largely
    ; ineffective with our bridge.
    mov word ptr es:[di+6],2
    mov word ptr es:[di+8],2
    mov word ptr es:[di+10],0
    mov word ptr es:[di+12],0

    mov ax,14
    pop di
    pop es
    pop ds
    pop bp
    retf 4
Inquire ENDP

; ---------------------------------------------------------------------------
; VOID FAR PASCAL Enable(lpEventProc)
; Registers an INT 33h function 0Ch callback with the existing DOS mouse
; driver. CH375USB.SYS keeps the USB mouse path alive while Windows runs;
; CH375USB now delivers the normal INT 33h callbacks directly.
; ---------------------------------------------------------------------------
Enable PROC FAR
    push bp
    mov bp,sp
    push ax
    push bx
    push cx
    push dx
    push ds
    push es

    mov ax,SEG event_proc
    mov ds,ax

    ; A FAR pointer is two words.  Store it atomically with respect to the
    ; INT33 callback, preserving the caller's original interrupt flag.
    pushf
    cli
    mov ax,[bp+6]
    mov word ptr [event_proc],ax
    mov ax,[bp+8]
    mov word ptr [event_proc+2],ax
    mov word ptr [prev_pos_x],0
    mov word ptr [prev_pos_y],0
    popf

    ; Verify/reset the resident DOS mouse driver. AX=FFFFh indicates present.
    xor ax,ax
    int 33h
    cmp ax,0FFFFh
    jne short enable_fail

    ; Match the conventional INT33 Win16 bridge setup: reset leaves the
    ; logical coordinate windows driver-defined, so normalize both axes.
    mov ax,0007h
    xor cx,cx
    mov dx,07FFFh
    int 33h
    mov ax,0008h
    xor cx,cx
    mov dx,07FFFh
    int 33h

    ; Windows 95 expects the Windows mouse driver to deliver relative motion
    ; and then applies its own Control Panel acceleration.  Because this bridge
    ; uses the resident driver's processed CX/DX coordinates, make the DOS layer neutral
    ; and deliberately slower while Windows owns the mouse:
    ;   INT33/0Fh: 16 horizontal / 32 vertical mickeys per 8 pixels
    ;               (2x the conventional 8/16 defaults -> ~half base speed)
    ;   INT33/13h: max double-speed threshold so the DOS layer does not pre-
    ;               accelerate motion before USER.DLL sees it.
    mov ax,000Fh
    mov cx,16
    mov dx,32
    int 33h
    mov ax,0013h
    mov dx,07FFFh
    int 33h

    ; Seed the coordinate baseline after reset/window setup. The resident core applies
    ; its mickey/pixel ratio and sensitivity to CX/DX; using position deltas
    ; here avoids feeding raw high-resolution mickeys straight to Windows.
    mov ax,0003h
    int 33h
    mov [prev_pos_x],cx
    mov [prev_pos_y],dx

    ; Request all standard motion/button callback events.
    mov ax,000Ch
    mov cx,007Fh
    push cs
    pop es
    mov dx,OFFSET Int33Callback
    int 33h
    mov byte ptr [driver_enabled],1

    ; No protected-mode idle/DPMI hotplug worker. CH375USB advances its
    ; CH375 enumeration state machine one bounded step per timer tick.
    mov byte ptr [ch375_bridge],0
    jmp short enable_done

enable_fail:
    pushf
    cli
    mov word ptr [event_proc],0
    mov word ptr [event_proc+2],0
    mov byte ptr [driver_enabled],0
    popf

enable_done:
    pop es
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    pop bp
    retf 4
Enable ENDP

; ---------------------------------------------------------------------------
; VOID FAR PASCAL Disable(void)
; Remove our INT33 callback without touching the USB controller.
; ---------------------------------------------------------------------------
Disable PROC FAR
    push ax
    push cx
    push dx
    push ds
    push es

    mov ax,SEG event_proc
    mov ds,ax

    call UnhookIdleInt2F
    mov byte ptr [ch375_bridge],0
    mov byte ptr [idle_divider],0
    mov byte ptr [idle_call_busy],0

    cmp byte ptr [driver_enabled],0
    je short disable_done

    ; Restore conventional DOS mouse motion parameters before returning control
    ; to DOS.  Function 13h with DX=0 restores the standard double-speed
    ; threshold; function 0Fh restores the conventional 8/16 ratio.
    mov ax,000Fh
    mov cx,8
    mov dx,16
    int 33h
    mov ax,0013h
    xor dx,dx
    int 33h

    mov ax,000Ch
    xor cx,cx
    xor dx,dx
    xor bx,bx
    mov es,bx
    int 33h

    pushf
    cli
    mov byte ptr [driver_enabled],0
    mov word ptr [event_proc],0
    mov word ptr [event_proc+2],0
    mov word ptr [prev_pos_x],0
    mov word ptr [prev_pos_y],0
    popf

disable_done:
    pop es
    pop ds
    pop dx
    pop cx
    pop ax
    retf
Disable ENDP

; ---------------------------------------------------------------------------
; int FAR PASCAL MouseGetIntVect(void)
; Windows 3.1+ asks which DOS interrupt vector backs this mouse driver.
; ---------------------------------------------------------------------------
MouseGetIntVect PROC FAR
    mov ax,0033h
    retf
MouseGetIntVect ENDP

; ---------------------------------------------------------------------------
; DPMI protected -> real-mode bridge.
;
; Input:  BX = CH375 private function
; Output: AX/BX = values returned by the real-mode INT 2Fh handler
;         CF set if DPMI could not simulate the interrupt
; DS must address DATASEG.
; ---------------------------------------------------------------------------
DpmiCh375Call PROC NEAR
    push cx
    push dx
    push si
    push di
    push es

    mov si,bx

    ; Clear the 52-byte DPMI real-mode register image.
    push ds
    pop es
    mov di,OFFSET rmcs
    xor ax,ax
    mov cx,26
    pushf
    cld
    rep stosw
    popf

    ; Real-mode EBX low word = private function; EAX low word = signature.
    mov word ptr [rmcs+10h],si
    mov word ptr [rmcs+12h],0
    mov word ptr [rmcs+1Ch],0C375h
    mov word ptr [rmcs+1Eh],0

    ; DPMI 0.9: Simulate Real Mode Interrupt 2Fh.
    ; SS:SP are zero in RMCS, so the host supplies a temporary real-mode stack.
    push ds
    pop es
    mov di,OFFSET rmcs
    mov ax,0300h
    mov bx,002Fh
    xor cx,cx
    int 31h
    jc short dpmi_call_fail

    mov ax,word ptr [rmcs+1Ch]
    mov bx,word ptr [rmcs+10h]
    clc
    jmp short dpmi_call_done

dpmi_call_fail:
    xor ax,ax
    mov bx,0FFFFh
    stc

dpmi_call_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    ret
DpmiCh375Call ENDP

; ---------------------------------------------------------------------------
; Install/remove a protected-mode INT 2Fh hook using the documented DOS vector
; calls. Microsoft documents this exact mechanism for receiving Windows 1689h
; from a FIXED Win16 driver/DLL segment.
; DS must address DATASEG.
; ---------------------------------------------------------------------------
HookIdleInt2F PROC NEAR
    cmp byte ptr [int2f_hooked],0
    jne short hook_2f_done

    push ax
    push bx
    push dx
    push es

    mov ax,352Fh
    int 21h
    mov word ptr [old_int2f],bx
    mov ax,es
    mov word ptr [old_int2f+2],ax

    push ds
    push cs
    pop ds
    mov dx,OFFSET Int2fIdleHook
    mov ax,252Fh
    int 21h
    pop ds

    mov byte ptr [int2f_hooked],1
    mov byte ptr [idle_divider],0
    mov byte ptr [idle_call_busy],0

    pop es
    pop dx
    pop bx
    pop ax
hook_2f_done:
    ret
HookIdleInt2F ENDP

UnhookIdleInt2F PROC NEAR
    cmp byte ptr [int2f_hooked],0
    je short unhook_2f_done

    push ax
    push bx
    push cx
    push dx
    push es

    ; Restore only if we are still the top protected-mode INT 2Fh handler.
    mov ax,352Fh
    int 21h
    mov dx,bx
    mov ax,es
    mov cx,cs
    cmp ax,cx
    jne short unhook_2f_mark
    cmp dx,OFFSET Int2fIdleHook
    jne short unhook_2f_mark

    push ds
    mov dx,word ptr [old_int2f]
    mov ax,word ptr [old_int2f+2]
    mov ds,ax
    mov ax,252Fh
    int 21h
    pop ds

unhook_2f_mark:
    mov byte ptr [int2f_hooked],0

    pop es
    pop dx
    pop cx
    pop bx
    pop ax
unhook_2f_done:
    ret
UnhookIdleInt2F ENDP

; ---------------------------------------------------------------------------
; Protected-mode Windows kernel-idle ISR (INT 2Fh AX=1689h).
;
; Keep this in CALLBACKS: the linker marks that segment PRELOAD FIXED.  We
; throttle the protected->real transition and ask CH375USB to do work only when
; it has a pending CONNECT. The real-mode service itself is guarded by usb_busy
; and win_hotplug_busy.
; ---------------------------------------------------------------------------
Int2fIdleHook PROC FAR
    ; Save caller DS first.  The chain epilogue uses RETF 2 so the old handler
    ; receives the original interrupt frame with every caller register intact.
    push ds
    push ax
    mov ax,SEG event_proc
    mov ds,ax
    pop ax

    cmp ax,1689h
    jne short idle_2f_chain
    cmp byte ptr [driver_enabled],1
    jne short idle_2f_chain
    cmp byte ptr [ch375_bridge],1
    jne short idle_2f_chain
    cmp byte ptr [idle_call_busy],0
    jne short idle_2f_chain

    ; Avoid a protected->real mode transition on every idle-loop iteration.
    ; Windows can issue 1689h very frequently; service one out of 64 calls.
    inc byte ptr [idle_divider]
    test byte ptr [idle_divider],03Fh
    jnz short idle_2f_chain

    mov byte ptr [idle_call_busy],1
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es

    mov bx,0010h
    call DpmiCh375Call

    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    mov byte ptr [idle_call_busy],0

idle_2f_chain:
    ; Chain without leaving our data selector in DS.  Stack here is:
    ;   callerDS, original INT frame...
    ; Push old SEG:OFF, restore caller DS from below them, then RETF 2 to both
    ; transfer control and discard the saved callerDS word.
    push word ptr [old_int2f+2]
    push word ptr [old_int2f]
    push bp
    mov bp,sp
    mov ds,[bp+6]
    pop bp
    retf 2
Int2fIdleHook ENDP

; ---------------------------------------------------------------------------
; DOS INT 33h function 0Ch callback.
;
; Conventional callback registers:
;   AX = event mask
;   BX = current button state
;   CX = X position
;   DX = Y position
;   SI = horizontal mickey delta
;   DI = vertical mickey delta
;
; Windows mouse-event callback registers:
;   AX = status (bit0 movement; button transitions in bits 2..7)
;   BX = relative X delta
;   CX = relative Y delta
;   DX = number of buttons
; ---------------------------------------------------------------------------
Int33Callback PROC FAR
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es

    ; Preserve the incoming INT33 event mask in BP while building Windows AX.
    mov bp,ax

    ; INT 33h calls us with the caller's DS. Load our own Win16 data selector
    ; through a loader-relocated SEG reference before touching driver state.
    mov ax,SEG event_proc
    mov ds,ax

    xor ax,ax

    test bp,0001h          ; movement
    jz short cb_no_move
    or ax,0001h
cb_no_move:

    ; Windows mouse status ABI:
    ; movement=0001h, B1 down/up=0002h/0004h, B2=0008h/0010h,
    ; B3=0020h/0040h.  INT33 function 0Ch uses the same transition bits.
    test bp,0002h          ; left press
    jz short cb_lu
    or ax,0002h
cb_lu:
    test bp,0004h          ; left release
    jz short cb_rd
    or ax,0004h
cb_rd:
    test bp,0008h          ; right press
    jz short cb_ru
    or ax,0008h
cb_ru:
    test bp,0010h          ; right release
    jz short cb_md
    or ax,0010h
cb_md:
    test bp,0020h          ; middle press
    jz short cb_mu
    or ax,0020h
cb_mu:
    test bp,0040h          ; middle release
    jz short cb_ready
    or ax,0040h

cb_ready:
    ; If Windows has not supplied a callback yet, silently ignore the event.
    cmp word ptr [event_proc+2],0
    je short cb_exit

    ; The resident INT33 core's SI/DI are raw mickey counters. Passing their
    ; differences directly made the Windows pointer much too fast on real
    ; hardware. CX/DX are the resident core's processed cursor coordinates, after its
    ; sensitivity and mickey/pixel conversion. Convert those coordinates back
    ; into per-event relative movement for the Win16 mouse callback.
    mov bx,cx
    sub bx,[prev_pos_x]
    mov [prev_pos_x],cx
    mov cx,dx
    sub cx,[prev_pos_y]
    mov [prev_pos_y],dx
    mov dx,3               ; number of buttons
    xor di,di              ; extra1 = 0
    xor si,si              ; extra2 = 0

    ; Windows supplies a FAR callback and expects AX/BX/CX/DX/DI/SI.
    call dword ptr [event_proc]

cb_exit:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    retf
Int33Callback ENDP

CALLBACKS ENDS
END
