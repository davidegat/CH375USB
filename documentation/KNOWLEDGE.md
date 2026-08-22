# CH375USB — Engineering knowledge

**Current reference version:** 0.5.1  
**Primary target:** Pocket386 and similar 386-class DOS PCs with an ISA-connected CH375 USB host controller  
**Tested I/O mapping:** data `0260h`, command/status `0261h`

This document records the current architecture and invariants. Release history belongs in [`../CHANGELOG.md`](../CHANGELOG.md); open work belongs in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md).

## 1. High-level architecture

```text
                         CH375USB.SYS
                              |
             +----------------+----------------+
             |                |                |
        DOS storage      USB keyboard       mouse core
             |                |                |
      native/BOT/SCSI     boot HID       internal INT 33h
                                               ^
                          +--------------------+------------------+
                          |                                       |
                physical BIOS PS/2                        CH375 USB HID
                  INT 15h/C207                              direct feed
                          |
                          +--------------------+------------------+
                                               |
                                          DOS programs
                                               |
                                          CH375MOU.DRV
                                               |
                                           Windows 95
```

The essential 0.5.1 design change is that **CH375USB owns `INT 33h` itself**. CuteMouse/CTMOUSE is no longer a runtime layer.

## 2. Mouse input invariants

### Physical PS/2

The physical mouse uses the machine's real BIOS `INT 15h/C207` callback. The callback itself must remain minimal: capture status/delta/button data and return with `RETF`. Do not perform USB traffic, DOS calls, Windows callbacks or heavy `INT 33h` work inside the BIOS callback.

The stored PS/2 event is drained later from safe resident paths. This rule was established by real-hardware testing: doing too much in the physical callback can hard-lock the Pocket386 on the first movement packet.

### CH375 USB mouse

USB HID mouse reports feed the internal mouse core **directly**. They do not travel through the virtual BIOS PS/2 callback. This keeps USB transport independent from ownership of the physical BIOS PS/2 interface.

### Coexistence

PS/2 and USB retain separate button state. The logical `INT 33h` button bitmap is the merge of both backends, while movement deltas are accumulated normally. One backend must not generate a false release for a button still held by the other backend.

## 3. DOS `INT 33h` core

The resident core implements the conventional services required by tested DOS software and `CH375MOU.DRV`, including reset/status/position, position ranges, press/release counters, motion counters, callbacks, mickey ratios, text-cursor masks, enable/disable and version/capability queries.

The reset function reports a three-button mouse (`AX=FFFFh`, `BX=3`). `INT 33h/0000` is treated as a hardware lifecycle reset as well as a logical-state reset: after clearing the common state, CH375USB re-arms the already-probed BIOS PS/2 C207 callback. `INT 33h/0020` performs the same PS/2 re-arm when the driver is re-enabled. This is required for compatibility with DOS software that assumes reset/enable restores the physical mouse backend.

`INT 33h/0004` (set position) and range changes through `0007h/0008h` clear the fractional mickey-to-pixel accumulators. A cursor warp starts a new positioning epoch; retaining pre-warp fractional movement can otherwise create residual movement in software that repeatedly recenters the pointer.

Unsupported functions must fail safely; do not chain to a null historical vector.

## 4. DOS cursor behavior

CH375USB does not display a mouse pointer merely because a mouse is connected.

When a DOS application calls `INT 33h/01h`, the driver enables its text-mode software cursor. The renderer:

1. restores the previously saved text cell;
2. maps logical mouse coordinates to the current text cell using BIOS Data Area video information;
3. saves the underlying cell;
4. applies the `INT 33h/0Ah` screen/cursor masks;
5. writes the cursor cell directly to `B800h` or `B000h` as appropriate.

`INT 33h/02h` hides the cursor using the normal nested hide count and restores the saved cell.

The renderer is intentionally limited to text modes. `INT 33h/09h` graphics masks are not drawn by CH375USB yet. Games that render their own pointer remain responsible for that graphics cursor.

## 5. Windows 95 bridge

`CH375MOU.DRV` is a Win16 mouse-driver bridge. It consumes the resident `INT 33h` callback stream and forwards relative movement/buttons to the Windows mouse event procedure.

When `CH375MOU.DRV` installs its non-zero `INT 33h/0Ch` callback, CH375USB re-arms the physical BIOS PS/2 callback (`C207` + enable/rate) because Windows initialization can otherwise leave the physical backend inactive. Do not re-run unnecessary packet-size/resolution negotiation during this Windows-time re-arm.

