# Project Roadmap & Future Enhancements

The Brainfuino project continues to evolve from an esoteric proof-of-concept into a polished, hackable hardware development platform. This roadmap outlines current progress, architectural enhancements, and future hardware/firmware milestones.

---

## Status Overview

| Feature | Subsystem | Status | Priority / Notes |
| :--- | :--- | :---: | :--- |
| **3D-Printable Enclosure** | Mechanical | :material-check-circle: **Done** | Custom case with retention hook and single M3 fastener ([Assembly Guide](case/assembly.md)) |
| **KiCad 9 PCB Revision (Rev 1.1)** | Hardware | :material-check-circle: **Done** | USB-C, length-tuned buses, JLCPCB SMT files ([Comparison](hardware/rev1-vs-rev2.md)) |
| **Documentation Suite & CI/CD** | Docs | :material-progress-clock: **Almost Done** | MkDocs Material suite, automated GitHub Pages deployment |
| **Dedicated Program Mode** | STM32 Firmware | :material-clock-outline: Planned | 3-second button hold to switch between controls and code execution |
| **Smart Variable Clock Rate** | STM32 Firmware | :material-clock-outline: Planned | Auto-throttles clock during `.` output to prevent UART buffer overflow |
| **Default Program Restore** | STM32 Firmware | :material-clock-outline: Planned | 10-second button hold restores burned-in default Brainfuck demo to ROM |
| **In-Circuit FPGA Flashing** | Hardware & FW | :material-lightbulb-outline: Future Idea | Potential concept: wire STM32 GPIOs to MachXO2 JTAG pins for USB bitstream updates |
| **QSPI Multi-Program Storage** | Hardware (Rev 1.2) | :material-clock-outline: Planned | Onboard SPI/QSPI Flash chip to store a library of Brainfuck programs |
| **Interactive Terminal Menu UI** | STM32 Firmware | :material-clock-outline: Planned | ANSI terminal menu for settings, clock tuning, and program loading |

---

## Firmware & Software Roadmap

### Dedicated Program Mode (3-Second Button Hold)
* **The Problem:** Single-key commands (`1`–`7` for clock speed, `!` for dump) currently intercept terminal keystrokes directly. Interactive programs cannot accept number keys without accidentally switching clock speeds, and ANSI escape sequences from arrow keys can trigger unintended code writes.
* **The Solution:** Require holding the hardware button for **3 seconds** to enter a dedicated **Program / Config Mode**. When in standard execution mode, all incoming characters pass cleanly through to the running Brainfuck soft-processor without interception.

### Smart Variable Clock Rate
* **The Problem:** The MachXO2 FPGA executes instructions with extreme parallelism. At clock speeds of **8 MHz and above**, write-intensive loops (such as Mandelbrot fractals or large ASCII art dumps) output bytes faster than the STM32 can package and send them over USB CDC, resulting in dropped characters.
* **The Solution:** Implement a dynamic clock throttle. The FPGA runs at full maximum speed (up to **48 MHz**) during computation, but whenever the FPGA asserts its output strobe (`portWR`), the STM32 temporarily suspends or slows the clock pulses until its serial transmission queue is emptied.

### Default Burned-in Program (10-Second Reset Hold)
* **The Goal:** Store a default, self-contained Brainfuck demo program directly within the STM32 microcontroller's internal Flash memory.
* **Operation:** If the user holds down the Reset button for **10 seconds**, the STM32 will automatically erase parallel Flash ROM and write this default program into address `0`. This provides an immediate out-of-the-box demo and quick sanity check without requiring a computer connection.

### Interactive Terminal Menu & UI Improvements
* **The Goal:** Enhance the companion serial terminal with an interactive, user-friendly text UI for navigating STM32 settings.
* **Features:**
    * Visual menu for selecting operating frequencies and checking hardware status.
    * Integrated program manager to inspect, erase, and load stored programs.
    * Live monitoring of FPGA status lines (`Incoming`, `InStrobe`, `OutStrobe`).

---

## Hardware Roadmap (Rev 1.2 & Beyond)

### In-Circuit FPGA Flashing via STM32 (Potential Future Idea)
* **The Concept:** A potential, maybe-someday feature to explore: allowing the STM32 coprocessor to flash the Lattice FPGA in-circuit, eliminating the need for an external JTAG programmer or Raspberry Pi Pico debugger.
* **Current Status:** Not actively planned for near-term milestones, but kept as a possible future hardware/firmware exploration.
* **Implementation Concept:**
    * Route unused GPIO pins from the STM32F072 microcontroller to the Lattice MachXO2 JTAG header pins (`TCK`, `TMS`, `TDI`, `TDO`).
    * Port Lattice's open-source **`embedded_jtag`** or **`ispVM Embedded`** C routines into the STM32 firmware.
    * Users would be able to update the FPGA soft-processor bitstream directly over USB-C using a simple utility.
* **Technical Feasibility:** [Read the technical feasibility analysis](fpga/architecture.md#roadmap-flashing-the-fpga-through-the-stm32).

### Onboard QSPI Flash for Multi-Program Storage
* **The Goal:** Expand Brainfuino's onboard library capacity beyond the single active program in parallel Flash ROM.
* **Implementation:**
    * Add a low-cost SPI or QSPI NOR Flash chip (e.g. W25Q32, 4 MB) connected to the STM32.
    * The STM32 can store dozens of classic Brainfuck programs, benchmarks, games, and utilities.
    * Users can select and load any stored program into the 256 kB parallel Flash ROM via the terminal menu.
