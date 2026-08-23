# CH375USB — Engineering knowledge

**Current reference version:** 0.5.1  
**Primary target:** Pocket386 and similar 386-class DOS PCs with an ISA-connected CH375 USB host controller  
**Tested I/O mapping:** data `0260h`, command/status `0261h`

This document is the durable engineering reference for CH375USB. It records the architecture, low-level invariants, real-hardware findings, compatibility boundaries and implementation lessons that should survive release-to-release documentation cleanup.

Release history belongs in [`../CHANGELOG.md`](../CHANGELOG.md), current limitations and deferred work belong in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md), and licensing/provenance details belong in [`NOTICE.md`](NOTICE.md).

The 0.5.1 mouse architecture supersedes the 0.5.0 runtime mouse path, but the USB/storage/controller knowledge established in 0.4.12/0.4.13 remains part of the current design unless explicitly replaced below.

---

## 1. Current known-good reference state

On the tested Pocket386 / MS-DOS 7.x / Windows 95 environment, the current baseline is:

| Function | DOS | DOS hotplug | Windows 95 GUI | Windows hotplug |
|---|---:|---:|---:|---:|
| USB mass storage | Yes, read/write | Yes, when idle/closed | Yes through DOS real-mode compatibility path | No native safe-removal/PnP |
| USB boot keyboard | Yes | Yes | No native Windows keyboard driver | No |
| USB boot mouse | Yes | Yes | Yes through `CH375MOU.DRV` | First insertion works; unplug -> replug in same session is a known issue |
| Physical PS/2 mouse | Yes through internal `INT 33h` | N/A | Yes through `CH375MOU.DRV` | N/A |
| Physical PS/2 + USB mouse together | Yes | Yes for USB side | Yes | Subject to the USB replug limitation |
| External USB hub | Experimental | Experimental | No | No |

The 0.5.1 release is self-contained for mouse input: **CH375USB owns DOS `INT 33h` itself. CuteMouse/CTMOUSE is no longer a runtime dependency and must not be loaded on top of CH375USB 0.5.1.**

Prebuilt 0.5.1 project binaries are published under [`../binary/`](../binary/), but source, binaries and documentation must always refer to the same release revision.

---

## 2. Architectural lineage: what changed and what did not

Understanding the version lineage prevents old experiments from being accidentally restored as current architecture.

### 0.4.11

Known-good DOS baseline with:

- CH375 mass storage;
- HID boot keyboard;
- HID boot mouse;
- DOS hotplug;
- early DOS mouse bridging.

### 0.4.12

Low-level CH375 audit against WCH CH375 Datasheets I/II, including:

- documented root reset sequencing;
- root speed detection / low-speed handling;
- explicit retry control;
- native storage size/sense corrections.

### 0.4.13

Hardware-tested storage/USB correctness baseline. Important fixes from this version remain current engineering rules:

- DOS Media Check semantics;
- actual EP0 maximum-packet handling;
- Direction Flag discipline;
- correct partial-transfer counts;
- generic MSC initialization;
- write/cache-flush error propagation;
- atomic controller ownership;
- LBA capacity bounds.

### 0.5.0

Introduced the Windows-aware input architecture:

- virtual BIOS PS/2 mouse interface;
- external conventional DOS mouse driver as the `INT 33h` owner;
- `CH375MOU.DRV` Win16 bridge;
- incremental Windows HID-mouse hotplug state machine;
- keyboard software typematic.

The virtual BIOS PS/2 layer and external DOS mouse-driver runtime path were useful stepping stones, but are **not the normal 0.5.1 USB mouse route**.

### 0.5.1

Current architecture:

- CH375USB supplies its own resident DOS `INT 33h` mouse core;
- USB HID mouse reports feed that core directly;
- the physical PS/2 mouse is received through the real BIOS `INT 15h/C207` callback;
- PS/2 callback work is deferred to avoid Pocket386 hard locks;
- PS/2 and USB mouse state are merged safely;
- DOS text-mode cursor support is internal;
- `CH375MOU.DRV` consumes the same resident `INT 33h` callback stream in Windows 95;
- Windows mouse servicing uses a bounded high-rate RTC source while the staged hotplug state machine remains on `INT 1Ch`.

Do not regress from the 0.5.1 architecture to synthetic 8042 AUX injection or to routing normal USB mouse reports through BIOS C207.

---

## 3. High-level 0.5.1 architecture

```text
                                CH375USB.SYS
                                     |
                   +-----------------+-----------------+
                   |                 |                 |
              DOS storage       USB keyboard       mouse core
                   |                 |                 |
           native / BOT/SCSI      boot HID         internal INT 33h
                                                       ^
                            +--------------------------+-------------------+
                            |                                              |
                  physical BIOS PS/2                              CH375 USB HID
                    INT 15h/C207                                  direct report feed
                            |                                              |
                            +--------------------------+-------------------+
                                                       |
                                                   DOS software
                                                       |
                                                INT 33h callback
                                                       |
                                                CH375MOU.DRV
                                                       |
                                                  Windows 95
```

The CH375 is a single shared controller. Storage, HID polling, enumeration and maintenance are different clients of the same hardware and must obey one ownership/serialization model.

---

## 4. Hardware baseline and controller ownership

The tested Pocket386 maps the CH375 parallel interface as:

```text
0260h  data
0261h  command/status
```

The resident driver probes the controller with `CHECK_EXIST`: it writes `55h` and expects the complemented `AAh` response before installing normal USB runtime behavior.

Two practical rules follow:

1. Do not assume all CH375 ISA boards use `0260h/0261h`; the mapping is hardware-specific even though this is the tested/default configuration.
2. Only one host driver may own a CH375 controller. Never load CH375USB together with `CH375286.SYS`, a WCH DOS driver or another CH375 host driver pointed at the same I/O ports.

On the CH375 parallel interface, command-port bit 7 mirrors the active-low `INT#` state. Therefore:

- bit 7 clear means a status event is pending;
- bit 7 set means no interrupt/status event is pending.

Do **not** issue `GET_STATUS` blindly. Status-consuming code should first observe pending `INT#`, otherwise it can consume the wrong lifecycle state or manufacture confusing timeout behavior.

---

## 5. Resident execution contexts

CH375USB is polling-oriented on the Pocket386. Different interrupt contexts have deliberately different responsibilities.

### DOS block-device Strategy / Interrupt entry points

These service DOS block requests. They must not trigger opportunistic USB enumeration when media is absent. A missing drive must return promptly as drive-not-ready.

### `INT 1Ch` — BIOS timer

