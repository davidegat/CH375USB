# CH375USB Project Notice

Copyright (C) 2026 **Davide "gat"**  
https://github.com/davidegat

CH375USB is an independent, unofficial open-source project and is not affiliated with, sponsored by, or endorsed by Nanjing Qinheng Microelectronics Co., Ltd. (WCH).

The names **WCH**, **WinChipHead**, and **CH375** are used solely to identify hardware, documentation, and historical software with which this project interoperates.

CH375USB is distributed under the GNU General Public License version 3 or, at your option, any later version. See [`LICENSE`](../LICENSE).

## Source-code provenance

The source code in this repository is independently written. The project does **not** contain proprietary WCH driver source code, the vendor `CH375DOS.SYS`, or FreddyV's `CH375286.SYS` binary/source.

No substantial third-party source-code block has been copied or transliterated into CH375USB. Standard USB constants, CH375 command values, DOS/BIOS interrupt numbers, Win16 ABI structures, SCSI command values and other protocol/interface facts necessarily match the public interfaces they implement.

Version 0.5.0 adds two independently written interoperability components:

- a BIOS PS/2 pointing-device layer based on the public `INT 15h AH=C2h` interface;
- `CH375MOU.DRV`, a Win16 mouse-driver bridge based on the documented Windows mouse-driver ABI and the conventional DOS `INT 33h` mouse API.

The 0.5.0 BIOS PS/2 code intentionally does **not** contain or translate Bret Johnson's USBMOUSE source code. Bret Johnson's work was consulted as historical/practical background for DOS USB mouse behavior and staged polling ideas, but CH375USB uses its own CH375-specific implementation and architecture.

One hardware procedure remains explicitly attributed: the legacy CH375B low-speed setup sequence using command/data values `0Bh -> 17h -> D8h` was learned from W.ch/WinChipHead-derived CH375 USB-host examples, including Makeblock `MeUSBHost` and related Arduino material. CH375USB implements the sequence independently in NASM; the external C/C++ source itself was not copied.

The low-level CH375 audit was checked against WCH's **CH375 Datasheet (I), Version 4** (`CH375DS1.PDF`) and **CH375 Datasheet (II): USB Basic Transmission Commands, Version 4** (`CH375DS2.PDF`). Those documents are used as hardware/protocol specifications, not source-code dependencies.

## Binary distribution

Prebuilt 0.5.0 binaries are published under [`../binary/`](../binary/):

- [`CH375USB.SYS`](../binary/CH375USB.SYS)
- [`CH375MOU.DRV`](../binary/CH375MOU.DRV)

They are build outputs of this project and must not be confused with vendor or third-party binaries.

## Sources, prior work and inspirations

The following material was consulted during development. Inclusion here does not imply that its source code is incorporated into CH375USB.

### WCH / WinChipHead documentation and examples

- **Nanjing Qinheng Microelectronics (WCH) — CH375 Datasheet (I), Version 4.** Defines the CH375 parallel host interface, command/data timing, host modes, interrupt/status behavior and native mass-storage commands.
- **Nanjing Qinheng Microelectronics (WCH) — CH375 Datasheet (II): USB Basic Transmission Commands, Version 4.** Defines additional host-transaction commands and external-firmware transfer behavior including speed, retry, token and data-buffer operations.
- Historical **W.ch / WinChipHead USB 1.1 Host Examples for CH375** were used as behavioral references for command sequencing and legacy low-speed handling.
- Historical WCH DOS drivers were treated only as behavioral references for expected CH375 storage operation. No vendor driver binary/source is redistributed.

### FreddyV — CH375286.SYS

FreddyV's CH375286.SYS work was an important practical reference for ISA CH375 operation on older x86 systems, especially around port selection, timing, performance and compatibility. CH375USB does not contain FreddyV's binary/source and was not produced by translating that driver.

### Open-source CH375 host examples

- **xeecos / CH375-Keyboard-Arduino** — consulted as a practical CH375 HID-host reference.
- **Makeblock / MeUSBHost** — consulted for CH375 sequencing and the historical W.ch-derived low-speed procedure.

No source block from these projects is included in CH375USB.

### DOS USB and mouse implementations

