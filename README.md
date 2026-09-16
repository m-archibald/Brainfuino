# Brainfuino [-]+.

```text
  ____            _        __       _           
 | __ ) _ __ __ _(_)_ __  / _|_   _(_)_ __   ___  
 |  _ \| '__/ _` | | '_ \| |_| | | | | '_ \ / _ \ 
 | |_) | | | (_| | | | | |  _| |_| | | | | | (_) |
 |____/|_|  \__,_|_|_| |_|_|  \__,_|_|_| |_|\___/ 
```

**An Arduino-like development board that executes raw Brainfuck natively in silicon.**

[<img src="./documentation/docs/imgs/bfboard2.png" alt="The Brainfuino [-]+." width="700"/>](https://youtu.be/QloNq8AoHvU)

[![Documentation](https://img.shields.io/badge/docs-MkDocs%20Material-526cfe?style=for-the-badge&logo=materialformkdocs)](https://m-archibald.github.io/Brainfuino/)
[![GitHub Pages](https://img.shields.io/badge/GitHub%20Pages-Online%20Docs-success?style=for-the-badge&logo=github)](https://m-archibald.github.io/Brainfuino/)
[![GitHub Release](https://img.shields.io/github/v/release/m-archibald/Brainfuino?style=for-the-badge&color=orange)](https://github.com/m-archibald/Brainfuino/releases)
[![YouTube Demo](https://img.shields.io/badge/YouTube-Video%20Demo-red?style=for-the-badge&logo=youtube)](https://youtu.be/QloNq8AoHvU)
[![Hackaday](https://img.shields.io/badge/Hackaday-Project%20Log-black?style=for-the-badge&logo=hackaday)](https://hackaday.io/project/176757-brainfuino)

> ### 📖 [Read the Full Documentation](https://m-archibald.github.io/Brainfuino/)
>
> Comprehensive guides covering quick start, hardware architecture, FPGA toolchain, DirtyJTAG / Diamond flashing, STM32 firmware, 3D printing, and Brainfuck programming are live at **[https://m-archibald.github.io/Brainfuino/](https://m-archibald.github.io/Brainfuino/)**!

---

## What is Brainfuino?

Most programmers who discover [Brainfuck](https://en.wikipedia.org/wiki/Brainfuck)—the famous 1993 esoteric language created by Urban Müller with only eight single-character commands (`+`, `-`, `<`, `>`, `[`, `]`, `.`, `,`)—write a quick software interpreter, marvel at its Turing completeness, and move on.

**Brainfuino takes the concept to bare-metal hardware.**

The **Brainfuino [-]+.** (pronounced *"Brainfuino Uno"*) is an open-source development board built around a custom Verilog soft-processor called [**`brainfuck_uP`**](./brainfuck_uP-FPGA-softprocessor/).

There is no compiler, no bytecode, and no software emulation layer. When you send a Brainfuck source file to the board, the **plain ASCII text** (`0x2B` for `+`, `0x2D` for `-`, `0x5B` for `[`, etc.) is burned directly into physical Flash memory. The FPGA soft-processor fetches those raw ASCII characters from ROM and executes them instruction-by-instruction across a physical SRAM tape in hardware logic gates.

---

## Hardware Architecture

The Brainfuino pairs a high-speed FPGA soft-processor with a modern ARM microcontroller and dedicated parallel memories in a classic Arduino Uno form factor:

- **Lattice MachXO2 FPGA Core:** The `brainfuck_uP` soft-processor is synthesized in a Lattice MachXO2 FPGA (LCMXO2-1200HC in a TQFP-100 package). It implements the complete Brainfuck instruction decoder, data pointer tracking, bracket matching state machine, and I/O registers in pure digital logic.
- **True Harvard Architecture:** The processor features two completely independent parallel buses for program memory and the tape:
  - **Program Memory (ROM):** 256 kB parallel Flash ROM chip storing raw ASCII Brainfuck instructions.
  - **Data Tape (RAM):** 128 kB parallel high-speed SRAM chip acting as the physical execution tape.
- **STM32F072 Companion Coprocessor:** An ARM Cortex-M0 MCU serves as the host interface and system controller:
  - **USB Virtual COM Port (CDC):** Provides seamless serial terminal input/output.
  - **Programmable Clock Generation:** Synthesizes the master clock signal for the FPGA, dynamically tunable from 500 kHz up to 48 MHz.
  - **In-System Flash Programmer:** Receives Brainfuck source code over USB and burns it into the parallel Flash ROM on the fly with no external chip programmers required.
  - **Analog Input:** Donates an on-chip 12-bit ADC channel to bring analog sensing capabilities to Brainfuck.
- **5V Level Shifting & Shield Compatibility:** Dedicated high-speed bus transceivers (74ALVC164245) bridge the FPGA's 3.3V logic to 5V TTL, maintaining full electrical and mechanical compatibility with Arduino Uno shields.
- **In-System Programming Headers:** Convienient DFU mode jumper for flashing the STM32 firmware and headers for flashing the FPGA bitstream (JTAG via Lattice Diamond or openFPGALoader).

---

## Why Brainfuino?

This project is an homage to esoteric programming languages: part technical feat, part educational platform, and part delightful geek toy.

If we look at the Arduino Uno as a playful benchmark, Brainfuino delivers:

1. **Native Silicon Execution:** You are not running an interpreter or a virtual machine inside a C program. Raw ASCII bytes directly drive digital logic gates.
2. **One of a Kind:** Brainfuino is the only dedicated physical development board built from the ground up to execute native Brainfuck in silicon.
3. **Arduino Uno Shield Compatibility:** Standard Uno pin headers, 5V level-shifted I/O, and analog input let you connect real sensors, relays, displays, and shields to an 8-instruction computer.
4. **Transparent Computer Architecture:** A complete, accessible study in computer engineering—connecting Verilog soft-processor design, FPGA synthesis, memory bus arbitration, embedded firmware, and compiler theory.
5. **Pure Bragging Rights:** Writing an algorithm or a fractal generator in 8 single-character instructions and seeing it calculate in real silicon hardware provides unmatched hacker satisfaction and bragging rights. You certainly won't get that with an Arduino!

---

## Three Languages in One Project

Three different programming paradigms come together to bring Brainfuino to life:


| Language        | Layer                    | Purpose                                                                                                                              |
| :-------------- | :----------------------- | :----------------------------------------------------------------------------------------------------------------------------------- |
| **Brainfuck**   | **Application**          | The end-user language. Programs are written in standard Brainfuck and run directly on the hardware tape.                             |
| **Verilog HDL** | **Processor Core**       | Defines the`brainfuck_uP` digital architecture, instruction decoder, and state machine synthesized into the FPGA bitstream (`.jed`). |
| **Embedded C**  | **Coprocessor Firmware** | Powers the STM32F072 microcontroller, managing USB CDC communications, clock synthesis, and ROM flashing.                            |

---

## Repository Structure

This repository is the consolidated monorepo for the entire Brainfuino project:

- [`brainfuino-PCB/`](./brainfuino-PCB/): Hardware schematics and board layouts for **Rev 1.0** (Eagle) and **Rev 1.1** (KiCad with JLCPCB fabrication Gerbers, BOM, and CPL).
- [`brainfuck_uP-FPGA-softprocessor/`](./brainfuck_uP-FPGA-softprocessor/): Lattice MachXO2 soft-processor in Verilog, pin constraints (`.lpf`), and Lattice Diamond project files.
- [`companion-STM32-firmware/`](./companion-STM32-firmware/): STM32F072 firmware (STM32CubeIDE project) handling USB CDC serial, clock generation, and ROM flashing.
- [`3D-printable-case/`](./3D-printable-case/): SolidWorks (`.SLDPRT`) and 3D printing CAD models (`.STEP`) for the custom enclosure.
- [`documentation/`](./documentation/): Comprehensive MkDocs Material documentation source files and project guides.

---

## Project Origins & Acknowledgements

The Brainfuino project was originally conceived, designed, and built by **Eduardo Corpeño** ([kuashio](https://github.com/kuashio)).

- **Video Demonstration:** [Brainfuino: Hardware Brainfuck Processor (YouTube)](https://youtu.be/QloNq8AoHvU)
- **Project Build Log:** [Brainfuino on Hackaday.io](https://hackaday.io/project/176757-brainfuino)
- **Original Soft-Processor:** [kuashio/brainfuck_uP](https://github.com/kuashio/brainfuck_up)
- **Original STM32 Firmware:** [kuashio/brainfuino-firmware](https://github.com/kuashio/brainfuino-firmware)
- **Brainfuck IDE:** [Visual brainfuck](https://sites.google.com/site/visualbf/)

Continued and maintained by:

- **Matthew Archibald:** Hardware modernization, KiCad Rev 1.1 redesign, JLCPCB manufacturing packages, 3D-printable case, and documentation.
- **Thalia Archibald:** Software architecture, compiler engineering, and curator of the [bfcorpus repository](https://github.com/thaliaarchi/bfcorpus) and [bfcoq](https://github.com/thaliaarchi/bfcoq).

---

### Ready to Build or Program?

Check out the official documentation for step-by-step assembly guides, bitstream flashing, and terminal operation:
**[https://m-archibald.github.io/Brainfuino/](https://m-archibald.github.io/Brainfuino/)**
