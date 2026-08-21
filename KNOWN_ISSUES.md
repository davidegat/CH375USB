# Known issues / deferred work

This file tracks current CH375USB 0.5.0 limitations and work that may be worth doing later. Historical fixes belong in [`CHANGELOG.md`](CHANGELOG.md).

## Confirmed open issues

### Windows 95 mouse support is still experimental

`CH375MOU.DRV` and the Windows-aware hotplug state machine are implemented in 0.5.0, but broad real-hardware/application validation is incomplete.

The current architecture is:

```text
USB HID mouse
    -> CH375USB.SYS
    -> BIOS PS/2 callback / DOS mouse driver
    -> INT 33h callback stream
    -> CH375MOU.DRV
    -> Windows 95 mouse event procedure
```

Treat successful operation on one machine/application as evidence, not as a universal compatibility guarantee.

### Windows 95 USB keyboard support is not implemented

The keyboard path remains DOS-oriented. 0.5.0 improves DOS behavior with software typematic/repeat, but there is no protected-mode Win16/VxD keyboard companion equivalent to `CH375MOU.DRV`.

### Some DOS programs may bypass the supported keyboard path

The driver translates HID boot-keyboard reports into PC-compatible scan codes. Programs or games that read hardware in unusual ways, assume a physical keyboard controller, or bypass the conventional BIOS/DOS path may still fail to recognize the USB keyboard correctly.

### CH375 absent: resident cleanup is incomplete

If the controller probe fails, CH375USB avoids installing normal runtime USB behavior, but the initialization path is not yet optimized to return a zero-unit/minimal-resident image.

## Compatibility limitations

### Typematic is software-generated

0.5.0 adds repeat handling, but its timing is produced by the resident timer path and is not guaranteed to match every physical AT keyboard exactly.

### Windows storage is not native USB Plug and Play

A storage device detected by DOS can be carried into Windows 95 through the real-mode block-device path. Windows-side insertion/removal should not be described as native USB mass-storage hotplug.

### Mouse integration depends on a DOS mouse driver

The 0.5.0 architecture deliberately lets a conventional BIOS/PS2-aware DOS mouse driver such as CuteMouse own `INT 33h`. CH375USB provides the virtual BIOS PS/2 side and does not attempt to replace every mouse-driver function itself.

### HID boot protocol only

Keyboard and mouse handling target boot-protocol HID devices. Arbitrary HID report-descriptor devices are outside the current scope.

## Experimental / deferred features

### External hub completion

One external hub with limited downstream support exists, but it is not considered production-ready. It still needs systematic real-hardware validation with keyboard, mouse, storage, mixed devices and downstream hotplug.

Low-speed downstream behavior should remain conservative until validated on actual hardware.

### Full DOS removable-media capability advertisement

The block-device path provides media-change behavior, but full DOS removable-media capability advertisement and command handling remain deferred.

### FAT32

The documented storage target remains an MBR disk with a primary FAT16 partition no larger than 2 GiB. FAT32 needs explicit BPB/driver work and DOS 7.x testing; accepting partition type values alone would not be sufficient.

### Multi-LUN

The driver exposes one DOS block device and uses one active storage target. Querying/selecting other LUNs is deferred.

### Non-512-byte sector translation

Non-512-byte media is rejected safely. Translation would require a DOS-512-byte logical layer with buffering and read-modify-write semantics.

### User-visible DOS attach/detach notifications

Hotplug is handled internally, but there is no user-facing resident notification utility yet. If added later, console output should remain outside timer/interrupt-critical USB paths.

## Worth validating in dedicated experiments

### Windows hotplug timing and retry behavior

The Windows enumerator performs one bounded state-machine action per timer tick. Retry intervals, USB speed fallback and detach/replug watchdog behavior should only be tuned with explicit regression tests on real hardware.

### Windows pointer-speed behavior

`CH375MOU.DRV` deliberately normalizes the DOS layer and lets Windows own acceleration. Control Panel behavior should be checked across several Windows 95 configurations before changing thresholds or mickey ratios again.

### BIOS PS/2 coexistence edge cases

The virtual BIOS mouse path first gives the physical BIOS a chance to handle PS/2 requests and mirrors configuration to the USB side. This is intended to allow physical PS/2 plus USB mouse coexistence, but unusual BIOS implementations may still need compatibility handling.

### BOT Check Condition / REQUEST SENSE handling

The current BOT path recovers aggressively. More nuanced SCSI Check Condition handling may improve removable-media behavior, but should be implemented only with targeted devices and tests.

### Private ISR stack

Timer and idle paths use the interrupted program's stack. A private resident stack may improve robustness for unusually small application stacks, but it would introduce nesting/reentrancy complexity.

### Timing optimization

Current CH375 delays are conservative. Optimize only after measurement and keep timing changes separate from functional changes.

## Deliberately unsupported unless a real use case appears

- arbitrary HID report descriptors;
- native Windows 95 USB keyboard stack;
- automatic translation of non-512-byte media;
- multiple simultaneous DOS drive letters for multiple LUNs.

## Resolved before 0.5.0

The 0.4.13 baseline already fixed:

- DOS Media Check semantics;
- EP0 short-packet handling;
- Direction Flag isolation;
- partial transfer sector counts;
- generic MSC initialization;
- cache-flush error propagation;
- atomic CH375 controller ownership;
- LBA bounds checking.

See [`CHANGELOG.md`](CHANGELOG.md) for the historical details.