Under DOS, keep this work bounded:

- drain deferred physical PS/2 events;
- sample CH375 root connect/disconnect status;
- poll already-enumerated HID endpoints;
- run USB keyboard typematic;
- update retry/backoff counters;
- schedule heavier maintenance.

Do not perform full root enumeration or long multi-tick waits here.

Under Windows, `INT 1Ch` additionally advances the **incremental Windows hotplug state machine by at most one bounded stage per tick**.

### `INT 28h` — DOS idle

Under DOS, this is the place for heavier deferred USB maintenance:

- native storage probing;
- generic root enumeration;
- hub scanning;
- storage-ready validation;
- topology rebuilding after attach/detach.

Under Windows, `INT 28h` deliberately performs no Windows enumeration work.

### `INT 16h` — keyboard poll-before-chain

CH375USB does not replace the BIOS keyboard service wholesale. The hook gives the USB keyboard path a chance to inject translated scan-code input before chaining to the original BIOS handler.

### `INT 33h` — resident mouse API

In 0.5.1 CH375USB owns the DOS mouse API directly. Under DOS, calls also provide an opportunity for low-latency USB polling between BIOS timer ticks. Under Windows, the Windows timer/RTC path owns USB polling instead.

### BIOS `INT 15h/C207` physical PS/2 callback

The physical PS/2 callback is an extremely constrained producer: decode and accumulate the packet, then `RETF`. No USB transaction, DOS call, video work, Windows callback or substantial mouse processing belongs inside it.

### `INT 2Fh`

CH375USB observes Windows enhanced-mode transitions:

- `AX=1605h`: Windows entry;
- `AX=1606h`: Windows exit.

A private `C375h` compatibility multiplex probe remains, but the old protected-mode/DPMI hotplug worker is intentionally inert. Windows hotplug belongs to the timer-driven state machine.

### `INT 70h` / RTC IRQ8

The handler remains resident but the RTC periodic source is enabled only while Windows enhanced mode is active and mouse servicing needs it. It is a high-rate **bounded mouse sampler**, not an enumeration worker.

---

## 6. Serialize every CH375 command stream

The CH375 cannot safely process two overlapping software command lifecycles. DOS disk I/O, HID polling, timer work and maintenance all share the same hardware.

The current ownership primitive is the atomic 386 memory `XCHG` pattern:

```asm
mov al,1
xchg al,[usb_busy]
```

A memory `XCHG` is atomic on the target 386-class CPU. `CF=0` from `usb_lock_try` means ownership was acquired; `CF=1` means another context owns the controller.

Rules:

- every transaction sequence must acquire ownership before touching CH375 state;
- every exit path must release ownership;
- a cheap non-atomic read of `usb_busy` is acceptable only as a preliminary skip when the actual called routine still performs atomic acquisition;
- never reintroduce a check-then-set lock.

### Important Windows exception: lock state is not enough

The incremental Windows enumerator starts a CH375 command on one timer tick and may wait for its completion on a later tick. It can release `usb_busy` between those stages while a controller transaction is still logically in flight.

Therefore the Windows RTC fast mouse sampler must also require the enumeration state to be fully idle before touching CH375. **`usb_busy == 0` alone does not prove that the controller is available during asynchronous Windows enumeration.**

This is a critical 0.5.1 invariant.

---

## 7. CH375 reset, speed, address and retry state

Controller mode and transport state are explicit software state, not incidental implementation details.

### Root reset

The documented sequence used by the generic host path is:

```text
mode 5  -> host enabled/quiescent, no SOF
mode 7  -> assert USB bus reset
mode 6  -> release reset and resume host operation with SOF
```

Every failure path must release mode 7. Leaving the controller in reset makes healthy devices appear dead.

### Root speed detection

`GET_DEV_RATE` is used in host mode 5. Bit 4 of the result identifies a low-speed root device.

`SET_USB_MODE` restores full-speed operation, so when a device was detected as low speed the low-speed selection must be reapplied **after** the final switch to mode 6.

A legacy CH375B-compatible low-speed sequence remains as a compatibility fallback for older/revision-specific behavior. The historical sequence using command/data values `0Bh -> 17h -> D8h` is externally learned hardware behavior and is attributed in [`NOTICE.md`](NOTICE.md). Do not treat it as the primary documented WCH-II path.

### Retry policy

The CH375 reset/default retry behavior may retry NAK indefinitely. That is unacceptable in a resident polling driver because an idle HID interrupt endpoint normally NAKs.

CH375USB programs retry behavior explicitly using `SET_RETRY` and the required `25h` marker. Generic traffic must use bounded retry semantics so one quiet endpoint cannot stall the machine.

Enumeration, bulk storage and HID polling have different latency requirements. Do not casually replace them with one global retry policy.

### Address and endpoint toggle state

USB address, selected slot, endpoint number and DATA0/DATA1 toggle are explicit state. Recovery/reset paths must restore them coherently; never assume the CH375 implicitly matches the software slot table after a reset or endpoint stall recovery.

---

## 8. Status waits and timing

Low-level waits use two strategies depending on interrupt availability:

- BIOS tick-based timeouts where interrupt/tick state is usable;
- bounded CPU polling fallbacks where it is not.

The loops are intentionally conservative for the tested ISA/Pocket386 environment. Names such as `delay_10ms` describe intent, not a calibrated guarantee on every 386 clock/bus configuration.

Engineering rule: **datasheet minimum timing is not automatically a safe performance target.** ISA wait states, CPU speed, CH375 revision and software overhead matter.

When optimizing timing:

1. measure on real hardware;
2. change timing separately from functional logic;
3. keep an easy rollback;
4. regression-test storage read/write, keyboard, mouse and hotplug after each timing change;
5. compare cold boot and hot insertion.

The current conservative margins have more value than a theoretical micro-optimization that has not survived hardware testing.

---

## 9. USB control transfers and EP0

Endpoint zero is not always 64 bytes. A valid USB device can advertise EP0 maximum packet sizes such as 8, 16, 32 or 64 bytes.

CH375USB stores each device's actual `bMaxPacketSize0` and uses it when deciding whether an IN packet is a short packet. Treating every packet smaller than 64 bytes as terminal can truncate valid control transfers.

Other rules:

- the CH375 built-in descriptor path has a 64-byte internal transfer limit;
- configuration descriptors larger than the built-in path can safely return require the manual control-transfer fallback;
- control-transfer bookkeeping such as `usb_control_wait` must be cleared on both success and failure;
- zero-data OUT requests still have a complete command/status lifecycle;
- a control transfer must finish before another CH375 command stream starts.

