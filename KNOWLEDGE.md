# CH375USB — Development Knowledge and Lessons Learned

**Project:** CH375USB  
**Target:** Pocket386 and similar 386-class DOS PCs with a CH375 ISA USB controller  
**Author:** Davide "gat" — https://github.com/davidegat  
**Current reference version:** 0.4.11

This document is not a user manual and not a formal CH375 specification. It is a record of practical knowledge collected while developing and testing CH375USB on real hardware.

Its purpose is to save other retro-computing hobbyists from repeating the same experiments, wrong assumptions, timing traps and compatibility mistakes.

Some observations are specific to the Pocket386 implementation and its CH375 ISA interface. Treat empirical findings as exactly that: useful evidence from a working machine, not universal guarantees for every CH375 board.

---

## 1. Hardware baseline

The tested machine is a Pocket386 / 386-class PC with a CH375 USB controller exposed through ISA-style parallel I/O.

The mapping used by this machine is:

```text
0260h  CH375 data port
0261h  CH375 command/status port
```

The driver is therefore built around:

```text
IO_BASE   = 0260h
DATA_PORT = 0260h
CMD_PORT  = 0261h
```

The controller is detected with the CH375 `CHECK_EXIST` command before the resident USB logic is enabled.

### Important lesson

Do not assume every CH375 board uses the same port mapping. Verify the hardware first. A perfectly correct USB stack aimed at the wrong I/O ports looks exactly like a completely dead controller.

---

## 2. Do not load two CH375 drivers at once

The original `CH375286.SYS` and CH375USB both expect exclusive control of the same controller.

Never load both:

```dos
DEVICE=C:\CH375286.SYS ...
DEVICE=C:\CH375USB.SYS
```

This is not a useful compatibility test. It is two drivers racing for the same device.

When testing CH375USB, remove the vendor driver from that boot profile.

---

## 3. The first architecture was deliberately small

The earliest experimental CH375USB version was intentionally limited:

- resident DOS driver;
- polling instead of relying on an ISA IRQ;
- root-device support;
- read-only mass storage first;
- FAT12/FAT16-oriented DOS use;
- boot-protocol HID keyboard and mouse;
- no hub;
- no multiple-device ambition at the beginning.

That was the right development strategy.

Trying to solve storage, HID, hubs, hotplug, Windows compatibility and every filesystem case at the same time would have made debugging impossible.

### Lesson

On old hardware, build one known-good path at a time:

1. prove that commands reach the controller;
2. prove USB connection detection;
3. prove enumeration;
4. prove one transfer type;
5. only then add resident DOS integration;
6. only after that add hotplug and multiple classes.

A tiny driver that reliably reads one sector is more valuable during development than a giant half-working USB stack.

---

## 4. Polling was not just a lazy substitute for interrupts

The Pocket386 CH375 setup we worked with did not provide a useful controller IRQ path that CH375USB could simply depend on.

Polling therefore became a design constraint.

This has consequences:

- long waits inside an interrupt handler are dangerous;
- HID endpoints normally NAK when idle;
- re-enumeration cannot safely happen in every context;
- DOS applications call mouse services at very different rates;
- storage and HID must cooperate without blocking each other for long periods.

The final design uses several different polling contexts rather than one giant polling loop.

### Timer (`INT 1Ch`)

Used for lightweight periodic work:

- inspect root connect/disconnect state;
- poll active HID endpoints;
- maintain retry/backoff counters;
- mark heavier maintenance as pending.

The timer handler must stay short.

### DOS idle (`INT 28h`)

Used for deferred work that should not be done inside the timer ISR:

- hotplug maintenance;
- re-enumeration;
- hub scanning;
- storage readiness checks.

### Mouse API calls (`INT 33h`)

Used for extra on-demand mouse polling because applications such as DOS EDIT query the mouse much more frequently than the BIOS timer rate.

### General lesson

Polling is not one mechanism. On DOS, *where* you poll is as important as *how often* you poll.

---

## 5. Never perform heavy USB enumeration from DOS block-device requests

One of the important stability rules that emerged was:

> A DOS block-device request must not unexpectedly trigger full USB enumeration.

If DOS asks whether removable media is present, the driver should answer based on current state.

It should not suddenly:

- reset USB;
- enumerate a device;
- wait seconds for controller responses;
- probe hubs;
- rebuild the topology.

