# CH375USB — Engineering knowledge

**Current reference version:** 0.5.0  
**Target:** Pocket386 and similar 386-class DOS PCs with an ISA-connected CH375 USB host controller  
**I/O mapping used by the tested hardware:** data `0260h`, command/status `0261h`

This document is the project's engineering reference. Release history belongs in [`CHANGELOG.md`](../CHANGELOG.md); open work belongs in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md).

---

## 1. Architecture in 0.5.0

CH375USB 0.5.0 keeps the established DOS storage/HID core but changes how mouse integration is exposed to software and adds a Windows-aware companion path.

```text
                      +--------------------------+
                      |      CH375USB.SYS        |
                      | unified resident driver  |
                      +------------+-------------+
                                   |
               +-------------------+-------------------+
               |                   |                   |
        DOS block device      HID keyboard       HID mouse
               |                   |                   |
     native DISK_* / BOT       Set-1 scan          BIOS PS/2
               |                 injection         INT 15h/C2h
               |                                       |
               |                                  DOS mouse driver
               |                                     INT 33h
               |                                       |
               |                          +------------+------------+
               |                          |                         |
               |                       DOS apps             CH375MOU.DRV
               |                                                   |
               +---------------------------------------------- Windows 95
```

The Windows mouse companion is a bridge, not a native USB stack.

---

## 2. Hardware ownership

The tested machine exposes CH375 at:

```text
0260h  data
0261h  command/status
```

Only one driver may own that controller. Do not load CH375USB together with `CH375286.SYS` or another CH375 host driver.

All controller transaction sequences share the same resident lock. The atomic `XCHG`-based ownership primitive introduced in 0.4.13 remains a core correctness rule.

---

## 3. DOS storage path

The storage architecture remains deliberately dual-path:

```text
try CH375 native storage
        |
        +-- success -> native DISK_* backend
        |
        +-- failure -> generic USB enumeration -> MSC BOT/SCSI
```

The driver exposes one DOS block-device unit. Hotplug works by retaining the logical DOS unit and changing media-ready/media-changed state rather than dynamically creating and deleting DOS device headers.

Current storage assumptions:

- 512-byte physical sectors;
- FAT12/FAT16-oriented DOS use;
- one active storage target/LUN;
- capacity checked before I/O;
- write completion includes final BOT cache-flush status.

---

## 4. USB core rules retained from 0.4.13

Important invariants remain unchanged:

1. Root generic reset follows mode `5 -> 7 -> 6`.
2. Low-speed handling uses documented speed helpers where available, with compatibility fallback for older CH375 behavior.
3. Retry policy is explicit and bounded; idle HID NAKs must not block the machine indefinitely.
4. EP0 short-packet decisions use the device's actual maximum packet size.
5. CH375 command/status/data lifecycles must finish before another command stream starts.
6. Resident string operations must not assume `DF=0`; save FLAGS, use `CLD`, restore FLAGS.
7. DOS block requests must return coherent media-change, partial-transfer and LBA-bound results.

---

## 5. Keyboard path

CH375USB targets USB HID boot keyboards.

The driver:

- selects HID boot protocol;
- tracks modifier and six-key rollover boot reports;
- translates usages to PC/AT Set-1 make/break scan codes;
- handles `E0h` extended keys where mapped;
- injects keyboard bytes through the conventional 8042-compatible keyboard input path;
- clears remembered state on detach/re-enumeration;
- implements software typematic/repeat in 0.5.0.

Typematic state is kept per USB device slot. A new key starts an initial delay, then repeats at a shorter interval until release.

This improves normal DOS behavior but does not guarantee compatibility with software that bypasses ordinary keyboard interfaces or assumes direct physical-controller behavior.

---

## 6. Mouse path: why 0.5.0 changed it

Earlier mouse work relied mainly on the DOS `INT 33h` integration problem. 0.5.0 adds a lower-level compatibility layer: a virtual BIOS PS/2 pointing-device interface.

The important design choice is what the driver **does not** do:

