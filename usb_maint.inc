; CH375USB 0.4.12-exp1 - usb_maint.inc
; Author: Davide "gat" - https://github.com/davidegat
; Copyright (C) 2026 Davide "gat"
; SPDX-License-Identifier: GPL-3.0-or-later
; Module: Deferred DOS-idle hotplug maintenance and re-enumeration.

usb_maintenance:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    cmp byte [usb_present],1
    jne .done
    cmp byte [usb_busy],0
    jne .done
    mov byte [usb_busy],1

    cmp byte [root_event],1
    jne .no_root_event
    mov byte [root_event],0
    mov byte [maintenance_due],0

    cmp byte [root_link_state],0
    jne .root_connected_event


    call clear_all_slots
    mov byte [media_ready],0
    mov byte [media_changed],1
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE
    mov byte [hub_slot],0FFh
    jmp .unlock

.root_connected_event:
    call root_reenumerate
    jmp .unlock

.no_root_event:


    cmp byte [hub_slot],0FFh
    jne .scan_hub
    cmp byte [storage_slot],0FFh
    jne .validate_storage
    mov byte [maintenance_due],0
    jmp .unlock

.scan_hub:

    call hub_scan_ports

.validate_storage:
    cmp byte [maintenance_due],1
    jne .unlock
    mov byte [maintenance_due],0

    cmp byte [storage_slot],0FFh
    je .claim

    call storage_ready_check
    jnc .unlock


    mov byte [media_ready],0
    mov byte [media_changed],1
    cmp byte [storage_mode],STORAGE_NATIVE
    jne .drop_bot
    mov byte [root_event],1
    mov byte [maintenance_due],1
    jmp .unlock

.drop_bot:
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE

.claim:
    call claim_first_storage

.unlock:
    mov byte [usb_busy],0
.done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret


root_reenumerate:
    push ax
    push di

    call clear_all_slots
    mov byte [root_connect_seen],0
    mov byte [enum_fail_stage],10h


    call native_storage_try
    jnc .native_ok


    call generic_root_enumerate
    jc .none
    pop di
    pop ax
    clc
    ret

.native_ok:
    mov byte [root_link_state],1
    mov byte [root_connect_seen],1
    mov byte [enum_fail_stage],00h
    pop di
    pop ax
    clc
    ret

.none:
    mov byte [media_ready],0
    mov byte [media_changed],1
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE
    mov byte [root_link_state],0
    mov byte [root_retry_ticks],18
    pop di
    pop ax
    stc
    ret


; Root classification tries CH375 native storage first, then generic HID/hub enumeration.
generic_root_enumerate:
    push ax

    ; Prefer the documented DS2 speed detection path.  GET_DEV_RATE itself
    ; is not available on every CH375 revision, so the old low-speed sequence
    ; remains a final compatibility fallback rather than the primary path.
    mov byte [usb_legacy_low_speed],0
    call ch_detect_root_speed

    call generic_root_enumerate_wch
    jnc .ok

    ; Some devices have configuration descriptors that exceed the built-in
    ; GET_DESCR 64-byte control buffer; retry the detected speed using the
    ; manual control-transfer enumeration path.
    mov al,[usb_detected_speed]
    mov [usb_speed_mode],al
    call generic_root_enumerate_manual
    jnc .ok

    ; GET_DEV_RATE is documented as unsupported on some chip revisions.  If
    ; the detected result was unusable, try the opposite documented speed
    ; before resorting to the old compatibility sequence.
    mov al,[usb_detected_speed]
    xor al,1
    mov [usb_speed_mode],al
    call generic_root_enumerate_wch
    jnc .ok
    call generic_root_enumerate_manual
    jnc .ok

    ; Final compatibility attempt: reproduce the legacy 0.4.11 low-speed
    ; sequence only after all documented paths failed.
    mov byte [usb_speed_mode],0
    mov byte [usb_legacy_low_speed],1
    call generic_root_enumerate_wch
    mov byte [usb_legacy_low_speed],0
    jc .fail
.ok:
    pop ax
    clc
    ret
.fail:
    pop ax
    stc
    ret