Those operations belong in deferred maintenance.

The final driver therefore keeps a reserved removable drive and returns a quick "drive not ready" state when no media is available.

### Why this matters

DOS may issue filesystem or media checks at inconvenient times. Turning a routine media check into a hardware discovery operation can produce stalls, reentrancy problems and very confusing behaviour.

---

## 6. Reserving the DOS drive letter simplifies hotplug

CH375USB reserves one DOS removable block-device unit even when no USB storage is attached.

When no media exists, requests return drive-not-ready.

When storage later appears, the same DOS device becomes usable.

This is much cleaner than trying to dynamically create and destroy DOS block devices every time a flash drive is connected.

### Practical consequence

The drive letter exists before the stick does.

That may look slightly unusual to a user, but it makes DOS hotplug practical.

---

## 7. CH375 native storage is extremely useful

For a root USB flash drive, the CH375's native disk commands turned out to be a valuable path:

```text
DISK_INIT
DISK_SIZE
DISK_READY
DISK_READ
DISK_RD_GO
DISK_WRITE
DISK_WR_GO
DISK_SENSE
```

The current architecture tries native root mass storage before falling back to generic USB enumeration.

This is deliberate.

The CH375 can perform a substantial amount of the mass-storage transaction itself. On a 386, using that facility avoids implementing every operation through generic USB BOT when it is not necessary.

### Current strategy

For a root device:

```text
CH375 native storage
        |
        +-- success -> STORAGE_NATIVE
        |
        +-- failure -> generic USB enumeration
                         |
                         +-- HID
                         +-- hub
                         +-- USB MSC BOT/SCSI
```

The generic BOT/SCSI implementation is still valuable, especially for devices reached through a hub or where native handling is not appropriate.

---

## 8. Generic USB Mass Storage still matters

The fallback path implements the standard USB Mass Storage Bulk-Only Transport model with SCSI commands including:

```text
TEST UNIT READY
READ CAPACITY(10)
READ(10)
WRITE(10)
SYNCHRONIZE CACHE
```

Important BOT lessons:

- maintain IN and OUT data toggles;
- validate the CSW signature;
- validate the CSW tag against the CBW tag;
- perform bulk-only reset recovery when a transaction fails;
- clear endpoint halts during recovery;
- reset endpoint toggles after recovery.

Ignoring recovery works until the first error. Then the device often appears permanently dead even though the USB connection itself is fine.

---

## 9. Storage media can fool you into debugging the wrong subsystem

A major practical lesson from testing was that a USB flash drive itself can be the problem.

At one stage controller timing and driver delay settings looked guilty, but changing the flash drive changed the behaviour. An older small-capacity drive was useful for comparison.

### Rule for retro USB debugging

Before rewriting low-level timing code:

- test at least two different flash drives;
- preferably include an older, smaller USB 2.0-era device;
- cold boot with the device inserted;
- test insert after boot;
- test unplug/replug;
- test both reads and writes.

A marginal or unusual flash controller can make a working host implementation look broken.

Do not overfit the driver to one USB stick.

---

## 10. Bounded waits are mandatory

A controller wait that can run forever is unacceptable in a resident DOS driver.

The driver uses bounded waits for different contexts:

- control transfers;
- normal transfers;
- native disk operations;
- connect detection;
- HID polling.

The timeouts are intentionally context-specific.

A control transfer may need to tolerate connection-status events while waiting for its real completion status. An HID poll, on the other hand, must return quickly because a NAK from an idle mouse or keyboard is completely normal.

### Important observation

A CH375 status event is not automatically the final answer to the operation you care about.

Connection and disconnection events may need to be consumed/recorded while the code continues waiting for the transfer status.

---

## 11. USB reset must always be released

For this CH375 host mode sequence, mode `7` asserts a USB bus reset and mode `6` returns the controller to normal host operation.

A reset path must therefore guarantee:

```text
mode 7 -> reset
mode 6 -> release reset / resume host operation
```

Even error paths must return the controller to mode 6.

Leaving the controller in reset creates a wonderfully convincing imitation of dead hardware.

---

## 12. Full-speed and low-speed enumeration may require different treatment

Generic root enumeration eventually gained separate attempts rather than assuming every HID device behaves identically.

