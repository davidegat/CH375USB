# CH375USB Project Notice

Copyright (C) 2026 **Davide "gat"**  
https://github.com/davidegat

CH375USB is an independent, unofficial open-source project and is not affiliated with, sponsored by, or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH).

The names **WCH**, **WinChipHead**, and **CH375** are used solely to identify hardware, documentation, and historical software with which this project interoperates.

CH375USB is distributed under the GNU General Public License version 3 or, at your option, any later version. See `LICENSE`.

## Source-code provenance

The CH375USB source code in this repository is independently written in 16-bit NASM assembly for DOS. The repository does **not** contain the proprietary WCH DOS driver source code, the vendor `CH375DOS.SYS`, or FreddyV's `CH375286.SYS` binary.

No substantial third-party source-code block has been copied or transliterated into CH375USB. Standard USB constants, CH375 command values, DOS interrupt numbers, SCSI command values and other protocol/interface facts necessarily match their respective specifications and documented interfaces.

One specific hardware procedure deserves explicit attribution: the legacy CH375B low-speed setup sequence using command/data values `0Bh -> 17h -> D8h` was learned from W.ch/WinChipHead-derived CH375 USB-host examples, including the Makeblock `MeUSBHost` implementation and related Arduino CH375 material. CH375USB implements that procedure independently in NASM; the external C/C++ source itself was not copied.

The 0.4.12-exp1-datasheet low-level audit was checked directly against WCH's **CH375 Datasheet (I), Version 4** (`CH375DS1.PDF`) and **CH375 Datasheet (II): USB Basic Transmission Commands, Version 4** (`CH375DS2.PDF`). Those documents were used as hardware/protocol specifications, not as source-code dependencies.

When a release contains `CH375USB.SYS`, that file is the executable driver built from the source code in this project. It must not be confused with the vendor `CH375DOS.SYS` or FreddyV's `CH375286.SYS`.

## Sources, prior work and inspirations

The following material was consulted during development. Inclusion here does not imply that its source code is incorporated into CH375USB.

### WCH / WinChipHead documentation and examples

- **Nanjing Qinheng Microelectronics (WCH) — `CH375DS1.PDF`, CH375 Datasheet (I), Version 4.** This document defines the CH375 parallel host interface, command/data timing, host modes, interrupt/status behavior and native mass-storage commands. The 0.4.12-exp1-datasheet audit specifically used it to verify command-port bit 7 as the parallel-mode `INT#` state, the mode 5 / mode 7 / mode 6 host-reset sequence, `DISK_SIZE` result handling and `DISK_R_SENSE` completion behavior.  
  https://www.wch-ic.com/downloads/CH375DS1_PDF.html
- **Nanjing Qinheng Microelectronics (WCH) — `CH375DS2.PDF`, CH375 Datasheet (II): USB Basic Transmission Commands, Version 4.** This document defines additional host-transaction commands and external-firmware transfer behavior. The 0.4.12-exp1-datasheet audit specifically used it for `SET_USB_SPEED`, `GET_DEV_RATE`, `SET_RETRY` (including the documented `25h` marker, NAK retry semantics and reset default `85h`), `SET_ENDP6`, `SET_ENDP7`, `ISSUE_TOKEN`, `ISSUE_TKN_X`, `CLR_STALL`, `RD_USB_DATA0`, and the documented setup/data/status stages of USB control transfers. The datasheet also notes that some commands, including `SET_USB_SPEED`, are not supported on every chip model/revision; CH375USB therefore retains compatibility fallbacks where appropriate.
- **W.ch / WinChipHead USB 1.1 Host Examples for CH375 (historical 2004-2005 examples).** Used as a reference for CH375 host command sequencing and legacy low-speed behaviour. These examples are also acknowledged by later open-source CH375 projects such as Makeblock's `MeUSBHost`.
- **WCH `CH375DOS.SYS` / historical W.ch DOS driver, including V1.9A.** Used only as a historical and behavioural reference for what CH375 storage support on DOS was expected to do. No vendor driver binary or source is distributed by this project.

### FreddyV — CH375286.SYS

- **FreddyV, `CH375286.SYS` v0.22 — “CH375 ISA to USB Driver for 80286”.** FreddyV's driver identifies itself as being based on the W.ch driver V1.9A and was an important practical reference for CH375 ISA operation on old x86 systems, including port selection, timing sensitivity, performance and hardware compatibility. CH375USB does not contain FreddyV's binary or source code and was not produced by translating his driver.  
  VOGONS development/testing thread: https://www.vogons.org/viewtopic.php?t=43311  
  Pocket 8086 discussion identifying the v0.22 driver: https://www.vogons.org/viewtopic.php?t=101513

### Open-source CH375 host examples

- **xeecos — `CH375-Keyboard-Arduino`.** Consulted as a practical CH375 HID-host reference, especially for CH375 command definitions, host-mode behaviour and keyboard-oriented USB transactions. No source block from this project is included in CH375USB.  
  https://github.com/xeecos/CH375-Keyboard-Arduino
