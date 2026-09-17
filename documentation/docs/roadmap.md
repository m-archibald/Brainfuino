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
| **13-Speed Frequency Ladder** | STM32 Firmware | :material-check-circle: **Done** | 62.5 kHz to 12 MHz ladder, compliant with 55 ns parallel Flash silicon timing |
| **Non-Volatile Settings Storage** | STM32 Firmware | :material-check-circle: **Done** | Flash Page 63 emulated EEPROM preserves user configuration across power cycles |
| **Default Program Restore** | STM32 Firmware | :material-check-circle: **Done** | 10-second button hold restores burned-in default Brainfuck demo to ROM |
| **In-Circuit FPGA Flashing** | Hardware & FW | :material-lightbulb-outline: Future Idea | Potential concept: wire STM32 GPIOs to MachXO2 JTAG pins for USB bitstream updates |
| **QSPI Multi-Program Storage** | Hardware (Rev 1.2) | :material-lightbulb-outline: Future Idea | Onboard SPI/QSPI Flash chip to store a library of Brainfuck programs |
| **Interactive Terminal Menu UI** | STM32 Firmware | :material-check-circle: **Done** | Full raspi-config style dual-mode terminal interface for all coprocessor settings |
| **USB DFU Bootloader Mode** | STM32 Firmware | :material-check-circle: **Done** | Soft-jump into ST factory ROM bootloader over USB (`!DFU!` or `.\build.ps1 -Flash`) |

---

## Firmware & Software Roadmap

### Dedicated Program Mode — :material-check-circle: Completed
* **The Problem:** Single-key commands (`1`–`7` for clock speed, `!` for dump) previously intercepted terminal keystrokes directly. Interactive programs could not accept number keys without accidentally switching clock speeds.
* **The Solution:** Implemented a dedicated **Program Mode** entered via a 3-second button hold. When in standard Run Mode, incoming characters pass cleanly through to the running Brainfuck soft-processor without interception.

### Smart Variable Clock Rate & Output Throttle — :material-check-circle: Completed
* **The Problem:** At high clock speeds, output instructions (`.`) hold data on the bus for only 20 clock cycles ($1.66\ \mu\text{s}$ at 12 MHz). Closely-spaced prints (e.g. `\r\n` line endings in Mandelbrot) outpaced Cortex-M0 interrupt latency, causing dropped characters.
* **The Solution:** Implemented hardware clock-pausing directly in `EXTI2_3_IRQHandler` on `BF_OUTSTRB` falling edge. The STM32 gates MCO in 1 instruction (`RCC->CFGR &= ~RCC_CFGR_MCO`), latches data with zero bus skew, and resumes or throttles based on USB queue capacity.

### 13-Speed Frequency Ladder (62.5 kHz – 12 MHz) — :material-check-circle: Completed
* **The Finding:** Discovered that the `SST39LF020-55` parallel Flash ROM has a maximum address access time of $55\text{ ns}$ ($18.18\text{ MHz}$ physical limit). Speeds of 24 MHz ($41.6\text{ ns}$) and 48 MHz ($20.8\text{ ns}$) violated silicon access times during single-cycle fetch.
* **The Solution:** Replaced out-of-spec speeds with a granular 13-speed table ranging from **62.5 kHz** up to **12 MHz** ($+28.3\text{ ns}$ margin above ROM access time). Verified across all 13 speeds with 100.0% character fidelity.

### Non-Volatile Flash Configuration Persistence — :material-check-circle: Completed
* **The Feature:** All configuration menu settings (active clock frequency, upload threshold, auto-reset, endless loop injection, and run mode speed hotkeys) automatically write to the STM32's top internal Flash page (Page 63: `0x0801F800`) on exit with CRC verification and wear-mitigation checking. Settings persist across power cycles and hard reboots.

### Default Burned-in Program (10-Second Reset Hold)
* **The Goal:** Store a default, self-contained Brainfuck demo program directly within the STM32 microcontroller's internal Flash memory.
* **Operation:** If the user holds down the Reset button for **10 seconds**, the STM32 will automatically erase parallel Flash ROM and write this default program into address `0`. This provides an immediate out-of-the-box demo and quick sanity check without requiring a computer connection.

### Interactive Terminal Menu & UI Improvements (raspi-config style) — :material-check-circle: Completed
* **The Goal:** Enhance the companion serial terminal with an interactive, user-friendly text UI (reminiscent of Raspberry Pi's `raspi-config`) for configuring STM32 firmware features, accessible via a dedicated 6-second button hold (with smooth breathing PWM fade on the Red LED) or terminal command (`!MENU` / `!CONFIG`).
* **Implementation Details:**
    * **Architecture:** Asynchronous Producer-Consumer design separating lightweight USB ISR keystroke parsing from Thread Mode frame rendering.
    * **Dual Navigation:** Full ANSI / VT100 cursor control (`Up`/`Down`/`Space`/`Enter`/`ESC`) plus basic terminal direct numeric shortcuts (`1`–`8`, `0`).
    * **Debounce State Machine:** 35 ms level stability filter on physical button transitions eliminating tactile chatter and false restarts during long holds.
    * **Hardware Feedback:** 70 ms LED blip on short-press reset; continuous 1 kHz PWM breathing pulse while inside the menu.
    * **Full Documentation:** See the [Configuration Menu Guide](user-guide/config-menu.md).

---

## Hardware Roadmap (Rev 1.2 & Beyond)

### In-Circuit FPGA Flashing via STM32 (Potential Future Idea)
* **The Concept:** A potential, maybe-someday feature to explore: allowing the STM32 coprocessor to flash the Lattice FPGA in-circuit, eliminating the need for an external JTAG programmer or Raspberry Pi Pico debugger.
* **Current Status:** Not actively planned for near-term milestones, but kept as a possible future hardware/firmware exploration.
* **Implementation Concept:**
    * Route unused GPIO pins from the STM32F072 microcontroller to the Lattice MachXO2 JTAG header pins (`TCK`, `TMS`, `TDI`, `TDO`).
    * Port Lattice's open-source **`embedded_jtag`** or **`ispVM Embedded`** C routines into the STM32 firmware.
    * Users would be able to update the FPGA soft-processor bitstream directly over USB-C using a simple utility.

### Onboard QSPI Flash for Multi-Program Storage
* **The Goal:** Expand Brainfuino's onboard library capacity beyond the single active program in parallel Flash ROM.
* **Implementation:**
    * Add a low-cost SPI or QSPI NOR Flash chip (e.g. W25Q32, 4 MB) connected to the STM32.
    * The STM32 can store dozens of classic Brainfuck programs, benchmarks, games, and utilities.
    * Users can select and load any stored program into the 256 kB parallel Flash ROM via the terminal menu.