The implementation tries the normal path and also contains a legacy CH375B low-speed sequence derived from known CH375 host behaviour.

This is especially relevant to old boot-protocol HID devices.

### Lesson

Do not assume that "USB 1.x" means every device can be enumerated with exactly the same host-state sequence.

A keyboard or mouse may be low-speed while a flash drive is full-speed.

---

# HID KEYBOARD

## 13. Boot-protocol HID is the correct first target for DOS

For keyboards, supporting HID boot protocol is much more realistic than trying to implement arbitrary HID report descriptors immediately.

After enumeration, CH375USB sets:

- boot protocol;
- idle rate;
- interrupt IN polling.

A boot keyboard report gives modifiers plus six key usages.

That is enough for conventional DOS keyboard operation.

---

## 14. USB HID usages are not PC/AT scan codes

USB keyboards speak HID usage codes.

DOS software expects the PC keyboard world.

CH375USB therefore translates HID usages into Set 1 scan codes and injects those bytes into the PC keyboard path.

Extended keys require the `E0h` prefix.

Modifier handling has to include make and break transitions independently from the six ordinary keys in the boot report.

### Lesson

"Keyboard packet received" is only half the job.

A DOS keyboard bridge must correctly reproduce:

- make;
- break;
- modifiers;
- extended prefixes;
- key state transitions.

If releases are lost, DOS thinks keys are still held.

---

## 15. Injecting keyboard bytes through the 8042-compatible path worked

The implementation uses the keyboard-controller command that writes an output-buffer byte and then supplies the scan code.

Conceptually:

```text
wait until controller input buffer is empty
send 8042 command D2h
wait again
write scan byte to port 60h
```

This lets USB keyboard events enter the conventional PC keyboard path and be seen by DOS software.

### Lesson

For DOS compatibility, emulating the interface software already understands is often more useful than inventing a parallel API.

---

# HID MOUSE

## 16. Mouse support was much harder than keyboard support

Receiving a three-byte boot mouse report is easy:

```text
buttons
delta X
delta Y
```

Making that behave like a real DOS mouse in arbitrary programs is not.

The difficult part is not USB. The difficult part is the DOS mouse API and the lifetime of the existing mouse driver.

---

## 17. CTMOUSE/MOUSE.COM is loaded after CONFIG.SYS

CH375USB is normally a `DEVICE=` driver loaded from `CONFIG.SYS`.

A conventional DOS mouse driver such as CuteMouse is normally loaded later from `AUTOEXEC.BAT`.

Therefore CH375USB cannot simply capture `INT 33h` once during its own initialization and assume that vector is the final DOS mouse driver.

At CH375USB initialization time, CTMOUSE may not exist yet.

### Solution

Record the boot-time `INT 33h` vector.

Later, during DOS idle processing, inspect the vector again.

When it changes because CTMOUSE/MOUSE.COM has installed itself, chain CH375USB's `INT 33h` shim in front of that real mouse driver.

### General DOS lesson

Resident software loaded in `CONFIG.SYS` must remember that many TSR services do not exist yet.

Initialization order is architecture.

---

## 18. Do not replace the conventional mouse driver if you can bridge it

The successful design lets the existing DOS mouse driver retain responsibility for conventional cursor behaviour.

CH375USB supplies USB-derived:

- position;
- buttons;
- motion counters ("mickeys");
- press/release counters;
- callbacks.

The existing driver remains part of the chain.

This is much more compatible than trying to impersonate every mouse-driver behaviour from scratch.

---

## 19. BIOS timer polling alone is too slow for a responsive DOS mouse

The BIOS timer rate is about 18.2 Hz.

That is fine for maintenance.

It feels terrible as the sole sampling path for a mouse.

DOS EDIT made this obvious: once polling was added directly from the `INT 33h` calls used by applications, mouse movement became dramatically more responsive.

### Final pattern

Mouse reports can be obtained from:

- periodic timer polling;
- on-demand polling when an application calls relevant `INT 33h` services.

### Lesson

Match polling frequency to the consumer.

A GUI-like DOS application may query mouse state hundreds of times between BIOS ticks.

---

## 20. Movement working does not mean the mouse implementation is finished

During development we reached a very revealing state:

> Mouse movement in DOS EDIT was responsive, but mouse buttons still did not work.

This was useful because it separated two problems that initially looked like one.

