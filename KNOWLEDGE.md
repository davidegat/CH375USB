# CH375USB — Engineering knowledge

**Current reference version:** 0.4.13  
**Target:** Pocket386 and similar 386-class DOS PCs with an ISA-connected CH375 USB host controller  
**I/O mapping used by the tested hardware:** data `0260h`, command/status `0261h`

This document is the project's engineering reference. It explains the current architecture, the rules that proved important on real hardware, and the limits that should be kept in mind when changing the driver. It is intentionally not a chronological diary; release-by-release details belong in [`CHANGELOG.md`](CHANGELOG.md), while open work belongs in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md).

---

## 1. Current known-good state

On the tested Pocket386 / DOS environment, CH375USB 0.4.13 provides:

| Function | DOS | DOS hotplug | Windows 95 GUI |
|---|---:|---:|---:|
| USB flash storage read/write | Yes | Yes | Usable through the documented pre-Windows DOS path |
| USB boot keyboard | Yes | Yes | No |
| USB boot mouse | Yes | Yes | No |
| External hub | Experimental | Experimental | No |

The completed 0.4.13 source was regression-tested with storage, keyboard and mouse. All three classes worked correctly with live attach/detach under DOS.

The driver is a **real-mode DOS driver**. Windows 95 storage compatibility is a carry-over of the DOS block device, not a native Windows USB stack.

---

## 2. Hardware baseline and CH375 access

The tested machine exposes the CH375 through ISA-style parallel I/O:

```text
0260h  data
0261h  command/status
```

The controller is probed with `CHECK_EXIST` before runtime USB hooks are installed.

Two rules follow from this:

1. Do not assume every CH375 board uses the same I/O mapping.
2. Never load CH375USB together with the vendor `CH375286.SYS`; both need exclusive access to the same controller.

The CH375 command port on CH375B/C hardware also exposes the active-low `INT#` state on bit 7. Polled status handling should therefore avoid consuming `GET_STATUS` blindly when no controller event is pending.

---

## 3. Overall architecture

CH375USB is deliberately split into small layers:

```text
DOS block device / DOS input bridges
                |
       shared CH375 ownership
                |
        storage abstraction
       /                   \
CH375 native DISK_*      USB MSC BOT/SCSI
                |
        generic USB core
        /       |       \
      HID      MSC      HUB
                |
          CH375 hardware I/O
```

The root mass-storage strategy is:

```text
try CH375 native storage
        |
        +-- success -> STORAGE_NATIVE
        |
        +-- failure -> generic USB enumeration
                         |
                         +-- HID
                         +-- hub
                         +-- MSC BOT/SCSI
```

Keeping the native path is intentional. It is efficient on a 386 and is already proven on the target hardware. The generic path remains necessary for devices the native firmware path cannot handle and for future hub use.

---

## 4. Polling and deferred work

The Pocket386 setup does not provide a controller IRQ path that CH375USB can rely on directly, so the driver is polling-oriented. Polling is divided by context.

### `INT 1Ch` timer

Used only for bounded, lightweight work:

- sample root connect/disconnect state;
- poll active HID endpoints;
- update retry/backoff counters;
- request deferred maintenance.

### `INT 28h` DOS idle

Used for heavier work:

- re-enumeration;
- native-storage probing;
- generic enumeration;
- hub scanning;
- storage-ready validation.

### `INT 33h` mouse path

Mouse-aware DOS applications call the mouse API much faster than 18.2 Hz, so the driver also polls the USB mouse on demand from relevant `INT 33h` activity.

**Rule:** never move full enumeration or long hardware waits into the timer just to make hotplug seem faster. Set state and defer heavy work.

---

## 5. Serialize every CH375 transaction

The CH375 is one controller shared by DOS block I/O, the timer, idle maintenance and on-demand mouse polling. Two command streams must never overlap.

0.4.13 uses a single atomic ownership primitive:

```asm
mov al,1
xchg al,[usb_busy]
```

A memory `XCHG` is atomic on the 386, eliminating the old check-then-set race. Code that performs CH375 transactions must acquire the lock and release it on every exit path.

A cheap non-atomic read of `usb_busy` may still be used only as an early skip optimization when the called routine performs the real atomic acquisition itself.

---

## 6. USB reset, speed and retry state

### Root reset

The documented root reset sequence used by the generic path is:

```text
mode 5  -> host enabled, no SOF
mode 7  -> assert USB reset
mode 6  -> release reset, resume host operation with SOF
```

Every error path must release reset. Leaving the CH375 in mode 7 makes healthy hardware look dead.

### Bus speed

Datasheet II documents `GET_DEV_RATE` in mode 5 and `SET_USB_SPEED` for low-speed operation. `SET_USB_MODE` restores full-speed mode, so low speed must be reapplied after the final mode switch.

Because some CH375 revisions do not support the documented speed helpers, the older CH375B-compatible low-speed sequence remains available as a compatibility fallback.