Windows hotplug reuses the bounded built-in descriptor commands where possible, but starts a command on one tick and consumes its completion only when `INT#` asserts on a later tick.

---

## 10. USB topology and slot model

The unified source keeps a small resident device table:

- `MAX_DEVICES = 5`;
- `MAX_HUB_PORTS = 4`;
- one root device plus limited hub/downstream bookkeeping;
- per-slot address, parent/port, capabilities, EP0 size, HID/MSC endpoints, toggles and interface numbers.

Capabilities are tracked independently for hub, MSC, keyboard and mouse functions.

External-hub support remains experimental. Direct-root keyboard, mouse and mass-storage behavior is the stability baseline; do not let hub experimentation destabilize the direct-root paths.

On detach, remembered keyboard keys, mouse button state and topology must not survive as phantom input/device state.

---

## 11. Storage architecture: preserve the dual path

The storage architecture intentionally keeps two backends:

```text
try CH375 native storage
        |
        +-- success -> STORAGE_NATIVE / native DISK_* backend
        |
        +-- failure -> generic USB enumeration
                         |
                         +-- Mass Storage BOT/SCSI backend
```

The native path is valuable on a 386 because it is compact, efficient and proven on the target hardware. The generic BOT path is necessary for devices the native firmware path does not handle and for generic USB topology.

Do not replace the known-good native path merely to make the architecture look more uniform.

---

## 12. Native CH375 mass-storage backend

The native backend uses the CH375's mass-storage commands, including:

```text
DISK_INIT
DISK_SIZE
DISK_READY
DISK_READ / DISK_RD_GO
DISK_WRITE / DISK_WR_GO
DISK_R_SENSE
```

Important rules:

- `DISK_SIZE` returns both capacity and physical sector size;
- the current driver accepts 512-byte sectors and rejects unsupported physical sector sizes safely;
- native capacity is tracked as a **sector count**, not an inclusive last-LBA value;
- `DISK_R_SENSE` is a real command with its own completion/status/data lifecycle;
- sense data must be drained before another native command is issued;
- native waits are bounded and record timeout/failure stage information for diagnosis.

A native-command failure must leave the CH375 in a usable state for later retry or generic fallback; do not strand the controller in a half-completed native transaction.

---

## 13. Generic USB Mass Storage BOT/SCSI backend

The fallback path implements USB Mass Storage Bulk-Only Transport with SCSI block commands including:

```text
TEST UNIT READY
REQUEST SENSE
INQUIRY
READ CAPACITY(10)
READ(10)
WRITE(10)
SYNCHRONIZE CACHE(10)
```

### BOT framing invariants

- CBW length is 31 bytes;
- CSW length is 13 bytes;
- CBW/CSW signatures must be correct;
- every command uses an active tag and the returned CSW tag must match;
- CSW status must indicate success;
- IN and OUT endpoint toggles are tracked independently;
- short/failed bulk transfers must not be silently accepted.

### BOT recovery

On a BOT transport failure, recovery performs the standard class reset/endpoint cleanup pattern used by this implementation:

- Mass Storage Reset request;
- clear halt on bulk OUT;
- clear halt on bulk IN;
- reset both bulk toggles to DATA0;
- bounded settling delay.

Do not merely retry the same packet with stale endpoint-toggle state.

### MSC initialization

Recognizing an MSC interface is not enough. `msc_init_slot` must complete before a slot becomes usable storage. The current initialization path:

- starts with LUN 0;
- probes readiness with bounded `TEST UNIT READY` retries;
- reads capacity;
- requires a 512-byte block size;
- claims the storage slot only if the volume can be mounted successfully.

The current DOS interface exposes one active storage target/LUN.

### Capacity representation

`READ CAPACITY(10)` returns an **inclusive last LBA**. This differs from the native backend's sector-count representation. Keep this distinction explicit when validating request ranges.

---

## 14. DOS block-device contract

CH375USB exposes **one DOS block-device unit**. DOS reserves the drive letter even when no USB storage is attached. Hotplug therefore changes media-ready/media-changed state; it does not create and destroy DOS device headers dynamically.

This architectural choice is fundamental to practical DOS hotplug.

### No enumeration from disk requests

A DOS read/build-BPB/media request with absent media must fail promptly. It must not launch enumeration from inside the block request path.

### Media Check semantics

DOS expects the result byte:

```text
FFh  media changed
00h  unknown
01h  media not changed
```

The internal `media_changed` flag is a boolean and cannot be copied directly. Current behavior returns `FFh` once after a change and `01h` in the stable state, then clears the internal change flag.

### Partial-transfer counts

On a read/write failure, DOS request-header word `+18` must contain the number of sectors **actually completed**, not the originally requested sector count.

Returning an error together with the full requested count is contradictory and can confuse DOS/filesystem code.

### LBA bounds

CH375USB validates the entire requested range before touching media and guards sector access against reported capacity.

Out-of-range requests return `ERR_SECTOR_NOT_FOUND` and zero completed sectors.

Remember:

- native capacity = sector count;
- BOT capacity = inclusive last LBA.

### Write / write-verify / synchronization

The current release has writes enabled and `SYNC_AFTER_WRITE=1`.

- normal write requests write each requested sector;
- DOS write-verify requests additionally read/verify each sector through the storage abstraction;
- after the request, the backend synchronization/cache-flush operation is issued;
- a failed final synchronization is a failed DOS write, not a success.

This synchronization covers only data already passed to CH375USB. SMARTDRV, DOS, Windows or another higher-level cache can still hold unwritten data above the driver.

---

## 15. Storage filesystem and partition model

CH375USB is a DOS block-device driver, not a BIOS `INT 13h` hard-disk driver.

The driver mounts the relevant partition and exposes the logical volume under the reserved DOS drive letter. Consequently:

- DOS `FDISK` is not the documented way to create the physical USB disk's MBR/partition through CH375USB;
- partition creation should be done in Linux or another environment that can access the physical USB disk directly;
- once the partition exists, a compatible DOS `FORMAT` can recreate the filesystem on the logical volume.

### Validated cross-platform FAT16 geometry

The currently validated Pocket386/Linux/DOS/Windows 95 layout is deliberately conservative:

- MBR (`msdos`) partition table;
- one primary FAT16 partition;
- 1 GiB partition size;
- 512-byte logical sectors;
- 64 sectors per cluster = 32 KiB clusters;
- two FAT copies;
- 512 root-directory entries;
- boot flag enabled for broad DOS-era compatibility.

Linux reference procedure:

