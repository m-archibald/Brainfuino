# Project Roadmap & Future Enhancements

The Brainfuino project continues to evolve from an esoteric proof-of-concept into a polished, hackable hardware development platform. This roadmap outlines current progress, architectural enhancements, and future hardware/firmware milestones.

---

## Status Overview

| Feature | Subsystem | Status | Priority / Notes |
| :--- | :--- | :---: | :--- |
| **3D-Printable Enclosure** | Mechanical | :material-check-circle: **Done** | Custom case with retention hook and single M3 fastener ([Assembly Guide](case/assembly.md)) |
| **KiCad 9 PCB Revision (Rev 1.1)** | Hardware | :material-check-circle: **Done** | USB-C, length-tuned buses, JLCPCB SMT files ([Comparison](hardware/rev1-vs-rev2.md)) |
| **Documentation Suite & CI/CD** | Docs | :material-check-circle: **Done** | MkDocs Material suite, automated GitHub Pages deployment |
| **Dedicated Program Mode** | STM32 Firmware | :material-check-circle: **Done** | Dedicated mode switch via 3s button hold, fast flasher with progress bar and streaming fallback |
| **Smart Clock-Pausing & Throttle** | STM32 Firmware | :material-check-circle: **Done** | 1-cycle MCO gating on falling edge with bus settle and outbox backpressure |
| **25-Speed Ladder & Manual Stepping** | STM32 Firmware | :material-check-circle: **Done** | 10 Hz to 12 MHz dual-engine (PWM + MCO), single & burst stepping accumulator |
| **Non-Volatile Settings Storage** | STM32 Firmware | :material-check-circle: **Done** | Flash Page 63 emulated EEPROM preserves user configuration across power cycles |
| **Multi-Program Library & TOC** | STM32 Firmware | :material-check-circle: **Done** | Internal 76 kB flash partition, 64-slot TOC, dynamic compaction, non-BF pruning |
| **Interactive Terminal Menu UI** | STM32 Firmware | :material-check-circle: **Done** | Multi-page raspi-config style interface with Left/Right option cycling ([Guide](user-guide/config-menu.md)) |
| **USB DFU Bootloader Mode** | STM32 Firmware | :material-check-circle: **Done** | Soft-jump into ST factory ROM bootloader over USB (`!DFU!` or `.\build.ps1 -Flash`) |
| **In-Circuit FPGA Flashing** | Hardware & FW | :material-lightbulb-outline: Future Idea | Potential concept: wire STM32 GPIOs to MachXO2 JTAG pins for USB bitstream updates |
| **External QSPI Multi-Program Storage**| Hardware (Rev 1.2) | :material-lightbulb-outline: Future Idea | High-capacity SPI/QSPI NOR Flash (e.g. 4 MB) for massive program collections |

---

## Firmware & Software Roadmap

### Multi-Program Flash Library & Dynamic TOC Engine — :material-check-circle: Completed
* **The Goal:** Enable storing, organizing, and launching multiple Brainfuck programs directly from non-volatile memory without relying on an external PC.
* **Implementation Details:**
    * **Memory Architecture:** Dedicated 76 kB user partition (Pages 25–62) and 2 kB Table of Contents (Page 24) in the STM32F072's internal 128 kB Flash.
    * **Dynamic Allocation:** Programs of arbitrary lengths (from tiny 50-byte snippets to 76 kB full-scale programs) pack consecutively.
    * **Zero-Erase Tombstone Deletion:** Deleting a program writes `0x0000` to its 16-bit status field in the TOC without erasing Flash pages.
    * **On-Demand Compaction:** Active programs are packed forward only when contiguous space at the end of the pool is exhausted.
    * **Comment Pruning Engine:** Settings for pruning non-Brainfuck characters on paste and when uploading to the library maximize storage efficiency.
    * **Pre-Run Verification:** Interactive prompt allows running a program on the FPGA soft-processor to verify correct execution before committing to Flash, with a 10-second countdown and option to skip (`n`) for instant saving.
    * **Protected Slot 00:** Factory Brainfuino ASCII demo is permanently preserved in Slot 00.

### Dedicated Program Mode — :material-check-circle: Completed
* **The Problem:** Single-key commands (`1`–`7` for clock speed, `!` for dump) previously intercepted terminal keystrokes directly. Interactive programs could not accept number keys without accidentally switching clock speeds.
* **The Solution:** Implemented a dedicated **Program Mode** entered via a 3-second button hold. When in standard Run Mode, incoming characters pass cleanly through to the running Brainfuck soft-processor without interception.

