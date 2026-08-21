# CH375USB 0.4.13

**CH375USB 0.4.13** is an open-source resident MS-DOS USB host driver for ISA CH375 controllers using the common parallel I/O mapping at `0260h` / `0261h`.

Version 0.4.13 promotes the hardware-tested 0.4.12 experimental line and keeps the datasheet-audit improvements while adding several correctness fixes discovered during a subsequent code review.

The main 0.4.13 fixes are:

- correct DOS Media Check semantics after storage hotplug (`FFh` for changed media, `01h` for unchanged media);
- use the device's real EP0 maximum packet size during manual control-IN transfers instead of assuming 64 bytes;
- isolate the CPU Direction Flag around resident `LODSB`/`STOSB` USB packet operations;
- report the number of sectors actually transferred when a DOS block read/write fails part-way through;
- initialize BOT/SCSI mass storage when the CH375 built-in generic enumeration path recognizes an MSC interface.

USB mass storage, USB keyboard and USB mouse were regression-tested on the target DOS hardware after these changes and all three classes worked correctly with live plug-and-play/hotplug behavior.

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
- Can also remain available in Windows 95 when CH375USB is loaded with `DEVLOAD` from `AUTOEXEC.BAT` and a flash drive is connected BEFORE starting Windows 95.
- Windows hotplug is **not** fully supported: the USB drive must already be inserted and detected by DOS before `WIN` is started, in this case hotplug MAY HAPPEN and you may be able to remove your media and connect another.
- If unable to use your drive plug and play (DOS), just let the system boot with the usb drive connected.


### USB keyboard

- Works in DOS.
- Can be connected and disconnected while DOS is running.
- Does not currently work in the Windows 95 GUI.

### USB HUB (up to four downstream devices)
- Experimental feature, not complete.
- Support present, but not tested.
- Try at own risk.

## Tested setup

- Pocket386 / 386-class PC.
- MS-DOS 7.x / Windows 95 DOS environment.
- CH375 ISA USB controller at I/O base `0260h` (`data=0260h`, `command/status=0261h`).
- USB flash drive: read/write and DOS hotplug working.
- USB HID keyboard: DOS input and hotplug working.
- USB HID mouse: DOS input and hotplug working.
- CH375USB 0.4.13: storage, keyboard and mouse verified working with live attach/detach and plug-and-play under DOS after the 0.4.13 correctness fixes.

The Pocket386 used for testing was configured with non-standard, performance-oriented BIOS settings intended to speed up the 386. These settings are more aggressive than typical defaults and may reduce system stability on some machines:

- AT Bus Clock: PCLK2/8 (quite agggressive!)
- I/O Recovery: Disabled
- I/O Recovery Period: 0.75 µs
- 16Bit ISA Insert Wait: Disabled
- Slow Refresh: 120 µs

They are not required by the driver. Standard or more conservative BIOS settings should also work and may provide better stability. If you experience problems, test the driver with the BIOS defaults before changing anything else.

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
- `KNOWN_ISSUES.md` — confirmed open issues, deferred work and review candidates still requiring targeted validation.

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
- the USB drive must be inserted and detected before `WIN` starts (in this case hotplug MAY work);
- USB storage hotplug generally does not work after Windows has started if the USB drive has not been detected before in DOS environment;
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

## Troubleshooting

### Windows 95 does not see the USB drive

CH375USB does not provide native Windows USB mass-storage hotplug. For the tested Windows 95 compatibility path, connect the USB flash drive **before switching on the PC** and make sure DOS sees it before Windows starts. If the drive was not detected by CH375USB before `WIN`, do not expect Windows 95 to discover it later as a native USB device.

### Windows 95 reports MS-DOS compatibility mode

Do not load CH375USB from the Windows profile in `CONFIG.SYS`. Instead, leave the Windows `CONFIG.SYS` section without CH375USB and load the driver from `AUTOEXEC.BAT` with `DEVLOAD` immediately before starting Windows:

```dos
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS
WIN
```

The USB drive should already be connected and detected before `WIN` is executed. This is the tested workaround for using the real-mode storage driver with Windows 95; it is not a native Windows USB driver.

### Mouse does not work or suddenly stops working

Some old/recreated 386-class systems, including the Pocket386 used during development, have shown BIOS settings occasionally reverting or not being retained reliably. If the USB mouse does not work, or previously worked and then stops, enter the BIOS and check that **mouse support is enabled**. Check it again before debugging CH375USB: on this hardware the BIOS mouse option has been observed to become disabled unexpectedly.

### USB flash drive is not recognized

For maximum DOS/CH375 compatibility, use an **MBR (MS-DOS) partition table** with a **single primary FAT16 partition no larger than 2 GiB**. The USB device may physically be larger; the recommended DOS-compatible partition should be 2 GiB or smaller. CH375USB 0.4.13 currently accepts **512-byte physical sectors** on the native storage path.

On a Windows PC, one example is to use Disk Management: delete the existing partitions on the USB drive, initialize/use it as **MBR**, create a primary partition of **2 GB or less**, and format that partition as **FAT** (FAT16). Be absolutely sure you selected the correct removable drive before deleting partitions.

On Linux, for example, replacing `/dev/sdX` with the **correct USB device**:

```sh
sudo umount /dev/sdX?* 2>/dev/null
sudo wipefs -a /dev/sdX
sudo parted -s /dev/sdX mklabel msdos mkpart primary fat16 1MiB 2048MiB set 1 boot on
sudo partprobe /dev/sdX
sudo mkfs.fat -F 16 -n DOSUSB /dev/sdX1
```

**Warning:** these commands destroy the existing partition table and data on the selected device. Verify `/dev/sdX` before running them.

If one flash drive still fails, try another, preferably an older/simple USB 2.0-era device. Flash-drive controller compatibility can vary on retro USB host hardware.

### USB hub support

Hub support is **experimental and untested**. The developer has started implementing support for one external hub with up to four downstream devices, but no real-hardware hub validation has been completed yet. For now, do not rely on it: connect the keyboard, mouse or storage device directly to the CH375 USB port.

### Windows keyboard and mouse support

CH375USB is primarily a **DOS driver**, not a native Windows USB driver. The Windows 95 exception is the limited USB-storage compatibility path described above, where a DOS-detected drive can remain available after Windows starts.

USB keyboard and USB mouse support currently work in DOS only. Windows 95 does not recognize them through CH375USB in the GUI. Proper Windows input support would require a separate protected-mode Windows companion driver; such a component may be developed in the future.

### Windows 3.1 / Windows for Workgroups 3.11

CH375USB has **not been tested at all** under Windows 3.1 or Windows for Workgroups 3.11. Current Windows-related testing and documentation apply only to Windows 95. No compatibility claim is made for Windows 3.x.
