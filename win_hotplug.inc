; CH375USB 0.5.0 - win_hotplug.inc
; Incremental Windows-95 root HID-mouse hotplug enumerator.
;
; Design rule: one bounded action per INT 1Ch invocation.  No multi-tick wait,
; no generic_root_enumerate(), no ch_root_reset(), no DOS-idle/DPMI worker.
; CH375 built-in commands are started on one tick and completed on later ticks
; after INT# asserts.  This mirrors the staged/asynchronous strategy used by
; Bret Johnson's USBMOUSE while keeping CH375-specific transport independent.

%if WIN_MOUSE_INT33_BRIDGE

%define WENUM_IDLE             0
%define WENUM_SETTLE           1
%define WENUM_MODE5            2
%define WENUM_MODE7            3
%define WENUM_RESET_HOLD       4
%define WENUM_MODE6            5
%define WENUM_WAIT_CONNECT     6
%define WENUM_PREP_SLOT        7
%define WENUM_GET_DEV_START    8
%define WENUM_GET_DEV_WAIT     9
%define WENUM_SET_ADDR_START  10
%define WENUM_SET_ADDR_WAIT   11
%define WENUM_ADDR_SETTLE     12
%define WENUM_GET_CFG_START   13
%define WENUM_GET_CFG_WAIT    14
%define WENUM_PARSE_CFG       15
%define WENUM_SET_CFG_START          16
%define WENUM_SET_CFG_WAIT           17
%define WENUM_SET_PROTO_SETUP_START  18
%define WENUM_SET_PROTO_SETUP_WAIT   19
%define WENUM_SET_PROTO_STATUS_START 20
%define WENUM_SET_PROTO_STATUS_WAIT  21
%define WENUM_SET_IDLE_SETUP_START   22
%define WENUM_SET_IDLE_SETUP_WAIT    23
%define WENUM_SET_IDLE_STATUS_START  24
%define WENUM_SET_IDLE_STATUS_WAIT   25
%define WENUM_READY                  26
%define WENUM_RETRY_WAIT             27

; Start discovery after a CONNECT status was observed by timer_1c.
win_mouse_enum_start:
    ; Suspend the optional high-rate RTC mouse sampler for the entire multi-tick
    ; enumeration.
    call rtc_win_mouse_disable
    mov byte [win_hotplug_pending],0
    mov byte [win_hotplug_delay],0
    mov byte [win_hotplug_busy],0
    mov byte [enum_hid_only],1
    mov byte [win_enum_attempt],0
    mov byte [win_enum_wait],2
    mov byte [win_enum_timeout],0
    mov byte [win_enum_state],WENUM_SETTLE
    mov byte [root_link_state],0
    mov byte [root_connect_seen],1
    mov byte [root_event],0
    mov byte [maintenance_due],0
    ret

; Immediate detach/abort path.  Safe to call whether or not enumeration is active.
win_mouse_enum_disconnect:
    call rtc_win_mouse_disable
    mov byte [win_enum_state],WENUM_IDLE
    mov byte [win_enum_wait],0
    mov byte [win_enum_timeout],0
    mov byte [win_enum_attempt],0
    mov byte [win_hotplug_pending],0
    mov byte [win_hotplug_delay],0
    mov byte [win_hotplug_busy],0
    mov byte [enum_hid_only],0
    mov byte [root_link_state],0
    mov byte [root_connect_seen],0
    mov byte [root_event],0
    mov byte [maintenance_due],0
    call mark_all_detached_fast

    ; A DISCONNECT may be the completion status of an HID IN token.
    ; Put CH375 back into a neutral root-host state so a later attachment can
    ; generate and complete a fresh reset/enumeration sequence.
    push ax
    mov al,6
    call ch_set_mode
    xor al,al
    call ch_select_addr
    mov al,CH375_RETRY_BOUNDED
    call ch_set_retry
    pop ax

    ; Normal CONNECT remains the fast path.  This watchdog is armed only after
    ; a real detach and catches revisions that fail to raise a second CONNECT.
    mov byte [win_replug_armed],1
    mov byte [win_replug_ticks],9
    mov byte [maintenance_due],0
    ret