- **crazii / USBDDOS** — consulted as an independent DOS USB stack and architectural reference for DOS USB/HID/storage design questions.
- **CuteMouse / CTMOUSE** — the DOS mouse driver used as the interoperability target and **runtime dependency of the tested/reference CH375USB 0.5.0 mouse stack**. CH375USB does not distribute or incorporate CuteMouse source code. The reference package/project is available through FreeDOS and the CuteMouse SourceForge project.
- **Bret Johnson / USBMOUSE** — consulted as historical/practical background for DOS USB mouse support and nonblocking/staged behavior. CH375USB's BIOS PS/2 and CH375 transport code are independent implementations.

### USB and storage specifications

Implementation work was also informed by public protocol specifications, principally:

- USB 2.0 Specification;
- USB HID 1.11 / HID Usage Tables;
- USB Mass Storage Class Bulk-Only Transport;
- SCSI block-command conventions such as TEST UNIT READY, READ CAPACITY(10), READ(10), WRITE(10) and SYNCHRONIZE CACHE.

These are protocol specifications, not source-code dependencies.

### DOS, BIOS and Windows interface references

Public references were consulted for:

- DOS `INT 33h` mouse services;
- BIOS PS/2 pointing-device services via `INT 15h AH=C2h`;
- Windows enhanced-mode notifications through `INT 2Fh`;
- the Win16 mouse-driver ABI and Windows NE driver/DLL format;
- historical Windows 95 VMOUSE/VKD/VxD behavior while assessing architectural boundaries.

These interface descriptions were used to build interoperable software, not to copy proprietary implementation code.

### FreeDOS DEVLOAD

FreeDOS `DEVLOAD` is not part of CH375USB. It remains useful for the documented DOS-to-Windows boot profile in which `CH375USB.SYS` is loaded immediately before `WIN`.

### Pocket386 hardware/community references

Pocket386-specific behavior has also been informed by community reports and real-hardware observation. In particular, the machine does not use a separate coin-cell-style CMOS backup battery; BIOS/RTC retention depends on the main system battery. A complete discharge can therefore restore BIOS defaults, including disabling mouse support and re-enabling the floppy/FDC configuration.

Community discussions have also published modified Pocket386 BIOS defaults intended to keep mouse support enabled and the unused floppy/FDC setting disabled. These third-party firmware modifications are **not part of CH375USB, have not been tested by this project, and are not endorsed by this project**. CH375USB currently provides no verified Pocket386 BIOS flashing procedure.

References include:

- https://forum.vcfed.org/index.php?threads/pocket-386.1247640/page-6
- https://www.vogons.org/viewtopic.php?start=100&t=99751

### Community research and real-hardware reports

Historical VOGONS and VCFed discussions about CH375 ISA cards, Pocket386/Book8088-style systems, FreddyV's CH375 driver and CH375 external-firmware mode were used as experimental clues. Claims affecting CH375USB behavior were treated cautiously and, where possible, checked against documentation and real hardware.

### AI-assisted development and review

AI tools were used during iterative design, code generation/review, debugging and documentation.

- **OpenAI ChatGPT** assisted with iterative design, implementation review, debugging hypotheses and documentation while the driver was repeatedly tested on real Pocket386 hardware.
- **Anthropic Claude** was used as an independent code-review pass on an early prototype. In particular, that review identified that an early experimental `.SYS` skeleton lacked a valid DOS block-device header, real Strategy/Interrupt request handling and a complete block-device implementation; those findings influenced the subsequent redesign.

Claude output is not included as third-party source code in this repository. AI suggestions from either tool were treated as hypotheses and engineering assistance, not as authoritative hardware documentation; working behavior is established by source review, public interface specifications and real-hardware testing.

## Independence statement

CH375USB is best described as an **independent interoperable implementation** informed by public hardware/protocol documentation, public DOS/BIOS/Windows interfaces, prior community work, historical driver behavior and real-hardware experimentation.

Where a specific externally learned hardware procedure or architectural inspiration is relevant, it is identified above. No proprietary WCH driver source code, FreddyV driver code, CuteMouse source, Bret Johnson USBMOUSE source, or other third-party source file is redistributed as part of CH375USB.