Movement can be represented as accumulated deltas.

Clicks are *edges*.

A short press and release can happen between two application polls.

If the driver only stores the current button state, the application may observe:

```text
not pressed
not pressed
```

and the click has vanished.

### Correct approach

Latch button transitions when HID reports arrive:

- left press;
- left release;
- right press;
- right release;
- middle press;
- middle release.

Maintain counters and positions for press/release events.

Do not derive all click behaviour only from the latest button bitmap.

This is a general event-system lesson, not just a DOS mouse lesson.

---

## 21. Burst draining helps with button events

When a mouse endpoint has multiple reports queued, reading only one report can leave the system behind.

The mouse poll therefore attempts a small bounded burst of reports.

This improves the chance of consuming both movement and short button transitions without turning the timer ISR into an unbounded loop.

Again, bounded work is important.

---

## 22. Direct PS/2 / IRQ12 emulation was intentionally avoided

A tempting design is to make the USB mouse look like hardware arriving through the PS/2 auxiliary device and IRQ12.

That path is much more invasive.

CH375USB retained the DOS `INT 33h` bridge instead of depending on an artificial IRQ12 mouse path.

### Lesson

When extending a retro system, use the highest compatibility layer that is sufficient.

If DOS applications already agree on `INT 33h`, modifying emulated hardware below that level adds risk without necessarily adding compatibility.

---

# HOTPLUG

## 23. Hotplug needs a state machine, not just "device present?"

A connect/disconnect event can bounce through several states while USB resets and enumeration happen.

The driver keeps explicit state such as:

- current root-link state;
- pending root event;
- maintenance pending;
- retry/backoff ticks;
- storage slot;
- storage mode;
- media-ready flag;
- media-changed flag.

This prevents every status sample from immediately launching another full enumeration.

### Lesson

Hotplug is asynchronous state management.

Treating it as a single boolean produces re-enumeration storms.

---

## 24. Separate fast detach from slow discovery

Disconnect handling should be immediate enough that DOS stops using stale storage or HID state.

Discovery is slower.

The useful split is:

### Fast path

- mark devices detached;
- invalidate storage;
- mark media changed;
- schedule maintenance.

### Deferred path

- reset controller as needed;
- classify root device;
- try native storage;
- otherwise perform generic enumeration;
- scan hub ports;
- claim storage again.

This keeps interrupts short and gives DOS a consistent view of removal.

---

## 25. Retry/backoff prevents hotplug thrashing

After connect/disconnect, retry counters intentionally delay repeated attempts.

Without this, a device that is physically settling can cause:

```text
connect
enumerate
fail
enumerate
fail
enumerate
...
```

on every timer tick.

Old PCs are slow enough that this can become catastrophic.

A small stateful backoff is far better than heroic retry loops.

---

# WINDOWS 95

## 26. DOS support and Windows 95 support are different problems

This was one of the most important conceptual corrections in the project.

A real-mode DOS USB keyboard or mouse implementation does **not** automatically become a Windows 95 USB keyboard or mouse implementation.

Windows 95 enhanced mode uses protected-mode virtual-device infrastructure for these input paths.

For proper Windows GUI input, a separate protected-mode component would be required, conceptually involving the Windows keyboard and virtual-mouse services.

### Current reality

```text
DOS:
  USB storage   YES
  USB keyboard  YES
  USB mouse     YES
  hotplug       YES

Windows 95 GUI:
  USB storage   usable through the documented pre-Windows DOS method
  USB keyboard  NO
  USB mouse     NO
  reliable USB hotplug NO
```

Trying to "make the DOS mouse keep working inside Windows" is the wrong architectural goal.

The DOS HID bridge should get out of the way when Windows takes over.

---

## 27. A stale INT 33h callback can poison the DOS -> Windows transition

This produced one of the most valuable bugs in the project.

The observed symptom was:

- boot DOS;
- use/move the USB mouse in DOS;
- start Windows;
- Windows sometimes fails to start or reaches a blue screen.

If the mouse had not been used first, Windows was much more likely to start normally.

The plausible cause was a DOS application's mouse callback registered through `INT 33h`.

CH375USB retained that callback address after the DOS program had terminated.

That address could then point into memory that no longer contained the original program.

A later mouse event could effectively call dead code.

### General TSR lesson

