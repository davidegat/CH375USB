# CH375USB — Development Knowledge and Lessons Learned

**Project:** CH375USB  
**Target:** Pocket386 and similar 386-class DOS PCs with a CH375 ISA USB controller  
**Author:** Davide "gat" — https://github.com/davidegat  
**Current reference version:** 0.4.13

This is the project's practical engineering record, not a formal CH375 specification. It documents what has actually worked on real hardware, why the current architecture exists, mistakes found during development, and changes that should not be made casually.

Some observations are Pocket386-specific. Treat real-hardware results as evidence, not as a guarantee for every CH375 board or clone.

---

# Hardware and architecture

## CH375 ISA interface

The tested Pocket386 exposes the CH375 through ISA-style parallel I/O:

```text
0260h  CH375 data port
0261h  CH375 command/status port
```

The driver uses:

```text
IO_BASE   = 0260h
DATA_PORT = 0260h
CMD_PORT  = 0261h
```

The controller is probed with `CHECK_EXIST` before the resident USB logic is enabled. Do not assume that every CH375 board uses this mapping.

Never load CH375USB together with the vendor `CH375286.SYS`; both require exclusive access to the same controller.

## Polling is part of the design

The tested hardware does not provide a useful CH375 IRQ path that CH375USB can simply depend on. Polling is therefore intentional and split across several contexts:

- `INT 1Ch`: lightweight root-event and HID polling;
- `INT 28h`: deferred hotplug, enumeration, hub and storage maintenance;
- `INT 33h`: additional on-demand mouse polling for responsive DOS applications.

Heavy USB work must not be moved into the timer ISR or into routine DOS block-device requests. Enumeration, resets and topology changes belong in deferred maintenance.

The driver reserves one DOS block-device unit even when no USB storage is attached. No media means drive-not-ready; when storage appears, the same DOS drive becomes usable. This is a deliberate part of DOS hotplug support.

---

# Storage

## Keep the native CH375 path and the generic BOT path

For root mass storage the driver first tries the CH375 native `DISK_*` firmware path:

```text
DISK_INIT
DISK_SIZE
DISK_READY
DISK_READ / DISK_RD_GO
DISK_WRITE / DISK_WR_GO
DISK_R_SENSE
```

If native storage is not appropriate or fails, generic USB enumeration can expose USB Mass Storage through BOT/SCSI.

The generic path implements at least:

```text
TEST UNIT READY
READ CAPACITY(10)
READ(10)
WRITE(10)
SYNCHRONIZE CACHE(10)
```

BOT must maintain endpoint toggles, validate CSW signature/tag/status, and perform reset recovery/endpoint halt clearing when required. A generic implementation is useful, but it is not a reason to delete a native path already proven on the target hardware.

## Storage media can mimic driver bugs

When debugging CH375 storage, try more than one flash drive before changing timing or protocol code. Older/simple USB 2.0-era drives are useful references. Test cold boot, insertion after boot, unplug/replug, reads and writes separately.

The currently recommended DOS storage layout is an MBR partition table with one primary FAT16 partition no larger than 2 GiB. The native path currently accepts 512-byte physical sectors.

## Non-512-byte sectors

Starting with 0.4.12, native `DISK_SIZE` data is read and parsed rather than assuming a 512-byte sector. Non-512-byte media is rejected safely.

This is validation, not translation. Supporting 1024/2048/4096-byte media would require a DOS 512-byte logical-sector translation layer; writes may require read-modify-write and caching. Do not implement this without a real device that needs it.

## Multi-LUN

Multi-LUN remains deferred. A useful first implementation would query the maximum LUN and select the first ready LUN while still exposing one DOS drive. Multiple simultaneous DOS drive letters would be a larger architectural change.

---

# CH375 control and timing lessons

## Bound hardware waits

A resident DOS driver must not wait forever. Control transfers, native disk operations, ordinary USB transactions, connect detection and HID polling need bounded waits appropriate to their context.

An idle HID endpoint normally returns NAK; that is not exceptional. Connection/disconnection events may also appear while waiting for the completion of another operation and must not automatically be mistaken for that operation's final status.

## Always release USB reset

The documented root reset sequence used by the 0.4.12+ generic path is:

```text
mode 5 -> host enabled, no SOF
mode 7 -> assert USB reset
mode 6 -> release reset and resume normal host operation/SOF
```

Every failure path that reaches reset must still return the controller to mode 6.

## Full-speed and low-speed devices

0.4.12 added the documented `GET_DEV_RATE` / `SET_USB_SPEED` handling while retaining the older CH375B-compatible low-speed sequence as a fallback. `SET_USB_MODE` restores full-speed operation, so a detected low-speed setting must be reapplied after the final mode switch.