```sh
sudo umount /dev/sdX?* 2>/dev/null
sudo wipefs -a /dev/sdX
sudo parted -s /dev/sdX mklabel msdos mkpart primary fat16 1MiB 1025MiB set 1 boot on
sudo partprobe /dev/sdX
sleep 1
sudo mkfs.fat -F 16 -S 512 -s 64 -f 2 -r 512 -n DOSUSB /dev/sdX1
```

The earlier near-2 GiB FAT16 layout created under Linux with automatically selected `mkfs.fat` geometry became unreliable/corrupted in the tested workflow once populated with many files and directories. The 1 GiB layout above was subsequently exercised with roughly 300 files and multiple DOS software trees without reproducing the problem.

This is a **real-hardware compatibility result**, not a claim that all FAT16 volumes larger than 1 GiB are inherently invalid.

### DOS reformat of an existing partition

Once the partition already exists and is mounted by CH375USB, a compatible MS-DOS FORMAT may be used, for example:

```dos
FORMAT E: /U /V:DOSUSB
```

Replace `E:` with the actual drive letter printed by CH375USB. This recreates the filesystem on the existing logical partition; it does not create the MBR or resize the partition.

---

## 16. Storage removal rules

### DOS

Hot-unplug is supported **only when the drive is idle**:

1. close files and programs using the drive;
2. wait for disk activity to stop;
3. if SMARTDRV write-behind caching is active, run `SMARTDRV /C`;
4. then remove the USB storage device.

### Windows 95

Do **not** unplug CH375USB storage while Windows is running.

The Windows storage path is a DOS real-mode compatibility path, not native USB mass-storage Plug and Play and not a Windows safe-removal implementation. Exit Windows or shut down first, then remove media under the DOS rules.

### Linux host preparation/copying

Unmount the FAT16 filesystem before removing the drive. A successful unmount flushes Linux filesystem buffers.

---

## 17. Storage debugging: media can mislead you

Different USB flash controllers behave differently on minimal retro hosts. A device-specific incompatibility can look exactly like a CH375 timing or enumeration bug.

Before rewriting low-level code because one flash drive fails:

- test at least two different USB storage devices;
- include an older/simple USB 2.0-era stick if available;
- compare cold boot with the stick already inserted against live insertion;
- test reads and writes separately;
- verify the filesystem after remount/reboot;
- test a known validated FAT16 layout;
- check whether failure follows the media rather than the driver revision.

Do not generalize from one flash-controller implementation.

---

## 18. USB keyboard architecture

The keyboard path targets **USB HID boot protocol**, not arbitrary HID report descriptors.

Initialization selects:

- HID boot protocol;
- idle setting zero.

A boot-keyboard report contains modifier state plus up to six ordinary key usages. CH375USB:

- tracks modifier transitions;
- tracks the previous six-key report per USB slot;
- emits make codes for newly pressed keys;
- emits break codes for released keys;
- translates mapped usages to PC/AT Set-1 scan codes;
- emits `E0h` prefixes for mapped extended keys;
- clears remembered state when the device disappears/re-enumerates;
- polls before chaining the BIOS `INT 16h` handler.

USB HID usages are not PC scan codes. Do not pass them through unconverted.

### Software typematic

USB boot keyboards do not generate AT-style keyboard-controller typematic by themselves. CH375USB therefore maintains one repeat candidate per USB keyboard slot.

The current timing is approximately:

- initial delay: 9 BIOS timer ticks, about 0.49 s;
- repeat interval: 2 ticks, about 0.11 s.

This gives practical DOS repeat behavior but is not guaranteed to reproduce every physical AT keyboard's exact typematic timing.

### Compatibility boundary

Programs that bypass the ordinary BIOS/DOS keyboard path or make unusual assumptions about the physical 8042 may still fail to recognize the USB keyboard. Windows 95 native USB keyboard input is not implemented.

---

## 19. Direction Flag and interrupted CPU-state discipline

Resident code can interrupt software that has `DF=1`. Never assume the interrupted program left the Direction Flag clear.

String-copy code reachable from resident/interrupt paths must ensure the copy runs with `DF=0` and that caller-visible FLAGS/DF state is not accidentally corrupted. Established low-level routines use save/`CLD`/restore discipline, while outer interrupt hooks preserve the caller's FLAGS around their resident work.

When adding `LODS*`, `STOS*`, `MOVS*` or other DF-sensitive code:

- establish `CLD` locally before the operation;
- preserve the caller's expected FLAGS/DF semantics;
- do not rely on a previous routine having happened to clear DF.

This was a real correctness fix in the 0.4.13 line and remains an invariant.

---

## 20. Current 0.5.1 mouse architecture

The 0.5.1 mouse design intentionally keeps the two hardware backends independent until they feed one logical DOS mouse:

```text
Physical PS/2 -> real BIOS C207 callback -> deferred event ----+
                                                               +-> internal INT 33h core
CH375 USB HID -> direct boot-mouse report ---------------------+
```

The current USB path **does not**:

- stuff synthetic bytes into the physical 8042 AUX queue;
- use command `D3h` as fake PS/2 injection;
- route ordinary USB mouse reports through BIOS C207;
- require CuteMouse/CTMOUSE or another external DOS mouse driver.

The old 0.5.0 virtual BIOS PS/2 compatibility layer remains useful historical/fallback code, but it is not the current internal-driver data path.

---

## 21. Physical PS/2 mouse initialization

The internal driver talks to the real BIOS directly rather than proxying `INT 15h` in the normal 0.5.1 build.

The conservative initialization sequence is:

1. disable the BIOS PS/2 mouse (`C200h`, disabled);
2. cancel any existing C207 callback;
3. request classic 3-byte packet format with `C205h`, `BH=3`;
4. set resolution using `C203h`, `BH=3` (8 counts/mm in the reference path);
5. probe that a C207 FAR callback is accepted using a harmless `RETF` handler;
6. cancel the probe callback;
7. install CH375USB's real C207 handler;
8. if BIOS status is `04h`/RESEND while installing the handler, retry that registration once;
9. enable the PS/2 mouse with `C200h`;
10. request the normal 100 reports/s rate with `C202h`, `BH=5`.

Rate selection is performance tuning: if rate selection alone fails after the mouse is otherwise working, do not tear down a functional backend.

BIOS result handling checks **both CF and AH**, because real firmware exists that reports failure inconsistently between those mechanisms.

On initialization failure, best-effort rollback disables the mouse and cancels the C207 callback. Never leave a half-installed resident callback behind.

---

## 22. The physical C207 callback must remain minimal