The Win16 bridge uses the resident core's processed `CX/DX` position as a hidden counter and converts successive positions back into relative motion for Windows. After setting the Windows-time `INT 33h` range to `0..32767`, `CH375MOU.DRV` must center that hidden counter at `16384,16384` before seeding `prev_pos_x/prev_pos_y`. Starting near a clamp can otherwise make the Windows pointer stop before reaching one screen edge until movement in the opposite direction creates headroom again.

DOS text-cursor rendering is suspended while Windows is active.

## 6. Windows USB hotplug

Windows mouse enumeration is incremental and bounded. Never replace it with one long synchronous USB enumeration inside a timer callback.

A first USB mouse hotplug after Windows starts is validated. Detach followed by a second replug in the same session currently fails to resume input; this is the principal minor mouse issue left for later.

## 7. Storage path

Storage remains dual-path:

```text
try CH375 native storage
        |
        +-- success -> native DISK_* backend
        |
        +-- failure -> generic USB enumeration -> MSC BOT/SCSI
```

Core invariants retained from the stable storage work:

- serialize every CH375 command sequence;
- root reset follows the documented mode sequence used by this driver;
- generic USB retries are explicit and bounded;
- EP0 decisions use the device's actual maximum packet size;
- preserve/restore FLAGS/DF around resident string operations;
- bound block I/O to reported media capacity;
- propagate partial-transfer and final write/cache-flush errors correctly.

Write builds use `SYNC_AFTER_WRITE=1`; the storage path issues its backend cache/synchronize operation after writes. This only covers data already delivered to CH375USB. DOS or a disk-cache utility may still hold unwritten data above the driver.

### Storage removal policy

- **DOS:** hot-unplug is supported, but only after every file/program using the drive is closed and disk activity has stopped. If SMARTDRV write-behind caching is enabled, run `SMARTDRV /C` before unplugging.
- **Windows 95:** do **not** unplug USB storage while Windows is running. The drive is exposed through the DOS real-mode compatibility path; CH375USB does not provide native Windows mass-storage PnP or a safe-removal handshake. Exit Windows or shut down first, then remove the drive under the DOS rules above.

## 8. Keyboard path

The USB keyboard path targets HID boot keyboards, translates usages to PC/AT Set-1 make/break scan codes, handles mapped extended keys, clears state on detach and implements software typematic/repeat.

Windows 95 native keyboard input is not implemented.

## 9. Build architecture

- `CH375USB.SYS`: flat real-mode binary built by NASM.
- `MTEST.COM`: tiny NASM flat-binary diagnostic.
- `CH375MOU.DRV`: Win16 NE driver assembled/linked with Open Watcom WASM/WLINK.

`build.sh` validates that the resulting `.DRV` is actually a Windows NE DLL/driver image. Linker warnings are treated as build failures.

`BUILD.BAT` builds all three runtime/test binaries under real DOS/Windows 9x DOS mode using fixed NASM/Open Watcom paths.

## 10. Hardware-tested regression order

For mouse changes, preserve this order:

1. DOS boot with physical PS/2 only: no freeze.
2. `MTEST`: PS/2 movement and all buttons.
3. USB first hotplug in DOS: `MTEST` movement/buttons.
4. PS/2 + USB simultaneously.
5. EDIT: cursor plus movement/buttons with PS/2 and USB.
6. DOS Shell (`DOSSHELL`) with PS/2 and USB.
7. Real DOS games with both physical PS/2 and USB mouse input: The Secret of Monkey Island, The Games: Winter Challenge, Wolfenstein 3D, Ken's Labyrinth, Hexxagon, Cannon Fodder, Battle Chess, Tyrian 2000, XQuest 2, and Warcraft: Orcs & Humans.
8. Windows 95 physical PS/2.
9. Windows 95 USB present at startup.
10. Windows 95 first USB hotplug.
11. Windows 95 PS/2 + USB together.
12. Confirm the Windows pointer can reach all four screen edges immediately after startup, without needing a compensating movement to the opposite edge.
13. Separately track unplug/replug as the remaining known mouse issue.

## 11. Rules for future changes

1. Keep physical PS/2 callback work minimal and deferred.
2. Keep USB mouse direct-to-INT33; do not reintroduce BIOS PS/2 emulation as the normal USB path.
3. Preserve separate backend button state.
4. Keep Windows enumeration incremental/nonblocking.
5. Do not add a permanent DOS cursor; render only when requested by `INT 33h`.
6. Preserve the proven storage paths unless a replacement is measurably safer/better.
7. Do not advertise Windows USB-storage safe removal; the current Windows path is DOS-backed compatibility storage.
8. Keep source, published binaries and documentation on the same release version.
9. Distinguish tested behavior from planned behavior.
