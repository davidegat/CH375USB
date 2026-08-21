# Known issues / deferred work

This file tracks confirmed open CH375USB issues, deliberately unsupported
features, and code-review observations that still require targeted validation.
Items are not treated as bugs merely because an external review suggested them.

## Fixed in 0.4.13

### Direction Flag was not isolated around resident string operations

**Status:** FIXED IN `0.4.13` — basic real-hardware regression passed.

`ch_write_packet`, `ch_read_packet`, `native_read_chunk64`, and
`native_write_chunk64` use `LODSB`/`STOSB`. Earlier builds implicitly assumed
DF=0. Because resident work can run while a DOS application owns the CPU state,
a caller with DF=1 could make those string operations run backwards.

**0.4.13 fix:** each affected routine saves FLAGS, executes `CLD`, and
restores FLAGS before returning. This makes the copy direction local to the
routine without changing the caller's DF state.

### DOS read/write errors left the requested sector count unchanged

**Status:** FIXED IN `0.4.13` — normal read/write regression passed; targeted partial-error injection remains untested.

On a partial block read/write failure, DOS expects request-header word `+18` to
contain the number of sectors actually completed. Earlier builds returned an
error status but left the original requested count in place.

**0.4.13 fix:** the failure paths report `requested - remaining`, where
CX still includes the sector that failed.

### CH375 built-in generic enumeration recognized MSC but did not initialize it

**Status:** FIXED IN `0.4.13` — no regression observed; targeted forced BOT-fallback testing remains unverified.

The `generic_root_enumerate_wch` path parsed HID/hub capabilities but did not
run `msc_init_slot` when `CAP_MSC` was present. Root flash drives normally hide
this because CH375 native `DISK_*` support is attempted first.

**0.4.13 fix:** a root MSC interface recognized by the built-in generic path
now runs the existing BOT/SCSI initialization. Failure is not fatal to an
otherwise useful composite device, matching the existing manual path.

## Confirmed open issues

### BOT `SYNCHRONIZE CACHE` failure is not propagated to DOS

**Status:** TODO — deferred for a later driver revision.

When `SYNC_AFTER_WRITE = 1`, `drv_write` calls `storage_sync_cache` after all
requested sectors have been written. On the generic BOT path this sends SCSI
`SYNCHRONIZE CACHE(10)`. If it fails, Carry is returned by the storage layer but
`drv_write` currently ignores it and still returns `ST_DONE` to DOS.

The preceding `WRITE(10)` and CSW validation may have succeeded; the bug is the
final flush-error reporting. The CH375 native `DISK_*` path currently considers
a write complete when `DISK_WRITE` returns `USB_INT_SUCCESS` and has no separate
flush command implemented.

**Planned fix:** check Carry after `storage_sync_cache` and return an appropriate
write error on BOT flush failure.

### `usb_busy` acquisition is not atomic

**Status:** CONFIRMED DESIGN RACE — fix deferred for a dedicated test build.

Several paths check `usb_busy` and set it in separate instructions. In contexts
such as DOS idle processing, an interrupt can occur between the check and the
set, allowing two CH375 command sequences to overlap. A future fix should use a
single ownership helper (for example an atomic `XCHG`) and audit every acquire
and release path rather than mechanically replacing individual instructions.

### No LBA bounds check before sector I/O

**Status:** TODO / hardening.

`msc_last_lba` and `native_sector_count` are recorded but are not used to reject
requests beyond the device. A future change should validate the partition base
and requested LBA/count before issuing storage commands and return
`ERR_SECTOR_NOT_FOUND` for out-of-range requests.

### CH375 absent: the driver still reserves its DOS unit/resident memory

**Status:** TODO / cleanup.

If the controller probe fails, CH375USB does not install its runtime interrupt
hooks, but initialization still reports one block-device unit and keeps the
normal resident image. A later cleanup can return zero units and a minimal break
address when no controller exists.

### No user-visible DOS notification for attach/detach events

**Status:** TODO — feature not implemented yet.

Hotplug is handled internally for supported storage, boot keyboard, and boot
mouse devices, but DOS does not display a connection/disconnection message.
Direct console output from timer/USB paths should be avoided.

**Planned design:** a small resident event queue exposed through a private
`INT 2Fh` API, with an optional `CH375MON.COM` utility to display/log events.

## Deliberately unsupported / deferred features

### Full DOS removable-media capability advertisement