- it does not stuff USB mouse bytes into the physical 8042 AUX output queue;
- it does not use controller command `D3h` as a fake hardware-mouse injection mechanism.

Instead:

```text
USB boot-mouse report
        -> virtual INT 15h AH=C2h BIOS PS/2 callback
        -> CuteMouse / other BIOS-PS2-aware DOS mouse driver
        -> normal INT 33h API
```

This makes CH375USB look more like a conventional BIOS-provided pointing device to the DOS mouse driver.

---

## 7. Physical PS/2 + USB mouse coexistence

`bios_mouse.inc` is designed as a proxy/fallback rather than as an unconditional replacement for the real BIOS PS/2 service.

For supported `INT 15h/C2xx` requests:

1. configuration changes are mirrored into the USB mouse state;
2. the physical BIOS handler is called first;
3. if the BIOS succeeds, its result remains authoritative;
4. if the BIOS fails and an enumerated USB mouse exists, the virtual USB backend can service the request.

This allows one DOS mouse driver to remain the owner of `INT 33h` while potentially consuming both the real PS/2 mouse and CH375 USB mouse path.

Do not weaken this ordering casually: it is what avoids CH375USB stealing the physical PS/2 interface.

---

## 8. Mouse polling

Mouse reports are polled in short bursts. Movement/button changes are forwarded immediately into the BIOS PS/2 callback path.

The resident state still tracks buttons and event edges because short clicks cannot be reconstructed reliably from an occasional sampled button bitmap.

Where DOS applications drive `INT 33h` frequently, on-demand polling remains useful for responsiveness; the base 18.2 Hz timer alone is visibly coarse for pointer movement.

---

## 9. Windows enhanced mode

Windows 95 enhanced mode changes the rules because DOS real-mode hooks no longer map directly to native GUI input.

0.5.0 has two cooperating pieces:

- `CH375USB.SYS` remains resident and is Windows-aware;
- `CH375MOU.DRV` is an independently written Win16 mouse-driver bridge.

With `WIN_MOUSE_INT33_BRIDGE=1` (the default unified build), the virtual BIOS/INT33 mouse route is deliberately kept alive for the Windows companion.

This is different from pretending Windows can consume a DOS USB HID driver natively.

---

## 10. CH375MOU.DRV

`CH375MOU.DRV` implements the documented Win16 mouse-driver ABI and registers for conventional DOS `INT 33h` callback events.

Its intended flow is:

```text
CH375 USB mouse
 -> CH375USB BIOS PS/2 layer
 -> DOS mouse driver (e.g. CuteMouse)
 -> INT 33h callback
 -> CH375MOU.DRV
 -> Windows event procedure
```

The driver advertises a standard relative three-button mouse.

Windows should own pointer acceleration. The bridge therefore normalizes the DOS mouse layer rather than applying an additional acceleration curve before the event reaches USER.DLL.

The current Windows support is experimental and must be described that way until broader real-hardware/application validation exists.

---

## 11. Windows hotplug state machine

Full USB enumeration cannot safely run as one long operation in a Windows timer callback.

`win_hotplug.inc` therefore advances enumeration in bounded stages, one small action per timer tick. Examples include:

- settle delay;
- host mode changes;
- reset hold/release;
- connect wait;
- device descriptor start/wait;
- set-address start/wait;
- configuration descriptor start/wait;
- HID protocol/idle setup;
- ready/retry states.

Long waits are represented as state and counters, not blocking loops.

This rule is fundamental: while Windows is active, do not reintroduce `generic_root_enumerate()`-style long synchronous work into `INT 1Ch`.

---

## 12. Disconnect/replug behavior under Windows

The Windows state machine has explicit fast-disconnect cleanup and delayed retry/watchdog behavior.

On detach it:

- disables the optional Windows mouse sampler;
- aborts active enumeration;
- clears device topology;
- restores a neutral CH375 host mode/address/retry policy;
- arms a conservative replug watchdog.

Some CH375 revisions may not generate every connect event reliably after detach. The watchdog exists to recover without turning a permanently unplugged port into an endless enumeration storm.