generic_root_enumerate_wch:
    push ax
    push bx
    push cx
    push di


    call clear_all_slots
    call ch_root_reset
    jc .fail


    cmp byte [usb_legacy_low_speed],1
    jne .speed_ready
    call ch_set_low_speed_legacy
    call delay_10ms
.speed_ready:


    xor ax,ax
    call clear_slot_ax
    xor ax,ax
    call slot_ptr_ax
    mov byte [di+D_USED],1
    mov byte [di+D_ADDR],0
    mov byte [di+D_PARENT],0FFh
    mov byte [di+D_PORT],0
    mov byte [di+D_FLAGS],0
    mov byte [di+D_EP0],8
    mov byte [xfer_slot],0


    call ch_get_device_descriptor0
    jc .fail
    mov al,[usb_buf+7]
    mov [di+D_EP0],al


    mov bl,[next_address]
    cmp bl,127
    jae .fail
    mov [enum_addr],bl
    call usb_assign_address0
    jc .fail
    mov al,0
    call slot_ptr_ax
    mov bl,[enum_addr]
    mov [di+D_ADDR],bl
    inc byte [next_address]


    mov al,[di+D_ADDR]
    call ch_select_addr
    call delay_10ms


    call ch_get_config_descriptor_builtin
    jc .fail
    cmp byte [cfg_buf+1],2
    jne .fail
    mov al,[cfg_buf+5]
    mov [di+D_CFG],al


    xor ax,ax
    call usb_parse_config
    jc .fail


    xor ax,ax
    call slot_ptr_ax
    mov bl,[di+D_CFG]
    call ch_set_config_builtin
    jc .fail


    mov al,CH375_RETRY_FAST
    call ch_set_retry


    xor ax,ax
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_KBD|CAP_MOUSE
    jz .not_hid
    xor ax,ax
    call hid_init_slot
.not_hid:
    xor ax,ax
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_HUB
    jz .not_hub
    mov byte [hub_slot],0
    xor ax,ax
    call hub_init_slot
    jc .fail
.not_hub:


    mov byte [root_link_state],1
    mov byte [root_connect_seen],1
    mov byte [enum_fail_stage],0
    pop di
    pop cx
    pop bx
    pop ax
    clc
    ret

.fail:
    call clear_all_slots
    pop di
    pop cx
    pop bx
    pop ax
    stc
    ret

generic_root_enumerate_manual:
    push ax
    push di

    call clear_all_slots
    mov byte [enum_fail_stage],01h
    call ch_root_reset
    jc .fail

    xor ax,ax
    call clear_slot_ax
    xor ax,ax
    call slot_ptr_ax
    mov byte [di+D_PARENT],0FFh
    mov byte [di+D_PORT],0
    mov byte [di+D_FLAGS],0

    mov byte [enum_fail_stage],02h
    xor ax,ax
    call usb_enumerate_addr0
    jc .fail


    cmp byte [hub_slot],0FFh
    je .claim
    call hub_scan_ports

.claim:
    call claim_first_storage
    mov byte [root_link_state],1
    mov byte [root_connect_seen],1
    mov byte [enum_fail_stage],00h
    pop di
    pop ax
    clc
    ret

.fail:
    mov byte [root_link_state],0
    pop di
    pop ax
    stc
    ret


claim_first_storage:
    push ax
    push cx
    push di

    cmp byte [storage_slot],0FFh
    jne .done
    cmp byte [storage_mode],STORAGE_NONE
    jne .done

    xor cx,cx
.loop:
    cmp cx,MAX_DEVICES
    jae .done
    mov ax,cx
    call slot_ptr_ax
    cmp byte [di+D_USED],1
    jne .next
    test byte [di+D_CAPS],CAP_MSC
    jz .next

    mov [storage_slot],cl
    mov byte [storage_mode],STORAGE_BOT
    mov word [xfer_seg],cs
    mov word [xfer_off],sector_buf
    call storage_mount_volume
    jc .bad
    mov byte [media_ready],1
    mov byte [media_changed],1
    jmp .done

.bad:
    mov byte [storage_slot],0FFh
    mov byte [storage_mode],STORAGE_NONE
.next:
    inc cx
    jmp .loop

.done:
    pop di
    pop cx
    pop ax
    ret
