# CH375USB

**CH375USB 0.4.11** is an open-source resident MS-DOS USB host driver for ISA CH375 controllers using the common parallel I/O mapping at `0260h` / `0261h`.

Author: **Davide "gat"**  
GitHub: **https://github.com/davidegat**  
License: **GNU GPL v3 or later (`GPL-3.0-or-later`)**

CH375USB is an independent, unofficial project. It is not affiliated with or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH). The names **WCH** and **CH375** are used only to identify compatible hardware. See [`NOTICE.md`](NOTICE.md).

For the practical development history, failed approaches, hardware observations and lessons learned while building the driver on a real Pocket386, see [`KNOWLEDGE.md`](KNOWLEDGE.md).

The driver is intended for 386-class DOS machines such as the Pocket386 and currently provides working USB mass-storage, USB keyboard and USB mouse support under DOS.

## Compatibility

| Device | DOS support | DOS hotplug | Windows 95 support | Windows 95 hotplug |
|---|---|---|---|---|
| USB mouse | Yes | Yes | No | No |
| USB flash drive / mass storage | Yes | Yes | Yes, when loaded from `AUTOEXEC.BAT` with `DEVLOAD` before Windows starts | Limited |
| USB keyboard | Yes | Yes | No | No |

### USB mouse

- Works in DOS.
- Can be connected and disconnected while DOS is running.
- Requires a conventional DOS mouse driver such as CuteMouse (`CTMOUSE`) for the DOS mouse interface.
- Does not currently work in the Windows 95 GUI.

### USB flash drive / mass storage

- Read/write support in DOS.
- Hotplug works in DOS.
- One DOS removable drive letter is reserved by CH375USB.
- Can also remain available in Windows 95 when CH375USB is loaded with `DEVLOAD` from `AUTOEXEC.BAT` immediately before Windows starts.
- Windows hotplug is **not** supported: the USB drive must already be inserted and detected by DOS before `WIN` is started.

### USB keyboard

- Works in DOS.
- Can be connected and disconnected while DOS is running.
- Does not currently work in the Windows 95 GUI.

## Tested setup

- Pocket386 / 386-class PC.
- MS-DOS 7.x / Windows 95 DOS environment.
- CH375 ISA USB controller at I/O base `0260h` (`data=0260h`, `command/status=0261h`).
- USB flash drive: read/write and DOS hotplug working.
- USB HID keyboard: DOS input and hotplug working.
- USB HID mouse: DOS input and hotplug working.

The Pocket386 used for testing was configured with non-standard, performance-oriented BIOS settings intended to speed up the 386. These settings are more aggressive than typical defaults and may reduce system stability on some machines:

- AT Bus Clock: PCLK2/8 (quite agggressive!)
- I/O Recovery: Disabled
- I/O Recovery Period: 0.75 µs
- 16Bit ISA Insert Wait: Disabled
- Slow Refresh: 120 µs

They are not required by the driver. Standard or more conservative BIOS settings should also work and may provide better stability. If you experience problems, test the driver with the BIOS defaults before changing anything else.

Experimental support for one external USB hub with up to four downstream devices is also present but not tested yet.

## Files

- `CH375USB.SYS` — prebuilt driver binary. 
- `CH375USB.ASM` — main driver source.
- `ch375_defs.inc` — CH375/USB constants.
- `ch375_hw.inc` — low-level CH375 I/O.
- `ch375_native.inc` — CH375 native mass-storage path.
- `usb_core.inc` — USB enumeration and control transfers.
- `usb_msc.inc` — USB Mass Storage BOT/SCSI support.
- `usb_hid.inc` — USB HID keyboard/mouse support.
- `usb_hub.inc` — experimental external-hub support.
- `usb_maint.inc` — hotplug and re-enumeration maintenance.
- `BUILD.BAT` / `build.sh` — NASM build scripts.
- `LICENSE` — GNU GPL v3 license text.
- `NOTICE.md` — project independence, provenance, sources and compatibility notice.
- `KNOWLEDGE.md` — development knowledge, experiments, pitfalls and lessons learned from real-hardware testing.