The current header remains a basic block device. The Media Check bug is fixed
without changing the attribute word. A future implementation may advertise DOS
Open/Close/Removable-Media support with bit 11 (`0800h`) and implement command
15 correctly. Do **not** add bit 6 (`0040h`) unless Generic IOCTL support is
actually implemented; therefore `0840h` is not an accepted drop-in fix.

### FAT32

The documented storage target remains an MBR disk with a primary FAT16
partition no larger than 2 GiB. Merely accepting partition types `0Bh/0Ch` is
not sufficient: the resident BPB representation/copy path would also need the
FAT32 BPB fields. Treat this as a separate feature, not a one-line bug fix.

### Multi-LUN

The driver currently exposes one DOS block device and uses LUN 0. A useful first
extension would be to query the maximum LUN and select the first ready logical
unit while still exposing one DOS drive. Multiple simultaneous DOS drive letters
would require a larger architectural change.

### Non-512-byte sector translation

Non-512-byte physical/logical media is detected and rejected safely. Supporting
1024/2048/4096-byte sectors would require a DOS-512-byte translation layer and,
for writes, read-modify-write/cache semantics. Leave disabled until there is a
real test device that needs it.

### Low-speed devices behind an external hub

Hub support remains experimental and unvalidated. Low-speed downstream devices
are detected but intentionally not enumerated by the current hub path. Do not
present this as supported until the hub implementation has real-hardware tests.

### HID usability extensions

Deferred items include keyboard typematic/repeat, faster/on-demand keyboard
polling, a more complete non-US/numpad scancode table, keyboard LED SET_REPORT,
and configurable mouse mickey/sensitivity scaling. These are compatibility or
usability improvements, not regressions in the currently documented basic HID
support.

## Review candidates requiring targeted validation

The following observations are plausible enough to keep, but are **not yet
accepted as confirmed bugs** and should be handled in separate test builds:

- **Phase-specific `SET_RETRY` policy.** `05h` makes NAK visible to software and
  was already present in the 0.4.11 baseline; it is not a 0.4.12 regression.
  Built-in CH375 control commands may benefit from a finite hardware NAK-retry
  policy during enumeration, while HID polling should remain fast/nonblocking.
- **Defensive configuration-descriptor truncation check.** DS2 says built-in
  `GET_DESCR` returns `USB_INT_BUF_OVER` above its 64-byte control buffer, which
  already sends the driver to the manual path. Comparing `wTotalLength` against
  the received length as an additional guard is still reasonable hardening.
- **CPU-dependent delay loops.** `delay_10ms`/`delay_100ms` are much longer on a
  386 than their names imply, and `ch_delay`/`ch_cmd_delay` are deliberately
  conservative. A refresh-bit-based calibration may improve speed, but must be
  measured on Pocket386-class hardware before replacing proven timings.
- **Port `20h` used as a settling read.** It is a PIC command-port read and is
  not destructive, but a neutral delay source would avoid dependence on PIC
  state. Change only together with timing work.
- **`RD_USB_DATA0`.** DS2 calls it only slightly more efficient than
  `RD_USB_DATA`; consider an A/B performance test rather than a blind swap.
- **8042 keyboard injection critical section/output-buffer handling.** The
  command-`D2h` plus data write sequence deserves a dedicated concurrency test
  before adding interrupt masking or extra buffer handling.
- **Private ISR stack.** Timer/idle paths currently use the interrupted
  program's stack. A private stack may improve robustness with unusually small
  application stacks but is a structural change that needs careful nesting
  rules.
- **USB address recycling for hub devices.** `next_address` can grow across
  downstream attach/detach cycles. Relevant only once hub support itself is
  validated; add recycling before calling hub support stable.
- **BOT Check Condition handling.** Current BOT errors recover aggressively.
  Distinguishing normal SCSI Check Condition from phase errors and using
  REQUEST SENSE may improve removable-media behavior, but should be implemented
  with explicit CSW status handling and tests.
- **Second `CHECK_EXIST` probe pattern.** A second complement test would make
  controller detection slightly more defensive; low priority.
- **Maintenance cleanup.** `DRIVER_VERSION` is not yet used to build every
  visible version string, and some constants/fields/routines appear unused.
  Clean these only after functional changes settle.

## Resolved

### DOS Media Check result mapping

**Resolved in `0.4.13` (first tested in `0.4.12-exp1a`).** Internal
`media_changed=1` now returns DOS `FFh` once and the steady state returns `01h`.

### Manual control-IN EP0 short-packet threshold

**Resolved in `0.4.13` (first tested in `0.4.12-exp1a`).** The
manual control-IN loop now uses the selected device's `D_EP0` value, with an
8-byte fallback if it is not initialized.
