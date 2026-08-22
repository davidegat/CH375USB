# CH375USB 0.5.1

**CH375USB 0.5.1** is an open-source USB host driver stack for 386-class DOS / Windows 95 systems using an ISA-connected CH375 controller at the common parallel-I/O mapping `0260h` / `0261h`.

Version 0.5.1 keeps the established DOS mass-storage, HID keyboard, hotplug and Windows-aware paths, and makes the mouse stack self-contained: `CH375USB.SYS` now owns the DOS `INT 33h` mouse API directly, merges the physical BIOS PS/2 mouse with CH375 USB HID mouse input, provides the text-mode cursor requested by DOS applications, and feeds the independently written Win16 companion `CH375MOU.DRV` under Windows 95.

**CuteMouse / CTMOUSE is no longer a runtime dependency.** Do not load another DOS mouse driver on top of CH375USB 0.5.1.

Prebuilt 0.5.1 binaries are published in [`binary/`](binary/): `CH375USB.SYS`, `CH375MOU.DRV` and the `MTEST.COM` mouse diagnostic. The supplied build scripts can also rebuild them from source.

Author: **Davide "gat"**  
GitHub: **https://github.com/davidegat**  
License: **GNU GPL v3 or later (`GPL-3.0-or-later`)**

CH375USB is an independent, unofficial project. It is not affiliated with or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH). See [`documentation/NOTICE.md`](documentation/NOTICE.md).

## Table of contents

