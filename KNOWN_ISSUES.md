# Known issues / deferred work

This file tracks **current** CH375USB limitations and work that may be worth doing later. Items already fixed in 0.4.13 are listed briefly at the end for traceability; the full history belongs in [`CHANGELOG.md`](CHANGELOG.md).

## Confirmed open issues

### CH375 absent: the driver still reserves its DOS unit/resident memory

**Status:** confirmed cleanup issue; low priority.

If the controller probe fails, CH375USB does not install its runtime interrupt hooks, but initialization still reports one block-device unit and keeps the normal resident image. A future cleanup can return zero units and a minimal break address when no controller exists.

## Useful future features

### User-visible DOS notification for attach/detach events

Hotplug already works internally for supported storage, boot keyboard and boot mouse devices, but DOS does not display connection/disconnection messages.

**Preferred design:** keep console output out of timer/USB paths. Add a small resident event queue exposed through a private `INT 2Fh` API, with an optional `CH375MON.COM` utility to display or log events.

### Full DOS removable-media capability advertisement

The current driver keeps a basic block-device header and uses the corrected Media Check semantics for hotplug. A future implementation may advertise DOS Open/Close/Removable-Media support with attribute bit 11 (`0800h`) and implement command 15 correctly.

Do **not** add bit 6 (`0040h`) unless Generic IOCTL support is actually implemented; `0840h` is therefore not an accepted drop-in change.

### FAT32

The documented storage target remains an MBR disk with a primary FAT16 partition no larger than 2 GiB. Merely accepting partition types `0Bh`/`0Ch` is not sufficient because the resident BPB representation/copy path would also need FAT32 fields.

Treat FAT32 as a separate feature with explicit DOS 7.x testing.

### Multi-LUN

The driver currently exposes one DOS block device and uses LUN 0. A useful first extension would be to query the maximum LUN and select the first ready logical unit while still exposing one DOS drive. Multiple simultaneous DOS drive letters would require a larger architectural change.

### External hub completion

One external hub with up to four downstream ports is partially implemented, but the path has not yet received real-hardware validation. Low-speed downstream devices are detected but intentionally not enumerated by the current hub code.

Hub support should remain experimental until tested systematically with keyboard, mouse, storage, mixed devices and downstream hotplug.

### HID compatibility / usability

Potential improvements include:

- keyboard typematic/repeat;
- faster or on-demand keyboard polling;
- a more complete non-US/numpad scancode table;
- keyboard LED `SET_REPORT` support;
- configurable mouse mickey/sensitivity scaling.

These are compatibility/usability improvements, not regressions in the currently documented basic HID support.

## Deliberately unsupported unless a real use case appears

### Non-512-byte sector translation

Non-512-byte media is detected and rejected safely. Supporting 1024/2048/4096-byte sectors would require a DOS-512-byte translation layer and read-modify-write/cache semantics for partial writes.

Do not implement this merely because the CH375 can report other sector sizes; wait for a real target device that needs it.

### Windows 95 HID

DOS keyboard and mouse support do not automatically become Windows 95 GUI input. Proper Windows HID support would require a separate protected-mode companion driver. Keep that architectural boundary explicit.

## Worth validating only in dedicated experiments

### Phase-specific `SET_RETRY` policy

The current `05h` bounded policy was already present in the 0.4.11 baseline; it was not introduced as a 0.4.12 regression. Built-in CH375 control commands may benefit from a finite hardware NAK-retry policy during enumeration, while HID polling must stay fast and nonblocking.

Change this only in a dedicated test build with clear enumeration regression tests.

### Defensive configuration-descriptor length check

Datasheet II says built-in `GET_DESCR` reports `USB_INT_BUF_OVER` when the descriptor exceeds its 64-byte internal buffer, which already pushes CH375USB toward the manual path. Comparing `wTotalLength` with the received length would still be reasonable defensive hardening.

### BOT Check Condition / REQUEST SENSE handling

The current BOT path recovers aggressively after command failure. Distinguishing normal SCSI Check Condition from BOT phase errors and issuing REQUEST SENSE may improve removable-media behavior, but requires explicit CSW-status handling and targeted tests.

### Private ISR stack

Timer and idle paths currently use the interrupted program's stack. A private resident stack could improve robustness for applications with unusually small stacks, but it is a structural change with nesting/reentrancy implications. Do not add it without a concrete test case.

### Timing optimization

The current software delays are conservative and CPU-dependent. They are not treated as a correctness bug because the tested Pocket386 is stable with them. If performance work becomes worthwhile, measure first and change timing separately from functional changes.

## Resolved in 0.4.13

The following issues were fixed and regression-tested as part of the same 0.4.13 release:

- DOS Media Check returned the internal boolean instead of DOS `FFh` / `01h` semantics.
- Manual control-IN assumed EP0 was always 64 bytes.
- Resident `LODSB`/`STOSB` operations did not isolate the Direction Flag.
- Partial DOS read/write failures left the originally requested sector count unchanged.
- Generic CH375 enumeration could recognize MSC without initializing the BOT/SCSI path.
- BOT `SYNCHRONIZE CACHE(10)` failure was ignored by `drv_write`.
- `usb_busy` acquisition used a non-atomic check-then-set sequence.
- Storage requests were not bounded against the capacity reported by the active backend.
