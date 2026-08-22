# Known issues / deferred work — CH375USB 0.5.1

This file tracks current limitations and intentionally deferred work. Historical fixes belong in [`../CHANGELOG.md`](../CHANGELOG.md).

## Confirmed open issues

### Windows 95 USB mouse unplug -> replug

A USB mouse works when present before Windows starts and also works on the **first hotplug after Windows is already running**. If that USB mouse is then unplugged and replugged again in the same Windows 95 session, USB mouse input currently does not resume.

The physical PS/2 mouse remains operational. This is considered a minor hotplug state-machine cleanup item and is deliberately deferred so the validated 0.5.1 mouse architecture is not disturbed.

### Windows 95 USB keyboard support is not implemented

The USB keyboard path remains DOS-oriented. Boot-protocol HID translation and software typematic work in DOS, but there is no protected-mode/Win16 Windows keyboard companion equivalent to `CH375MOU.DRV`.

### Some DOS keyboard software may bypass the supported path

Programs that read hardware in unusual ways, assume a physical keyboard controller, or bypass conventional BIOS/DOS keyboard handling may fail to recognize the USB keyboard correctly.

### CH375 absent: resident cleanup is incomplete

If the controller probe fails, normal USB runtime behavior is not installed, but the initialization path is not yet optimized to return a minimal/zero-unit resident image.

## Mouse limitations

### Graphics cursor masks

CH375USB implements the text-mode cursor requested through `INT 33h/01h` and the text masks supplied by `INT 33h/0Ah`. Programmable `INT 33h/09h` 16x16 graphics AND/XOR masks are not rendered yet; hotspot information is retained for API compatibility.

Games that draw their own cursor are unaffected by this limitation.

### Wheel support

The current logical mouse is exposed as a three-button mouse. Wheel reporting is not implemented.

### Sensitivity and double-speed compatibility

The related `INT 33h` settings are stored/reported where implemented, but the current movement path does not attempt exact Microsoft/CuteMouse acceleration/sensitivity parity. Windows is expected to own GUI acceleration.

## Storage limitations

### Windows storage is not native USB Plug and Play

Storage detected in DOS can remain accessible to Windows through the DOS real-mode compatibility path. Windows-side insertion/removal is not a native USB mass-storage PnP implementation.

### FAT32

The documented storage target remains primarily MBR + FAT12/FAT16. FAT32 requires explicit BPB/driver work and testing.

### Multi-LUN

The driver exposes one DOS block-device unit and one active storage target/LUN.

### Non-512-byte sectors

Non-512-byte media is rejected safely. Sector translation is not implemented.

## HID / hub limitations

- HID boot protocol only; arbitrary report descriptors are outside the current scope.
- External hub support remains experimental and requires broader real-hardware validation.
- Low-speed downstream behavior should remain conservative until validated.

## Pocket386 hardware caveats

The Pocket386 does not use a conventional separate coin-cell CMOS battery. A fully discharged main battery can reset BIOS settings. In particular, mouse support may revert to disabled and the floppy/FDC configuration may revert to enabled. Check BIOS settings before treating such a change as a driver regression.

Third-party CMOS save/restore utilities and modified Pocket386 BIOS images are outside CH375USB support.

## Deferred engineering work

- Fix Windows 95 USB mouse detach/replug state reset.
- Broader DOS game/application `INT 33h` compatibility testing.
- Optional complete graphics-cursor-mask renderer.
- Native Windows 95 USB keyboard bridge.
- External-hub completion.
- FAT32 support.
- Multi-LUN exposure.
- Non-512-byte translation.
- Complete DOS removable-media capability advertisement.
- Minimal resident cleanup when no CH375 controller is found.
- Private ISR stack only if real applications demonstrate a need.

## Validated 0.5.1 baseline that should not be casually disturbed

On the tested Pocket386, the following now work together: physical PS/2 in DOS and Windows, USB mouse in DOS, USB first-hotplug in Windows, simultaneous PS/2 + USB mouse input, DOS text cursor in EDIT, and real-game mouse/cursor operation in Monkey Island. Future hotplug fixes should preserve this matrix.