- **Makeblock — `MeUSBHost` in Makeblock-Libraries.** Consulted for CH375 host command sequencing and the historical W.ch-derived low-speed setup procedure. No Makeblock C/C++ source block is included in CH375USB.  
  https://github.com/Makeblock-official/Makeblock-Libraries/blob/master/src/MeUSBHost.cpp
- The Makeblock source itself credits the older **W.ch USB 1.1 Host Examples for CH375**, making the lineage of that hardware procedure explicit.

### DOS USB implementations

- **crazii — `USBDDOS`.** Consulted as an independent open-source DOS USB stack and as a reference for general DOS USB design questions involving HID, mass storage, hubs, polling, hardware ownership and old-PC compatibility. Its PCI UHCI/OHCI/EHCI architecture is different from the CH375 interface used here; no USBDDOS source code is incorporated in CH375USB.  
  https://github.com/crazii/USBDDOS
- **CuteMouse / CTMOUSE.** Used as the conventional DOS mouse-driver interoperability target while designing the `INT 33h` bridge. CH375USB chains to an existing DOS mouse driver rather than incorporating CuteMouse source code.

### USB and storage specifications

The implementation was also informed by public protocol specifications, principally:

- **USB 2.0 Specification** — enumeration, descriptors, endpoint behaviour, transfers, hubs and standard requests.
- **USB HID 1.11 / HID Usage Tables** — boot-protocol keyboard and mouse reports and usage codes.
- **USB Mass Storage Class Bulk-Only Transport (BOT)** — CBW/CSW transactions and reset recovery.
- **SCSI block-command conventions** used by USB mass-storage devices, including `TEST UNIT READY`, `READ CAPACITY(10)`, `READ(10)`, `WRITE(10)` and `SYNCHRONIZE CACHE`.

These are protocol specifications, not source-code dependencies.

### DOS and Windows 95 interface references

- **Ralf Brown's Interrupt List (RBIL)** and mirrors of it were consulted for DOS/Windows interrupt interfaces, particularly the DOS mouse API and Windows enhanced-mode notifications through `INT 2Fh`, including `AX=1605h` and `AX=1606h`.
- **Microsoft Windows 95 documentation** was consulted to distinguish real-mode DOS mouse/keyboard handling from Windows 95 protected-mode keyboard and mouse services. This led to the deliberate decision that CH375USB's DOS HID bridge must suspend itself during Windows enhanced mode rather than pretending to provide a native Windows input driver.
- Historical Windows VxD/VKD/VMOUSE documentation and interrupt-reference material was consulted while assessing a possible future Windows companion driver. No Windows VxD code is included in release 0.4.12-exp1-datasheet.

### FreeDOS DEVLOAD

- **FreeDOS `DEVLOAD`** is not part of CH375USB, but it was used to develop and document the practical Windows 95 boot workaround in which `CH375USB.SYS` is loaded from `AUTOEXEC.BAT` immediately before `WIN`.  
  https://github.com/FDOS/devload

### Community research and real-hardware reports

Several VOGONS discussions were consulted for historical CH375 behaviour and real-hardware experience, including:

- **“USB ISA cards?”** — includes FreddyV's CH375 driver development and tests on 8086/286-class hardware.  
  https://www.vogons.org/viewtopic.php?t=43311
- **“Pocket 8086 - How to get a USB drive to work?”** — comparison of the stock CH375 DOS driver and FreddyV's `CH375286.SYS`.  
  https://www.vogons.org/viewtopic.php?t=101513
- **“Ch375 alternative, so can also read External CD/DVD USB Drive support.. or even USB Floppy?”** — useful discussion of the CH375's external-firmware mode and the possibility of implementing non-storage USB classes such as a mouse in host software.  
  https://www.vogons.org/viewtopic.php?t=98232
- **“Pocket 386, the brother of Hand386 and Book8088, the story so far ...”** — practical reports about the Pocket386 CH375 implementation and flash-drive/filesystem compatibility.  
  https://www.vogons.org/viewtopic.php?t=100645

Forum reports were treated as experimental clues and were verified on the actual Pocket386 whenever they affected CH375USB behaviour.

### AI-assisted review during development

The development process also used AI-assisted analysis as a debugging aid:

- **OpenAI ChatGPT** assisted with iterative design, code generation/review, debugging hypotheses and documentation while the driver was repeatedly tested on real Pocket386 hardware.
- **Google Gemini** was used as an independent code-review pass on an early prototype. In particular, it correctly identified that the first experimental `.SYS` skeleton lacked a valid DOS block-device header, real Strategy/Interrupt request handling and a complete block-device implementation. Those findings influenced the subsequent redesign. Gemini output is not included as third-party source code in this repository.

AI suggestions were treated as hypotheses and engineering assistance, not as authoritative hardware documentation; working behaviour was established by source review, documented interfaces and repeated real-hardware testing.

## Independence statement

CH375USB is therefore best described as an **independent interoperable implementation** informed by public hardware/protocol documentation, prior community work, historical driver behaviour and real-hardware experimentation.

Where a specific externally learned hardware procedure is retained, it is identified above. No proprietary WCH driver source code, FreddyV driver code, or other third-party source file is redistributed as part of CH375USB.