The distributed `CH375USB.SYS`, when present, is the binary built from the source code in this project. It is **not** the vendor `CH375286.SYS` driver.

## Basic DOS installation

Copy `CH375USB.SYS` to the DOS boot drive and add it to `CONFIG.SYS`:

```dos
DEVICE=C:\CH375USB.SYS
```

Do **not** load `CH375USB.SYS` and `CH375286.SYS` at the same time. Both need exclusive access to the CH375 controller.

For USB mouse support, load a conventional DOS mouse driver after CH375USB. Example `AUTOEXEC.BAT`:

```dos
@ECHO OFF
C:\CTMOUSE.EXE
```

If the machine is used mainly in DOS, this is the simplest setup and gives storage, keyboard, mouse and DOS hotplug support.

## DOS / Windows 95 dual boot

Loading CH375USB normally from `CONFIG.SYS` on the Windows profile can make Windows 95 slower because the real-mode storage driver remains resident during startup.

A practical workaround is:

1. Load CH375USB normally from `CONFIG.SYS` for the DOS profile.
2. Leave CH375USB out of the Windows profile in `CONFIG.SYS`.
3. On the Windows profile, load CH375USB from `AUTOEXEC.BAT` with `DEVLOAD`, then start Windows with `WIN`.

`DEVLOAD` is a FreeDOS utility that loads DOS device drivers from the command line and emulates a `DEVICE=` entry.

DEVLOAD 3.25a:

- Package page: https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/unstable/html/en/base/devload/20250409.8/index.html
- Direct ZIP: https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/unstable/base/devload.zip
- Source / documentation: https://github.com/FDOS/devload

Only `DEVLOAD.COM` is required at runtime.

### Example `CONFIG.SYS`

```dos
[MENU]
MENUITEM=DOS,DOS with CH375USB
MENUITEM=WINDOWS,Windows 95
MENUDEFAULT=WINDOWS,5

[COMMON]
DOS=HIGH,UMB
FILES=40
BUFFERS=20
LASTDRIVE=Z

[DOS]
DEVICE=C:\CH375USB.SYS

[WINDOWS]
REM CH375USB is intentionally not loaded here.
```

### Example `AUTOEXEC.BAT`

```dos
@ECHO OFF
GOTO %CONFIG%

:DOS
REM CH375USB.SYS is already resident from CONFIG.SYS.
C:\CTMOUSE.EXE
GOTO END

:WINDOWS
REM Load CH375USB only now, immediately before Windows.
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS

REM The USB drive must already be inserted and visible in DOS here.
WIN
GOTO END

:END
```

With this setup:

- the **DOS** menu entry loads CH375USB normally and supports USB storage, keyboard, mouse and hotplug;
- the **Windows 95** menu entry loads CH375USB with `DEVLOAD` immediately before Windows starts;
- USB storage can remain available in Windows 95;
- the USB drive must be inserted and detected before `WIN` starts (in this case hotplug may work);
- USB storage hotplug generally does not work after Windows has started if the USB drive has not been detected before;
- USB keyboard and USB mouse are NOT currently supported in the Windows 95 GUI.


## Building

NASM 2.x is required.

Linux / Unix:

```sh
./build.sh
```

DOS / Windows command prompt:

```dos
BUILD.BAT
```

Equivalent command:

```sh
nasm -f bin CH375USB.ASM -o CH375USB.SYS
```

`CH375USB.ASM` includes the `.inc` modules in the same directory.

## License and source provenance

CH375USB is released under **GPL-3.0-or-later**.

The source code in this project is independently written. No proprietary WCH driver source code or vendor driver binary is included. Hardware interface behavior is implemented for compatibility with CH375 hardware using documented interface information and interoperability testing.

See [`NOTICE.md`](NOTICE.md) for the detailed source/provenance and inspiration record, and [`KNOWLEDGE.md`](KNOWLEDGE.md) for the practical development knowledge gathered during the project.

## Current limitations

- Windows 95 USB keyboard and mouse support would require a separate protected-mode Windows driver; it is not provided by this release.
- USB storage works in Windows 95 only with the pre-boot DOS/`DEVLOAD` method described above; Windows hotplug is not fully supported.
- Hub support is experimental and not tested yet.
