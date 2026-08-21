# CH375USB 0.5.0

**CH375USB 0.5.0** is an open-source USB host driver stack for 386-class DOS/Windows 95 systems using an ISA-connected CH375 controller at the common parallel-I/O mapping `0260h` / `0261h`.

Version 0.5.0 is a source release focused on input compatibility and the DOS-to-Windows transition. It keeps the established DOS mass-storage path, adds a BIOS/PS/2 mouse bridge intended to look more like a conventional pointing device to DOS software, adds keyboard typematic handling, introduces a Windows-aware hotplug state machine, and adds an independently written Win16 mouse-driver companion named `CH375MOU.DRV`.

**No prebuilt 0.5.0 binaries are committed in the repository.** Build the driver from source with the supplied scripts.

Author: **Davide "gat"**  
GitHub: **https://github.com/davidegat**  
License: **GPL-3.0-or-later**

CH375USB is independent and unofficial. It is not affiliated with or endorsed by WCH. See [`NOTICE.md`](NOTICE.md).

## What 0.5.0 contains

- `CH375USB.SYS` source: unified DOS/Windows-aware resident driver.
- DOS USB mass-storage support inherited from the 0.4.13 line.
- USB HID boot keyboard support, including software typematic/repeat.
- USB HID boot mouse support through a virtual BIOS PS/2 interface (`INT 15h`, `AH=C2h`) instead of unsafe AUX/D3 stuffing into the physical 8042.
- Continued interoperability with a conventional DOS `INT 33h` mouse driver such as CuteMouse.
- Windows enhanced-mode detection and bounded Windows-side HID hotplug enumeration.
- `CH375MOU.DRV`: an original Win16 mouse-driver bridge that obtains events through the DOS `INT 33h` path.
- One experimental external USB hub path, still not considered production-ready.

## Support status

| Function | DOS | DOS hotplug | Windows 95 GUI |
|---|---:|---:|---:|
| USB mass storage | Yes | Yes | DOS-backed compatibility path; not native Windows USB |
| USB boot keyboard | Yes | Yes | No native Windows keyboard driver |
| USB boot mouse | Yes, through BIOS/PS/2 + `INT 33h` path | Yes | Experimental through `CH375MOU.DRV`; final broad hardware validation is incomplete |
| External hub | Experimental | Experimental | No |

The 0.5.0 source intentionally distinguishes **implemented** from **fully validated** behavior. In particular, the Win16 mouse companion and Windows hotplug state machine are new work and should not be read as a claim of universal Windows 95 compatibility.

## DOS installation

Build `CH375USB.SYS`, copy it to the DOS boot drive, and load it from `CONFIG.SYS`:

```dos
DEVICE=C:\CH375USB.SYS
```

Do **not** load CH375USB together with `CH375286.SYS` or another driver that owns the same CH375 controller.

For mouse support, load a conventional DOS mouse driver after CH375USB, for example:

```dos
C:\CTMOUSE.EXE
```

The 0.5.0 mouse path presents USB HID mouse activity through the standard BIOS PS/2 callback interface. A BIOS/PS2-aware DOS mouse driver can therefore remain the owner of `INT 33h`, including on systems that also have a physical PS/2 mouse.

## Windows 95 mouse experiment

The source tree contains `CH375MOU.ASM` and `CH375MOU.LNK`. On Linux, `build.sh` builds both the unified DOS driver and the Win16 companion.

Typical setup after building:

1. Load `CH375USB.SYS` before starting Windows and load CuteMouse in DOS.
2. Copy `CH375MOU.DRV` to `C:\WINDOWS\SYSTEM\CH375MOU.DRV`.
3. Edit `C:\WINDOWS\SYSTEM.INI` and set under `[boot]`:

```ini
mouse.drv=CH375MOU.DRV
```

4. Restart Windows.

`CH375MOU.DRV` is a Win16 mouse-driver bridge, not a native USB HID stack. It depends on the real-mode CH375USB/CuteMouse path remaining available while Windows runs. USB keyboard support is still DOS-only.