- [Compatibility](#compatibility)
- [Mouse architecture and runtime dependencies](#mouse-architecture-and-runtime-dependencies)
- [Hardware-tested status](#hardware-tested-status)
- [Tested setup](#tested-setup)
- [USB storage formatting](#usb-storage-formatting)
- [Pocket386 BIOS / CMOS warning](#pocket386-bios--cmos-warning)
- [Files](#files)
- [Basic DOS installation](#basic-dos-installation)
- [DOS / Windows 95 dual boot](#dos--windows-95-dual-boot)
- [Windows 95 mouse companion](#windows-95-mouse-companion)
- [MTEST mouse diagnostic](#mtest-mouse-diagnostic)
- [Building](#building)
- [Troubleshooting](#troubleshooting)
- [Project documentation](#project-documentation)
- [License and source provenance](#license-and-source-provenance)

## Compatibility

| Device | DOS support | DOS hotplug | Windows 95 support | Windows 95 hotplug |
|---|---|---|---|---|
| Physical PS/2 mouse | Yes, through CH375USB internal `INT 33h` | N/A | Yes through `CH375MOU.DRV` | N/A |
| USB mouse | Yes | Yes | Yes through `CH375MOU.DRV` on the tested Pocket386 setup | First hotplug works; unplug then replug in the same Windows session is a known limitation |
| USB flash drive / mass storage | Yes | Yes, when idle/closed | Yes through the DOS-backed compatibility path when detected before Windows starts | No native safe removal; do not unplug while Windows is running |
| USB keyboard | Yes | Yes | No native Windows keyboard driver | No |
| External USB hub | Experimental | Experimental | No | No |

### USB and PS/2 mouse

- `CH375USB.SYS` now provides its own resident DOS `INT 33h` mouse driver.
- The physical PS/2 mouse is received through the real BIOS `INT 15h/C207` callback.
- CH375 USB HID mouse reports feed the internal mouse core directly; they no longer need to pass through a synthetic BIOS PS/2 layer.
- Physical PS/2 and USB mouse input can coexist and control the same logical three-button DOS mouse.
- The USB and PS/2 backends keep separate button state before being merged into the common `INT 33h` state.
- The driver deliberately avoids stuffing synthetic AUX bytes into the physical 8042 output buffer.
- DOS programs receive the normal `INT 33h` API without requiring `CTMOUSE.EXE` or `MOUSE.COM`.
- Windows 95 GUI mouse support is provided by `CH375MOU.DRV`, which consumes the same resident `INT 33h` callback stream.

### DOS cursor behavior

CH375USB 0.5.1 implements the **text-mode software cursor requested by DOS programs through `INT 33h/01h`**. The underlying text cell is restored when the cursor moves or is hidden, and the text-cursor masks configured through `INT 33h/0Ah` are honored.

The driver does **not** draw a permanent cursor at the DOS prompt merely because a mouse is connected. A DOS application must request the cursor through the normal mouse API. Programs and games that render their own graphical pointer continue to do so normally.

Programmable `INT 33h/09h` 16x16 graphics-cursor masks are not currently rendered by CH375USB; the hotspot/mask metadata is retained for API compatibility. This does not prevent software that draws its own graphical cursor from working.

### USB flash drive / mass storage

- Read/write support in DOS.
- Hotplug works in DOS.
- One DOS drive letter is reserved by CH375USB even when no USB storage is attached.
- A storage device already detected by DOS can remain available in Windows 95 through the real-mode compatibility path.
- Windows-side insertion/removal is **not** a native USB mass-storage Plug-and-Play implementation.
- For reliable cross-platform use, use the **validated 1 GiB FAT16 layout** documented in [USB storage formatting](#usb-storage-formatting), rather than a near-2 GiB FAT16 volume with automatically selected geometry.
- **DOS removal:** close files/programs using the drive and wait for disk activity to stop before unplugging. If SMARTDRV write-behind caching is enabled, run `SMARTDRV /C` first.
- **Windows 95 removal:** do **not** unplug USB storage while Windows is running. Exit Windows or shut down first; the current Windows storage path has no native safe-removal handshake.
- CH375USB synchronizes writes that have already reached the driver, but it cannot flush data still held by DOS, Windows, or a higher-level disk cache.
- If a particular flash drive does not enumerate reliably after insertion, cold-boot testing with the drive already connected remains a useful compatibility check.

### USB keyboard

- Works in DOS.
- Can be connected and disconnected while DOS is running.
- Uses HID boot-protocol keyboard reports translated to PC-compatible scan codes.
- Software typematic/repeat is implemented.
- There is currently no Windows 95 protected-mode keyboard companion.

### USB hub

- One external hub with up to four downstream ports is partially implemented.
- Hub support remains **experimental** and has not received the same validation as direct-root devices.
- Low-speed downstream behavior should not be considered generally supported yet.

## Mouse architecture and runtime dependencies

The 0.5.1 mouse stack is self-contained:

```text
Physical PS/2 mouse -> BIOS INT 15h/C207 callback --+
                                                   +-> CH375USB internal INT 33h -> DOS applications
CH375 USB HID mouse -> direct HID report path -----+                         |
                                                                             +-> CH375MOU.DRV -> Windows 95
```

No external DOS mouse driver is needed. In particular, **CuteMouse / CTMOUSE is no longer a runtime dependency and should not be loaded on top of CH375USB 0.5.1**.

CuteMouse remains relevant only as historical/open-source reference material documented in [`documentation/NOTICE.md`](documentation/NOTICE.md); its source or binary is not incorporated or redistributed by CH375USB.

The physical PS/2 backend uses the real BIOS callback rather than CH375USB proxying all BIOS mouse services. This avoids the earlier hard-lock behavior seen when taking over the Pocket386 PS/2 path through an unnecessary `INT 15h` proxy. The USB backend bypasses BIOS C207 entirely and feeds the common mouse core directly.

## Hardware-tested status

The current 0.5.1 mouse architecture has been exercised on real Pocket386 hardware with diagnostic tests and with a range of real DOS programs and games.

### DOS

| Test | Result |
|---|---|
| Physical PS/2 movement/buttons through `MTEST` | Working |
| USB mouse movement/buttons through `MTEST` | Working |
| USB mouse hotplug through `MTEST` | Working |
| Physical PS/2 + USB simultaneously | Working |

### Windows 95

| Test | Result |
|---|---|
| Physical PS/2 mouse | Working |
| USB mouse present at startup | Working |
| First USB mouse hotplug after startup | Working |
| Physical PS/2 + USB simultaneously | Working |
| USB unplug then replug in the same session | Known limitation |

### DOS programs and games

The following DOS software has been tested on real Pocket386 hardware with **both the physical PS/2 mouse and a CH375 USB mouse**.

| Program / game | Type | Physical PS/2 | USB mouse | Notes |
|---|---|---|---|---|
| MS-DOS Editor (`EDIT`) | Program | Working | Working | Text cursor/input working |
| DOS Shell (`DOSSHELL`) | Program | Working | Working | Input working |
| QBasic 4.50 | Program | Working | Working | Mouse input working |
| Norton 4.55 | Program | Working | Working | Mouse input working |
| FastTracker II | Program | Working | Working | Mouse input working |
| The Secret of Monkey Island | Game | Working | Working | Cursor/input working |
| Monkey Island 2 | Game | Working | Working | Cursor/input working |
| The Games: Winter Challenge | Game | Working | Working | Accolade, 1991 |
| Wolfenstein 3D | Game | Working | Working | Mouse input working |
| Ken's Labyrinth | Game | Working | Working | Mouse input working |
| Hexxagon | Game | Working | Working | Mouse input working |
| Cannon Fodder | Game | Working | Working | Mouse input working |
| Battle Chess | Game | Working | Working | Mouse input working |
| Tyrian 2000 | Game | Working | Working | Mouse input working |
| XQuest 2 | Game | Working | Working | Mouse input working |
| Warcraft: Orcs & Humans | Game | Working | Working | Mouse input working |
| Lemmings | Game | Working | Working | Mouse input working |
| SimEarth: The Living Planet | Game | Working | Working | Mouse input working |
| Sid Meier's Civilization 474.03 | Game | Working | Working | Mouse input working |
| Arkanoid | Game | Working | Working | Mouse input working |
| Strip Poker Professional | Game | Working | Working | Mouse input working |

These tests establish the current Pocket386 baseline; they are not a blanket guarantee for every BIOS, DOS program, game or Windows 95 machine.

## Tested setup

- Pocket386 / 386-class PC.
- MS-DOS 7.x / Windows 95 DOS environment.
- CH375 ISA USB controller at I/O base `0260h` (`data=0260h`, `command/status=0261h`).
- USB storage using the validated 1 GiB FAT16 layout documented below.
- Physical PS/2 mouse: DOS input through the internal `INT 33h` driver and Windows 95 input through `CH375MOU.DRV`.
- USB HID mouse: DOS input/hotplug through the direct HID -> internal `INT 33h` path and Windows 95 input through `CH375MOU.DRV`.

The Pocket386 used during development has also been run with these performance-oriented BIOS settings:

- AT Bus Clock: `PCLK2/8`
- I/O Recovery: `Disabled`
- I/O Recovery Period: `0.75 µs`
- 16Bit ISA Insert Wait: `Disabled`
- Slow Refresh: `120 µs`

These values are **not CH375USB requirements**. Standard or conservative BIOS settings may be more stable on another machine. If something behaves unexpectedly, test with BIOS defaults before changing low-level timing.

## USB storage formatting

For cross-platform use between Linux, DOS and Windows 95, the currently **validated conservative layout** is:

- MBR / `msdos` partition table;
- one primary FAT16 partition;
- partition size: **1 GiB** (from 1 MiB to 1025 MiB in the Linux example below);
- 512-byte logical sectors;
- 64 sectors per cluster = **32 KiB clusters**;
- two FAT copies;
- 512 root-directory entries;
- boot flag enabled for broad DOS-era compatibility.

This replaces the earlier recommendation to use a near-2 GiB FAT16 partition with `mkfs.fat` choosing the geometry automatically. On the tested Pocket386 setup, that near-limit layout became unreliable once the filesystem contained many files and directories: the volume could be reported as corrupted when moved between Linux, DOS and Windows. The 1 GiB layout above was subsequently exercised with roughly 300 files and multiple DOS software trees without reproducing that corruption.

This is a **real-hardware compatibility result**, not a claim that every theoretically valid FAT16 geometry above 1 GiB is inherently broken.

### Linux: create the validated layout

Replace `/dev/sdX` with the **whole USB device**, for example `/dev/sdb`. These commands erase the selected device completely.

```sh
sudo umount /dev/sdX?* 2>/dev/null
sudo wipefs -a /dev/sdX
sudo parted -s /dev/sdX mklabel msdos mkpart primary fat16 1MiB 1025MiB set 1 boot on
sudo partprobe /dev/sdX
sleep 1
sudo mkfs.fat -F 16 -S 512 -s 64 -f 2 -r 512 -n DOSUSB /dev/sdX1
```

After copying files from Linux, unmount the filesystem before removing the drive:

```sh
sudo umount /dev/sdX1
```

A normal successful unmount flushes Linux filesystem buffers. If desired, `udisksctl power-off -b /dev/sdX` may be used after unmounting before physically removing the device.

### DOS: format an already prepared 1 GiB partition

CH375USB exposes storage as a **DOS block-device drive letter**, not as a BIOS `INT 13h` hard disk. For that reason, the recommended way to create the MBR and 1 GiB partition itself is the Linux procedure above (or another partitioning environment that can access the physical USB disk directly). Do not rely on DOS `FDISK` to partition the CH375USB drive through this driver.

Once the 1 GiB partition already exists and CH375USB has mounted it as a DOS drive letter, it can be reformatted from MS-DOS using the normal `FORMAT` command. Replace `E:` with the drive letter printed by CH375USB at boot:

```dos
FORMAT E: /U /V:DOSUSB
```

This reformats the existing logical partition; it does **not** recreate the MBR or change the partition size. Do not use `/S` unless you deliberately want DOS system files copied to that volume.

If a particular DOS `FORMAT` build refuses to format the installable block-device volume, prepare both the partition and FAT16 filesystem with the validated Linux command instead. The important compatibility target is the resulting 1 GiB FAT16 geometry above.

## Pocket386 BIOS / CMOS warning

The Pocket386 has an unusual and important hardware limitation: **it does not have a separate CMOS backup battery dedicated to retaining BIOS/RTC settings**. BIOS/CMOS retention depends on the same rechargeable battery that powers the computer itself.

As a result, when the main battery is allowed to discharge completely, BIOS settings can be lost or reset to defaults. This is not a CH375USB failure.

Two defaults are especially relevant to this project:

- **mouse support may revert to Disabled**, which prevents the physical BIOS PS/2 mouse path from operating normally;
- the **floppy/FDC configuration may revert to Enabled**, which can produce the familiar floppy-controller/FDC failure message or boot beeps on a machine without a usable floppy drive/controller configuration.

Therefore, if the physical PS/2 mouse suddenly appears completely dead after the Pocket386 has been stored with a flat battery, **check the BIOS before debugging the driver**. On commonly reported Pocket386 firmware, hold **Delete while powering on** to enter BIOS setup, then verify mouse support and the floppy/FDC settings.

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

### Prebuilt binaries

- [`binary/CH375USB.SYS`](binary/CH375USB.SYS) — prebuilt 0.5.1 unified DOS/Windows-aware driver with internal `INT 33h` mouse support.
- [`binary/CH375MOU.DRV`](binary/CH375MOU.DRV) — prebuilt 0.5.1 Win16 mouse-driver bridge.
- [`binary/MTEST.COM`](binary/MTEST.COM) — prebuilt 0.5.1 DOS `INT 33h` diagnostic.

### Source and build files

- `CH375USB.ASM` — unified resident driver source and DOS block-device interface.
- `internal_mouse.inc` — resident `INT 33h` core, physical PS/2 backend, direct USB mouse feed and DOS text-cursor support.
- `CH375MOU.ASM` — independently written Win16 mouse-driver bridge source.
- `CH375MOU.LNK` — shared Open Watcom linker definition for the Win16 driver; used by both build scripts.
- `MTEST.ASM` — tiny DOS `INT 33h` coordinate/button diagnostic source.
- `bios_mouse.inc` — BIOS PS/2 compatibility / legacy virtual-device support.
- `win_hotplug.inc` — bounded Windows root HID-mouse hotplug state machine.
- `ch375_defs.inc` — CH375/USB constants.
- `ch375_hw.inc` — low-level CH375 I/O and transaction helpers.
- `ch375_native.inc` — CH375 native mass-storage path and storage abstraction.
- `usb_core.inc` — USB enumeration and control transfers.
- `usb_msc.inc` — USB Mass Storage BOT/SCSI support.
- `usb_hid.inc` — USB HID keyboard/mouse support and typematic handling.
- `usb_hub.inc` — experimental external-hub support.
- `usb_maint.inc` — hotplug and deferred maintenance.
- `BUILD.BAT` — native-DOS build of `CH375USB.SYS`, `CH375MOU.DRV` and `MTEST.COM` using NASM and Open Watcom from fixed DOS paths.
- `build.sh` — Linux/Unix build of the same outputs; bootstraps Open Watcom and validates the Win16 NE image.
- `CHANGELOG.md` — release history.
- [`documentation/KNOWN_ISSUES.md`](documentation/KNOWN_ISSUES.md) — current open issues, hardware caveats and deferred work.
- [`documentation/KNOWLEDGE.md`](documentation/KNOWLEDGE.md) — engineering notes and architecture.
- [`documentation/NOTICE.md`](documentation/NOTICE.md) — project independence, provenance and external references.
- `LICENSE` — GNU GPL v3 license text.

The published binaries are project build outputs from this source tree. The repository does not contain the vendor `CH375DOS.SYS`, FreddyV's `CH375286.SYS` binary, or CuteMouse binaries/source.

## Basic DOS installation

Copy `binary/CH375USB.SYS` from the repository to the DOS boot drive as `CH375USB.SYS` and add it to `CONFIG.SYS`:

```dos
DEVICE=C:\CH375USB.SYS
```

Do **not** load `CH375USB.SYS` and `CH375286.SYS` at the same time. Both need exclusive access to the CH375 controller.

Do **not** load CuteMouse/CTMOUSE, `MOUSE.COM`, or another DOS mouse driver on top of CH375USB 0.5.1. CH375USB itself owns `INT 33h`.

For a machine used mainly in DOS, this single driver provides storage, keyboard, mouse and DOS hotplug support.

## DOS / Windows 95 dual boot

Loading CH375USB normally from the Windows profile in `CONFIG.SYS` can make Windows 95 startup slower because the real-mode storage driver remains resident throughout the boot path.

A practical workaround tested during development is to use separate DOS and Windows profiles:

1. Load CH375USB normally from `CONFIG.SYS` for the DOS profile.
2. Leave CH375USB out of the Windows profile in `CONFIG.SYS`.
3. In the Windows profile, load CH375USB from `AUTOEXEC.BAT` with FreeDOS `DEVLOAD.COM` immediately before Windows starts.
4. **Do not load a separate DOS mouse driver**; CH375USB 0.5.1 provides `INT 33h` itself.
5. For storage compatibility, insert the USB drive and make sure DOS sees it **before** starting Windows.
6. Do **not** unplug that USB storage device while Windows is running; exit Windows or shut down first.
7. Optionally, users who already maintain a CMOS backup with a third-party DOS utility may restore it in this pre-Windows part of `AUTOEXEC.BAT`; this is outside CH375USB support.

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
REM No CTMOUSE/MOUSE.COM is needed.
GOTO END

:WINDOWS
REM OPTIONAL and unsupported by CH375USB:
REM If you use your own third-party CMOS restore utility, it may be called here.

REM Load CH375USB only now, immediately before Windows.
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS

REM CH375USB itself supplies the DOS INT 33h mouse driver.
REM For USB storage, the drive must already be inserted and visible in DOS here.
WIN
GOTO END

:END
```

With this setup:

- the **DOS** profile provides USB storage, keyboard, mouse and DOS hotplug;
- the **Windows 95** profile loads CH375USB only immediately before Windows starts;
- a USB storage device already detected by DOS can remain available in Windows 95;
- `CH375MOU.DRV` consumes CH375USB's own `INT 33h` callback stream;
- the physical PS/2 and CH375 USB mouse can coexist on the same Windows pointer;
- USB keyboard input remains DOS-only;
- Windows storage remains a DOS compatibility path, **not** a native Windows USB stack or safe-removal implementation.

## Windows 95 mouse companion

Use the prebuilt [`binary/CH375MOU.DRV`](binary/CH375MOU.DRV), or build `CH375MOU.DRV` locally with `build.sh` or `BUILD.BAT`.

Typical setup:

1. Make sure `CH375USB.SYS` is resident before `WIN`.
2. Do not load CuteMouse/CTMOUSE or another DOS mouse driver.
3. Copy `CH375MOU.DRV` to:

```text
C:\WINDOWS\SYSTEM\CH375MOU.DRV
```

4. In `C:\WINDOWS\SYSTEM.INI`, set under `[boot]`:

```ini
mouse.drv=CH375MOU.DRV
```

5. Restart Windows.

`CH375MOU.DRV` is a Win16 mouse-driver bridge, not a native USB HID stack. It registers a normal `INT 33h` callback with the resident CH375USB mouse core. When Windows enables the bridge, CH375USB re-arms the physical BIOS PS/2 callback so the physical mouse remains active after Windows enters enhanced mode.

The bridge sets a wide hidden `INT 33h` coordinate range and centers that internal counter before seeding its relative-motion baseline. This prevents the first Windows session from exhausting coordinate headroom in one direction before the visible pointer reaches the screen edge.

The Windows-side root USB mouse enumerator performs bounded work over multiple timer ticks rather than performing a long blocking enumeration inside one timer callback.

On the tested Pocket386 baseline:

- a USB mouse already connected before Windows starts works;
- a USB mouse can be hotplugged for the first time after Windows has started;
- physical PS/2 and USB mice can work simultaneously;
- unplugging the USB mouse and then plugging it back in again during the **same** Windows 95 session does not currently restore USB input. The physical PS/2 mouse continues to work. This is a known minor issue retained for later work.

## MTEST mouse diagnostic

`MTEST.COM` is built from `MTEST.ASM` and continuously displays the current DOS `INT 33h` X/Y coordinates and button state without relying on application cursor drawing.

Run:

```dos
MTEST.COM
```

Move either the physical PS/2 or CH375 USB mouse and press the buttons. Press `Esc` to exit.

`MTEST` is useful for separating transport/input failures from application-specific cursor behavior: if coordinates/buttons change in `MTEST` but an application does not show a pointer, the mouse transport and `INT 33h` core are already functioning and the remaining problem is likely application/cursor-API compatibility.

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

The script builds into the working-tree root:

- `CH375USB.SYS` with NASM;
- `CH375MOU.DRV` with Open Watcom WASM/WLINK;
- `CH375MOU.MAP` as a linker map;
- `MTEST.COM` with NASM.

The repository's published prebuilt copies live in `binary/`; local builds are intentionally produced in the project root by the current build script.

Open Watcom is downloaded into the project-local `.toolchains/` directory when required. The build checks that `CH375MOU.DRV` is actually a Win16 NE DLL/driver before reporting success.

### Native DOS / Windows 9x DOS mode

`BUILD.BAT` builds **all 0.5.1 release outputs**, using the same essential assembler/linker options as `build.sh`:

```text
CH375USB.ASM -- NASM -f bin --------------------------> CH375USB.SYS
CH375MOU.ASM -- WASM -q -2 -fo=CH375MOU.OBJ --------> CH375MOU.OBJ
CH375MOU.OBJ -- WLINK @CH375MOU.LNK -----------------> CH375MOU.DRV
                                                       CH375MOU.MAP
MTEST.ASM ---- NASM -f bin --------------------------> MTEST.COM
```

The DOS build deliberately uses fixed, simple 8.3-safe tool locations. Before running `BUILD.BAT`, install/extract the tools exactly as follows:

```text
C:\NASM\NASM.EXE
C:\OPENWAT\BINW\WASM.EXE
C:\OPENWAT\BINW\WLINK.EXE
```

#### Open Watcom V2 for DOS / Win16

Use the **Open Watcom V2 C/C++ installer for DOS/16-bit Windows** and install it with the root directory set to `C:\OPENWAT`.

- Open Watcom V2 releases/download page: https://github.com/open-watcom/open-watcom-v2/releases
- Direct current DOS/Win16 C/C++ installer: https://github.com/open-watcom/open-watcom-v2/releases/download/Current-build/open-watcom-2_0-c-dos.exe

For the native DOS host, Open Watcom's tools are under `BINW`; `BUILD.BAT` expects `WASM.EXE`, `WLINK.EXE` and the linker system-definition files there. It sets:

```dos
SET WATCOM=C:\OPENWAT
SET EDPATH=C:\OPENWAT\EDDAT
SET INCLUDE=C:\OPENWAT\H
SET PATH=C:\NASM;C:\OPENWAT\BINW;%PATH%
```

It also points `WLINK_LNK` at `C:\OPENWAT\BINW\WLINK.LNK` when that file is present. This is important because `CH375MOU.DRV` must be linked as a **16-bit Windows NE DLL/driver**, not as an ordinary Windows executable.

#### NASM for DOS

Use a **native DOS NASM executable**, not the Win32/Win64 package. The documented DOS package for this build is NASM **3.01** from the FreeDOS repository:

- FreeDOS NASM 3.01 package page (platform: DOS): https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/latest/html/en/devel/nasm/20251011.1/index.html
- NASM project/download information: https://www.nasm.us/

The FreeDOS package page provides the DOS `nasm.zip`. Extract/copy `NASM.EXE` so that the final path is:

```text
C:\NASM\NASM.EXE
```

NASM's flat `bin` output is used for the DOS `.SYS` and `MTEST.COM`; Open Watcom WASM emits the OMF object used by WLINK for the Win16 `.DRV`.

#### Run the DOS build

From the CH375USB source directory:

```dos
BUILD.BAT
```

A successful build produces in the source directory:

```text
CH375USB.SYS
CH375MOU.DRV
CH375MOU.MAP
MTEST.COM
```

`CH375MOU.OBJ` is temporary and is removed after a successful link.

The batch file also refuses a `CH375MOU.DRV` build when WLINK reports `Warning!`, because a misconfigured Open Watcom linker can otherwise emit an apparently usable file with the wrong Windows image configuration.

The Linux build additionally performs a Python-level inspection of the final NE header. The native DOS batch does not require Python; it relies on the explicit `CH375MOU.LNK` Win16 DLL/driver definition, WLINK's exit status, warning check and final-file existence check.

## Troubleshooting

### DOS BUILD.BAT says NASM is missing

The batch file does not search arbitrary locations. It requires:

```text
C:\NASM\NASM.EXE
```

Install/extract the native DOS NASM package there, then run `BUILD.BAT` again.

### DOS BUILD.BAT says Open Watcom is missing

It requires the DOS-host Open Watcom tools at:

```text
C:\OPENWAT\BINW\WASM.EXE
C:\OPENWAT\BINW\WLINK.EXE
```

If Open Watcom was installed somewhere else, reinstall/move it to `C:\OPENWAT` or edit the two root variables at the top of `BUILD.BAT` deliberately.

### WLINK warns or CH375MOU.DRV is not produced

Do not ignore the warning. Make sure the **DOS/Win16 Open Watcom distribution** is installed, `C:\OPENWAT\BINW` is intact, and `WLINK.LNK` is present there. The named Windows linker definitions must be available when `CH375MOU.LNK` requests a Windows DLL/driver image.

### Physical PS/2 mouse worked before, but is now completely dead

**Check the Pocket386 BIOS first.** Because there is no separate CMOS backup battery, a fully discharged main battery can reset BIOS settings. Mouse support may revert to **Disabled**.

Re-enter BIOS and enable mouse support before assuming that CH375USB has regressed.

### FDC failure / floppy error suddenly appears at boot

This can have the same cause: a BIOS reset can restore the default floppy/FDC setting to **Enabled**. On Pocket386 configurations without a usable floppy setup, this can produce an FDC failure message and/or boot beeps.

Check the BIOS and disable the unused floppy/FDC option again.

### Mouse input works in MTEST but no cursor is visible in a DOS program

CH375USB draws a text-mode cursor only after an application requests it through `INT 33h/01h`. The driver does not display a permanent DOS-prompt cursor.

If `MTEST` shows movement/buttons but a specific program still has no pointer, verify whether that program expects a text cursor, supplies its own graphics cursor, or depends on an unimplemented graphics-cursor rendering detail. The software matrix above is the current real-hardware validation baseline.

### Windows 95 does not see the USB drive

CH375USB does not provide native Windows USB mass-storage hotplug. Connect the USB flash drive before starting Windows and make sure DOS sees it before `WIN`.

If the drive was not detected by CH375USB first, do not expect Windows 95 to discover it later as a native USB device.

### Can I unplug the USB drive?

- **DOS:** yes, but only after closing files/programs using the drive and waiting for disk activity to stop. If SMARTDRV write-behind caching is enabled, run `SMARTDRV /C` first.
- **Windows 95:** no. Do not unplug USB storage while Windows is running. Exit Windows or shut down first, then remove the drive.

The reason is that Windows sees the storage through CH375USB's DOS real-mode compatibility path, not through a native Windows USB mass-storage stack with a safe-removal protocol.

### Windows 95 reports MS-DOS compatibility mode or starts very slowly

Do not load CH375USB from the Windows profile in `CONFIG.SYS`. Instead, use the dual-boot arrangement above and load it from `AUTOEXEC.BAT` with `DEVLOAD` immediately before starting Windows:

```dos
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS
WIN
```

The USB storage device should already be connected and visible in DOS. This is a real-mode compatibility workaround, not native Windows USB support.

### Windows 95 mouse does not work

Check all of the following:

1. For the physical PS/2 mouse, Pocket386 BIOS mouse support is enabled.
2. `CH375USB.SYS` is resident before Windows starts.
3. The mouse works through CH375USB's `INT 33h` path in DOS before `WIN` (`MTEST.COM` is useful here).
4. `CH375MOU.DRV` is present in `C:\WINDOWS\SYSTEM`.
5. `[boot]` in `SYSTEM.INI` contains `mouse.drv=CH375MOU.DRV`.
6. No competing DOS mouse driver such as CTMouse is loaded on top of CH375USB.

If DOS mouse support does not work before Windows starts, fix that first; `CH375MOU.DRV` depends on the resident CH375USB `INT 33h` path.

### USB mouse works in Windows until it is unplugged and replugged

This is a known 0.5.1 limitation. A USB mouse can be present when Windows starts or can be hotplugged for the first time after Windows is running, but after an unplug followed by a replug in the same Windows 95 session the USB mouse currently does not resume input. The physical PS/2 mouse continues to work.

This is intentionally retained as a minor follow-up issue rather than changing the validated 0.5.1 architecture late in the release.

### USB flash drive is not recognized or appears corrupted after being heavily populated

Use the **validated 1 GiB FAT16 layout** from [USB storage formatting](#usb-storage-formatting). Do not use the older near-2 GiB Linux recipe with geometry left entirely to `mkfs.fat`; that layout was observed to become unreliable after adding many files and directories on the tested Pocket386 workflow.

CH375USB currently supports **512-byte storage sectors** and safely rejects unsupported sector sizes.

If one flash drive still fails with the validated geometry, try another, preferably an older/simple USB 2.0-era device. Flash-controller compatibility varies on retro USB host hardware.

### USB hub support

Hub support remains experimental and has not yet received systematic real-hardware validation. For reliable use, connect storage, keyboard or mouse directly to the CH375 USB port.

### Some DOS games do not see the USB keyboard

CH375USB translates boot-HID reports to PC-compatible scan codes, but some programs bypass conventional BIOS/DOS behavior or make assumptions about the physical keyboard controller. Such programs may still be incompatible.

### Windows keyboard support

USB keyboard input remains DOS-only in 0.5.1. There is no Windows keyboard companion equivalent to `CH375MOU.DRV`.

### Windows 3.1 / Windows for Workgroups 3.11

CH375USB 0.5.1 has not been validated as a Windows 3.x input solution. No Windows 3.1/WfW 3.11 compatibility claim is made.

## Project documentation

- [`CHANGELOG.md`](CHANGELOG.md) — complete release history and fixes.
- [`documentation/KNOWN_ISSUES.md`](documentation/KNOWN_ISSUES.md) — current open issues, hardware caveats and intentionally deferred work.
- [`documentation/KNOWLEDGE.md`](documentation/KNOWLEDGE.md) — architecture, debugging rules and engineering notes.
- [`documentation/NOTICE.md`](documentation/NOTICE.md) — project independence and source/reference provenance.

## License and source provenance

CH375USB is released under **GPL-3.0-or-later**.

The source code in this project is independently written. No proprietary WCH driver source code or vendor driver binary is included. No CuteMouse, Bret Johnson USBMOUSE, FreddyV CH375286 or other third-party source file is incorporated or redistributed as part of CH375USB.

Public hardware/protocol specifications, documented DOS/BIOS/Windows interfaces, historical implementations and community reports were used as interoperability and engineering references.

See [`documentation/NOTICE.md`](documentation/NOTICE.md) for the detailed provenance/reference record.