Real-hardware testing established a hard rule: doing too much inside the Pocket386 physical PS/2 callback can freeze the machine on the first movement packet.

The current C207 handler therefore only:

- saves minimal CPU/segment state;
- decodes the BIOS-provided status/X/Y words;
- sign-extends X and Y;
- converts PS/2 positive-up Y to the DOS screen convention, positive-down;
- accumulates pending relative movement;
- stores the latest physical button bitmap;
- marks one pending event;
- returns immediately with `RETF`.

It must **not** perform:

- CH375/USB traffic;
- DOS calls;
- BIOS calls;
- `INT 33h` application callbacks;
- video/cursor rendering;
- Windows calls;
- lengthy processing.

Pending PS/2 data is drained later from safe resident paths (`INT 33h`, `INT 1Ch`, and under Windows the RTC path). The snapshot/clear operation disables interrupts only for the brief critical section required to take the pending event.

---

## 23. USB mouse direct path and coexistence

USB boot-mouse reports provide at least:

```text
byte 0  buttons
byte 1  signed X
byte 2  signed Y
```

The current path feeds signed relative movement directly into the internal mouse core.

### Separate backend button state

USB and physical PS/2 button state are stored separately, then ORed into the logical `INT 33h` bitmap.

This matters when both mice are present. A release report from one backend must not release a button that is still physically held on the other backend.

On USB detach, the USB backend state is explicitly cleared and the logical core is given any necessary release transition while preserving buttons still held by PS/2.

### Bounded adaptive polling

Mouse polling is burst-capable, but the loop continues only after a genuinely changed report. A NAK/timeout or an unchanged zero-motion report exits immediately.

This gives low latency without turning an idle HID endpoint into a busy loop.

---

## 24. Internal DOS `INT 33h` core

CH375USB 0.5.1 implements the conventional DOS mouse services needed by the tested software and by `CH375MOU.DRV`.

Implemented/currently handled services include:

- `0000h` reset/presence;
- `0001h` show cursor;
- `0002h` hide cursor;
- `0003h` position/buttons;
- `0004h` set position;
- `0005h` button press counters/position;
- `0006h` button release counters/position;
- `0007h` / `0008h` horizontal/vertical ranges;
- `0009h` graphics cursor metadata;
- `000Ah` text cursor definition;
- `000Bh` raw motion counters;
- `000Ch` event callback;
- `000Fh` mickey/pixel ratio;
- `0011h` wheel-capability query, currently reporting no wheel;
- `0013h` double-speed threshold setting;
- `0014h` callback exchange;
- `0015h` state-storage size query, currently zero/no private save image;
- `001Ah` / `001Bh` sensitivity settings;
- `001Eh` display page query;
- `001Fh` disable;
- `0020h` enable;
- `0021h` software reset;
- `0024h` version/capability information;
- `0026h` maximum coordinates;
- `0031h` current coordinate range;
- `004Dh` resident identification string.

Unsupported functions may chain to a pre-existing provider only when the saved vector is non-null. **Never jump to `0000:0000` merely because no old driver existed during CONFIG.SYS initialization.**

### Reset semantics

`INT 33h/0000h` returns a present three-button mouse:

```text
AX = FFFFh
BX = 3
```

It is also a hardware lifecycle reset. If the physical PS/2 backend was successfully probed earlier, reset re-arms its BIOS C207 callback.

`INT 33h/0020h` enable performs the same physical re-arm.

### Motion scaling

The core keeps raw motion counters separately from logical position.

Default ratios are:

```text
X: 8 mickeys per 8 pixels
Y: 16 mickeys per 8 pixels
```

Fractional remainders are retained so slow movement is not lost when the configured ratio is greater than one logical pixel per event.

`INT 33h/0004h` cursor warp and range changes (`0007h`/`0008h`) clear those fractional remainders. A warp starts a new positioning epoch; retaining old sub-pixel motion produces false residual movement in software that repeatedly recenters the pointer.

### Button event bookkeeping

The core tracks:

- current three-button bitmap;
- press/release counts for left/right/middle;
- position associated with each press/release;
- raw X/Y motion counters.

This preserves short clicks that would be lost if software sampled only the latest bitmap.

### Callback mask and reentrancy

Standard event bits are generated for movement and each button press/release. `INT 33h/0Ch` callbacks are guarded by a resident reentrancy flag so a callback cannot recursively corrupt the same event state.

---

## 25. DOS cursor renderer

CH375USB does **not** draw a permanent pointer merely because a mouse exists.

A DOS application must request the cursor through `INT 33h/01h`. Hide/show follows the conventional nested hide-count model.

### Text-mode renderer

The current renderer supports BIOS text modes 0, 1, 2, 3 and 7. It avoids `INT 10h` from timer/INT33 paths and instead uses:

- BIOS Data Area video mode;
- column count;
- active page offset;
- row count where available;
- direct `B800h` or `B000h` text VRAM access.

The previous cell is restored before drawing at the new position or hiding the cursor.

Logical position uses the conventional default text range:

```text
X = 0..639
Y = 0..199
```

The text cell is derived from that virtual coordinate range.

### Text cursor masks

`INT 33h/0Ah` software text masks are honored. Current defaults are:

```text
screen mask = 77FFh
cursor mask = 7700h
```

which gives a visible reverse-attribute style cursor.

If an application requests the hardware-cursor form of function `0Ah`, the current implementation uses a visible software fallback rather than programming CRTC scan lines.

### Graphics cursor limitation

`INT 33h/09h` hotspot/mask metadata is retained, but CH375USB does not currently render the programmable 16x16 graphics AND/XOR mask.

Games/programs that render their own graphical pointer are unaffected.

DOS text rendering is suspended while Windows is active so the Windows GUI owns the visible pointer.

---

## 26. Why CuteMouse matters but is not a runtime dependency

The internal 0.5.1 DOS mouse core was developed through direct study/adaptation of relevant behavior and code concepts from **CuteMouse / CTMouse 2.1 beta4**, especially:

- BIOS PS/2 setup/callback behavior;
- conventional Microsoft-compatible `INT 33h` semantics;
- motion/button bookkeeping;
- reset/enable lifecycle expectations;
- cursor behavior and compatibility defaults.

That relationship is a source/provenance fact and is documented in detail in [`NOTICE.md`](NOTICE.md) and the comments in `internal_mouse.inc`.

It does **not** mean CuteMouse is installed or needed at runtime. CH375USB 0.5.1 contains its own integrated NASM resident implementation and should not have CTMOUSE loaded on top of it.