### Smart Variable Clock Rate & Output Throttle — :material-check-circle: Completed
* **The Problem:** At high clock speeds, output instructions (`.`) hold data on the bus for only 20 clock cycles ($1.66\ \mu\text{s}$ at 12 MHz). Closely-spaced prints (e.g. `\r\n` line endings in Mandelbrot) outpaced Cortex-M0 interrupt latency, causing dropped characters.
* **The Solution:** Implemented hardware clock-pausing directly in `EXTI2_3_IRQHandler` on `BF_OUTSTRB` falling edge. The STM32 gates MCO in 1 instruction (`RCC->CFGR &= ~RCC_CFGR_MCO`), latches data with zero bus skew, and resumes or throttles based on USB queue capacity.

### 25-Speed Frequency Ladder (10 Hz – 12 MHz) & Manual Stepping Mode — :material-check-circle: Completed
* **The Finding:** Discovered that the `SST39LF020-55` parallel Flash ROM has a maximum address access time of $55\text{ ns}$ ($18.18\text{ MHz}$ physical limit). Speeds of 24 MHz ($41.6\text{ ns}$) and 48 MHz ($20.8\text{ ns}$) violated silicon access times during single-cycle fetch.
* **The Solution:** Implemented a dual-engine architecture providing 25 speeds spanning 6 orders of magnitude:
    * **10 Hz – 50 kHz:** Hardware TIM1 PWM generating square wave clocks with full cycle precision.
    * **62.5 kHz – 12 MHz:** MCO hardware clock dividers with positive timing margin ($+28.3\text{ ns}$ at 12 MHz).
* **Manual Stepping Mode:** Added single-stepping and burst-stepping capabilities with an accumulator queue engine supporting Spacebar, Tab, or Enter trigger keys with $1$ to $100\text{k}$ tick multipliers.

### Non-Volatile Flash Configuration Persistence & Delayed Wear Leveling — :material-check-circle: Completed
* **The Feature:** All configuration menu settings automatically write to the STM32's top internal Flash page (Page 63: `0x0801F800`) on exit with CRC verification and wear-mitigation checking.
* **Delayed Wear Leveling:** Runtime hotkey speed changes (`PgUp`/`PgDn`) use a 3-second debounce timer before committing to Flash, safeguarding flash cycle endurance during rapid adjustments.

### Interactive Terminal Menu & UI Improvements (raspi-config style) — :material-check-circle: Completed
* **The Goal:** Enhance the companion serial terminal with an interactive, user-friendly multi-page text UI for managing programs and configuring STM32 firmware features, accessible via a 6-second button hold (with smooth breathing PWM fade on the Red LED) or terminal command (`!MENU` / `!CONFIG`).
* **Implementation Details:**
    * **Architecture:** Multi-page layout (Main Menu, Program Library, Hardware Settings) with non-blocking ISR-to-Thread-Mode event passing.
    * **Full Navigation:** ANSI / VT100 cursor control (`Up`/`Down`), Left/Right value cycling, direct numeric shortcuts, and quick-action hotkeys (`a` to add, `d` to delete).
    * **Full Documentation:** See the [Configuration Menu & Program Library Guide](user-guide/config-menu.md).

---

## Hardware Roadmap (Rev 1.2 & Beyond)

### In-Circuit FPGA Flashing via STM32 (Potential Future Idea)
* **The Concept:** A potential, maybe-someday feature to explore: allowing the STM32 coprocessor to flash the Lattice FPGA in-circuit, eliminating the need for an external JTAG programmer or Raspberry Pi Pico debugger.
* **Implementation Concept:**
    * Route unused GPIO pins from the STM32F072 microcontroller to the Lattice MachXO2 JTAG header pins (`TCK`, `TMS`, `TDI`, `TDO`).
    * Port Lattice's open-source **`embedded_jtag`** or **`ispVM Embedded`** C routines into the STM32 firmware.
    * Users would be able to update the FPGA soft-processor bitstream directly over USB-C using a simple utility.

### External QSPI NOR Flash for Multi-Megabyte Libraries
* **The Concept:** While the STM32's internal 76 kB partition supports up to 63 user programs, an external SPI or QSPI NOR Flash chip (e.g. W25Q32, 4 MB) could be added to store vast collections of Brainfuck programs, operating systems, and massive benchmarks.