Do not remove a proven compatibility fallback merely because a cleaner documented path exists; WCH itself notes that some commands are not supported by every CH375 revision.

Low-speed devices behind an external hub remain experimental and are not presently claimed as supported.

## `SET_RETRY` is controller state

WCH Datasheet II documents reset default `85h`, which can retry NAK indefinitely inside the CH375. Generic USB paths therefore explicitly set retry behaviour instead of accidentally inheriting the reset default.

The current `05h` bounded enumeration policy was already present in the 0.4.11 baseline. 0.4.12 centralized and named it; it did not introduce the policy. A separate finite-NACK-retry policy for built-in enumeration remains worth testing, but it is not a proven 0.4.12 regression.

## Command-port bit 7 / `INT#`

On CH375B/C in parallel mode, command-port bit 7 mirrors the interrupt request. The 0.4.12 control fallback checks for an actual pending event before consuming `GET_STATUS`. This avoids reading status blindly when no completion is pending.

## `DISK_R_SENSE` must be completed and drained

`DISK_R_SENSE` is not fire-and-forget. Its completion status and returned sense data must be consumed before another native disk command, otherwise stale state can be mistaken for the following operation.

## Conservative delays remain intentional

The current CPU-loop delays are longer than the datasheet minima and are CPU dependent. There is a plausible future performance project involving refresh-bit calibration and reduction of `ch_delay`/`ch_cmd_delay`, but timing changes should be measured on the Pocket386 before replacing proven values.

---

# HID keyboard

CH375USB targets boot-protocol keyboards rather than arbitrary HID report descriptors. It sets boot protocol/idle state, polls the interrupt endpoint, translates USB usages into PC Set-1 scan codes and injects them into the conventional keyboard-controller path.

Important rules:

- USB usages are not PC scan codes;
- make and break transitions both matter;
- extended keys need `E0h` where appropriate;
- modifier transitions must be tracked separately;
- injecting through the interface DOS software already understands is preferable to inventing a parallel keyboard API.

Potential future improvements include typematic/repeat, faster/on-demand keyboard polling, a more complete non-US/numpad map and LED `SET_REPORT` support. These are usability/compatibility extensions rather than regressions in the current basic keyboard support.

---

# HID mouse

Receiving a boot mouse report is straightforward; fitting it into arbitrary DOS applications is not.

CH375USB bridges USB mouse state through a conventional DOS mouse driver such as CuteMouse rather than replacing the entire DOS mouse ecosystem. `CTMOUSE`/`MOUSE.COM` normally loads from `AUTOEXEC.BAT`, after CH375USB has already loaded from `CONFIG.SYS`, so CH375USB must discover and hook the final `INT 33h` chain later.

The mouse path combines timer polling with on-demand polling from relevant `INT 33h` calls. Timer-only polling at approximately 18.2 Hz was visibly too slow in DOS EDIT.

Button presses and releases are edges, not just a current bitmap. Short clicks can occur between application polls; therefore press/release transitions are latched and counted. A small bounded burst of HID reports helps drain queued motion/button events without turning the timer ISR into an unbounded loop.

Direct PS/2/IRQ12 emulation was intentionally avoided. The DOS `INT 33h` layer is a safer compatibility boundary for the current design.

---

# Hotplug

Hotplug is a state machine, not a single present/absent boolean. Relevant state includes root link state, root event, maintenance flag, retry/backoff, storage slot/mode, media-ready and media-changed state.

Fast detach and slow discovery are deliberately separated:

```text
fast path:
    invalidate device state
    mark storage unavailable/media changed
    schedule maintenance

deferred path:
    reset/classify
    try native storage
    otherwise generic enumeration
    scan hub if applicable
    initialize/claim device classes
```

Retry/backoff prevents enumeration storms while a physical device is settling.

All three currently supported root-device classes — storage, boot keyboard and boot mouse — have been tested successfully with live attach/detach on the target DOS hardware.

---

# Windows 95 boundary

DOS HID support is not Windows 95 protected-mode HID support. The DOS keyboard/mouse bridge should get out of the way when Windows enhanced mode takes over.

CH375USB uses the Windows enhanced-mode notifications on `INT 2Fh` (`AX=1605h` entry and `AX=1606h` exit) to suspend DOS HID state, clear stale mouse callbacks and remove/rebuild its `INT 33h` shim safely.

A stale DOS mouse callback once caused a valuable failure mode: using the mouse in DOS before entering Windows could leave a callback into dead application memory and destabilize Windows startup. Resident software must never assume application callback addresses remain valid forever.

