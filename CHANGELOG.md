# Changelog

All notable CH375USB changes are recorded here.

## [0.5.1] - 2026-08-22

0.5.1 makes CH375USB self-contained for mouse input and removes CuteMouse/CTMOUSE as a runtime dependency.

### Added

- Resident DOS `INT 33h` mouse provider in `internal_mouse.inc`.
- Direct CH375 USB HID mouse feed into the internal `INT 33h` core.
- Physical PS/2 backend using the real BIOS `INT 15h/C207` callback.
- Deferred PS/2 event processing outside the BIOS callback to avoid hard-locking the machine inside the callback path.
- Separate PS/2 and USB button state merged into one logical three-button mouse.
- Windows-time physical PS/2 callback re-arm when `CH375MOU.DRV` installs its `INT 33h/0Ch` callback.
- On-demand DOS text-mode cursor rendering for `INT 33h/01h`, with hide/restore behavior and `INT 33h/0Ah` text masks.
- `MTEST.ASM`, a compact DOS `INT 33h` coordinate/button diagnostic.
- Native DOS and Linux build rules for `MTEST.COM`.

### Changed

- USB mouse input no longer travels through the virtual BIOS PS/2 layer when the internal mouse driver is enabled.
- CH375USB no longer needs an external DOS mouse driver to own `INT 33h`.
- Physical PS/2 and CH375 USB mice can coexist and control the same logical mouse in DOS and Windows 95.
- Windows 95 mouse support through `CH375MOU.DRV` no longer requires CuteMouse.
- DOS cursor rendering occurs only when requested by an application; there is no always-visible driver cursor at the DOS prompt.
- Build scripts now produce `CH375USB.SYS`, `CH375MOU.DRV`, `CH375MOU.MAP`, and `MTEST.COM`.

### Hardware validation

Verified on Pocket386:

- DOS PS/2 movement/buttons through `MTEST`.
- DOS USB hotplug movement/buttons through `MTEST`.
- DOS PS/2 + USB simultaneous operation.
- EDIT with PS/2 and USB, including the requested software cursor.
- Monkey Island with PS/2, including cursor/input operation.
- Windows 95 PS/2 operation.
- Windows 95 USB mouse when present at startup.
- Windows 95 first USB mouse hotplug after startup.
- Windows 95 PS/2 + USB simultaneous operation.
- No hard freeze when moving the physical PS/2 mouse.

### Known limitation

- Under Windows 95, USB mouse **unplug followed by replug in the same session** does not currently restore USB mouse input. A first hotplug works and the physical PS/2 mouse remains operational. Deferred as a minor follow-up.

## [0.5.0] - 2026-08-21

0.5.0 introduced the unified DOS/Windows-aware driver, BIOS PS/2 interoperability layer, `CH375MOU.DRV`, Windows-aware HID mouse hotplug state machine, keyboard software typematic, and native DOS build of both drivers. Mouse operation still used a conventional external DOS `INT 33h` driver as the reference runtime path.

## [0.4.13] - 2026-08-21

Hardware-tested DOS storage/HID baseline with corrected media-change semantics, EP0 handling, Direction Flag isolation, partial transfer counts, generic MSC initialization, cache-flush error propagation, CH375 locking and LBA bounds checking.

## [0.4.12] - 2026-08-20

Low-level CH375 host audit against the WCH datasheets, including documented root reset/speed handling and native mass-storage size/sense fixes.

## [0.4.11] - baseline

Earlier known-good DOS reference with USB storage, boot-protocol keyboard/mouse support and DOS hotplug.