### Retry policy

CH375 retry behavior is controller state. The reset default can retry NAK indefinitely, which is unsuitable for a resident polling driver because idle HID endpoints normally NAK.

The generic path therefore programs retry behavior explicitly. Do not change retry policy globally without separately considering enumeration, bulk traffic and HID polling.

---

## 7. Control transfers and EP0

Manual control transfers must use the endpoint-zero maximum packet size advertised by the device descriptor. EP0 is not always 64 bytes; valid devices may use 8, 16, 32 or 64 bytes.

0.4.13 stores `D_EP0` and uses it when deciding whether an IN packet is a short packet. Treating every packet smaller than 64 bytes as terminal would truncate control transfers for smaller EP0 sizes.

The CH375 built-in `GET_DESCR` path has an internal 64-byte buffer. Large configuration descriptors therefore need the manual control-transfer fallback.

Control-transfer state such as `usb_control_wait` must be cleared on both success and failure paths. A leaked wait mode can change the behavior of unrelated later transactions.

---

## 8. Storage model

CH375USB exposes one DOS block-device unit. The drive letter remains reserved even when no media is present; absent storage returns drive-not-ready rather than dynamically creating and destroying DOS devices.

This architecture is what makes practical DOS hotplug possible.

### Native CH375 storage

The native path uses commands such as:

```text
DISK_INIT
DISK_SIZE
DISK_READY
DISK_READ / DISK_RD_GO
DISK_WRITE / DISK_WR_GO
DISK_R_SENSE
```

`DISK_SIZE` returns both capacity and sector size. The current driver accepts 512-byte sectors and rejects unsupported sizes safely.

`DISK_R_SENSE` is a real command with its own completion/status/data lifecycle; issue it, wait for completion, and drain returned sense data before sending another native command.

### Generic BOT/SCSI storage

The fallback path implements USB Mass Storage Bulk-Only Transport with commands including:

```text
TEST UNIT READY
READ CAPACITY(10)
READ(10)
WRITE(10)
SYNCHRONIZE CACHE(10)
```

BOT correctness requires:

- correct CBW construction;
- separate IN/OUT toggles;
- CSW signature validation;
- CSW tag validation;
- CSW status checking;
- bounded recovery;
- endpoint halt clearing and toggle reset after recovery.

A recognized MSC interface is not usable until `msc_init_slot` completes. The generic CH375 enumeration path now follows that same rule.

---

## 9. DOS block-device correctness

Several details are easy to get subtly wrong.

### Media Check

DOS expects:

```text
FFh  media changed
00h  unknown
01h  media not changed
```

The driver's internal `media_changed` boolean cannot be copied directly to the DOS result byte. 0.4.13 returns `FFh` once after a change and `01h` in steady state.

### Partial transfers

On read/write failure, request-header word `+18` must describe how many sectors were actually completed. Returning the originally requested count together with an error gives DOS contradictory information.

### LBA bounds

Both backends record capacity:

- native path: sector **count**;
- READ CAPACITY(10): inclusive **last LBA**.

0.4.13 validates the entire DOS request range before I/O and also guards individual sector access. Requests beyond the device return `ERR_SECTOR_NOT_FOUND`.

### Write completion

On the BOT path, a successful `WRITE(10)` followed by a failed `SYNCHRONIZE CACHE(10)` is not a fully successful DOS write. 0.4.13 propagates that final flush failure instead of returning `ST_DONE`.

---

## 10. Direction Flag discipline

Resident code cannot assume that the interrupted application left `DF=0`.

Low-level routines that use `LODSB` or `STOSB` therefore save FLAGS, execute `CLD`, perform the copy, and restore FLAGS before returning. This both protects the USB buffer operation and preserves the caller's original Direction Flag state.

This rule matters especially for code reachable from interrupt-driven resident paths.

---

## 11. HID keyboard

CH375USB targets **USB HID boot protocol**, not arbitrary HID report descriptors.

Keyboard initialization sets boot protocol and idle state. Reports contain modifiers plus up to six ordinary key usages. The driver translates HID usages to PC/AT Set 1 scan codes and injects them through the 8042-compatible keyboard path.

Important compatibility rules:

- track make and break transitions separately;
- preserve extended-key `E0h` prefixes;
- clear remembered key state on detach/re-enumeration;
- do not interpret USB HID usages as PC scan codes directly.

The current implementation provides basic working DOS keyboard support. Typematic, LED synchronization and a more complete non-US/numpad mapping remain optional future improvements.

---

## 12. HID mouse and the DOS mouse driver

Receiving a three-byte boot mouse report is simple; integrating it into DOS is not.

CH375USB does not try to replace every function of a conventional DOS mouse driver. Instead it detects a later-installed driver such as CuteMouse and hooks `INT 33h` in front of it.

This matters because CH375USB is normally loaded from `CONFIG.SYS`, while CTMOUSE is usually loaded later from `AUTOEXEC.BAT`.