Never assume a callback pointer registered by an application remains valid after that application is gone.

This is especially dangerous for resident drivers because *the driver outlives the caller*.

---

## 28. Windows announces the transition: use it

The 0.4.11 design handles the documented Windows enhanced-mode notifications through multiplex interrupt `INT 2Fh`:

```text
AX=1605h  Windows enhanced-mode initialization
AX=1606h  Windows enhanced-mode exit
```

On Windows entry CH375USB:

- marks Windows active;
- clears pending mouse state;
- clears user mouse callback state;
- removes its `INT 33h` shim if it is currently at the top of the chain;
- stops the DOS HID bridge from operating while Windows owns input.

On return from Windows it resets the relevant DOS-side state so the bridge can be established again safely.

### Lesson

OS transition notifications exist for a reason.

A DOS TSR that hooks global interrupts must explicitly define what happens when Windows enhanced mode starts.

---

## 29. INT 33h reset must reset callback state too

Mouse reset is not just cursor position and button state.

The registered user callback is part of the mouse driver's runtime state.

Leaving a previous callback alive across reset is an invitation to call stale application memory.

The reset path therefore clears callback mask, offset and segment.

---

## 30. Loading the storage driver from CONFIG.SYS can slow Windows 95

A real-mode block driver loaded conventionally in `CONFIG.SYS` stays resident as Windows starts.

On the Pocket386, that can noticeably affect Windows startup/performance.

A useful workaround was discovered using FreeDOS `DEVLOAD.COM`.

### Windows boot profile

Do not load CH375USB in the Windows `CONFIG.SYS` section.

Instead, immediately before `WIN`:

```dos
C:\DEVLOAD\DEVLOAD.COM C:\CH375USB.SYS
WIN
```

This allows the USB storage drive already discovered in DOS to remain usable when Windows starts, without loading CH375USB at the earliest CONFIG.SYS stage of that boot profile.

### Important limitation

This is not native Windows USB hotplug.

Treat it as:

> detect the storage device in DOS first, then carry that DOS block-device access into Windows.

The flash drive should already be inserted and visible before `WIN`.

Do not advertise arbitrary plug/unplug after the Windows GUI is running as supported behaviour.

---

# BIOS AND ISA TIMING

## 31. The development machine is not using conservative BIOS timings

The final documented test machine uses performance-oriented Pocket386 BIOS settings:

```text
AT Bus Clock:          PCLK2/8
I/O Recovery:          Disabled
I/O Recovery Period:   0.75 µs
16Bit ISA Insert Wait: Disabled
Slow Refresh:          120 µs
```

These are *test conditions*, not CH375USB requirements.

They may be unstable on another Pocket386 or another ISA peripheral combination.

### Correct interpretation

A successful test under aggressive settings is useful evidence that the driver does not require extremely conservative timing on that machine.

It does **not** mean everyone should copy those BIOS values.

When troubleshooting:

1. use normal/default BIOS settings first;
2. verify USB operation;
3. only then experiment with performance-oriented timing.

---

## 32. Do not confuse BIOS timing problems with USB-media compatibility

We repeatedly had reasons to suspect timing.

Sometimes timing really matters on ISA.

But one troublesome flash drive can create almost the same symptom pattern.

When debugging, change one variable at a time:

```text
same BIOS + different stick
different BIOS + same stick
cold boot vs hotplug
read-only vs write
```

Without that matrix, it is easy to "fix" the wrong thing.

---

# DOS / DRIVER ENGINEERING LESSONS

## 33. Preserve interrupt-chain ownership carefully

For every hooked interrupt:

- store the previous vector;
- chain correctly;
- only remove your hook if the vector still points to your handler;
- never blindly restore a vector if another TSR has installed itself after you.

The Windows-transition code follows this rule for `INT 33h`.

This is essential TSR etiquette.

---

## 34. Installation order is part of compatibility

Relevant order on the working DOS setup is conceptually:

```text
CONFIG.SYS:
    CH375USB.SYS

AUTOEXEC.BAT:
    CTMOUSE.EXE
```

CH375USB must therefore be able to discover and chain CTMOUSE later.

A driver architecture that assumes all cooperating TSRs already exist during `DEVICE=` initialization will fail in normal DOS boot order.

---

## 35. Be suspicious of work done inside an ISR