## Windows 95 storage compatibility

CH375USB does not provide native Windows USB mass-storage Plug and Play. The practical compatibility path remains:

1. make the USB storage device visible to DOS;
2. load CH375USB immediately before Windows if it is not already resident;
3. start `WIN`.

FreeDOS `DEVLOAD.COM` can be used to load `CH375USB.SYS` from `AUTOEXEC.BAT` in a Windows boot profile:

```dos
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS
WIN
```

A DOS-detected drive may remain usable in Windows through the real-mode block-device path. Arbitrary Windows-side storage hotplug is not promised.

## Storage notes

For maximum compatibility with DOS and the tested CH375 path, use:

- MBR partition table;
- one primary FAT16 partition;
- partition size no larger than 2 GiB;
- 512-byte sectors.

Non-512-byte media is rejected safely rather than translated.

## Building

### Linux / Unix

Requirements:

- NASM;
- `unzip`;
- Python 3;
- `curl` or `wget` when the local Open Watcom toolchain has not yet been bootstrapped.

Run:

```sh
./build.sh
```

The script builds:

- `CH375USB.SYS` with NASM;
- `CH375MOU.DRV` with Open Watcom WASM/WLINK;
- `CH375MOU.MAP` as a linker map.

Open Watcom is downloaded into the project-local `.toolchains/` directory when required. The build validates that `CH375MOU.DRV` is actually a Win16 NE DLL/driver before accepting it.

### DOS / Windows command prompt

```dos
BUILD.BAT
```

`BUILD.BAT` builds the unified `CH375USB.SYS` only.

Equivalent SYS command:

```sh
nasm -f bin CH375USB.ASM -o CH375USB.SYS
```

## Source layout

- `CH375USB.ASM` — unified resident driver and DOS block-device layer.
- `CH375MOU.ASM` — Win16 mouse-driver bridge.
- `CH375MOU.LNK` — Open Watcom linker definition.
- `bios_mouse.inc` — virtual BIOS PS/2 mouse interface.
- `win_hotplug.inc` — incremental Windows root HID-mouse hotplug state machine.
- `usb_core.inc` — enumeration and control transfers.
- `usb_hid.inc` — boot keyboard/mouse polling and DOS input bridging.
- `usb_msc.inc` — USB Mass Storage BOT/SCSI path.
- `usb_hub.inc` — experimental hub support.
- `usb_maint.inc` — attach/detach and deferred maintenance.
- `ch375_hw.inc` / `ch375_native.inc` / `ch375_defs.inc` — CH375 hardware/native-storage support and constants.
- `build.sh` / `BUILD.BAT` — build scripts.

## Important limitations

- The keyboard path still targets HID boot keyboards and PC-compatible scan-code consumers; some games/programs that bypass normal BIOS/DOS keyboard paths may remain incompatible.
- Typematic is implemented in software, but timing/behavior may not exactly match every physical AT keyboard.
- The Windows 95 mouse bridge is experimental and requires more real-hardware/application coverage.
- Windows 95 USB keyboard input is not implemented.
- External hub support remains experimental.
- FAT32, multi-LUN exposure and non-512-byte translation are not implemented.

See [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) for the current detailed list.

## Documentation

- [`CHANGELOG.md`](CHANGELOG.md) — release history.
- [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) — known limitations and deferred work.
- [`KNOWLEDGE.md`](KNOWLEDGE.md) — engineering architecture and design rules.
- [`NOTICE.md`](NOTICE.md) — independence, provenance and external references.
- `LICENSE` — GNU GPL v3 license text.

## Provenance

The source in this repository is independently written. No proprietary WCH driver source or vendor binary is included, and the repository does not redistribute FreddyV's `CH375286.SYS`. Public hardware/protocol specifications and historical implementations were used as interoperability references. See [`NOTICE.md`](NOTICE.md) for details.
