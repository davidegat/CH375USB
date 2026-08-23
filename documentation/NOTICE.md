# CH375USB Project Notice

Copyright (C) 2026 **Davide "gat"**  
https://github.com/davidegat

CH375USB is an independent, unofficial open-source project and is not affiliated with, sponsored by, or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH).

The names **WCH**, **WinChipHead**, **CH375**, Microsoft, Windows, FreeDOS, CuteMouse and the names of other third-party projects are used only to identify hardware, software, documentation and interoperability references.

CH375USB is distributed under the **GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`)**. See [`../LICENSE`](../LICENSE).

## Source-code provenance summary

Most CH375USB source is independently written around public CH375, USB, SCSI, DOS, BIOS and Windows interfaces.

The important exception is the internal DOS mouse core in `internal_mouse.inc`: its architecture and API coverage were developed through direct study/adaptation of **CuteMouse / CTMouse 2.1 beta4**. That relationship, the relevant copyright and the GPL terms are documented explicitly below and in the source comments.

The repository does **not** contain proprietary WCH driver source code, the vendor `CH375DOS.SYS`, FreddyV's `CH375286.SYS` binary/source, Bret Johnson's USBMOUSE source, or other third-party driver binaries as project source.

Standard USB constants, CH375 command values, HID usages, SCSI command values, DOS/BIOS interrupt numbers, Windows/Win16 ABI structures and similar protocol/interface facts necessarily match the public interfaces they implement.

## Internal mouse driver and CuteMouse / CTMouse

CH375USB 0.5.1 replaces the previous external DOS mouse-driver dependency with its own resident `INT 33h` implementation in `internal_mouse.inc`.

The module was developed with direct study/adaptation of relevant behavior and code concepts from **CuteMouse / CTMouse 2.1 beta4**, principally:

- BIOS PS/2 setup and callback handling;
- conventional Microsoft-compatible `INT 33h` lifecycle and API semantics;
- mouse packet/button/motion bookkeeping;
- position ranges, motion ratios and compatibility defaults;
- cursor-show/hide and text-mode cursor behavior;
- reset/re-enable expectations used by DOS applications.

The resulting code is a NASM adaptation integrated into CH375USB's resident `.SYS` architecture and CH375 USB transport. It is not a bundled CuteMouse executable and CuteMouse is **not a runtime dependency** of CH375USB 0.5.1.

CuteMouse material examined during development:

- **CuteMouse / CTMouse 2.1 beta4** — Copyright (c) 1997-2002 Nagy Daniel.
- BIOS wheel-mouse portions in CuteMouse credit **Konstantin Koll** and **Eric Auer**.
- Main source mirror examined: https://github.com/davidebreso/ctmouse
- Principal source file: https://github.com/davidebreso/ctmouse/blob/main/ctmouse.asm
- Original project site: https://cutemouse.sourceforge.net/
- FreeDOS mouse repository: https://github.com/FDOS/mouse

The `ctmouse.asm` license header permits redistribution/modification under **GPL version 2 or, at the user's option, any later version**. CH375USB therefore distributes the adapted/combined work under `GPL-3.0-or-later`. Relevant attribution and licensing notices are preserved here and in `internal_mouse.inc`.

CH375USB does **not** incorporate CuteMouse's TSR loader, command-line parser, serial/UART backend, relocation machinery or `asmlib` framework.

## WCH / WinChipHead CH375 documentation

The CH375 hardware and low-level command implementation was developed and audited against public WCH documentation, especially:

- **WCH CH375 Datasheet (I), Version 4 (`CH375DS1.PDF`)** — CH375 host/device interface, parallel I/O, host modes, status/interrupt behavior and native mass-storage operations.  
  https://www.wch-ic.com/downloads/CH375DS1_PDF.html
- **WCH CH375 Datasheet (II): USB Basic Transmission Commands, Version 4 (`CH375DS2.PDF`)** — external-firmware/basic USB transaction commands, retry/token/data-buffer behavior and related host operations.  
  https://www.wch-ic.com/downloads/CH375DS2_PDF.html

Historical **W.ch / WinChipHead CH375 USB-host examples** were also used as behavioral references for legacy command sequencing. Historical WCH DOS drivers were treated only as interoperability/behavioral references; no proprietary vendor driver source or binary is incorporated or redistributed.

### Legacy CH375B low-speed sequence

One externally learned hardware procedure is explicitly attributed: the legacy CH375B low-speed setup sequence using command/data values `0Bh -> 17h -> D8h` was learned from W.ch/WinChipHead-derived CH375 examples, including Makeblock material. CH375USB implements the sequence independently in NASM.

## FreddyV — CH375286 / CH375 USB driver work

FreddyV's CH375 driver work was an important practical reference for CH375 ISA operation on older x86 systems, especially around I/O-port selection, timing, performance, flash-drive compatibility and real-hardware behavior.

References:

- FreddyVRetro / CH375 USB driver repository:  
  https://github.com/FreddyVRetro/CH375-USB-DRIVER
- Long-running VOGONS **USB ISA cards?** development/testing thread, including FreddyV's CH375286 work:  
  https://www.vogons.org/viewtopic.php?t=43311

CH375USB does not contain FreddyV's `CH375286.SYS` binary/source and was not produced by translating that driver.

## Open-source CH375 host examples

The following open-source CH375 projects were consulted as practical protocol/sequencing references:

- **xeecos / CH375-Keyboard-Arduino** — CH375 HID-host and command reference:  
  https://github.com/xeecos/CH375-Keyboard-Arduino
- **Makeblock / MeUSBHost** in Makeblock-Libraries — CH375 host sequencing and a W.ch-derived low-speed setup reference:  
  https://github.com/Makeblock-official/Makeblock-Libraries  
  https://github.com/Makeblock-official/Makeblock-Libraries/blob/master/src/MeUSBHost.h

No source block from these projects is redistributed as part of CH375USB.

## Other DOS USB and mouse implementations

### crazii / USBDDOS

USBDDOS was consulted as an independent DOS USB stack and as architectural background for DOS USB/HID/storage design questions.

- Repository: https://github.com/crazii/USBDDOS

CH375USB uses a different CH375-specific host architecture and does not incorporate USBDDOS source code.

### Bret Johnson / USBDOS / USBMOUSE

Bret Johnson's DOS USB work was consulted as historical/practical background for DOS USB mouse behavior, BIOS/PS2-facing interoperability, diagnostics and nonblocking/staged processing ideas.

References:

- Bret Johnson's DOS USB software site: https://bretjohnson.us/
- FreeDOS USBDOS documentation: https://help.fdos.org/en/hhstndrd/util/usbdos.htm
- FreeDOS USBMOUSE documentation: https://help.fdos.org/en/hhstndrd/util/usbmouse.htm

CH375USB does not redistribute USBMOUSE/USBDOS source or binaries, and its CH375 transport and current physical-PS/2 architecture are separate implementations.

## USB, HID, mass-storage and SCSI specifications

Implementation work was informed by public protocol specifications rather than third-party driver source.

Principal references include:

- **USB 2.0 Specification — USB Implementers Forum**:  
  https://www.usb.org/document-library/usb-20-specification
- **Device Class Definition for Human Interface Devices (HID), Version 1.11**:  
  https://www.usb.org/sites/default/files/documents/hid1_11.pdf
- **HID Usage Tables — USB-IF document library**:  
  https://www.usb.org/documents?search=HID+usage+tables
- **USB Mass Storage Class — Bulk-Only Transport, Revision 1.0**:  
  https://www.usb.org/sites/default/files/usbmassbulk_10.pdf
- **SCSI command-set conventions** used for operations including TEST UNIT READY, REQUEST SENSE, READ CAPACITY(10), READ(10), WRITE(10) and SYNCHRONIZE CACHE; standards work is maintained through INCITS/T10:  
  https://www.t10.org/

These are protocol specifications/interface definitions, not source-code dependencies.

## DOS, BIOS and Windows interface references

Public interface documentation was consulted for:

- DOS `INT 33h` mouse services;
- BIOS PS/2 pointing-device services through `INT 15h AH=C2h`;
- DOS installable block-device request packets and Strategy/Interrupt entry points;
- Windows enhanced-mode notifications through `INT 2Fh`;
- the Win16 mouse-driver ABI used by `CH375MOU.DRV`;
- the Windows NE DLL/driver format;
- historical Windows 95 VMOUSE/VKD/VxD behavior while defining the boundary between the DOS resident driver and Windows.

A major public reference for PC BIOS/DOS interrupt behavior was **Ralf Brown's Interrupt List (RBIL)**:

- https://www.cs.cmu.edu/~ralf/files.html

These interface descriptions were used to build interoperable software; they are not copied proprietary implementations.

## Toolchains and external utilities

The following tools are used by the project or documented workflow but are **not incorporated runtime code dependencies** of CH375USB:

- **NASM** — assembler for `CH375USB.SYS` and `MTEST.COM`:  
  https://www.nasm.us/
- **Open Watcom V2** — WASM/WLINK toolchain used for the Win16 `CH375MOU.DRV`:  
  https://github.com/open-watcom/open-watcom-v2
- **FreeDOS DEVLOAD** — optional external utility used by the documented DOS-to-Windows boot profile to load `CH375USB.SYS` immediately before `WIN`:  
  https://github.com/FDOS/devload  
  https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.4/html/en/base/devload/20250409.8/index.html

The documented host-side USB-storage preparation also uses standard external filesystem tools; they are not part of CH375USB:

- **dosfstools / `mkfs.fat`**: https://github.com/dosfstools/dosfstools
- **GNU Parted**: https://www.gnu.org/software/parted/

## Pocket386 hardware and community references

Pocket386-specific behavior has been established primarily through repeated testing on real hardware, with public community reports used as additional clues.

Relevant discussions include:

- VCFed Pocket386 discussion:  
  https://forum.vcfed.org/index.php?threads/pocket-386.1247640/
- VCFed page discussing community BIOS/default changes referenced by the README:  
  https://forum.vcfed.org/index.php?threads/pocket-386.1247640/page-6
- VOGONS Pocket386 discussion referenced by the README:  
  https://www.vogons.org/viewtopic.php?start=100&t=99751
- VOGONS CH375 ISA / FreddyV development thread:  
  https://www.vogons.org/viewtopic.php?t=43311

Community reports helped identify issues worth testing, including BIOS/CMOS reset behavior, physical PS/2 enablement, floppy/FDC defaults and CH375 storage behavior. Claims affecting CH375USB were treated as hypotheses until checked against documentation or real hardware.

Third-party modified Pocket386 BIOS images and CMOS save/restore utilities are not part of CH375USB, have not been validated by the project unless explicitly stated otherwise, and are not endorsed by the project.

The validated 1 GiB FAT16 layout documented in the README is a **CH375USB real-hardware compatibility finding** from cross-platform Linux/DOS/Windows testing; it should not be read as a claim that all larger FAT16 filesystems are invalid.

## AI-assisted development and independent review

AI tools were used during iterative design, implementation assistance, code review, debugging hypotheses and documentation.

- **OpenAI ChatGPT** assisted with iterative design, source review, debugging and documentation while changes were repeatedly validated on real Pocket386 hardware.
- **Anthropic Claude** was used as an independent review pass on an early prototype. Among other findings, that review identified that an early experimental `.SYS` skeleton lacked a valid DOS block-device header, real Strategy/Interrupt request handling and a complete block-device implementation; those findings influenced the subsequent redesign.

AI output is not treated as authoritative hardware documentation and is not a substitute for the cited specifications or real-hardware validation. AI-generated suggestions were reviewed, modified and tested before inclusion.

## Binary distribution

Published 0.5.1 binaries under [`../binary/`](../binary/) are project build outputs:

- [`CH375USB.SYS`](../binary/CH375USB.SYS)
- [`CH375MOU.DRV`](../binary/CH375MOU.DRV)
- [`MTEST.COM`](../binary/MTEST.COM)

They must correspond to the same source revision as the documented release and must not be confused with vendor WCH drivers, FreddyV's CH375 drivers, CuteMouse/CTMOUSE or other third-party binaries.

## Independence statement

CH375USB is an interoperable open-source implementation built from:

1. independently written CH375USB-specific code;
2. the explicitly disclosed CuteMouse-informed/adapted portions of the internal DOS mouse core, under compatible GPL terms;
3. public hardware/protocol specifications and DOS/BIOS/Windows interface definitions;
4. prior open-source/community work used as behavioral or architectural reference;
5. repeated real-hardware experimentation and regression testing.

Except for the CuteMouse-derived/adapted relationship explicitly documented above, no proprietary WCH driver source, FreddyV driver code, Bret Johnson USBMOUSE source, USBDDOS source or other third-party driver source is redistributed as part of CH375USB.