A 386 is slow enough that code which looks tiny on a modern machine may be expensive.

Inside timer/interrupt paths:

- do not enumerate devices;
- do not wait through long USB retry loops;
- do not perform filesystem probing;
- do not spin indefinitely on NAK;
- do not scan an entire topology unnecessarily.

Set a flag and let the DOS idle hook do the heavy work.

---

## 36. Keep diagnostic stage markers

The source keeps stage values such as enumeration/native-failure stages.

This is extremely useful on hardware where you do not have:

- a kernel debugger;
- USB protocol tracing;
- a convenient serial debug console;
- modern crash dumps.

A one-byte "last stage reached" value can tell you whether failure happened during:

```text
reset
descriptor
address assignment
configuration descriptor
configuration set
native disk init
capacity
ready
mount
```

### Lesson

On retro bare-metal work, crude diagnostics are often better than elegant diagnostics you cannot actually observe.

---

## 37. Keep the known-good path while adding features

The driver evolved from storage into HID and hotplug.

An important discipline is to avoid replacing a working storage path merely because a generic architecture looks cleaner.

That is why CH375 native root storage remains available while generic USB MSC exists as a fallback.

Retro hardware rewards boring working code.

---

# TESTING PRACTICES THAT PAID OFF

## 38. Test cold boot and hotplug separately

These are different paths.

A device that works when present during boot may still expose bugs when:

- attached after boot;
- removed while idle;
- removed after file access;
- reattached;
- replaced by a different class of USB device.

Every class should be tested in both conditions.

---

## 39. Test the mouse in a real DOS application

A synthetic "mouse packet received" test is not enough.

DOS EDIT was valuable because it exercised:

- frequent `INT 33h` position queries;
- visible cursor motion;
- actual clicks;
- callbacks / mouse-driver behaviour.

The application exposed problems that raw HID logging would not.

---

## 40. Specifically test DOS -> Windows after using the mouse

This deserves its own regression test.

The critical sequence is:

```text
boot DOS
load/use mouse
move it repeatedly
click buttons
run a DOS mouse-aware program
exit that program
start Windows 95
```

Windows starting successfully only when the mouse was *not* touched is a strong signal of leaked DOS-side mouse state.

The 0.4.11 Windows-transition safeguards were created because of this exact class of failure.

---

## 41. Test short clicks, not only held buttons

For mouse buttons:

- press and hold;
- press/release quickly;
- double click;
- move while clicking;
- click without moving.

Fast press/release is the test that proves whether the driver preserves edges instead of only storing current state.

---

## 42. Test write behaviour explicitly

A drive that reads correctly is not proof that writes are safe.

Useful checks include:

- create a file;
- copy a file;
- compare contents;
- delete it;
- remount/reboot and verify filesystem integrity;
- test writes crossing sector boundaries at the DOS filesystem level.

The driver optionally verifies written sectors and issues cache synchronization on the BOT path.

Retro removable media corruption is not amusing after the first time.

---

# EXTERNAL USB HUBS

## 43. Hub support exists but should still be considered experimental

The source contains support for one external USB hub with a limited number of downstream ports.

It includes:

- hub descriptor retrieval;
- port power;
- port status;
- reset;
- connection-change acknowledgement;
- attach/detach handling.

However, this path has not received the same real-hardware validation as root storage, keyboard and mouse.

### Do not confuse "implemented" with "tested"

For public documentation, keep that distinction explicit.

---

# WHAT NOT TO ASSUME

## 44. Do not assume a modern USB device will be easier

Newer flash drives and composite HID devices may be less friendly to a minimal USB 1.x host implementation than older simple devices.

For bring-up, boring old hardware is often ideal.

---

## 45. Do not assume every HID device is boot-protocol compatible

CH375USB currently targets boot keyboard and boot mouse behaviour.

An arbitrary gaming keyboard, touchpad, multimedia device or composite HID peripheral may require report-descriptor parsing that the current driver does not implement.

---

## 46. Do not assume Windows will inherit DOS input services

Windows 95 does not simply keep calling the DOS mouse and keyboard interfaces as if it were a DOS application.

Protected-mode input support is a separate project.

A future Windows companion would need to integrate with Windows 95's protected-mode keyboard/mouse architecture rather than extending the DOS shim deeper into Windows.

---

## 47. Do not assume a timeout means "device absent"