; Enumeration failed without a physical DISCONNECT.  Keep Windows responsive,
; restore host mode, and retry later.  Alternate detected/opposite USB speed on
; successive attempts; after both fail, pause ~1 second and detect again.
win_mouse_enum_fail:
    call rtc_win_mouse_disable
    push ax
    mov al,6
    call ch_set_mode
    call clear_all_slots
    mov byte [root_link_state],0
    mov byte [root_event],0
    mov byte [maintenance_due],0
    mov byte [enum_hid_only],1
    cmp byte [win_enum_attempt],0
    jne .second_failed
    mov byte [win_enum_attempt],1
    mov byte [win_enum_wait],9
    jmp .arm_retry
.second_failed:
    mov byte [win_enum_attempt],0
    cmp byte [win_replug_armed],1
    jne .second_normal_retry
    ; Watchdog recovery should not keep a permanently unplugged controller in
    ; continuous enumeration. Return to IDLE and retry about once per second.
    mov byte [win_enum_timeout],0
    mov byte [win_enum_state],WENUM_IDLE
    mov byte [win_replug_ticks],18
    mov byte [enum_hid_only],0
    pop ax
    ret
.second_normal_retry:
    mov byte [win_enum_wait],18
.arm_retry:
    mov byte [win_enum_timeout],0
    mov byte [win_enum_state],WENUM_RETRY_WAIT
    pop ax
    ret

; Read one pending CH375 status without waiting.
; CF=1: no INT# pending.  CF=0: AL=status and last_status updated.
win_mouse_status_now:
    push dx
    mov dx,CMD_PORT
    in al,dx
    test al,080h
    jnz .none
    mov al,CMD_GET_STATUS
    call ch_cmd
    call ch_data_in
    mov [last_status],al
    pop dx
    clc
    ret
.none:
    pop dx
    stc
    ret

; Timeout helper for asynchronous command wait states.
; CF=1 when timeout expired, otherwise CF=0.
win_mouse_timeout_tick:
    cmp byte [win_enum_timeout],0
    je .expired
    dec byte [win_enum_timeout]
    cmp byte [win_enum_timeout],0
    je .expired
    clc
    ret
.expired:
    stc
    ret

; One state-machine step.  Called with CH375 usb_busy lock held by timer_1c.
win_mouse_enum_tick:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    mov al,[win_enum_state]
    cmp al,WENUM_SETTLE
    je .settle
    cmp al,WENUM_MODE5
    je .mode5
    cmp al,WENUM_MODE7
    je .mode7
    cmp al,WENUM_RESET_HOLD
    je .reset_hold
    cmp al,WENUM_MODE6
    je .mode6
    cmp al,WENUM_WAIT_CONNECT
    je .wait_connect
    cmp al,WENUM_PREP_SLOT
    je .prep_slot
    cmp al,WENUM_GET_DEV_START
    je .get_dev_start
    cmp al,WENUM_GET_DEV_WAIT
    je .get_dev_wait
    cmp al,WENUM_SET_ADDR_START
    je .set_addr_start
    cmp al,WENUM_SET_ADDR_WAIT
    je .set_addr_wait
    cmp al,WENUM_ADDR_SETTLE
    je .addr_settle
    cmp al,WENUM_GET_CFG_START
    je .get_cfg_start
    cmp al,WENUM_GET_CFG_WAIT
    je .get_cfg_wait
    cmp al,WENUM_PARSE_CFG
    je .parse_cfg
    cmp al,WENUM_SET_CFG_START
    je .set_cfg_start
    cmp al,WENUM_SET_CFG_WAIT
    je .set_cfg_wait
    cmp al,WENUM_SET_PROTO_SETUP_START
    je .set_proto_setup_start
    cmp al,WENUM_SET_PROTO_SETUP_WAIT
    je .set_proto_setup_wait
    cmp al,WENUM_SET_PROTO_STATUS_START
    je .set_proto_status_start
    cmp al,WENUM_SET_PROTO_STATUS_WAIT
    je .set_proto_status_wait
    cmp al,WENUM_SET_IDLE_SETUP_START
    je .set_idle_setup_start
    cmp al,WENUM_SET_IDLE_SETUP_WAIT
    je .set_idle_setup_wait
    cmp al,WENUM_SET_IDLE_STATUS_START
    je .set_idle_status_start
    cmp al,WENUM_SET_IDLE_STATUS_WAIT
    je .set_idle_status_wait
    cmp al,WENUM_READY
    je .ready
    cmp al,WENUM_RETRY_WAIT
    je .retry_wait
    jmp .done

