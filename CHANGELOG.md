# Changelog

All notable CH375USB changes from the current maintained baseline are recorded here.

The changelog starts with **0.4.12**; **0.4.11** is treated as the previous known-good baseline. Future driver changes should add a new entry here when they are committed.

## [Unreleased]

### Documentation

- Replaced the README `Current limitations` section with a practical troubleshooting guide.
- Documented the Windows 95 requirement to have USB storage connected and detected before Windows starts.
- Documented the `AUTOEXEC.BAT` / `DEVLOAD` workaround when Windows 95 reports MS-DOS compatibility mode instead of loading CH375USB from the Windows `CONFIG.SYS` profile.
- Added the Pocket386 BIOS mouse-support check for cases where the USB mouse unexpectedly stops working.
- Added the recommended USB-storage layout: MBR partition table, one primary FAT16 partition no larger than 2 GiB, with Windows and Linux formatting examples.
- Clarified that hub support is implemented only experimentally and has not yet been validated on real hardware.
- Clarified that CH375USB is a DOS driver; Windows 95 keyboard/mouse input is not supported, while the documented DOS-backed storage compatibility path remains available.
- Explicitly documented that Windows 3.1 / Windows for Workgroups 3.11 have not been tested.

## [0.4.12] - 2026-08-20

First changelog entry. Changes are relative to CH375USB 0.4.11.

### Changed

- Audited the low-level CH375 host code against WCH **CH375 Datasheet (I), Version 4** and **CH375 Datasheet (II): USB Basic Transmission Commands, Version 4**.
- Root generic enumeration now follows the documented host-mode flow more explicitly: mode 5 before reset, mode 7 to assert USB reset, and mode 6 to release reset/resume normal host operation.
- Added documented root-device speed detection with `GET_DEV_RATE`; low-speed devices use `SET_USB_SPEED 02h` after the final mode switch because `SET_USB_MODE` restores full-speed operation.
- Retained the older CH375B low-speed sequence as a compatibility fallback rather than removing a path already proven useful on real hardware.
- Centralized explicit `SET_RETRY` handling for generic USB traffic so the driver does not accidentally inherit the CH375 reset default `85h`, which can retry NAK indefinitely.
- Control-transfer fallback now checks command-port bit 7 / `INT#` before consuming `GET_STATUS`.
- Native `DISK_SIZE` output is now read and parsed; the current driver accepts 512-byte physical sectors and safely rejects unsupported non-512-byte media instead of assuming 512 bytes.
- Native `DISK_R_SENSE` recovery now waits for command completion and drains returned sense data before the next `DISK_*` command.
- Added named Datasheet II constants for speed/retry/status/transaction commands used or documented by the driver.

### Fixed

- A failed zero-data control OUT request now clears `usb_control_wait`, preventing later transfers from inheriting the wrong long-wait state.

### Verified on Pocket386

- USB mass storage remained operational after the audit.
- USB keyboard remained operational.
- Live root-device replacement was tested successfully: flash drive -> keyboard -> flash drive, without rebooting.
- USB mouse remained operational; an apparent failure during testing was traced to the BIOS mouse setting being disabled rather than to a driver regression.

### Intentionally unchanged

- `RD_USB_DATA0` is not substituted globally only for its small documented efficiency gain.
- Multi-LUN support is not enabled yet.
- Non-512-byte physical-sector translation is not implemented.
- Low-speed devices behind an external hub remain experimental.
- Existing conservative 386-side software delays are retained.

## [0.4.11] - baseline

Previous known-good reference version used as the starting point for this changelog. It already provided DOS USB mass storage, boot-protocol keyboard and mouse support, DOS hotplug, the CTMOUSE/`INT 33h` bridge, and Windows 95 transition safeguards for the DOS HID hooks.
