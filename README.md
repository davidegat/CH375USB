# CH375USB 0.5.1

CH375USB is an open-source USB host driver stack for 386-class DOS / Windows 95 systems using an ISA-connected WCH CH375 controller at the common `0260h` / `0261h` I/O mapping.

**0.5.1 removes CuteMouse/CTMOUSE as a runtime dependency.** `CH375USB.SYS` now provides its own resident DOS `INT 33h` mouse driver and merges input from the physical BIOS PS/2 mouse and CH375 USB HID mouse. `CH375MOU.DRV` bridges that same `INT 33h` event stream into Windows 95.

Author: **Davide "gat"**  
GitHub: **https://github.com/davidegat**  
License: **GNU GPL v3 or later (`GPL-3.0-or-later`)**

CH375USB is an independent, unofficial project. It is not affiliated with or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH). See [`documentation/NOTICE.md`](documentation/NOTICE.md).

## Mouse architecture

```text
Physical PS/2 mouse -> BIOS INT 15h/C207 callback --+
                                                   +-> CH375USB internal INT 33h -> DOS applications
CH375 USB HID mouse -> direct HID report path -----+                         |
                                                                             +-> CH375MOU.DRV -> Windows 95
```

The USB mouse no longer needs to pass through a virtual BIOS PS/2 layer. USB HID reports feed the internal mouse core directly. The physical PS/2 backend uses the real BIOS C207 callback. The two backends keep separate button state and are merged into one logical three-button `INT 33h` device.

No `CTMOUSE.EXE`, `MOUSE.COM`, or other external DOS mouse driver is required or recommended on top of CH375USB 0.5.1.

## Hardware-tested status

Validated on Pocket386 / MS-DOS 7.x / Windows 95:

| Test | Result |
|---|---|
| DOS physical PS/2 movement/buttons through `MTEST` | Working |
| DOS USB mouse hotplug through `MTEST` | Working |
| DOS physical PS/2 + USB simultaneously | Working |
| DOS EDIT with physical PS/2 | Working, including cursor |
| DOS EDIT with USB mouse | Working, including cursor |
| Monkey Island with physical PS/2 | Working, including cursor |
| Windows 95 physical PS/2 | Working |
| Windows 95 USB mouse present at startup | Working |
| Windows 95 first USB hotplug after startup | Working |
| Windows 95 physical PS/2 + USB simultaneously | Working |
| Windows 95 USB unplug then replug in the same session | Known limitation |

### DOS cursor behavior

The driver now implements the **text-mode software cursor requested by DOS programs through `INT 33h/01h`**. It restores the underlying text cell on movement/hide and honors the text-cursor masks configured through `INT 33h/0Ah`.

The driver does **not** display a permanent cursor at the DOS prompt merely because a mouse is connected. A program must request it through the normal mouse API. Programs and games that draw their own graphics cursor continue to do so normally.

Programmable `INT 33h/09h` 16x16 graphics-cursor masks are not rendered by CH375USB yet; the current implementation stores the hotspot for compatibility.

## Other device support

- **USB mass storage:** DOS read/write and hotplug. Windows access remains DOS-backed compatibility, not native Windows USB mass-storage Plug and Play.
- **USB keyboard:** HID boot protocol in DOS with software typematic/repeat. No native Windows 95 keyboard companion yet.
- **USB hub:** experimental external-hub support.
- **Storage scope:** primarily MBR + FAT12/FAT16, one active storage target/LUN, 512-byte sectors.

## Files

- `CH375USB.ASM` — unified resident DOS/Windows-aware driver.
- `internal_mouse.inc` — resident `INT 33h` core, physical PS/2 backend, USB mouse feed and DOS text-cursor support.
- `CH375MOU.ASM` / `CH375MOU.LNK` — Win16 Windows 95 mouse bridge.
- `MTEST.ASM` — small DOS `INT 33h` coordinate/button diagnostic.
- `bios_mouse.inc` — BIOS PS/2 compatibility/legacy virtual-device support.
- `usb_hid.inc` — USB HID keyboard/mouse handling.
- `win_hotplug.inc` — bounded Windows HID-mouse hotplug state machine.
- `usb_core.inc`, `usb_msc.inc`, `usb_hub.inc`, `usb_maint.inc` — USB core, storage, hub and maintenance code.
- `BUILD.BAT` / `build.sh` — build the driver pair and `MTEST.COM`.
- `binary/` — published build outputs.

## Basic DOS installation

Copy `CH375USB.SYS` to the DOS boot drive and add:

```dos
DEVICE=C:\CH375USB.SYS
```

Do **not** load `CH375286.SYS` at the same time; only one driver can own the CH375 controller.

Do **not** load CuteMouse/CTMOUSE or another DOS mouse driver on top of CH375USB 0.5.1.

## DOS / Windows 95 profile

`CH375USB.SYS` must be resident before `WIN`. It can be loaded normally from `CONFIG.SYS`, or a Windows-only profile can load it with FreeDOS `DEVLOAD.COM` immediately before Windows starts.

Example Windows profile fragment:

```dos
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS
WIN
```

For USB storage compatibility, the storage device should be visible to DOS before starting Windows.

## Windows 95 mouse companion

Copy `CH375MOU.DRV` to:

```text
C:\WINDOWS\SYSTEM\CH375MOU.DRV
```

and configure:

```ini
[boot]
mouse.drv=CH375MOU.DRV
```

`CH375MOU.DRV` is a Win16 mouse-driver bridge, not a native Windows USB HID stack. It registers an `INT 33h` callback into the resident CH375USB mouse core. At that point CH375USB re-arms the physical BIOS PS/2 callback so the physical mouse remains active after Windows enters enhanced mode.

## MTEST mouse diagnostic

`MTEST.COM` is built from `MTEST.ASM`. It continuously displays `INT 33h` X/Y coordinates and button state without relying on application cursor drawing.

```dos
MTEST.COM
```

Move either mouse and press buttons; press `Esc` to exit. It is useful for separating transport/input failures from application-specific cursor behavior.

## Building

### Native DOS / Windows 9x DOS mode

Expected tool paths:

- NASM: `C:\NASM\NASM.EXE`
- Open Watcom: `C:\OPENWAT\BINW\WASM.EXE` and `WLINK.EXE`

Run:

```dos
BUILD.BAT
```

Outputs:

- `CH375USB.SYS`
- `CH375MOU.DRV`
- `CH375MOU.MAP`
- `MTEST.COM`

### Linux / Unix

Requirements: NASM, Python 3, unzip, and curl/wget if the local Open Watcom toolchain has not already been bootstrapped.

Run:

```sh
./build.sh
```

The script builds the same outputs and validates the Win16 NE image before reporting success.

## Pocket386 BIOS note

On Pocket386 systems, BIOS settings may be lost if the main battery fully discharges. Mouse support can revert to disabled and the floppy/FDC setting can revert to enabled. If mouse behavior suddenly changes after a flat battery, verify BIOS settings before debugging the driver.

## Known issue retained for later

A USB mouse can be present when Windows starts or can be hotplugged after Windows is already running. However, after an **unplug followed by replug in the same Windows 95 session**, USB mouse input currently does not resume. The physical PS/2 mouse continues to work. This is intentionally retained as a minor follow-up rather than destabilizing the validated 0.5.1 mouse architecture.

See [`documentation/KNOWN_ISSUES.md`](documentation/KNOWN_ISSUES.md), [`documentation/KNOWLEDGE.md`](documentation/KNOWLEDGE.md), and [`documentation/NOTICE.md`](documentation/NOTICE.md).