.settle:
    call win_mouse_status_now
    jc .settle_count
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_CONNECT
    jne .settle_count
    mov byte [root_connect_seen],1
.settle_count:
    cmp byte [win_enum_wait],0
    je .settle_done
    dec byte [win_enum_wait]
    jnz .done
.settle_done:
    mov byte [win_enum_state],WENUM_MODE5
    jmp .done

.mode5:
    mov al,5
    call ch_set_mode
    jc .fail
    call ch_collect_root_events
    cmp byte [win_enum_attempt],0
    jne .use_opposite_speed

    mov al,CMD_GET_DEV_RATE
    call ch_cmd
    mov al,007h
    call ch_data_out
    call ch_data_in
    mov byte [usb_speed_mode],0
    test al,010h
    jz .speed_detected
    mov byte [usb_speed_mode],1
.speed_detected:
    mov al,[usb_speed_mode]
    mov [usb_detected_speed],al
    jmp .mode5_done

.use_opposite_speed:
    mov al,[usb_detected_speed]
    xor al,1
    mov [usb_speed_mode],al
.mode5_done:
    mov byte [root_connect_seen],0
    mov byte [win_enum_state],WENUM_MODE7
    jmp .done

.mode7:
    mov al,7
    call ch_set_mode
    jc .fail
    mov byte [win_enum_wait],1
    mov byte [win_enum_state],WENUM_RESET_HOLD
    jmp .done

.reset_hold:
    cmp byte [win_enum_wait],0
    je .reset_release
    dec byte [win_enum_wait]
    jnz .done
.reset_release:
    mov byte [win_enum_state],WENUM_MODE6
    jmp .done

.mode6:
    mov al,6
    call ch_set_mode
    jc .fail
    call ch_apply_root_speed
    mov byte [win_enum_timeout],36
    mov byte [win_enum_state],WENUM_WAIT_CONNECT
    jmp .done

.wait_connect:
    call win_mouse_status_now
    jc .wait_connect_timeout
    cmp al,USB_INT_DISCONNECT
    je .wait_connect_timeout
    cmp al,USB_INT_CONNECT
    jne .wait_connect_timeout
    mov byte [root_connect_seen],1
    xor al,al
    call ch_select_addr
    mov al,CH375_RETRY_BOUNDED
    call ch_set_retry
    mov byte [win_enum_state],WENUM_PREP_SLOT
    jmp .done
.wait_connect_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.prep_slot:
    call clear_all_slots
    xor ax,ax
    call slot_ptr_ax
    mov byte [di+D_USED],1
    mov byte [di+D_ADDR],0
    mov byte [di+D_PARENT],0FFh
    mov byte [di+D_PORT],0
    mov byte [di+D_FLAGS],0
    mov byte [di+D_EP0],8
    mov byte [xfer_slot],0
    mov byte [win_enum_state],WENUM_GET_DEV_START
    jmp .done

.get_dev_start:
    xor al,al
    call ch_select_addr
    mov al,CMD_GET_DESCR
    call ch_cmd
    mov al,1
    call ch_data_out
    mov byte [win_enum_timeout],10
    mov byte [win_enum_state],WENUM_GET_DEV_WAIT
    jmp .done

.get_dev_wait:
    call win_mouse_status_now
    jc .get_dev_timeout
    cmp al,USB_INT_CONNECT
    je .get_dev_seen_connect
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_SUCCESS
    jne .fail
    push cs
    pop es
    mov di,usb_buf
    mov cx,18
    call ch_read_packet
    cmp ax,8
    jb .fail
    xor ax,ax
    call slot_ptr_ax
    mov al,[usb_buf+7]
    mov [di+D_EP0],al
    mov byte [win_enum_state],WENUM_SET_ADDR_START
    jmp .done
.get_dev_seen_connect:
    mov byte [root_connect_seen],1
.get_dev_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.set_addr_start:
    mov bl,[next_address]
    cmp bl,127
    jae .fail
    mov [enum_addr],bl
    xor al,al
    call ch_select_addr
    mov al,CMD_SET_ADDRESS
    call ch_cmd
    mov al,bl
    call ch_data_out
    mov byte [win_enum_timeout],10
    mov byte [win_enum_state],WENUM_SET_ADDR_WAIT
    jmp .done