Do not remove the CuteMouse attribution/dependency relationship from provenance documentation merely because the executable is no longer a runtime dependency.

---

## 27. Windows 95 boundary

DOS USB support and Windows GUI integration are different layers.

### Storage

Windows sees CH375USB storage through the DOS real-mode compatibility path. This is not native Windows USB mass-storage Plug and Play.

### Keyboard

No native Windows 95 USB keyboard companion exists. The keyboard path remains DOS-oriented.

### Mouse

Windows GUI mouse input is bridged by `CH375MOU.DRV`:

```text
physical PS/2 / CH375 USB
        -> CH375USB internal INT 33h
        -> INT 33h function 0Ch callback stream
        -> CH375MOU.DRV
        -> Windows mouse event procedure
```

The resident DOS mouse provider remains installed while Windows is active; only DOS-visible cursor/runtime presentation state is cleared/suspended as needed.

---

## 28. `CH375MOU.DRV` Win16 bridge details

`CH375MOU.DRV` is a Win16 NE mouse-driver bridge built with Open Watcom WASM/WLINK.

### Writable data must be writable

Driver state such as the Windows event procedure and previous position counters lives in a writable data segment. Do not move it into a code segment: Windows 95 may map code selectors execute/read-only, and writing state through `CS` can produce fatal protection exception `0Dh`.

### Win16 initialization

This is pure assembly and uses the raw Win16 DLL/driver initialization entry point, not a C runtime `LibMain` wrapper.

### Advertised mouse characteristics

The bridge advertises:

- relative mouse;
- three buttons;
- 100-event/s nominal rate;
- Windows acceleration thresholds compatible with the current bridge behavior.

Windows should own GUI acceleration rather than receiving motion already strongly accelerated by the DOS layer.

### Enable sequence

When enabled, the bridge:

1. stores the Windows event procedure safely;
2. resets/probes the resident `INT 33h` provider and requires `AX=FFFFh`;
3. sets hidden X/Y coordinate ranges to `0..32767`;
4. sets the DOS mickey ratios to `16/32` while Windows owns input;
5. raises the DOS double-speed threshold to `7FFFh` to avoid pre-accelerating motion;
6. centers the hidden DOS coordinate counter at `16384,16384`;
7. reads that centered position as the previous-position baseline;
8. installs an `INT 33h/0Ch` callback for all standard movement/button events.

The bridge converts successive processed `CX/DX` positions back into relative motion for Windows.

### Why the hidden counter must start centered

If the resident coordinate counter starts near 0 or 32767, it can clamp before the Windows pointer reaches the corresponding screen edge. Centering at `16384,16384` gives symmetrical headroom and prevents the pointer from appearing unable to reach an edge until the user first moves in the opposite direction.

### Disable sequence

On disable, the bridge removes its callback and restores conventional DOS motion settings (`8/16` ratio and normal threshold behavior).

### Physical PS/2 re-arm under Windows

Windows initialization can replace or disable the BIOS C207 callback. When `CH375MOU.DRV` installs a non-zero `INT 33h/0Ch` callback, CH375USB re-arms the already-probed physical PS/2 backend.

The Windows-time re-arm intentionally performs only:

- C207 callback installation;
- enable;
- sample-rate request.

It does **not** redo packet-size or resolution negotiation during Windows startup.

---

## 29. Windows fast mouse sampling with RTC IRQ8

The normal BIOS timer is about 18.2 Hz, giving a worst-case scheduling interval around 55 ms. That is visibly coarse for GUI pointer input.

During Windows enhanced mode, CH375USB can enable RTC periodic IRQ8 / `INT 70h` at **128 Hz** (about 7.8 ms per tick).

Important design rules:

- the RTC hook is resident but the periodic source is enabled only for Windows mouse use;
- preserve the pre-Windows RTC register A/B values and PIC masks;
- restore those exact values when the fast sampler is disabled;
- call the previous `INT 70h` handler first so RTC status-C acknowledgement, BIOS wait services and normal EOI behavior remain intact;
- keep PIE asserted because some legacy BIOS handlers disable it when no BIOS wait is active;
- keep IRQ8 and the master IRQ2 cascade unmasked while the sampler is active;
- do only bounded mouse work per tick.

### Do not mix 128 Hz and 18.2 Hz mouse polling

When RTC fast polling is active, the `INT 1Ch` path must **not also poll the USB mouse endpoint**. Mixing the two independent cadences caused small report bursts and perceptible micro-judder.

`INT 1Ch` remains responsible for root events and Windows hotplug state progression; RTC handles high-rate mouse sampling.

### Adaptive drain

The RTC path may attempt a small bounded burst after a successful changed report. It exits immediately on NAK, timeout or unchanged zero-motion input. This catches up after an occasional missed tick without imposing idle busy-wait cost.

### Enumeration exclusion

The RTC sampler must not touch CH375 while the Windows enumerator is in any non-idle state, even if `usb_busy` happens to be clear between asynchronous stages.

---

## 30. Windows USB mouse hotplug state machine

Full CH375 reset/enumeration is unsafe as one monolithic timer or DPMI callback operation. Windows hotplug is therefore an explicit incremental state machine.

Current states conceptually cover:

```text
IDLE
SETTLE
MODE5
MODE7
RESET_HOLD
MODE6
WAIT_CONNECT
PREP_SLOT
GET_DEVICE_DESCRIPTOR start/wait
SET_ADDRESS start/wait
ADDRESS_SETTLE
GET_CONFIGURATION_DESCRIPTOR start/wait
PARSE_CONFIGURATION
SET_CONFIGURATION start/wait
SET_PROTOCOL setup/status start/wait
SET_IDLE setup/status start/wait
READY
RETRY_WAIT
```

Rules:

- one bounded state transition/action per `INT 1Ch` invocation;
- no `generic_root_enumerate()` from the Windows timer;
- no monolithic `ch_root_reset()` from the Windows timer;
- no DOS-idle/DPMI enumeration worker;
- a built-in CH375 command may be started on one tick and completed only after `INT#` on a later tick;
- RTC mouse polling is disabled for the duration of enumeration;
- the Windows enumerator is root HID-mouse focused and does not initialize storage/hubs.

### Speed fallback

The first attempt uses detected root speed; a failed retry can try the opposite speed before backing off. This retains compatibility with CH375/device combinations whose speed reporting/behavior is imperfect.

### Disconnect cleanup

On disconnect:

- disable RTC fast mouse sampling;
- abort/reset enumeration state;
- clear stale device topology/input state;
- return CH375 to neutral host mode 6;
- select address 0;
- restore bounded retry policy;
- arm a conservative replug watchdog.