For storage, the tested Windows 95 workaround is to leave CH375USB out of the Windows `CONFIG.SYS` profile, load it with `DEVLOAD.COM` from `AUTOEXEC.BAT` immediately before `WIN`, and ensure the USB drive has already been detected in DOS. This is not native Windows USB hotplug.

USB keyboard and mouse are currently DOS-only. Proper Windows GUI input would require a separate protected-mode companion/VxD-style component.

---

# BIOS and real-hardware testing

The Pocket386 used during development has also been tested with aggressive performance-oriented BIOS settings such as PCLK2/8, disabled I/O recovery and slow refresh 120 µs. These are test conditions, not driver requirements. Debug with default/conservative BIOS settings first.

A practical hardware lesson: the Pocket386 BIOS mouse-support option has been observed to become disabled unexpectedly. If storage and keyboard work but a previously working mouse suddenly does not, verify the BIOS mouse setting before changing the driver.

Test cold boot and hotplug separately. For storage, explicitly test create/copy/read/delete and verify data after remount/reboot. For mouse, test actual DOS applications, short clicks, movement while clicking, and DOS-to-Windows transition after active mouse use.

---

# 0.4.12 datasheet audit

0.4.12 was a conservative audit of the known-good 0.4.11 code against WCH CH375 Datasheet (I) Version 4 and Datasheet (II) Version 4. Durable changes/lessons from that audit were:

- documented mode 5 -> 7 -> 6 root-reset sequence;
- `GET_DEV_RATE` / `SET_USB_SPEED` handling with the legacy low-speed fallback retained;
- explicit generic `SET_RETRY` state instead of relying on reset default `85h`;
- command-port bit 7 checked before fallback `GET_STATUS` consumption;
- native `DISK_SIZE` result consumed and 512-byte sector size validated;
- native `DISK_R_SENSE` completion/data drained correctly;
- failed zero-data control OUT clears `usb_control_wait`;
- named Datasheet-II constants added for the audited commands/status values.

`RD_USB_DATA0`, Multi-LUN, non-512 translation, low-speed-behind-hub support and timing optimization were deliberately left out of that audit rather than mixed into a low-level correctness pass.

Real-hardware 0.4.12 regression testing kept storage, keyboard, mouse and live root replacement working. An apparent mouse regression was traced to the BIOS mouse setting, not to the driver.

---

# 0.4.13 correctness pass

0.4.13 promotes the tested 0.4.12 experimental line after a second review focused on DOS block-driver semantics and resident-code correctness. Only findings strong enough to justify another hardware regression test were included.

## Media Check values must be translated explicitly

`media_changed` is an internal boolean, but DOS command 1 uses different result values:

```text
FFh = media changed
00h = unknown
01h = media not changed
```

Earlier code copied `0/1` directly, meaning the precise moment the driver knew media had changed could be reported to DOS as `01h` — the opposite meaning.

0.4.13 returns `FFh` once when a change is pending and `01h` in steady state. The fix was first tested successfully in `0.4.12-exp1a`.

**Lesson:** never reuse an internal boolean encoding as a DOS protocol field without checking the protocol's actual values.

## Manual control-IN must use the real EP0 packet size

The device descriptor's `bMaxPacketSize0` is stored as `D_EP0`, but the old manual control-IN loop considered any packet shorter than 64 bytes to be a short packet. That breaks devices whose EP0 maximum packet is 8, 16 or 32 bytes.

0.4.13 compares the returned packet length against `D_EP0`, with 8 bytes as a safe pre-descriptor/default value. This was also first verified successfully in `0.4.12-exp1a`.

This fix improves manual enumeration but does not by itself solve the explicitly separate low-speed-behind-hub limitation.

## Resident string operations must establish their own Direction Flag

`ch_write_packet`, `ch_read_packet`, `native_read_chunk64` and `native_write_chunk64` use `LODSB`/`STOSB`. Resident code may run while a foreground DOS application owns CPU flags, so assuming DF=0 is unsafe.

0.4.13 wraps these routines with saved FLAGS, `CLD`, and FLAGS restoration. Copies therefore run forward while preserving the caller's original DF state.

**Lesson:** ISR/TSR code that uses x86 string instructions must establish its own DF contract.

## Partial block errors must report the completed sector count

For DOS block reads/writes, error status is not the whole result. Request-header word `+18` must reflect how many sectors actually completed.

Earlier code returned an error while leaving the original requested count unchanged. 0.4.13 computes `requested - remaining`; at the failure point CX still includes the sector that failed, so this is the number completed successfully before the failing sector.