.set_addr_wait:
    call win_mouse_status_now
    jc .set_addr_timeout
    cmp al,USB_INT_CONNECT
    je .set_addr_seen_connect
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_SUCCESS
    jne .fail
    xor ax,ax
    call slot_ptr_ax
    mov bl,[enum_addr]
    mov [di+D_ADDR],bl
    inc byte [next_address]
    mov al,bl
    call ch_select_addr
    mov byte [win_enum_wait],1
    mov byte [win_enum_state],WENUM_ADDR_SETTLE
    jmp .done
.set_addr_seen_connect:
    mov byte [root_connect_seen],1
.set_addr_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.addr_settle:
    cmp byte [win_enum_wait],0
    je .addr_ready
    dec byte [win_enum_wait]
    jnz .done
.addr_ready:
    mov byte [win_enum_state],WENUM_GET_CFG_START
    jmp .done

.get_cfg_start:
    mov al,CMD_GET_DESCR
    call ch_cmd
    mov al,2
    call ch_data_out
    mov byte [win_enum_timeout],10
    mov byte [win_enum_state],WENUM_GET_CFG_WAIT
    jmp .done

.get_cfg_wait:
    call win_mouse_status_now
    jc .get_cfg_timeout
    cmp al,USB_INT_CONNECT
    je .get_cfg_seen_connect
    cmp al,USB_INT_DISCONNECT
    je .disconnect
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
    cmp byte [cfg_buf+1],2
    jne .fail
    xor ax,ax
    call slot_ptr_ax
    mov al,[cfg_buf+5]
    mov [di+D_CFG],al
    mov byte [win_enum_state],WENUM_PARSE_CFG
    jmp .done
.get_cfg_seen_connect:
    mov byte [root_connect_seen],1
.get_cfg_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.parse_cfg:
    xor ax,ax
    call usb_parse_config
    jc .fail
    xor ax,ax
    call slot_ptr_ax
    test byte [di+D_CAPS],CAP_MOUSE
    jz .fail
    cmp byte [di+D_MOUSE_EP],0
    je .fail
    mov byte [win_enum_state],WENUM_SET_CFG_START
    jmp .done

.set_cfg_start:
    xor ax,ax
    call slot_ptr_ax
    mov bl,[di+D_CFG]
    mov al,CMD_SET_CONFIG
    call ch_cmd
    mov al,bl
    call ch_data_out
    mov byte [win_enum_timeout],10
    mov byte [win_enum_state],WENUM_SET_CFG_WAIT
    jmp .done

.set_cfg_wait:
    call win_mouse_status_now
    jc .set_cfg_timeout
    cmp al,USB_INT_CONNECT
    je .set_cfg_seen_connect
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_SUCCESS
    jne .fail
    mov byte [win_enum_state],WENUM_SET_PROTO_SETUP_START
    jmp .done
.set_cfg_seen_connect:
    mov byte [root_connect_seen],1
.set_cfg_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.set_proto_setup_start:
    xor ax,ax
    call slot_ptr_ax
    mov bl,[di+D_MOUSE_ITF]
    mov byte [setup_buf+0],021h
    mov byte [setup_buf+1],00Bh
    mov word [setup_buf+2],0
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],0
    xor al,al
    call usb_select_slot
    jc .fail
    mov al,TOG_DATA0
    call ch_set_out_toggle
    mov si,setup_buf
    mov cx,8
    call ch_write_packet
    mov al,CMD_ISSUE_TOKEN
    call ch_cmd
    mov al,PID_SETUP
    call ch_data_out
    mov byte [win_enum_timeout],10
    mov byte [win_enum_state],WENUM_SET_PROTO_SETUP_WAIT
    jmp .done

.set_proto_setup_wait:
    call win_mouse_status_now
    jc .set_proto_setup_timeout
    cmp al,USB_INT_CONNECT
    je .set_proto_setup_seen_connect
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_SUCCESS
    jne .fail
    mov byte [win_enum_state],WENUM_SET_PROTO_STATUS_START
    jmp .done
.set_proto_setup_seen_connect:
    mov byte [root_connect_seen],1
