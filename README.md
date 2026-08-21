# CH375USB 0.5.0

**CH375USB 0.5.0** is an open-source USB host driver stack for 386-class DOS / Windows 95 systems using an ISA-connected CH375 controller at the common parallel-I/O mapping `0260h` / `0261h`.

Version 0.5.0 keeps the established DOS mass-storage path and extends the input side substantially: it adds the BIOS/PS/2 mouse bridge, software keyboard typematic, a Windows-aware HID mouse hotplug state machine, and the independently written Win16 companion `CH375MOU.DRV`.

Prebuilt 0.5.0 binaries (`CH375USB.SYS` and `CH375MOU.DRV`) are published alongside the source. The supplied build scripts can also rebuild them from source.

Author: **Davide "gat"**  
GitHub: **https://github.com/davidegat**  
License: **GNU GPL v3 or later (`GPL-3.0-or-later`)**

CH375USB is an independent, unofficial project. It is not affiliated with or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH). See [`NOTICE.md`](NOTICE.md).

## Table of contents

- [Compatibility](#compatibility)
- [Runtime dependency for mouse support](#runtime-dependency-for-mouse-support)
- [Tested setup](#tested-setup)
- [Pocket386 BIOS / CMOS warning](#pocket386-bios--cmos-warning)
- [Files](#files)
- [Basic DOS installation](#basic-dos-installation)
- [DOS / Windows 95 dual boot](#dos--windows-95-dual-boot)
- [Windows 95 mouse companion](#windows-95-mouse-companion)
- [Building](#building)
- [Troubleshooting](#troubleshooting)
- [Project documentation](#project-documentation)
- [License and source provenance](#license-and-source-provenance)

## Compatibility

| Device | DOS support | DOS hotplug | Windows 95 support | Windows 95 hotplug |
|---|---|---|---|---|
| USB mouse | Yes | Yes | Experimental through `CH375MOU.DRV` | Experimental |
| USB flash drive / mass storage | Yes | Yes | Yes through the DOS-backed compatibility path when detected before Windows starts | Limited; not native Windows USB PnP |
| USB keyboard | Yes | Yes | No native Windows keyboard driver | No |
| External USB hub | Experimental | Experimental | No | No |

### USB mouse

- Works in DOS through the virtual BIOS PS/2 interface plus a conventional DOS mouse driver.
- Can be connected and disconnected while DOS is running.
- 0.5.0 deliberately avoids stuffing synthetic AUX bytes into the physical 8042 output buffer.
- The tested/reference stack uses **CuteMouse / CTMOUSE** as the DOS mouse driver.
- Windows 95 GUI mouse support is provided experimentally by `CH375MOU.DRV`, which consumes the DOS `INT 33h` event path.

### USB flash drive / mass storage

- Read/write support in DOS.
- Hotplug works in DOS.
- One DOS drive letter is reserved by CH375USB even when no USB storage is attached.
- A storage device already detected by DOS can remain available in Windows 95 through the real-mode compatibility path.
- Windows-side insertion/removal is **not** a native USB mass-storage Plug-and-Play implementation.
- If a particular flash drive does not enumerate reliably after insertion, cold-boot testing with the drive already connected remains a useful compatibility check.

### USB keyboard

- Works in DOS.
- Can be connected and disconnected while DOS is running.
- Uses HID boot-protocol keyboard reports translated to PC-compatible scan codes.
- 0.5.0 adds software typematic/repeat.
- There is currently no Windows 95 protected-mode keyboard companion.

### USB hub

- One external hub with up to four downstream ports is partially implemented.
- Hub support remains **experimental** and has not received the same validation as direct-root devices.
- Low-speed downstream behavior should not be considered generally supported yet.

## Runtime dependency for mouse support

The mouse stack has an important runtime dependency:

**CuteMouse / CTMOUSE is the DOS mouse driver used and tested with CH375USB 0.5.0.**

CH375USB itself supplies the USB HID transport and virtual BIOS PS/2 pointing-device interface. CuteMouse then owns the conventional DOS `INT 33h` mouse API. Under Windows 95, `CH375MOU.DRV` receives events through that same `INT 33h` path.

Reference stack:

```text
USB HID mouse
    -> CH375USB.SYS
    -> virtual BIOS PS/2 interface (INT 15h / AH=C2h)
    -> CuteMouse / CTMOUSE
    -> DOS INT 33h
    -> CH375MOU.DRV (Windows 95 only)
    -> Windows mouse event procedure
```

CuteMouse is **not distributed with CH375USB** and no CuteMouse source is incorporated into this project.

Project/package references:

- FreeDOS CTMouse package: https://gitlab.com/FreeDOS/base/ctmouse
- CuteMouse project files: https://sourceforge.net/projects/cutemouse/files/

Another BIOS/PS2-aware DOS mouse driver may theoretically work, but **0.5.0 has been developed and tested with CuteMouse**, so CuteMouse is the supported reference dependency.

## Tested setup

Primary development target:

- Pocket386 / 386-class PC.
- MS-DOS 7.x / Windows 95 DOS environment.
- CH375 ISA USB controller at I/O base `0260h` (`data=0260h`, `command/status=0261h`).
- USB flash drive: DOS read/write and hotplug.
- USB HID keyboard: DOS input and hotplug.
- USB HID mouse: DOS input/hotplug through the BIOS PS/2 + CuteMouse path.
- Windows 95 mouse bridge: implemented through `CH375MOU.DRV`; still considered experimental outside the tested setup.

The Pocket386 used during development has also been run with these performance-oriented BIOS settings:

- AT Bus Clock: `PCLK2/8`
- I/O Recovery: `Disabled`
- I/O Recovery Period: `0.75 µs`
- 16Bit ISA Insert Wait: `Disabled`
- Slow Refresh: `120 µs`

These values are **not CH375USB requirements**. Standard or conservative BIOS settings may be more stable on another machine. If something behaves unexpectedly, test with BIOS defaults before changing low-level timing.

## Pocket386 BIOS / CMOS warning

The Pocket386 has an unusual and important hardware limitation: **it does not have a separate CMOS backup battery dedicated to retaining BIOS/RTC settings**. BIOS/CMOS retention depends on the same rechargeable battery that powers the computer itself.

As a result, when the main battery is allowed to discharge completely, BIOS settings can be lost or reset to defaults. This is not a CH375USB failure.

Two defaults are especially relevant to this project:

- **mouse support may revert to Disabled**;
- the **floppy/FDC configuration may revert to Enabled**, which can produce the familiar floppy-controller/FDC failure message or boot beeps on a machine without a usable floppy drive/controller configuration.

Therefore, if USB mouse support suddenly appears completely dead after the Pocket386 has been stored with a flat battery, **check the BIOS before debugging the driver**. On commonly reported Pocket386 firmware, hold **Delete while powering on** to enter BIOS setup, then verify mouse support and the floppy/FDC settings.

This also explains apparently spontaneous BIOS resets reported by Pocket386 users: the machine is using its main battery for retention instead of a separate coin-cell-style CMOS battery.

### Saving/restoring BIOS settings with DOS utilities

Generic DOS utilities exist that can save CMOS/BIOS configuration data and restore it later. One possible workaround is to keep a known-good CMOS backup and invoke the corresponding restore utility from `AUTOEXEC.BAT`, before starting Windows with `WIN`.

This can be useful on a Pocket386 whose main battery has discharged and caused the firmware settings to reset. However, **CH375USB does not bundle, recommend, test, document commands for, or provide support for any CMOS backup/restore utility**. Formats and behavior are utility- and BIOS-specific, and writing incorrect CMOS data can create additional boot/configuration problems. Use such tools only if you already understand the specific utility and firmware involved.

In a Windows boot profile, the logical placement would be before `WIN`; the dual-boot example below leaves an explicit comment showing where such an optional external restore step could be placed without prescribing a particular program.

### Community-modified BIOS

A community-modified Pocket386 BIOS has been discussed online with the defaults changed so that:

- mouse support is **Enabled** by default;
- the floppy/FDC setting is **Disabled** by default.

Community thread supplied as a reference:

https://forum.vcfed.org/index.php?threads/pocket-386.1247640/page-6

A closely related Pocket386 discussion on VOGONS describes the same type of modification, made with AMIBCP 6.24, specifically to avoid the default floppy error/beeps and default-disabled mouse setting:

https://www.vogons.org/viewtopic.php?start=100&t=99751

**CH375USB has not tested or validated this modified BIOS.** It is not part of this project, and this project currently has **no verified instructions for flashing the Pocket386 BIOS**. The community report itself notes uncertainty about normal flashing tools and describes programming the firmware chip directly. Firmware flashing can render the machine unbootable; use any third-party BIOS image entirely at your own risk.

## Files

- `CH375USB.SYS` — prebuilt 0.5.0 unified DOS/Windows-aware driver.
- `CH375MOU.DRV` — prebuilt 0.5.0 Win16 mouse-driver bridge.
- `CH375USB.ASM` — unified resident driver source and DOS block-device interface.
- `CH375MOU.ASM` — independently written Win16 mouse-driver bridge source.
- `CH375MOU.LNK` — Open Watcom linker definition for the Win16 driver.
- `bios_mouse.inc` — virtual BIOS PS/2 mouse interface.
- `win_hotplug.inc` — bounded Windows root HID-mouse hotplug state machine.
- `ch375_defs.inc` — CH375/USB constants.
- `ch375_hw.inc` — low-level CH375 I/O and transaction helpers.
- `ch375_native.inc` — CH375 native mass-storage path and storage abstraction.
- `usb_core.inc` — USB enumeration and control transfers.
- `usb_msc.inc` — USB Mass Storage BOT/SCSI support.
- `usb_hid.inc` — USB HID keyboard/mouse support and typematic handling.
- `usb_hub.inc` — experimental external-hub support.
- `usb_maint.inc` — hotplug and deferred maintenance.
- `BUILD.BAT` — builds the unified `CH375USB.SYS` with NASM.
- `build.sh` — builds `CH375USB.SYS` and `CH375MOU.DRV` on Linux/Unix and validates the Win16 NE image.
- `CHANGELOG.md` — release history.
- `KNOWN_ISSUES.md` — current open issues, hardware caveats and deferred work.
- `KNOWLEDGE.md` — engineering notes and architecture.
- `NOTICE.md` — project independence, provenance and external references.
- `LICENSE` — GNU GPL v3 license text.

The published `CH375USB.SYS` and `CH375MOU.DRV` are project build outputs from this source tree. The repository does not contain the vendor `CH375DOS.SYS` or FreddyV's `CH375286.SYS` binary.

## Basic DOS installation

Copy `CH375USB.SYS` to the DOS boot drive and add it to `CONFIG.SYS`:

```dos
DEVICE=C:\CH375USB.SYS
```

Do **not** load `CH375USB.SYS` and `CH375286.SYS` at the same time. Both need exclusive access to the CH375 controller.

For mouse support, load CuteMouse after CH375USB. Example `AUTOEXEC.BAT`:

```dos
@ECHO OFF
C:\CTMOUSE.EXE
```

For a machine used mainly in DOS, this is the simplest setup and provides storage, keyboard, mouse and DOS hotplug support.

## DOS / Windows 95 dual boot

Loading CH375USB normally from the Windows profile in `CONFIG.SYS` can make Windows 95 startup slower because the real-mode storage driver remains resident throughout the boot path.

A practical workaround tested during development is to use separate DOS and Windows profiles:

1. Load CH375USB normally from `CONFIG.SYS` for the DOS profile.
2. Leave CH375USB out of the Windows profile in `CONFIG.SYS`.
3. In the Windows profile, load CH375USB from `AUTOEXEC.BAT` with FreeDOS `DEVLOAD.COM` immediately before Windows starts.
4. Load CuteMouse before `WIN` if the `CH375MOU.DRV` Windows mouse bridge is being used.
5. For storage compatibility, insert the USB drive and make sure DOS sees it **before** starting Windows.
6. Optionally, users who already maintain a CMOS backup with a third-party DOS utility may restore it in this pre-Windows part of `AUTOEXEC.BAT`; this is outside CH375USB support.

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
REM OPTIONAL and unsupported by CH375USB:
REM If you use your own third-party CMOS restore utility, it may be called here.

REM Load CH375USB only now, immediately before Windows.
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS

REM CuteMouse is the tested INT 33h dependency for CH375MOU.DRV.
C:\CTMOUSE.EXE

REM For USB storage, the drive must already be inserted and visible in DOS here.
WIN
GOTO END

:END
```

With this setup:

- the **DOS** profile provides USB storage, keyboard, mouse and DOS hotplug;
- the **Windows 95** profile loads CH375USB only immediately before Windows starts;
- a USB storage device already detected by DOS can remain available in Windows 95;
- `CH375MOU.DRV` can use the CH375USB -> BIOS PS/2 -> CuteMouse -> `INT 33h` mouse path;
- USB keyboard input remains DOS-only;
- Windows storage remains a DOS compatibility path, **not** a native Windows USB stack.

## Windows 95 mouse companion

On Linux/Unix, `build.sh` also builds `CH375MOU.DRV`.

Typical setup:

1. Make sure `CH375USB.SYS` and CuteMouse are loaded before `WIN`.
2. Copy `CH375MOU.DRV` to:

```text
C:\WINDOWS\SYSTEM\CH375MOU.DRV
```

3. In `C:\WINDOWS\SYSTEM.INI`, set under `[boot]`:

```ini
mouse.drv=CH375MOU.DRV
```

4. Restart Windows.

`CH375MOU.DRV` is a Win16 mouse-driver bridge, not a native USB HID stack. It depends on the real-mode CH375USB/CuteMouse event path remaining available while Windows runs.

The 0.5.0 Windows-side root mouse enumerator performs bounded work over multiple timer ticks rather than performing a long blocking enumeration inside one timer callback. Windows mouse hotplug is nevertheless still considered experimental.

## Building

### Linux / Unix

Requirements:

- NASM;
- `unzip`;
- Python 3;
- `curl` or `wget` if Open Watcom has not already been bootstrapped locally.

Run:

```sh
./build.sh
```

The script builds:

- `CH375USB.SYS` with NASM;
- `CH375MOU.DRV` with Open Watcom WASM/WLINK;
- `CH375MOU.MAP` as a linker map.

Open Watcom is downloaded into the project-local `.toolchains/` directory when required. The build checks that `CH375MOU.DRV` is actually a Win16 NE DLL/driver before reporting success.

### DOS / Windows command prompt

```dos
BUILD.BAT
```

`BUILD.BAT` builds only the unified `CH375USB.SYS`.

Equivalent SYS command:

```sh
nasm -f bin CH375USB.ASM -o CH375USB.SYS
```

`CH375USB.ASM` includes the `.inc` modules in the same directory.

## Troubleshooting

### Mouse worked before, but is now completely dead

**Check the Pocket386 BIOS first.** Because there is no separate CMOS backup battery, a fully discharged main battery can reset BIOS settings. Mouse support may revert to **Disabled**.

Re-enter BIOS and enable mouse support before assuming that CH375USB has regressed.

### FDC failure / floppy error suddenly appears at boot

This can have the same cause: a BIOS reset can restore the default floppy/FDC setting to **Enabled**. On Pocket386 configurations without a usable floppy setup, this can produce an FDC failure message and/or boot beeps.

Check the BIOS and disable the unused floppy/FDC option again.

### Windows 95 does not see the USB drive

CH375USB does not provide native Windows USB mass-storage hotplug. Connect the USB flash drive before starting Windows and make sure DOS sees it before `WIN`.

If the drive was not detected by CH375USB first, do not expect Windows 95 to discover it later as a native USB device.

### Windows 95 reports MS-DOS compatibility mode or starts very slowly

Do not load CH375USB from the Windows profile in `CONFIG.SYS`. Instead, use the dual-boot arrangement above and load it from `AUTOEXEC.BAT` with `DEVLOAD` immediately before starting Windows:

```dos
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS
C:\CTMOUSE.EXE
WIN
```

The USB storage device should already be connected and visible in DOS. This is a real-mode compatibility workaround, not native Windows USB support.

### Windows 95 mouse does not work

Check all of the following:

1. Pocket386 BIOS mouse support is enabled.
2. `CH375USB.SYS` is resident before Windows starts.
3. CuteMouse/`CTMOUSE.EXE` loads successfully and the mouse works in DOS before `WIN`.
4. `CH375MOU.DRV` is present in `C:\WINDOWS\SYSTEM`.
5. `[boot]` in `SYSTEM.INI` contains `mouse.drv=CH375MOU.DRV`.

If DOS mouse support does not work before Windows starts, fix that first; `CH375MOU.DRV` depends on the DOS `INT 33h` path.

### USB flash drive is not recognized

For maximum DOS/CH375 compatibility, use an **MBR (MS-DOS) partition table** with a **single primary FAT16 partition no larger than 2 GiB**. The physical USB device can be larger; the DOS-compatible partition should be 2 GiB or smaller.

CH375USB currently supports **512-byte storage sectors** and safely rejects unsupported sector sizes.

On Windows, use Disk Management to select the correct removable drive, remove the existing layout if necessary, use **MBR**, create a primary partition of **2 GB or less**, and format it as **FAT/FAT16**.

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

Hub support remains experimental and has not yet received systematic real-hardware validation. For reliable use, connect storage, keyboard or mouse directly to the CH375 USB port.

### Some DOS games do not see the USB keyboard

CH375USB translates boot-HID reports to PC-compatible scan codes, but some programs bypass conventional BIOS/DOS behavior or make assumptions about the physical keyboard controller. Such programs may still be incompatible.

### Windows keyboard support

USB keyboard input remains DOS-only in 0.5.0. There is no Windows keyboard companion equivalent to `CH375MOU.DRV`.

### Windows 3.1 / Windows for Workgroups 3.11

CH375USB 0.5.0 has not been validated as a Windows 3.x input solution. No Windows 3.1/WfW 3.11 compatibility claim is made.

## Project documentation

- [`CHANGELOG.md`](CHANGELOG.md) — complete release history and fixes.
- [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) — current open issues, hardware caveats and intentionally deferred work.
- [`KNOWLEDGE.md`](KNOWLEDGE.md) — architecture, debugging rules and engineering notes.
- [`NOTICE.md`](NOTICE.md) — project independence and source/reference provenance.

## License and source provenance

CH375USB is released under **GPL-3.0-or-later**.

The source code in this project is independently written. No proprietary WCH driver source code or vendor driver binary is included. No CuteMouse, Bret Johnson USBMOUSE, FreddyV CH375286 or other third-party source file is incorporated or redistributed as part of CH375USB.

Public hardware/protocol specifications, documented DOS/BIOS/Windows interfaces, historical implementations and community reports were used as interoperability and engineering references.

See [`NOTICE.md`](NOTICE.md) for the detailed provenance/reference record.