---

## 13. Timer versus DOS-idle work

Outside the Windows-specific incremental HID enumerator, the original split remains useful:

### `INT 1Ch`

Keep work bounded:

- detect connection changes;
- poll HID;
- run keyboard typematic;
- update retry/backoff state;
- schedule maintenance.

### `INT 28h`

Use DOS idle for heavier DOS-side work:

- native-storage probing;
- generic enumeration;
- hub scanning;
- storage-ready validation.

The Windows hotplug enumerator is the explicit exception: it performs only one bounded enumeration state transition per timer tick because DOS idle is not an appropriate Windows worker.

---

## 14. Build architecture

`CH375USB.SYS` remains a flat real-mode binary built with NASM.

`CH375MOU.DRV` is built with Open Watcom:

- WASM produces OMF;
- WLINK produces a 16-bit Windows NE DLL/driver image;
- the linker definition supplies the Windows DLL/driver format and initializer;
- writable driver state lives in a real writable data segment, not in code.

`build.sh` bootstraps Open Watcom into `.toolchains/openwatcom/` when necessary and validates the final NE header before reporting success.

A zero linker exit status is not considered sufficient: warnings cause the build to fail because an earlier toolchain-path problem could produce a superficially valid but incorrectly linked image.

---

## 15. Distribution policy for 0.5.0

Prebuilt `CH375USB.SYS` and `CH375MOU.DRV` binaries are published alongside the 0.5.0 source.

Published binaries must correspond to the current source. In particular, do not leave an older `CH375USB.SYS` next to newer source and let users mistake it for the current release.

Build artifacts are derived outputs; the source remains the authoritative implementation.

---

## 16. Testing discipline

For DOS changes, useful regression order remains:

1. CH375 probe and I/O mapping;
2. root attach/detach;
3. storage read/write/remount;
4. keyboard make/break/modifiers/repeat;
5. mouse movement/buttons/short clicks;
6. cross-class hotplug;
7. DOS-to-Windows transition.

For 0.5.0 mouse work add:

8. USB-only mouse through BIOS PS/2 + CuteMouse;
9. physical PS/2 mouse alone;
10. physical PS/2 + USB mouse coexistence;
11. Windows 95 with `CH375MOU.DRV`;
12. Windows mouse Control Panel speed/acceleration behavior;
13. detach/replug while Windows is active;
14. several DOS games/apps, because some input code bypasses normal abstractions.

Do not convert one successful program into a blanket compatibility claim.

---

## 17. Current support boundaries

### Supported DOS baseline

- one CH375 controller at the configured I/O mapping;
- one DOS block-device unit;
- 512-byte storage sectors;
- FAT12/FAT16-oriented storage use;
- root mass storage;
- HID boot keyboard;
- HID boot mouse;
- DOS hotplug;
- software keyboard typematic.

### New / experimental in 0.5.0

- BIOS PS/2 virtual USB mouse path;
- physical PS/2 + USB coexistence behavior;
- Win16 `CH375MOU.DRV` bridge;
- Windows HID-mouse hotplug state machine.

### Not implemented / deferred

- native Windows 95 USB keyboard support;
- native Windows USB mass-storage PnP;
- arbitrary HID report descriptors;
- FAT32;
- multi-LUN exposure;
- non-512-byte translation;
- complete validated external-hub support.

---

## 18. Rules for future changes

1. Preserve the proven storage paths unless a replacement is demonstrably better.
2. Serialize every CH375 command sequence.
3. Keep timer work bounded.
4. Never perform unsafe AUX/D3 mouse stuffing merely to imitate a PS/2 device.
5. Keep the physical BIOS authoritative when it can service PS/2 requests.
6. Treat Windows GUI input as a separate integration layer from DOS HID.
7. Keep Windows enumeration incremental and nonblocking.
8. Preserve FLAGS/DF discipline in resident code.
9. Distinguish implemented, tested and planned behavior in public docs.
10. Never ship stale binaries next to newer source.
11. Change one architectural variable at a time and keep rollback/test paths reproducible.