.set_proto_setup_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.set_proto_status_start:
    mov al,TOG_DATA1
    call ch_set_in_toggle
    mov al,CMD_ISSUE_TOKEN
    call ch_cmd
    mov al,PID_IN
    call ch_data_out
    mov byte [win_enum_timeout],10
    mov byte [win_enum_state],WENUM_SET_PROTO_STATUS_WAIT
    jmp .done

.set_proto_status_wait:
    call win_mouse_status_now
    jc .set_proto_status_timeout
    cmp al,USB_INT_CONNECT
    je .set_proto_status_seen_connect
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_SUCCESS
    jne .fail
    push cs
    pop es
    mov di,usb_buf
    mov cx,64
    call ch_read_packet
    mov byte [win_enum_state],WENUM_SET_IDLE_SETUP_START
    jmp .done
.set_proto_status_seen_connect:
    mov byte [root_connect_seen],1
.set_proto_status_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.set_idle_setup_start:
    xor ax,ax
    call slot_ptr_ax
    mov bl,[di+D_MOUSE_ITF]
    mov byte [setup_buf+0],021h
    mov byte [setup_buf+1],00Ah
    mov word [setup_buf+2],0
    mov [setup_buf+4],bl
    mov byte [setup_buf+5],0
    mov word [setup_buf+6],0
    xor al,al
    call usb_select_slot
    jc .fail
    mov al,TOG_DATA0
    call ch_set_out_toggle
    mov si,setup_buf
    mov cx,8
    call ch_write_packet
    mov al,CMD_ISSUE_TOKEN
    call ch_cmd
    mov al,PID_SETUP
    call ch_data_out
    mov byte [win_enum_timeout],10
    mov byte [win_enum_state],WENUM_SET_IDLE_SETUP_WAIT
    jmp .done

.set_idle_setup_wait:
    call win_mouse_status_now
    jc .set_idle_setup_timeout
    cmp al,USB_INT_CONNECT
    je .set_idle_setup_seen_connect
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_SUCCESS
    jne .fail
    mov byte [win_enum_state],WENUM_SET_IDLE_STATUS_START
    jmp .done
.set_idle_setup_seen_connect:
    mov byte [root_connect_seen],1
.set_idle_setup_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.set_idle_status_start:
    mov al,TOG_DATA1
    call ch_set_in_toggle
    mov al,CMD_ISSUE_TOKEN
    call ch_cmd
    mov al,PID_IN
    call ch_data_out
    mov byte [win_enum_timeout],10
    mov byte [win_enum_state],WENUM_SET_IDLE_STATUS_WAIT
    jmp .done

.set_idle_status_wait:
    call win_mouse_status_now
    jc .set_idle_status_timeout
    cmp al,USB_INT_CONNECT
    je .set_idle_status_seen_connect
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_SUCCESS
    jne .fail
    push cs
    pop es
    mov di,usb_buf
    mov cx,64
    call ch_read_packet
    mov al,CH375_RETRY_FAST
    call ch_set_retry
    mov byte [win_enum_state],WENUM_READY
    jmp .done
.set_idle_status_seen_connect:
    mov byte [root_connect_seen],1
.set_idle_status_timeout:
    call win_mouse_timeout_tick
    jc .fail
    jmp .done

.ready:
    mov byte [root_link_state],1
    mov byte [root_connect_seen],1
    mov byte [root_event],0
    mov byte [maintenance_due],0
    mov byte [enum_hid_only],0
    mov byte [win_enum_state],WENUM_IDLE
    mov byte [win_enum_wait],0
    mov byte [win_enum_timeout],0
    mov byte [win_enum_attempt],0
    mov byte [win_replug_armed],0
    mov byte [win_replug_ticks],0
    call rtc_win_mouse_enable_if_ready
    jmp .done

.retry_wait:
    call win_mouse_status_now
    jc .retry_count
    cmp al,USB_INT_DISCONNECT
    je .disconnect
    cmp al,USB_INT_CONNECT
    jne .retry_count
    mov byte [root_connect_seen],1
.retry_count:
    cmp byte [win_enum_wait],0
    je .retry_now
    dec byte [win_enum_wait]
    jnz .done
.retry_now:
    mov byte [win_enum_state],WENUM_MODE5
    jmp .done

.disconnect:
    call win_mouse_enum_disconnect
    jmp .done
.fail:
    call win_mouse_enum_fail
.done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

%endif