Mouse state includes more than the latest button bitmap. Short press/release events are edges, so the driver latches press/release counters and positions rather than assuming an application will sample at exactly the right moment.

On-demand polling from the `INT 33h` path is retained because the BIOS timer rate alone is visibly too slow for responsive pointer movement in programs such as DOS EDIT.

---

## 13. Hotplug state machine

Hotplug is not a single present/absent flag. The driver tracks root-link state, pending events, maintenance state, retry/backoff, storage ownership and media-change state.

The useful split is:

### Fast detach path

- mark the topology detached;
- stop exposing stale storage/HID state;
- mark storage media changed;
- schedule maintenance.

### Deferred discovery path

- reset/reconfigure the CH375 as necessary;
- try native storage;
- fall back to generic enumeration;
- initialize HID/MSC/hub functions;
- rebuild usable device state.

Backoff prevents a physically settling device from causing an enumeration storm on every timer tick.

---

## 14. Windows 95 boundary

DOS input support and Windows 95 GUI input are separate problems.

CH375USB listens for Windows enhanced-mode notifications through `INT 2Fh` (`AX=1605h` / `1606h`) and suspends DOS-side HID behavior while Windows owns input. This also prevents stale DOS mouse callbacks from surviving into the Windows transition.

For storage, the tested compatibility method is:

```text
DOS detects USB storage
        -> DEVLOAD loads CH375USB immediately before WIN
        -> Windows can continue using the DOS-backed block device
```

This is not native Windows USB hotplug. Keyboard and mouse support in the Windows 95 GUI would require a separate protected-mode companion driver.

---

## 15. Timing philosophy

Datasheet minimum timing values are not performance targets. The tested ISA/Pocket386 environment includes bus timing, software overhead and hardware variation.

Current CH375 delays are intentionally conservative and have passed real-hardware testing. Some loop names such as `delay_10ms` are not accurate across every 386 speed, but this is treated as a performance/calibration topic rather than an active correctness defect.

If timing is optimized later:

1. measure on real hardware first;
2. change timing separately from functional code;
3. retain an easy rollback path;
4. regression-test storage, keyboard, mouse and hotplug after every timing change.

Do not remove proven margins solely because the datasheet minimum is lower.

---

## 16. Storage media can mislead debugging

Different USB flash controllers behave differently on minimal retro hosts. Before rewriting low-level code because one stick fails:

- test at least two flash drives;
- include an older/simple USB 2.0-era device if possible;
- compare cold boot with hot insertion;
- test read and write separately;
- verify the filesystem after reboot/remount.

A device-specific compatibility problem can look exactly like a controller timing bug.

---

## 17. Test discipline

A useful regression order is:

1. **Controller:** `CHECK_EXIST`, correct I/O mapping.
2. **Root connection:** attach/detach detection and reset release.
3. **Storage:** read, write, delete, remount/reboot verification.
4. **Keyboard:** typing, modifiers, make/break behavior, hotplug.
5. **Mouse:** movement, short clicks, held buttons, hotplug, DOS EDIT.
6. **Cross-class hotplug:** storage -> keyboard -> mouse -> storage without reboot.
7. **DOS -> Windows 95:** actively use DOS HID, exit the application, then start Windows.
8. **Hub:** only after direct-root behavior is stable.

For storage changes, include at least one test where a device is removed and a different device is inserted under the same DOS drive letter.

---

## 18. Current limits and priorities

Keep these distinctions clear:

### Supported and tested

- one DOS block device;
- 512-byte storage sectors;
- FAT12/FAT16-oriented DOS storage use;
- root USB mass storage;
- boot-protocol keyboard;
- boot-protocol mouse;
- DOS hotplug.

### Implemented but experimental

- one external hub with limited downstream support.

### Deliberately deferred

- FAT32;
- Multi-LUN selection;
- non-512-byte sector translation;
- low-speed devices behind a hub;
- full DOS removable-media capability advertisement;
- native Windows 95 HID support;
- optional HID usability extensions.

See [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) for the current list and priorities.

---

## 19. Rules for future changes

When modifying CH375USB:

1. Preserve the known-good native storage path unless a tested replacement is clearly better.
2. Keep timer work bounded and defer heavy enumeration to DOS idle time.
3. Acquire the shared CH375 lock before any controller transaction sequence.
4. Preserve caller CPU state, including FLAGS/DF, in resident low-level routines.
5. Treat controller mode, speed, retry policy, endpoint toggles and address as explicit state.
6. Complete each CH375 command's full status/data lifecycle before starting another command.
7. Validate storage capacity and DOS request semantics before touching media.
8. Do not convert a reviewer's theoretical concern into a code change without checking the source, datasheets and real hardware.
9. Change one architectural variable at a time and keep regression tests reproducible.
10. Distinguish clearly between **implemented**, **tested**, and merely **planned** behavior in public documentation.

These rules are more important to the project's stability than any individual optimization.
