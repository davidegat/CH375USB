# Known issues / deferred work — CH375USB 0.5.1

This file tracks current limitations and intentionally deferred work in the **current `main` / 0.5.1 codebase**. Historical fixes belong in [`../CHANGELOG.md`](../CHANGELOG.md); durable architecture and engineering invariants belong in [`KNOWLEDGE.md`](KNOWLEDGE.md).

The list below distinguishes actual current limitations from behavior that was already fixed during 0.5.0/0.5.1 development.

---

## Confirmed open issues

### Windows 95 USB mouse unplug -> replug

A USB mouse works when present before Windows starts and also works on the **first hotplug after Windows is already running**. If that USB mouse is then unplugged and replugged again in the same Windows 95 session, USB mouse input currently does not resume.

The physical PS/2 mouse remains operational. This is considered a localized Windows hotplug state-machine cleanup item and should be fixed without disturbing the otherwise hardware-validated 0.5.1 mouse architecture.

### Windows 95 USB keyboard support is not implemented

The USB keyboard path remains DOS-oriented. Boot-protocol HID translation and software typematic work in DOS, but there is no protected-mode/Win16 Windows keyboard companion equivalent to `CH375MOU.DRV`.

### Some DOS keyboard software may bypass the supported path

CH375USB injects translated PC/AT Set-1 keyboard bytes through the conventional 8042-compatible path and also polls before the original BIOS `INT 16h` handler.

Programs that read hardware in unusual ways, assume particular physical keyboard-controller timing/state, or otherwise bypass conventional BIOS/DOS keyboard handling can still fail to recognize the USB keyboard correctly.

### CH375 absent: resident cleanup is incomplete

If the controller probe fails, normal USB runtime hooks are not installed, but the initialization path has already described one DOS block-device unit and does not yet shrink to a minimal/zero-unit resident image.

This is harmless on the normal tested machine with a CH375 present, but remains cleanup work for systems where the controller is absent or configured at a different I/O address.

---

## Mouse limitations

### Windows 95 second USB replug

The specific detach/replug failure described above remains the principal known functional mouse defect in 0.5.1. First Windows hotplug, USB present at Windows startup, physical PS/2, and simultaneous PS/2 + USB operation are part of the validated baseline.

### Programmable graphics cursor masks are not rendered

CH375USB implements the text-mode cursor requested through `INT 33h/01h` and the software text masks supplied by `INT 33h/0Ah`.

Programmable `INT 33h/09h` 16x16 graphics AND/XOR masks are **not** rendered yet. Hotspot/mask metadata is retained for API compatibility. Programs and games that draw their own graphical pointer are unaffected.

### `INT 33h/0Ah` hardware-cursor form is approximated

When software requests the hardware text-cursor form of `INT 33h/0Ah`, CH375USB currently falls back to its visible software reverse-video text cursor instead of programming CRTC cursor start/end scan lines.

The normal software-mask form of `0Ah` is implemented.

### Wheel support is not implemented

The logical mouse is exposed as a three-button mouse. The CuteMouse-compatible wheel capability query is recognized, but reports that no wheel is available. Wheel movement/reporting is not implemented.

### Sensitivity and double-speed controls do not currently alter motion

`INT 33h` sensitivity and double-speed threshold values are stored/reported where implemented, but the current `mouse_core_event` movement path applies the mickey-to-pixel ratio and fractional remainder logic, **not the stored sensitivity or double-speed acceleration parameters**.

Therefore these calls currently provide compatibility state/API behavior rather than full Microsoft/CuteMouse-equivalent motion processing. Windows 95 GUI acceleration is deliberately left to Windows.

### Complete `INT 33h` save/restore-state API is not implemented

The internal driver answers the state-storage-size query with zero and does not currently implement a private state image for the conventional save/restore functions (`INT 33h/16h` and `17h`).

No tested program in the current regression matrix requires this, but it is an API completeness limitation.

### Exact Microsoft/CuteMouse behavior is not a project-wide guarantee

The 0.5.1 internal mouse core was developed with direct study/adaptation of relevant CuteMouse/CTMouse behavior, but CH375USB does not claim bit-for-bit compatibility with every historical Microsoft Mouse / CuteMouse service, acceleration curve, sensitivity edge case, cursor mode or undocumented behavior.

Compatibility claims should remain tied to the real programs and games actually tested.

---

## Keyboard limitations

### HID boot protocol only

Keyboard support targets USB HID **boot protocol** reports. Arbitrary HID report descriptors, NKRO/vendor-specific keyboard formats and non-boot devices are outside the current implementation.

### Key mapping is intentionally incomplete

The current HID-to-Set-1 table covers the ordinary alphanumeric keys, common punctuation, function keys and a set of extended navigation keys used by the tested software.

It is **not a complete mapping of every HID keyboard usage**. In particular, broader keypad/international/vendor-specific coverage should not be assumed without checking `usb_hid.inc` and testing the actual keyboard/software combination.