The watchdog exists because some CH375 revisions may fail to raise every later CONNECT event. It must not become an endless enumeration storm when the port remains empty.

### Current known Windows limitation

A USB mouse works when present at Windows startup and on the **first hotplug after Windows is running**. If it is then unplugged and replugged again during the same Windows 95 session, USB mouse input currently does not resume.

The physical PS/2 mouse remains operational. Preserve the rest of the validated architecture while fixing this issue; do not rewrite the whole input stack merely to address the second-replug state reset.

---

## 31. DOS attach/detach and deferred maintenance

DOS hotplug is more than a `present` boolean. The driver tracks root-link state, root events, retry/backoff, storage ownership, media readiness/change and topology.

### Fast detach path

When a root disconnect is observed in timer context:

- mark root link detached;
- mark a root event/maintenance need;
- invalidate stale topology quickly;
- clear stale HID state;
- mark storage unavailable/media changed as appropriate;
- use a short retry/backoff.

Do not perform full re-enumeration during the disconnect interrupt/timer path.

### Deferred discovery

The DOS idle maintenance path can then:

1. reset/reconfigure CH375;
2. try native storage first;
3. fall back to generic USB enumeration;
4. initialize HID/MSC/hub functions;
5. rebuild usable device state.

Backoff is essential. A settling or incompatible device must not trigger reset/enumeration on every timer tick.

---

## 32. Build architecture

### `CH375USB.SYS`

- 16-bit flat real-mode binary;
- NASM;
- CPU target 386;
- unified DOS/Windows-aware resident driver.

### `MTEST.COM`

- small NASM flat-binary diagnostic;
- exercises the resident `INT 33h` coordinate/button interface independently of a game/application.

### `CH375MOU.DRV`

- 16-bit Windows NE driver/DLL image;
- assembled with Open Watcom WASM;
- linked with WLINK;
- linker map generated as `CH375MOU.MAP`.

### Linux/Unix build

`build.sh` builds the runtime/test outputs, bootstraps a project-local Open Watcom toolchain when needed, and validates that the generated `.DRV` is actually a Win16 NE image.

A zero linker exit code is not enough: linker warnings/malformed output are treated as build failures because earlier toolchain mistakes demonstrated that superficially generated output can still be unusable.

### DOS / Windows 9x DOS-mode build

`BUILD.BAT` builds the current runtime/test binaries using the configured NASM/Open Watcom DOS paths.

### Distribution rule

0.5.1 publishes project-built binaries under [`../binary/`](../binary/). These are outputs of the project and must correspond to the exact source/documentation release.

Do not leave an older binary next to newer source and allow users to mistake it for the current build.

---

## 33. Source layout and responsibility map

Current important files:

- `CH375USB.ASM` — resident DOS block-device driver, interrupt hooks, top-level state and Windows transition logic;
- `internal_mouse.inc` — self-contained DOS `INT 33h` mouse core and physical PS/2 backend;
- `bios_mouse.inc` — 0.5.0-era virtual BIOS PS/2 compatibility layer retained for non-current/fallback architecture;
- `win_hotplug.inc` — bounded Windows HID-mouse hotplug enumerator;
- `usb_core.inc` — generic USB enumeration/control transfers;
- `usb_hid.inc` — HID boot keyboard/mouse initialization and polling;
- `usb_msc.inc` — USB Mass Storage BOT/SCSI fallback;
- `usb_hub.inc` — experimental hub support;
- `usb_maint.inc` — DOS attach/detach and deferred maintenance;
- `ch375_hw.inc` — parallel-I/O access, probing, waits, reset/speed/retry helpers;
- `ch375_native.inc` — CH375 native mass-storage backend;
- `ch375_defs.inc` — controller/protocol constants;
- `CH375MOU.ASM` / `CH375MOU.LNK` — Win16 Windows mouse bridge and linker definition;
- `MTEST.ASM` — DOS mouse diagnostic;
- `build.sh` / `BUILD.BAT` — build orchestration.

Keep responsibilities separated. In particular, do not move slow enumeration into an input callback merely because the relevant source file is convenient.

---

## 34. Hardware and BIOS caveats

### Pocket386 BIOS/CMOS retention

The tested Pocket386 does not use a conventional separate coin-cell CMOS battery. A completely discharged main battery can reset BIOS settings.

Two resets are especially relevant:

- mouse support may revert to disabled;
- floppy/FDC configuration may revert to enabled.

If a previously working physical PS/2 mouse suddenly disappears after the machine has been stored with a flat battery, check BIOS settings before treating it as a CH375USB regression.

Third-party CMOS save/restore utilities and modified BIOS images are outside CH375USB support unless explicitly tested.

### Performance-oriented BIOS settings

The development Pocket386 has also been operated with:

- AT Bus Clock: `PCLK2/8`;
- I/O Recovery: `Disabled`;
- I/O Recovery Period: `0.75 µs`;
- 16Bit ISA Insert Wait: `Disabled`;
- Slow Refresh: `120 µs`.

These are **not driver requirements**. When diagnosing instability, conservative/default BIOS timings remain the better baseline.

---

## 35. Provenance and reference boundaries

Engineering facts and source provenance must not be conflated.

### Directly adapted/informed source relationship

The internal 0.5.1 DOS mouse core has an explicitly documented CuteMouse / CTMouse 2.1 beta4 study/adaptation relationship under compatible GPL terms. Preserve that attribution.

### Interoperability / behavioral references

Other important sources include:

- WCH CH375 Datasheet I/II;
- historical W.ch/WinChipHead CH375 examples;
- FreddyV's CH375 driver work and hardware reports;
- xeecos CH375 keyboard example;
- Makeblock MeUSBHost;
- crazii USBDDOS;
- Bret Johnson USBDOS/USBMOUSE;
- USB, HID, Mass Storage BOT and SCSI specifications;
- DOS/BIOS/Windows interface references including RBIL;
- real Pocket386 testing and community hardware reports.

The exact references, licenses and URLs belong in [`NOTICE.md`](NOTICE.md).

Do not describe CuteMouse as a runtime dependency in 0.5.1, but also do not erase its source-provenance role.

---

## 36. Hardware-tested mouse regression baseline

Mouse changes should preserve this order of testing:

1. DOS boot with physical PS/2 only: no freeze.
2. `MTEST`: PS/2 movement and all buttons.
3. USB first hotplug in DOS: `MTEST` movement/buttons.
4. PS/2 + USB simultaneously.
5. EDIT: text cursor, movement and buttons with both backends.
6. DOS applications with both mice: DOS Shell (`DOSSHELL`), QBasic 4.50, Norton 4.55, FastTracker II.
7. DOS games with both mice.
8. Windows 95 physical PS/2.
9. Windows 95 USB present at startup.
10. Windows 95 first USB hotplug.
11. Windows 95 PS/2 + USB simultaneously.
12. Verify the Windows pointer can immediately reach all four edges without first moving away from an exhausted hidden-coordinate clamp.
13. Track unplug/replug separately as the current remaining Windows mouse issue.

The current real-hardware DOS game baseline includes:

- The Secret of Monkey Island;
- Monkey Island 2;
- The Games: Winter Challenge;
- Wolfenstein 3D;
- Ken's Labyrinth;
- Hexxagon;
- Cannon Fodder;
- Battle Chess;
- Tyrian 2000;
- XQuest 2;
- Warcraft: Orcs & Humans;
- Lemmings;
- SimEarth: The Living Planet;
- Sid Meier's Civilization 474.03;
- Arkanoid;
- Strip Poker Professional.

A successful program is evidence for that program/configuration, not a universal DOS mouse compatibility guarantee.

---

## 37. Storage regression discipline

For storage changes, test at minimum:

1. `CHECK_EXIST` and correct I/O mapping;
2. cold boot with no media;
3. cold boot with known-good media inserted;
4. read files;
5. create/write/delete files;
6. write-verify path where applicable;
7. remount/reboot and verify filesystem integrity;
8. hot-remove only when idle;
9. insert a different device under the same reserved DOS drive letter;
10. cross-class hotplug: storage -> keyboard -> mouse -> storage;
11. DOS -> Windows transition with storage already detected;
12. validated 1 GiB FAT16 volume populated with hundreds of files/directories and checked under Linux, DOS and Windows 95.

For any low-level storage change, test more than one flash drive before concluding the driver is wrong.

---

## 38. Current support boundaries

### Supported / hardware-tested baseline

- one CH375 controller at the configured I/O mapping;
- one DOS block-device unit / drive letter;
- root USB mass storage;
- 512-byte physical sectors;
- FAT12/FAT16-oriented DOS storage use, with the documented 1 GiB FAT16 geometry as current validated recommendation;
- USB HID boot keyboard in DOS;
- USB HID boot mouse in DOS;
- physical BIOS PS/2 + USB mouse coexistence;
- internal DOS `INT 33h` provider;
- DOS hotplug;
- software keyboard typematic;
- DOS text-mode mouse cursor when requested;
- Windows 95 mouse bridge on the tested Pocket386 configuration;
- Windows first USB mouse hotplug.

### Implemented but experimental / incomplete

- external USB hub support;
- broader Windows 95 mouse compatibility beyond the tested hardware/software matrix;
- Windows USB mouse detach -> second replug recovery.

### Not implemented / deliberately deferred

- native Windows 95 USB keyboard support;
- native Windows USB mass-storage Plug and Play/safe removal;
- arbitrary HID report descriptors;
- mouse wheel reporting;
- full programmable `INT 33h/09h` graphics cursor rendering;
- exact Microsoft/CuteMouse acceleration/sensitivity parity for every setting;
- FAT32 support;
- multi-LUN exposure;
- non-512-byte sector translation;
- complete DOS removable-media capability advertisement;
- fully validated external-hub/low-speed downstream support;
- minimal/zero-unit resident cleanup when CH375 is absent.

See [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) for the current short list of open work.

---

## 39. Rules for future changes

These are project invariants, not suggestions:

1. **Preserve the proven native storage path** unless a replacement is measurably safer/better on real hardware.
2. **Serialize every CH375 command sequence** with the atomic ownership primitive.
3. **Treat Windows asynchronous enumeration state as controller ownership state** even between ticks when `usb_busy` is temporarily clear.
4. **Keep timer/RTC work bounded.** Never hide long enumeration waits inside an ISR.
5. **Keep DOS heavy maintenance in DOS idle**; keep Windows hotplug incremental on its dedicated timer state machine.
6. **Never leave CH375 in mode 7/reset** on an error path.
7. **Treat mode, speed, retry policy, address and endpoint toggles as explicit state.**
8. **Do not consume `GET_STATUS` unless `INT#` is pending.**
9. **Use the device's real EP0 maximum packet size** for short-packet logic.
10. **Complete each CH375 command/status/data lifecycle before starting another.**
11. **Preserve DOS Media Check and partial-transfer semantics.**
12. **Validate storage capacity before I/O** and preserve the distinction between native sector count and BOT inclusive last LBA.
13. **Propagate final write/synchronization failures** rather than reporting a false successful DOS write.
14. **Preserve caller CPU/FLAGS/DF expectations** in resident code.
15. **Keep the physical PS/2 C207 callback minimal and deferred.** Do not perform USB, DOS, video or application callbacks inside it.
16. **Keep USB mouse input direct-to-internal-INT33 in 0.5.1.** Do not reintroduce BIOS C207 as the normal USB route.
17. **Never inject synthetic mouse bytes through 8042 AUX/D3** just to imitate a physical PS/2 device.
18. **Keep PS/2 and USB button state separate until logical merge.**
19. **Do not draw a permanent DOS cursor.** Render text cursor only after an application requests it.
20. **Keep Windows GUI acceleration in Windows.** Avoid double acceleration in the resident DOS layer.
21. **Center the Windows hidden INT33 coordinate counter** before seeding relative-motion state.
22. **Do not run the 18.2-Hz USB mouse poll concurrently with the Windows 128-Hz RTC sampler.**
23. **Do not advertise Windows storage safe removal.** The current path is DOS-backed compatibility storage.
24. **Keep the validated 1 GiB FAT16 geometry as the default recommendation** until an alternative has comparable real-hardware stress testing.
25. **Do not treat one USB stick or one game as universal evidence.** Reproduce across media/software where relevant.
26. **Preserve source provenance accurately.** CuteMouse is not a runtime dependency, but its adaptation role in `internal_mouse.inc` must remain attributed.
27. **Keep source, binaries, README, changelog, knowledge and known-issues documentation on the same release version.**
28. **Distinguish implemented, tested and planned behavior.** Never turn one successful test into a blanket compatibility claim.
29. **Change one architectural variable at a time** and keep rollback/regression paths reproducible.
30. **Do not convert a theoretical reviewer concern into a code change without checking source, public specifications and real hardware.**

These invariants are more important to CH375USB stability than any individual cleanup or optimization.