Targeted fault injection of this path has not yet been performed, so that distinction remains documented in `KNOWN_ISSUES.md` even though the logic is corrected.

## Every successful enumeration path must finish class initialization

The CH375 built-in generic root path could parse a mass-storage interface and set `CAP_MSC` but finish without running `msc_init_slot`. Native `DISK_*` usually masked this for root flash drives, and the manual generic path already initialized MSC correctly.

0.4.13 now runs the existing BOT/SCSI initialization when the built-in path recognizes MSC. Capability discovery and class initialization must have equivalent completion semantics across parallel enumeration paths.

Forced generic BOT fallback has not yet received the same direct coverage as the normal native root-storage path.

## 0.4.13 hardware result

After all five correctness fixes were combined, the promoted code was tested on the target DOS machine:

- USB mass storage works read/write;
- USB keyboard works;
- USB mouse works;
- storage, keyboard and mouse all work with live plug/unplug and plug-and-play under DOS.

This is the current known-good reference.

---

# Confirmed/deferred work after 0.4.13

`KNOWN_ISSUES.md` is the authoritative tracker. The most important remaining items are:

- BOT `SYNCHRONIZE CACHE(10)` failure is not yet propagated back to DOS after an otherwise successful write;
- `usb_busy` acquisition is a real non-atomic design race and should be redesigned with explicit ownership rather than mechanically patched;
- no LBA bounds check is performed before storage I/O;
- when no CH375 is present, the driver still reserves its normal DOS unit/resident memory;
- DOS does not yet show attach/detach notifications; proposed design is a resident event queue + private `INT 2Fh` API + optional `CH375MON.COM`;
- full DOS removable-media capability advertisement remains deferred; if implemented, bit 11 (`0800h`) and command 15 must be implemented correctly. Do not use `0840h` unless Generic IOCTL is actually implemented;
- FAT32 is not implemented; accepting partition type `0Bh/0Ch` alone is insufficient because the DOS-facing BPB path also needs FAT32 fields;
- Multi-LUN, non-512 translation and low-speed devices behind hubs remain deferred;
- phase-specific `SET_RETRY`, config-descriptor truncation hardening, timing calibration, neutral delay ports, `RD_USB_DATA0`, 8042 injection hardening, a private ISR stack, hub USB-address recycling, BOT `REQUEST SENSE`, a second `CHECK_EXIST` pattern and dead-code cleanup remain review candidates that require targeted tests.

Do not turn code-review suggestions into fixes automatically. Separate proven defects from plausible hardening and from feature requests, then test one change set at a time.

---

# Current known-good functional summary

As of CH375USB 0.4.13 on the tested Pocket386:

| Function | DOS | DOS hotplug | Windows 95 GUI |
|---|---:|---:|---:|
| USB flash storage read | Yes | Yes | Yes via pre-Windows DOS driver method |
| USB flash storage write | Yes | Yes | Yes via pre-Windows DOS driver method |
| USB keyboard | Yes | Yes | No |
| USB mouse movement/buttons | Yes | Yes | No |
| External hub | Experimental | Experimental | Not supported |
| Windows USB hotplug | — | — | Not supported/reliable |

For Windows storage, detect the USB drive under DOS before starting Windows.

---

# Reusable engineering lessons

1. Keep interrupt handlers short and defer expensive work.
2. Respect DOS TSR/device-driver installation order.
3. Keep a proven hardware-specific path while adding a generic fallback.
4. Bound every hardware wait and understand controller-side retry state.
5. USB reset paths must always release reset.
6. Hotplug needs state, invalidation and retry/backoff — not just a presence bit.
7. Mouse movement and button edges are different classes of state.
8. Never retain application callback addresses indefinitely in resident code.
9. DOS input support is not Windows protected-mode input support.
10. Test multiple USB devices before blaming timing code.
11. Test real application behaviour, not only packet-level success.
12. Document what was actually tested separately from what merely exists in source.
13. Translate DOS protocol values explicitly instead of reusing internal boolean encodings.
14. Use the actual EP0 max-packet size to identify short control packets.
15. Resident x86 string code must establish and restore its own Direction Flag state.
16. On block-I/O errors, return the completed sector count as well as an error status.
17. Every successful enumeration path must complete the class-specific initialization it advertises.
18. Treat external code reviews as hypotheses to verify against the actual code, datasheets and hardware.

CH375USB became useful by isolating layers one at a time: CH375 controller state, USB transport, DOS interfaces, HID bridging, hotplug state and the DOS/Windows boundary. Preserving that separation is more important than adding every possible feature quickly.