### Keyboard LED synchronization is not implemented

CH375USB does not currently send HID output reports to synchronize USB keyboard LEDs such as Caps Lock, Num Lock and Scroll Lock with DOS/BIOS lock state.

The key itself may still be translated/injected; the physical LED state on the USB keyboard is simply not maintained by the driver.

### Typematic is software-generated

USB HID boot keyboards do not generate AT-controller typematic behavior for CH375USB, so repeat is synthesized by the resident timer path.

The current timing is practical and tested for ordinary DOS use, but is not guaranteed to reproduce every physical AT keyboard's exact initial-delay/repeat-rate behavior or software that depends on unusual controller timing.

---

## Storage limitations

### Windows storage is not native USB Plug and Play or safe-removal aware

Storage detected in DOS can remain accessible to Windows through the DOS real-mode compatibility path. Windows-side insertion/removal is not a native USB mass-storage PnP implementation and CH375USB does not provide a Windows safe-removal handshake.

**Do not unplug USB storage while Windows 95 is running.** Exit Windows or shut down before removing the drive. This avoids removing media while Windows, DOS, or a disk cache may still hold unwritten filesystem data.

### DOS hot-unplug requires an idle/closed drive

DOS storage hotplug is supported, but removal is not safe while files are open or disk activity is still occurring. Close every file/program using the drive and wait for activity to stop before unplugging. If SMARTDRV write-behind caching is enabled, run:

```dos
SMARTDRV /C
```

before removal.

CH375USB synchronizes writes that have already reached its storage backend, but it cannot flush data that DOS or another cache has not yet passed to the driver.

### BOT Check Condition / `REQUEST SENSE` handling is still basic

The generic Mass Storage BOT layer defines `REQUEST SENSE`, but the current normal command path does **not** perform nuanced SCSI sense handling after a CSW reports command failure.

`msc_read_csw` currently treats any non-zero CSW status as failure and the generic execution helpers enter BOT recovery (Mass Storage Reset, clear endpoint halts, reset toggles).

That is conservative and works for the tested storage baseline, but it does not distinguish all device-level SCSI conditions from transport-level failures. A future improvement could issue and interpret `REQUEST SENSE` for command-failed/check-condition cases before deciding whether full BOT recovery is necessary.

This should be developed only with targeted real devices and regression tests; do not destabilize the current working storage path merely to make the SCSI layer more elaborate.

### FAT16 geometry compatibility

The old documentation allowed a FAT16 partition almost 2 GiB in size and let Linux `mkfs.fat` select the remaining geometry automatically. On the tested Pocket386 workflow, that near-limit layout became unreliable once the filesystem contained many files/directories and could then be reported as corrupted across Linux, DOS and Windows.

The currently validated layout is deliberately more conservative:

- MBR partition table;
- one 1 GiB primary FAT16 partition;
- 512-byte logical sectors;
- 32 KiB clusters (64 sectors/cluster);
- two FAT copies;
- 512 root-directory entries.

This geometry was tested with roughly 300 files and multiple DOS software trees without reproducing the earlier corruption.

This is a compatibility finding from the tested hardware/workflow, **not** evidence that CH375USB inherently corrupts larger valid FAT16 volumes and not a claim that every FAT16 volume above 1 GiB is invalid.

### DOS partitioning through CH375USB

CH375USB exposes the mounted storage as a DOS installable block-device drive letter, not as a BIOS `INT 13h` hard disk. Therefore DOS `FDISK` is not the documented way to create the MBR/partition through CH375USB.

Prepare the 1 GiB primary partition using Linux or another environment with direct access to the physical USB disk. Once the partition exists, DOS `FORMAT <drive>: /U /V:DOSUSB` may be used to recreate the FAT filesystem on the logical volume if that DOS FORMAT build accepts the installable block device.

### Full DOS removable-media capability advertisement is incomplete

CH375USB reserves one block-device unit and implements media-change behavior, allowing practical DOS hotplug. It does not yet implement the complete set of DOS removable-media capability/command semantics that a maximally complete removable block-device driver could advertise.

The current tested media-check/drive-ready behavior should be preserved if this is expanded later.

### FAT32 is not implemented

The supported/documented storage target remains MBR plus FAT12/FAT16-oriented use. FAT32 requires explicit BPB/driver work and DOS 7.x testing; merely accepting FAT32 partition type values would not constitute support.

### Multi-LUN exposure is not implemented

The generic MSC code can query the class interface, but the current driver deliberately selects/exposes one active storage target and uses LUN 0. Multiple simultaneous LUNs/drive letters are not implemented.

### Non-512-byte sector translation is not implemented

Physical media with a block size other than 512 bytes is rejected safely. Translating such media to DOS 512-byte logical sectors would require buffering and, for writes, read-modify-write semantics.

---

## USB / hub limitations

### External hub support remains experimental

