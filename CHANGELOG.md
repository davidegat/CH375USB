# Changelog

All notable CH375USB changes from the current maintained baseline are recorded here.

The changelog starts with **0.4.12**; **0.4.11** is treated as the previous known-good baseline.

## [Unreleased]

No unreleased code changes yet.

## [0.4.13] - 2026-08-21

0.4.13 is the current hardware-tested release. It promotes the 0.4.12 experimental line and incorporates the focused correctness fixes found during subsequent code review and DOS real-hardware testing. The later fixes listed below remain part of the same **0.4.13** version; there is no additional version suffix.

### Fixed

- Corrected DOS Media Check semantics: internal `media_changed=1` now returns `FFh` (changed) once, while steady state returns `01h` (not changed).
- Manual control-IN transfers now use the selected device's real `D_EP0` maximum packet size when deciding whether a packet is short instead of assuming 64 bytes.
- `ch_write_packet`, `ch_read_packet`, `native_read_chunk64`, and `native_write_chunk64` now save FLAGS, execute `CLD`, and restore FLAGS so resident USB string operations cannot run backwards when interrupted code has DF set.
- Partial DOS block read/write failures now return the number of sectors actually completed in request-header word `+18` instead of leaving the original requested count unchanged.
- The CH375 built-in generic enumeration path now initializes a recognized root MSC interface through the existing BOT/SCSI `msc_init_slot` path.
- BOT `SYNCHRONIZE CACHE(10)` failures are now propagated back through `drv_write`; a failed final cache flush no longer returns a successful DOS write status.
- CH375 controller ownership now uses an atomic `usb_lock_try` / `usb_unlock` pair based on memory `XCHG`, removing the check-then-set race between timer, DOS idle, DOS block-I/O and on-demand mouse polling paths.
- Storage requests are now checked against the capacity reported by the active backend before I/O begins, and individual sector access is also guarded. Out-of-range requests return `ERR_SECTOR_NOT_FOUND` instead of being sent to the device.

### CH375 datasheet / low-level changes retained from 0.4.12

- Audited the low-level CH375 host code against WCH **CH375 Datasheet (I), Version 4** and **CH375 Datasheet (II): USB Basic Transmission Commands, Version 4**.
- Generic root reset follows the documented mode `5 -> 7 -> 6` sequence and always releases reset on error paths.
- Added documented root-device speed detection using `GET_DEV_RATE`, with `SET_USB_SPEED` reapplied after `SET_USB_MODE` when low speed is selected.
- Retained the older CH375B low-speed sequence as a compatibility fallback.
- Generic USB traffic programs CH375 retry policy explicitly instead of silently inheriting the reset default.
- Control-transfer fallback checks command-port bit 7 / `INT#` before consuming `GET_STATUS`.
- Native `DISK_SIZE` output is consumed and parsed; unsupported non-512-byte sectors are rejected safely.
- Native `DISK_R_SENSE` recovery waits for completion and drains returned sense data before continuing.
- A failed zero-data control OUT request clears `usb_control_wait` correctly.

### Verified on Pocket386 / DOS

- USB mass-storage read/write works.
- USB keyboard works.
- USB mouse movement and buttons work through the DOS mouse interface.
- Storage, keyboard and mouse were each tested with live attach/detach and all three worked correctly plug-and-play under DOS after the completed 0.4.13 changes.
- Earlier 0.4.13 development testing separately verified the Media Check and EP0 fixes before the later storage-safety and locking changes were added.

### Documentation

- README updated for the completed 0.4.13 release, with a table of contents and only three high-level fix groups; detailed changes remain here in the changelog.
- `KNOWN_ISSUES.md` updated so the cache-flush, controller-lock and LBA-range issues are no longer listed as open.
- `KNOWLEDGE.md` rewritten into a concise, ordered engineering reference without repeated historical explanations.

### Still deferred

- Cleanup when no CH375 controller is present: the driver still reserves its DOS unit/resident image after a failed probe.
- Optional user-visible DOS attach/detach notifications.
- Full DOS removable-media capability advertisement / command 15.
- FAT32 support.
- Multi-LUN selection.
- Non-512-byte sector translation.
- Real-hardware validation and completion of external-hub support, including low-speed downstream devices.
- Additional HID compatibility/usability work.
- Targeted experiments around phase-specific `SET_RETRY` policies and timing optimization.
- More nuanced BOT Check Condition / REQUEST SENSE handling.

## [0.4.12] - 2026-08-20

First changelog entry. Changes are relative to CH375USB 0.4.11.

### Changed

- Audited the low-level CH375 host code against WCH **CH375 Datasheet (I), Version 4** and **CH375 Datasheet (II): USB Basic Transmission Commands, Version 4**.
- Root generic enumeration follows the documented host-mode flow more explicitly: mode 5 before reset, mode 7 to assert USB reset, and mode 6 to release reset/resume normal host operation.
- Added documented root-device speed detection with `GET_DEV_RATE`; low-speed devices use `SET_USB_SPEED 02h` after the final mode switch because `SET_USB_MODE` restores full-speed operation.
- Retained the older CH375B low-speed sequence as a compatibility fallback rather than removing a path already proven useful on real hardware.
- Centralized explicit `SET_RETRY` handling for generic USB traffic so the driver does not accidentally inherit the CH375 reset default `85h`, which can retry NAK indefinitely.
- Control-transfer fallback checks command-port bit 7 / `INT#` before consuming `GET_STATUS`.
- Native `DISK_SIZE` output is read and parsed; the driver accepts 512-byte physical sectors and safely rejects unsupported non-512-byte media.
- Native `DISK_R_SENSE` recovery waits for command completion and drains returned sense data before the next `DISK_*` command.
- Added named Datasheet II constants for speed/retry/status/transaction commands used or documented by the driver.

### Fixed

- A failed zero-data control OUT request clears `usb_control_wait`, preventing later transfers from inheriting the wrong long-wait state.

### Verified on Pocket386

- USB mass storage remained operational after the audit.
- USB keyboard remained operational.
- Live root-device replacement worked in the sequence flash drive -> keyboard -> flash drive without rebooting.
- USB mouse remained operational; an apparent failure during testing was traced to the BIOS mouse setting being disabled rather than to a driver regression.

### Intentionally unchanged

- `RD_USB_DATA0` was not substituted globally only for its small documented efficiency gain.
- Multi-LUN support was not enabled.
- Non-512-byte physical-sector translation was not implemented.
- Low-speed devices behind an external hub remained experimental.
- Existing conservative 386-side software delays were retained.

## [0.4.11] - baseline

Previous known-good reference version. It already provided DOS USB mass storage, boot-protocol keyboard and mouse support, DOS hotplug, the CTMOUSE/`INT 33h` bridge, and Windows 95 transition safeguards for the DOS HID hooks.
