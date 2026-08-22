; MTEST.ASM - tiny INT 33h diagnostic for CH375USB 0.5.1
; NASM: nasm -f bin MTEST.ASM -o MTEST.COM
; SPDX-License-Identifier: GPL-3.0-or-later
bits 16
org 100h

start:
    mov dx,msg_title
    mov ah,09h
    int 21h

    xor ax,ax
    int 33h
    cmp ax,0FFFFh
    je .driver_ok
    mov dx,msg_no_driver
    mov ah,09h
    int 21h
    mov ax,4C01h
    int 21h

.driver_ok:
    mov dx,msg_ok
    mov ah,09h
    int 21h

.loop:
    mov ax,0003h
    int 33h                    ; BX buttons, CX X, DX Y
    push bx
    push cx
    push dx

    mov dl,13
    mov ah,02h
    int 21h
    mov dx,msg_x
    mov ah,09h
    int 21h

    pop ax                     ; Y temporarily
    pop si                     ; X
    pop di                     ; buttons
    push ax
    mov ax,si
    call print_hex16
    mov dx,msg_y
    mov ah,09h
    int 21h
    pop ax
    call print_hex16
    mov dx,msg_b
    mov ah,09h
    int 21h
    mov ax,di
    call print_hex16
    mov dx,msg_exit
    mov ah,09h
    int 21h

    mov ah,01h
    int 16h
    jz .loop
    xor ah,ah
    int 16h
    cmp al,27
    jne .loop

    mov dx,msg_done
    mov ah,09h
    int 21h
    mov ax,4C00h
    int 21h

print_hex16:
    push ax
    push bx
    push cx
    push dx
    mov bx,ax
    mov cx,4
.hex:
    rol bx,4
    mov dl,bl
    and dl,0Fh
    cmp dl,9
    jbe .digit
    add dl,'A'-10
    jmp .out
.digit:
    add dl,'0'
.out:
    mov ah,02h
    int 21h
    loop .hex
    pop dx
    pop cx
    pop bx
    pop ax
    ret

msg_title db 13,10,'CH375USB 0.5.1 INT33 test',13,10,'$'
msg_no_driver db 'INT33 mouse driver NOT detected.',13,10,'$'
msg_ok db 'INT33 detected. Move mouse / click. ESC exits.',13,10,'$'
msg_x db 'X=$'
msg_y db ' Y=$'
msg_b db ' BTN=$'
msg_exit db '   ESC=exit$'
msg_done db 13,10,'Done.',13,10,'$'
