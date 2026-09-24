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
| **In-Circuit FPGA JTAG Flashing** | Hardware (Rev 1.2) | :material-clock-outline: **Planned** | Route STM32 GPIOs directly to MachXO2 JTAG for zero-dongle single-cable bitstream updates |
| **High-Capacity QSPI/SPI Flash** | Hardware (Rev 1.2) | :material-clock-outline: **Planned** | 8 MB – 16 MB W25Q128 NOR Flash for massive offline program library and media streaming |
| **Hardware Flow Control UART** | Hardware (Rev 1.2) | :material-clock-outline: **Planned** | USART1 (PA9/PA10) + RTS/CTS (PD11/PD12) for DEC VT100 / vintage terminal pause support |

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

With over 15 uncommitted GPIO pins available on the 100-pin TQFP package of the `STM32F072V8T6`, the next hardware revision (Rev 1.2) is planned around three key additions:

---

### 1. In-Circuit FPGA JTAG Flashing via STM32
* **The Goal:** Eliminate external JTAG programmers (no Raspberry Pi Pico, ST-Link, or Lattice HW-USBN dongles) and allow both the FPGA bitstream (`.jed`) and Brainfuck code (`.b`) to be flashed over a single USB-C cable or directly through the Web Flasher.
* **Architecture & Wiring:**
    * Route 4 dedicated STM32 GPIO pins directly to the Lattice MachXO2 JTAG port:
        * **`PD8`** $\rightarrow$ `TCK` (Test Clock)
        * **`PD9`** $\rightarrow$ `TMS` (Test Mode Select)
        * **`PD10`** $\rightarrow$ `TDI` (Test Data In)
        * **`PD11_JTAG`** (or `PF6`) $\rightarrow$ `TDO` (Test Data Out)
    * The external 6-pin header is retained for backwards compatibility with external debuggers.
* **Firmware Implementation:**
    * Integrate an embedded XSVF/SVF player or expose a USB-JTAG bridge (such as CMSIS-DAP or DirtyJTAG) via a secondary USB endpoint.
    * Users can drag-and-drop a new `.jed` bitstream in the browser or terminal to update the `brainfuck_uP` core instantly.

---

### 2. High-Capacity QSPI / SPI NOR Flash Memory
* **The Goal:** Expand onboard offline storage from the internal 76 kB STM32 partition to **8 MB – 16 MB** using an inexpensive 8-pin NOR Flash IC (e.g. Winbond `W25Q64` or `W25Q128` in SOIC-8 / WSON-8).
* **Architecture & Wiring:**
    * Connected to STM32 high-speed SPI/QSPI peripheral pins (`SCK`, `MOSI`, `MISO`, `CS`).
    * Proposed pinout: `PD12` (CS), `PD13` (SCK), `PD14` (MISO), `PD15` (MOSI).
* **Use Cases:**
    * **Massive Program Library:** Store hundreds of full-length Brainfuck programs, self-interpreters, benchmarks, and operating systems permanently on-board.
    * **Offline Flasher Mode:** The STM32 can stage programs into the parallel Flash ROM on the fly with zero host PC connected.
    * **High-Bandwidth Media Streaming:** Store complete 30 fps video animations (e.g., Bad Apple) or audio samples and stream them into the soft-processor in real time.

---

### 3. Vintage Terminal UART with Hardware Flow Control (RTS/CTS)
* **The Goal:** Provide a dedicated serial interface with hardware handshaking for interfacing with vintage ASCII video terminals (DEC VT100, VT220, Wyse), thermal printers, and retro computers without dropping characters.
* **Why Hardware Flow Control is Critical:**
    * Vintage CRT terminals have small input buffers and slow screen-scrolling rates. When scrolling or processing escape sequences, the terminal drops its **CTS (Clear To Send)** line LOW to signal: *"Pause transmission, my screen buffer is full!"*
    * With dedicated CTS/RTS handshaking, the STM32 catches FPGA output characters into a ring buffer and automatically pauses UART transmission while CTS is deasserted, resuming instantly when the terminal is ready.
* **Simultaneous USB & UART Operation:**
    * On the STM32F072, `PA11` and `PA12` are dedicated to USB Full-Speed (`USB_DM` / `USB_DP`).
    * By assigning `USART1` data to `PA9`/`PA10` and mapping flow control to Port D, USB CDC and the hardware UART operate **simultaneously without pin conflicts**:
        * **`PA9`**: `USART1_TX` (Transmit Data to Terminal)
        * **`PA10`**: `USART1_RX` (Receive Data from Keyboard)
        * **`PD11`**: `USART1_CTS` (Clear To Send — input from Terminal)
        * **`PD12`**: `USART1_RTS` (Request To Send — output to Terminal)
* **Bonus — Emergency ROM Bootloader:**
    * ST's built-in factory ROM bootloader natively listens on **`PA9`/`PA10`** when `BOOT0 = 1`. If USB is unavailable, users can unbrick or flash firmware directly over this header using any standard USB-UART adapter.

---

### Rev 1.2 Pin Budget & Assignment Table

| Subsystem | Signal Name | STM32 Pin | Alternate Function / Role |
| :--- | :--- | :--- | :--- |
| **FPGA JTAG** | `TCK` | **`PD8`** | Clock input to MachXO2 JTAG engine |
| **FPGA JTAG** | `TMS` | **`PD9`** | Mode select input to MachXO2 |
| **FPGA JTAG** | `TDI` | **`PD10`** | Serial data input to MachXO2 |
| **FPGA JTAG** | `TDO` | **`PF6`** | Serial data output from MachXO2 |
| **SPI Flash** | `FLASH_CS` | **`PD13`** | Chip select for W25Q128 Flash |
| **SPI Flash** | `FLASH_SCK` | **`PD14`** | SPI master clock (up to 18 MHz) |
| **SPI Flash** | `FLASH_MISO` | **`PD15`** | SPI master in / slave out |
| **SPI Flash** | `FLASH_MOSI` | **`PF9`** | SPI master out / slave in |
| **Terminal UART** | `USART1_TX` | **`PA9`** | Transmit data (also ST ROM bootloader TX) |
| **Terminal UART** | `USART1_RX` | **`PA10`** | Receive data (also ST ROM bootloader RX) |
| **Terminal UART** | `USART1_CTS` | **`PD11`** | Hardware flow control input (AF0) |
| **Terminal UART** | `USART1_RTS` | **`PD12`** | Hardware flow control output (AF0) |
| **USB Interface** | `USB_DM` | **`PA11`** | Dedicated USB-C data minus line |
| **USB Interface** | `USB_DP` | **`PA12`** | Dedicated USB-C data plus line |
