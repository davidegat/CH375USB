# CH375USB Project Notice

Copyright (C) 2026 **Davide "gat"**  
https://github.com/davidegat

CH375USB is an independent, unofficial open-source project and is not affiliated with, sponsored by, or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH).

The names **WCH**, **WinChipHead**, and **CH375** are used solely to identify hardware, documentation, and historical software with which this project interoperates.

CH375USB is distributed under the GNU General Public License version 3 or, at your option, any later version. See [`../LICENSE`](../LICENSE).

## Source-code provenance

Most CH375USB source is independently written around public CH375, USB, DOS, BIOS and Windows interfaces. The repository does not contain proprietary WCH driver source code or FreddyV's `CH375286.SYS` binary/source.

### Internal mouse driver and CuteMouse

CH375USB 0.5.1 incorporates an internal DOS `INT 33h` mouse driver. The implementation in `internal_mouse.inc` was developed with direct study/adaptation of relevant behavior and code concepts from **CuteMouse / CTMouse 2.1 beta4**, principally its BIOS PS/2 callback handling, packet decoding, conventional `INT 33h` semantics, button/motion bookkeeping and compatibility defaults.

Reference examined during development:

- CuteMouse / CTMouse 2.1 beta4
- Copyright (c) 1997-2002 Nagy Daniel
- BIOS wheel-mouse work credited in CuteMouse to Eric Auer / Konstantin Koll
- source repository examined: `https://github.com/davidebreso/ctmouse`
- principal source: `ctmouse.asm`
- license: **GPL-2.0-or-later**

CuteMouse's GPL-2.0-or-later terms permit the adapted/combined work to be distributed under CH375USB's GPL-3.0-or-later license. Relevant copyright/license notices are preserved here and in the source comments.

CH375USB does **not** embed the CuteMouse TSR loader, command-line parser, serial/UART backend, relocation machinery or its `asmlib` framework. CuteMouse is **not a runtime dependency** of CH375USB 0.5.1 and is not bundled as `CTMOUSE.EXE`.

### CH375 / USB references

Implementation work has been informed by public material including:

- WCH CH375 Datasheet (I), Version 4;
- WCH CH375 Datasheet (II): USB Basic Transmission Commands, Version 4;
- historical W.ch/WinChipHead CH375 USB-host examples;
- FreddyV's CH375286 work as a practical interoperability reference;
- open-source CH375 host examples including xeecos/CH375-Keyboard-Arduino and Makeblock/MeUSBHost;
- USB 2.0, HID 1.11/HID Usage Tables, USB Mass Storage Bulk-Only Transport and SCSI block-command conventions;
- public DOS `INT 33h`, BIOS `INT 15h/AH=C2h`, Windows enhanced-mode and Win16 mouse-driver interface documentation.

One legacy CH375B low-speed setup procedure using the command/data sequence `0Bh -> 17h -> D8h` was learned from W.ch/WinChipHead-derived examples including Makeblock material; CH375USB implements the procedure independently in NASM.

### Other DOS USB references

- crazii / USBDDOS — architectural/reference material for DOS USB/HID/storage design.
- Bret Johnson / USBMOUSE — historical/practical reference for DOS USB mouse behavior and nonblocking/staged ideas. CH375USB does not redistribute USBMOUSE source.

### Toolchains and external utilities

Open Watcom V2 and NASM are build tools, not code dependencies. FreeDOS `DEVLOAD` is an optional external utility useful for the documented Windows boot-profile workflow and is not part of CH375USB.

### Pocket386 community references

Pocket386 behavior has also been informed by real-hardware testing and public community reports. Third-party modified BIOS images and CMOS save/restore utilities are not part of CH375USB and are not endorsed or supported by the project.

### AI-assisted development and review

AI tools were used during iterative design, code generation/review, debugging hypotheses and documentation. Working behavior is established by source review, public specifications/interfaces and repeated real-hardware testing rather than by AI output alone.

## Binary distribution

Published `binary/CH375USB.SYS`, `binary/CH375MOU.DRV` and `binary/MTEST.COM` are build outputs of this project. They must correspond to the same source revision as the release and must not be confused with vendor or third-party drivers.

## Independence statement

CH375USB is an interoperable open-source implementation informed by public hardware/protocol documentation, public DOS/BIOS/Windows interfaces, prior community work and real-hardware experimentation. Third-party source-derived portions are identified above with their applicable licensing/provenance.
