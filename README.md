# CH375USB 0.4.13

**CH375USB 0.4.13** is an open-source resident MS-DOS USB host driver for ISA CH375 controllers using the common parallel I/O mapping at `0260h` / `0261h`.

It provides working USB mass-storage read/write access, boot-protocol USB keyboard input, USB mouse support through the DOS mouse interface, and live DOS hotplug. Version 0.4.13 is the current hardware-tested release and incorporates the CH375 datasheet audit plus the correctness fixes validated on the Pocket386.

The three main 0.4.13 improvement areas are:

- **Safer DOS storage semantics:** correct media-change reporting, correct partial-transfer counts, LBA range validation, and propagation of BOT cache-flush failures.
- **More robust USB transfers and enumeration:** real EP0 packet sizing, Direction Flag isolation for resident string operations, and proper BOT/SCSI initialization from the generic CH375 enumeration path.
- **Serialized controller access:** CH375 ownership now uses an atomic `usb_busy` lock so timer, idle, DOS block-I/O and mouse-polling paths cannot start overlapping controller transactions.

See [`CHANGELOG.md`](CHANGELOG.md) for the complete 0.4.13 change list.

## Table of contents

- [Compatibility](#compatibility)
- [Tested setup](#tested-setup)
- [Files](#files)
- [Basic DOS installation](#basic-dos-installation)
- [DOS / Windows 95 dual boot](#dos--windows-95-dual-boot)
- [Building](#building)
- [Troubleshooting](#troubleshooting)
- [Project documentation](#project-documentation)
- [License and source provenance](#license-and-source-provenance)

Author: **Davide "gat"**  
GitHub: **https://github.com/davidegat**  
License: **GNU GPL v3 or later (`GPL-3.0-or-later`)**

CH375USB is an independent, unofficial project. It is not affiliated with or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH). The names **WCH** and **CH375** are used only to identify compatible hardware. See [`NOTICE.md`](NOTICE.md).

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
- One DOS drive letter is reserved by CH375USB even when no USB storage is attached.
- Can remain available in Windows 95 when CH375USB is loaded with `DEVLOAD` from `AUTOEXEC.BAT` and the flash drive is detected by DOS before `WIN` starts.
- Windows hotplug is not a supported native feature. A drive already known to DOS may remain usable after Windows starts, but arbitrary Windows-side plug-and-play is not guaranteed.
- If a particular flash drive does not enumerate reliably after insertion, cold-boot testing with the drive already connected is still a useful compatibility check.

### USB keyboard

- Works in DOS.
- Can be connected and disconnected while DOS is running.
- Uses HID boot-protocol keyboard reports translated to the conventional PC keyboard path.
- Does not currently work in the Windows 95 GUI.

### USB hub

- One external hub with up to four downstream ports is partially implemented.
- Hub support remains **experimental and not yet validated on real hardware**.
- Low-speed devices behind a hub are intentionally not advertised as supported yet.

## Tested setup

- Pocket386 / 386-class PC.
- MS-DOS 7.x / Windows 95 DOS environment.
- CH375 ISA USB controller at I/O base `0260h` (`data=0260h`, `command/status=0261h`).
- USB flash drive: read/write and DOS hotplug working.
- USB HID keyboard: DOS input and hotplug working.
- USB HID mouse: DOS input and hotplug working.
- The completed 0.4.13 source was regression-tested with all three supported device classes and live attach/detach; storage, keyboard and mouse all worked correctly plug-and-play under DOS.

The Pocket386 used for testing was configured with performance-oriented BIOS settings:

- AT Bus Clock: PCLK2/8
- I/O Recovery: Disabled
- I/O Recovery Period: 0.75 µs
- 16Bit ISA Insert Wait: Disabled
- Slow Refresh: 120 µs

These settings are **not driver requirements**. Standard or conservative BIOS settings may be more stable on another machine. If something behaves unexpectedly, test with BIOS defaults before changing low-level driver timing.

## Files

- `CH375USB.SYS` — prebuilt driver binary, when published.
- `CH375USB.ASM` — main resident DOS driver and block-device interface.
- `ch375_defs.inc` — CH375/USB constants.
- `ch375_hw.inc` — low-level CH375 I/O and transaction helpers.
- `ch375_native.inc` — CH375 native mass-storage path and storage abstraction.
- `usb_core.inc` — USB enumeration and control transfers.
- `usb_msc.inc` — USB Mass Storage BOT/SCSI support.
- `usb_hid.inc` — USB HID keyboard/mouse support.
- `usb_hub.inc` — experimental external-hub support.
- `usb_maint.inc` — hotplug and deferred re-enumeration maintenance.
- `BUILD.BAT` / `build.sh` — NASM build scripts.
- `CHANGELOG.md` — release history and complete change list.
- `KNOWN_ISSUES.md` — confirmed open issues, deferred features and deliberate limitations.
- `KNOWLEDGE.md` — concise engineering notes, architecture and lessons from real-hardware development.
- `NOTICE.md` — project independence, provenance and external references.
- `LICENSE` — GNU GPL v3 license text.

The distributed `CH375USB.SYS`, when present, is built from this project source. It is **not** the vendor `CH375286.SYS` driver.

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

For a machine used mainly in DOS, this is the simplest setup and provides storage, keyboard, mouse and DOS hotplug support.

## DOS / Windows 95 dual boot

Loading CH375USB normally from the Windows profile in `CONFIG.SYS` can make Windows 95 slower because the real-mode storage driver remains resident during startup.

A practical tested workaround is:

1. Load CH375USB normally from `CONFIG.SYS` for the DOS profile.
2. Leave CH375USB out of the Windows profile in `CONFIG.SYS`.
3. On the Windows profile, load CH375USB from `AUTOEXEC.BAT` with `DEVLOAD`, then start Windows with `WIN`.

`DEVLOAD` is a FreeDOS utility that loads DOS device drivers from the command line and emulates a `DEVICE=` entry. Only `DEVLOAD.COM` is required at runtime.

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

- the **DOS** profile provides USB storage, keyboard, mouse and hotplug;
- the **Windows 95** profile loads CH375USB only immediately before Windows starts;
- a USB storage device already detected by DOS can remain available in Windows 95;
- USB keyboard and mouse are **not** supported in the Windows 95 GUI;
- this is a DOS compatibility path, not a native Windows USB stack.

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

## Troubleshooting

### Windows 95 does not see the USB drive

CH375USB does not provide native Windows USB mass-storage hotplug. Connect the USB flash drive before starting Windows and make sure DOS sees it before `WIN`. If the drive was not detected by CH375USB first, do not expect Windows 95 to discover it later as a native USB device.

### Windows 95 reports MS-DOS compatibility mode

Do not load CH375USB from the Windows profile in `CONFIG.SYS`. Instead load it from `AUTOEXEC.BAT` with `DEVLOAD` immediately before starting Windows:

```dos
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS
WIN
```

The USB drive should already be connected and visible in DOS. This is a real-mode compatibility workaround, not native Windows USB support.

### Mouse does not work or suddenly stops working

Some old/recreated 386-class systems, including the Pocket386 used during development, have shown BIOS settings occasionally reverting. If the USB mouse previously worked and then appears completely dead, enter the BIOS and verify that **mouse support is enabled** before debugging CH375USB.

### USB flash drive is not recognized

For maximum DOS/CH375 compatibility, use an **MBR (MS-DOS) partition table** with a **single primary FAT16 partition no larger than 2 GiB**. The physical USB device can be larger; the DOS-compatible partition should be 2 GiB or smaller. CH375USB 0.4.13 currently supports **512-byte storage sectors** and safely rejects unsupported sector sizes.

On Windows, use Disk Management to select the correct removable drive, remove the existing layout if necessary, use **MBR**, create a primary partition of **2 GB or less**, and format it as **FAT** (FAT16).

On Linux, replacing `/dev/sdX` with the **correct USB device**:

```sh
sudo umount /dev/sdX?* 2>/dev/null
sudo wipefs -a /dev/sdX
sudo parted -s /dev/sdX mklabel msdos mkpart primary fat16 1MiB 2048MiB set 1 boot on
sudo partprobe /dev/sdX
sudo mkfs.fat -F 16 -n DOSUSB /dev/sdX1
```

**Warning:** these commands destroy the existing partition table and data on the selected device. Verify `/dev/sdX` before running them.

If one flash drive still fails, try another, preferably an older/simple USB 2.0-era device. Flash-controller compatibility varies on retro USB host hardware.

### USB hub support

Hub support remains experimental and has not yet received real-hardware validation. For reliable use, connect storage, keyboard or mouse directly to the CH375 USB port.

### Windows keyboard and mouse support

CH375USB is primarily a **DOS driver**. USB keyboard and mouse support work in DOS only. Proper Windows 95 GUI input would require a separate protected-mode companion driver.

### Windows 3.1 / Windows for Workgroups 3.11

CH375USB has **not been tested** under Windows 3.1 or Windows for Workgroups 3.11. No Windows 3.x compatibility claim is made.

## Project documentation

- [`CHANGELOG.md`](CHANGELOG.md) — complete release history and fixes.
- [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) — current open issues and intentionally deferred work.
- [`KNOWLEDGE.md`](KNOWLEDGE.md) — architecture, debugging rules and lessons learned on real hardware.
- [`DATASHEET-AUDIT.md`](DATASHEET-AUDIT.md) — focused notes from the CH375 Datasheet I/II audit that led to the 0.4.12 low-level changes retained by 0.4.13.
- [`NOTICE.md`](NOTICE.md) — project independence and source/reference provenance.

## License and source provenance

CH375USB is released under **GPL-3.0-or-later**.

The source code in this project is independently written. No proprietary WCH driver source code or vendor driver binary is included. Hardware interface behavior is implemented for compatibility with CH375 hardware using documented interface information and interoperability testing.

See [`NOTICE.md`](NOTICE.md) for the detailed provenance/reference record.
