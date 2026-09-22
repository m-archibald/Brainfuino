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

You may have seen the Brainfuino [Hackaday project](https://hackaday.com/2021/01/12/make-room-for-a-new-arduino-competitor-with-native-brainfck/) released during COVID in 2021. Originally created by Eduardo Corpeño, it's a programming board that runs the esoteric programming language Brainfuck completely in hardware. The language is extremely minimal, with just 8 characters, yet it is entirely Turing complete. When uploading code to the Brainfuino, there is no conversion or compression, the ascii brainfuck characters are saved into rom exactly as they are written. Programming in Brainfuck is difficult, hilarious, and rewarding, and doing it with Brainfuino is even better.

---

## Two Processors: Pure Hardware and Quality of Life

There are two processors on the Brainfuino, the actual Brainfuck based soft processor implemented in hardware on the Lattice FPGA, and the STM32 which makes the whole thing user "friendly". Since the FPGA is configured basically as an old fashioned Harvard CPU, instructions move through the soft processor in a continuous stream of parallel bits, these flash into and out of existence, and the STM32 is there to catch them and serialize them so you can use it with a modern PC instead of an ancient parallel based terminal interface. The FPGA does all the hard work here though. When programs are run, all the output is being generated real time in hardware via the FPGA. The FPGA reads bytes of data from good old parallel flash program memory, and when it encounters bytes representing one of the 8 ascii characters used in Brainfuck it executes that with its built in soft processor based instruction set according to the character. Since it's a harvard style cpu, it also uses parallel based RAM to read and write data to and from for computation purposes. The STM32 uses its hardware timer to drive a clock pin to the FPGA at speeds from 500 khz up to 12 mhz. You may wonder if the STM32 is overkill, and your curiosity is valid, it probably is overkill, but by leveraging a separate microcontroller for these quality of life improvements, it allows the Brainfuck soft processor implemented in Verilog on the Lattice FPGA to remain pure and stay true to its purpose to compute Brainfuck. We leave the rest to the STM32: Usb communication, parallel to serial conversion, program memory flushing and prep, variable clock speed control, and an interactive terminal interface.

---

## Why Brainfuino?

As to answer the question of why? Well: The challenge. And to quote the original author Eduardo Corpeño, with the Brainfuino you get "bragging rights for writing code that works! You certainly won't get that from the Arduino."

---

## What's in Revision 1.1

My revision 1.1 of the Brainfuino has:
- Improved clock routing to the FPGA soft processor using an FPGA pin optimized for clock input rather than a generic GPIO
- Proper VBus to 3.3v connection on the STM32 increasing STM32 clock stability
- JLCPCB compatible parts list and reduced cost by switching to prestocked "basic" components.
- USB C!
- A nifty 3D printed case
- KiCAD design files (migrated from the original Autodesk Eagle)

See the original demo video: [Brainfuino: Hardware Brainfuck Processor (YouTube)](https://www.youtube.com/watch?v=QloNq8AoHvU)

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