A timeout can mean:

- wrong controller state;
- endpoint NAK;
- incorrect toggle;
- failed reset release;
- media not ready;
- a transfer still settling;
- unsuitable USB device;
- overly aggressive hardware timing.

Keep diagnostic context around the timeout.

---

# RECOMMENDED DEBUGGING ORDER

When a new Pocket386/CH375 setup does not work, debug in this order.

## Step 1 — controller

Verify:

```text
I/O base
CHECK_EXIST response
command/data port mapping
```

## Step 2 — conservative hardware environment

Use default/conservative BIOS timings.

Remove unrelated variables.

Use a simple known-good USB device.

## Step 3 — root connection

Confirm connect/disconnect status is observed.

Confirm reset exits back to host mode.

## Step 4 — one class only

For storage, try root native storage first.

For HID, test one plain keyboard or plain mouse.

Avoid a hub initially.

## Step 5 — enumeration stages

Determine exactly where enumeration fails.

Do not change five delays at once.

## Step 6 — DOS integration

Only after raw USB operation works, test:

- DOS block-device requests;
- keyboard scan-code injection;
- CTMOUSE + INT 33h mouse bridge.

## Step 7 — hotplug

Then test:

- attach;
- detach;
- reattach;
- different device.

## Step 8 — Windows transition

Finally test Windows 95 startup after actively using DOS HID.

---

# CURRENT KNOWN-GOOD FUNCTIONAL SUMMARY

As of CH375USB 0.4.11 on the tested Pocket386:

| Function | DOS | DOS hotplug | Windows 95 GUI |
|---|---:|---:|---:|
| USB flash storage read | Yes | Yes | Yes via pre-Windows DOS driver method |
| USB flash storage write | Yes | Yes | Yes via pre-Windows DOS driver method |
| USB keyboard | Yes | Yes | No |
| USB mouse movement | Yes | Yes | No |
| USB mouse buttons | Yes | Yes | No |
| External hub | Experimental | Experimental | Not supported |
| Windows USB hotplug | — | — | Not supported/reliable |

For Windows storage, the USB drive should be detected under DOS before starting Windows.

---

# FUTURE WORK

## 48. Windows HID needs its own protected-mode companion

A sensible future architecture is:

```text
DOS:
    CH375USB.SYS
        storage
        USB keyboard -> DOS keyboard path
        USB mouse    -> INT 33h bridge

Windows 95:
    protected-mode companion/VxD
        USB keyboard -> Windows keyboard services
        USB mouse    -> Windows virtual mouse services
```

The 0.4.11 DOS driver already contains the important opposite behaviour: it knows when to suspend its DOS HID bridge as Windows enters enhanced mode.

That separation should be preserved.

---

## 49. Hub testing should be systematic

When hub testing begins, use:

1. hub alone;
2. keyboard through hub;
3. mouse through hub;
4. storage through hub;
5. two HID devices;
6. HID + storage;
7. unplug one downstream device;
8. unplug the hub itself.

Record low-speed/full-speed behaviour separately.

---

# FINAL TAKEAWAYS

The most valuable lessons from this project were not individual CH375 command numbers.

They were engineering lessons:

1. **Keep interrupt handlers short.**
2. **Defer expensive hotplug work.**
3. **A DOS `DEVICE=` driver must respect TSR installation order.**
4. **Mouse movement and mouse clicks are different classes of state.**
5. **Latch edges; do not only sample current button state.**
6. **Never retain application callbacks indefinitely in resident code.**
7. **Use Windows transition notifications to shut down DOS-only hooks safely.**
8. **DOS input support is not Windows protected-mode input support.**
9. **Try multiple USB devices before blaming timing code.**
10. **Keep a working hardware-specific path even when adding a cleaner generic path.**
11. **Bound every hardware wait.**
12. **A USB reset path must always release reset.**
13. **Use state machines and retry backoff for hotplug.**
14. **Test the real application behaviour, not only packet-level success.**
15. **Document what was actually tested separately from what merely exists in the source.**

CH375USB reached working DOS storage, keyboard, mouse and hotplug not because the CH375 suddenly became simple, but because each layer was gradually isolated: controller, USB transport, DOS interfaces, hotplug state and finally the DOS-to-Windows boundary.

That separation is probably the most reusable knowledge in the whole experiment.