One external hub path with limited downstream support exists, but it has not received the same systematic real-hardware validation as direct-root storage, keyboard and mouse operation.

Mixed downstream devices, repeated downstream hotplug, error recovery and topology changes need broader testing before hub support should be described as production-ready.

### Low-speed devices behind a hub are not a validated baseline

The root low-speed handling has documented and legacy compatibility paths, but low-speed downstream hub behavior should remain conservative/experimental until verified on real hardware.

### Windows hotplug enumeration is intentionally root HID-mouse focused

The Windows incremental enumerator is designed for bounded root HID-mouse discovery. It is not a general Windows USB PnP enumerator for hubs, keyboards or storage.

---

## Resident / low-level limitations

### Private ISR stack is not implemented

Timer, RTC, idle and callback paths use the interrupted program's/current execution stack rather than switching to a dedicated resident stack.

This has not produced a problem in the validated software matrix. A private ISR stack could improve robustness for pathological applications with extremely small stacks, but would add nesting/reentrancy complexity and should only be introduced if a real program demonstrates the need.

### Conservative CH375 timing remains intentional

Several low-level delay loops are conservative and not cycle-calibrated across every possible 386/bus configuration. This is not currently classified as a functional defect.

Any timing optimization must be isolated from functional changes and re-tested on real hardware with storage, keyboard, mouse and hotplug.

---

## Pocket386 hardware caveats

The Pocket386 does not use a conventional separate coin-cell CMOS battery. A fully discharged main battery can reset BIOS settings.

In particular:

- physical mouse support may revert to disabled;
- floppy/FDC configuration may revert to enabled.

Check BIOS settings before treating such a change as a driver regression.

Third-party CMOS save/restore utilities and modified Pocket386 BIOS images are outside CH375USB support unless explicitly tested by the project.

---

## Deferred engineering work

- Fix Windows 95 USB mouse detach/replug state reset.
- Broader DOS game/application `INT 33h` compatibility testing beyond the current validated matrix.
- Optional complete `INT 33h/09h` graphics-cursor-mask renderer.
- Optional real CRTC implementation for the hardware form of `INT 33h/0Ah`.
- Complete `INT 33h` state save/restore support if real software needs it.
- Implement mouse sensitivity/double-speed processing if compatibility testing demonstrates a need.
- Mouse wheel support.
- Broader keyboard usage/keypad/international mapping.
- USB keyboard LED synchronization.
- Native Windows 95 USB keyboard bridge.
- Smarter BOT command-failed / `REQUEST SENSE` handling.
- External-hub completion and real-hardware validation.
- FAT32 support.
- Multi-LUN exposure.
- Non-512-byte sector translation.
- Complete DOS removable-media capability advertisement.
- Minimal/zero-unit resident cleanup when no CH375 controller is found.
- Private ISR stack only if real applications demonstrate a need.

---

## Resolved issues that must not be accidentally re-listed as current defects

The following were fixed before/currently within 0.5.1 and are now engineering invariants rather than open issues:

- DOS Media Check result semantics (`FFh` changed / `01h` unchanged);
- actual device EP0 maximum-packet handling;
- resident Direction Flag isolation/discipline;
- DOS partial-transfer sector counts on failed reads/writes;
- generic MSC initialization before claiming storage;
- propagation of final write/cache-synchronization failures;
- atomic CH375 controller ownership with memory `XCHG`;
- LBA range validation against reported capacity;
- keyboard software typematic/repeat;
- self-contained DOS `INT 33h` mouse provider;
- physical PS/2 + USB logical mouse coexistence;
- hard freeze caused by doing too much in the physical PS/2 callback path;
- DOS `INT 33h` reset/enable re-arming of the physical PS/2 backend;
- residual fractional movement after DOS cursor warps/range changes;
- Windows initial hidden-coordinate headroom/cursor-edge problem;
- Windows 95 USB mouse present at startup;
- Windows 95 **first** USB mouse hotplug.

Do not reopen these as theoretical defects without reproducing a regression on current code.

---

## Validated 0.5.1 baseline that should not be casually disturbed

On the tested Pocket386, the following work together:

- physical PS/2 mouse in DOS and Windows 95;
- USB mouse in DOS;
- DOS USB mouse hotplug;
- Windows 95 USB mouse present at startup;
- Windows 95 first USB mouse hotplug;
- simultaneous physical PS/2 + USB mouse input;
- DOS text cursor in EDIT;
- USB keyboard in the supported DOS path with software typematic;
- DOS mass-storage read/write/hotplug under the documented removal rules;
- conservative 1 GiB FAT16 geometry stress-tested with roughly 300 files.

The current real-program/game mouse matrix with both physical PS/2 and USB includes:

- DOSSHELL;
- QBasic 4.50;
- Norton 4.55;
- FastTracker II;
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

Future hotplug, compatibility, storage or timing changes should preserve this matrix. A successful test on one application/device is evidence for that configuration, not a universal compatibility guarantee.
