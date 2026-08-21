# Changelog

All notable CH375USB changes are recorded here.

## [Unreleased]

No unreleased code changes yet.

## [0.5.0] - 2026-08-21

0.5.0 is a major input-architecture update built on the hardware-tested 0.4.13 DOS storage/HID baseline. The release moves the mouse path toward conventional BIOS/PS/2 semantics, adds a Win16 mouse-driver bridge for Windows 95 experiments, adds Windows-aware incremental HID hotplug handling, and improves keyboard behavior.

Prebuilt 0.5.0 `CH375USB.SYS` and `CH375MOU.DRV` binaries are published in [`binary/`](binary/) alongside the source.

### Added

- Added `bios_mouse.inc`, an independently written virtual BIOS PS/2 pointing-device interface using the public `INT 15h AH=C2h` callback API.
- Added the BIOS/PS2 mouse bridge so USB HID mouse reports can be consumed by a conventional BIOS/PS2-aware DOS mouse driver such as CuteMouse rather than relying on unsafe 8042 AUX/D3 stuffing.
- Added `CH375MOU.ASM` and `CH375MOU.LNK`, implementing an original Win16 mouse-driver bridge for Windows 95 experiments.
- Added a Windows-aware `INT 33h` bridge path so the DOS mouse event stream can remain available to `CH375MOU.DRV` after Windows enters enhanced mode.
- Added `win_hotplug.inc`, an incremental root HID-mouse enumeration state machine designed to perform only bounded work per timer tick while Windows is active.
- Added keyboard software typematic/repeat tracking.
- Added build-time validation that the generated `CH375MOU.DRV` is a real Win16 NE DLL/driver image.

### Changed

- `CH375USB.SYS` is now the unified DOS/Windows-aware build; there is no separate `CH375W.SYS` release driver.
- Mouse handling now prefers the BIOS PS/2 callback model while preserving the conventional DOS `INT 33h` ownership model.
- The BIOS PS/2 shim mirrors supported configuration requests to the USB side while allowing the physical BIOS to remain authoritative when it succeeds. This is intended to permit physical PS/2 and CH375 USB mouse coexistence through one DOS mouse driver.
- The Windows mouse companion uses processed `INT 33h` position/callback data and leaves Windows responsible for acceleration behavior.
- Windows-side USB mouse re-enumeration is split into a staged state machine instead of performing full controller reset/enumeration inside one timer invocation.
- `build.sh` now builds both `CH375USB.SYS` and `CH375MOU.DRV`, bootstraps a project-local Open Watcom V2 toolchain when needed, and rejects linker warnings or malformed NE output.
- `BUILD.BAT` remains a simple DOS/Windows command-line build of `CH375USB.SYS` only.
- Release documentation is organized under `documentation/` for `NOTICE.md`, `KNOWLEDGE.md` and `KNOWN_ISSUES.md`; prebuilt release binaries are organized under `binary/`.

### Compatibility status

- DOS USB mass storage remains supported through the established native/BOT paths.
- DOS USB boot keyboard remains supported; 0.5.0 adds software typematic/repeat.
- DOS USB boot mouse remains supported through the BIOS/PS2 plus conventional DOS mouse-driver path.
- CuteMouse / CTMOUSE is the tested/reference runtime DOS mouse dependency for the 0.5.0 mouse stack.
- Windows 95 USB mouse support through `CH375MOU.DRV` is implemented but remains experimental and is not claimed as universally validated.
- Windows 95 USB keyboard input is not implemented.
- Windows 95 storage remains a DOS-backed compatibility path rather than native USB mass-storage Plug and Play.

### Important design decision

The 0.5.0 mouse path deliberately avoids injecting mouse bytes through the physical 8042 AUX/D3 mechanism. The USB mouse is represented through the standard BIOS PS/2 callback interface instead. This keeps the integration closer to the interface expected by conventional DOS mouse drivers and avoids taking ownership of the physical controller's AUX output queue.

### Build / distribution

- Prebuilt 0.5.0 binaries are published as `binary/CH375USB.SYS` and `binary/CH375MOU.DRV`.
- The stale tracked `CH375USB.SYS` 0.4.13 binary was removed before the 0.5.0 binary was published.
- `build.sh` and `BUILD.BAT` still produce local build outputs in the working-tree root; `binary/` contains the published release copies.

### Still open / experimental

- Broader Windows 95 real-hardware and application validation of `CH375MOU.DRV`.
- Windows 95 USB keyboard support.
- Compatibility with programs/games that bypass normal BIOS/DOS keyboard paths.
- Exact typematic timing parity with all physical AT keyboards.
- External hub completion and real-hardware validation.
- FAT32 support.
- Multi-LUN exposure.
- Non-512-byte sector translation.
- Full DOS removable-media capability advertisement.
- Cleanup when no CH375 controller is present.

## [0.4.13] - 2026-08-21

0.4.13 is the completed hardware-tested DOS baseline from which 0.5.0 was developed.

### Fixed

- Corrected DOS Media Check semantics: internal `media_changed=1` returns `FFh` once, while steady state returns `01h`.
- Manual control-IN transfers use the selected device's real EP0 maximum packet size.
- Resident string operations isolate and restore the Direction Flag.
- Partial DOS block read/write failures return the number of sectors actually completed.
- Generic CH375 enumeration initializes a recognized MSC interface through the existing BOT/SCSI `msc_init_slot` path.
- BOT `SYNCHRONIZE CACHE(10)` failures propagate through `drv_write`.
- CH375 controller ownership uses atomic `usb_lock_try` / `usb_unlock` serialization.
- Storage requests are bounded against reported media capacity and return `ERR_SECTOR_NOT_FOUND` when out of range.

### Verified on Pocket386 / DOS

- USB mass-storage read/write.
- USB keyboard input.
- USB mouse movement/buttons.
- Live attach/detach for storage, keyboard and mouse.

### Datasheet / low-level work retained from 0.4.12

- Documented mode `5 -> 7 -> 6` root reset sequence.
- `GET_DEV_RATE` / `SET_USB_SPEED` handling with legacy fallback.
- Explicit bounded retry policy for generic USB traffic.
- Command-port `INT#` checking before consuming status.
- Native `DISK_SIZE` parsing and safe rejection of non-512-byte sectors.
- Proper `DISK_R_SENSE` completion/data draining.
- Correct cleanup of failed zero-data control OUT state.

## [0.4.12] - 2026-08-20

First changelog entry. Changes were relative to the 0.4.11 known-good baseline.

### Changed

- Audited low-level CH375 host code against WCH CH375 Datasheet (I), Version 4 and CH375 Datasheet (II), Version 4.
- Added documented root reset, speed detection and explicit retry handling.
- Improved native mass-storage size/sense handling.

### Fixed

- Failed zero-data control OUT requests now clear `usb_control_wait`.

## [0.4.11] - baseline

Previous known-good reference. It already provided DOS USB mass storage, boot-protocol keyboard and mouse support, DOS hotplug, the `INT 33h` mouse bridge, and Windows transition safeguards for DOS HID hooks.
