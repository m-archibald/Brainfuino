# Brainfuino: Native Hardware Brainfuck

> *The Brainfuino is an FPGA-based development board that doesn’t just emulate Brainfuck code—it runs it straight in hardware.*

## How It Works
ASCII Brainfuck characters are written directly to ROM, and the FPGA based micro-processor runs it directly with no conversions. 

Inside the FPGA, a continuously updating state machine transforms bytes of ASCII data in ROM and RAM in a constant stream of Brainfuck logic. Each clock pulse sent by the accompanying STM32 coprocessor advances the state machine by one. 

Because Brainfuck is an 8-bit (1-byte) language, streams of data flow through the FPGA in byte-large parallel chunks. If you have an oscilloscope fast enough, you can watch these bytes fly by on the Brainfuino output pins, or—much easier—read the bytes as serialized data over USB from the STM32. As far as Brainfuck in hardware goes, you can’t get much better than this.

## Project Origins
The Brainfuino project is the brainchild of the brilliant Eduardo Corpeño, a talented electrical and computer engineer and devoted university professor of 20+ years. Eduardo specializes in FPGAs and embedded systems, and shares his knowledge through a suite of online LinkedIn Learning courses.

I first came across this project five years ago when my sister, Thalia, showed it to me. She currently studies compilers at the University of Utah and was involved in the recovery of Unix v4. We both share an enjoyment for the esoteric and tackling things just for the challenge of it. You can find her impressive array of projects on her website and GitHub.

## The Team
The Brainfuino has become a passion project for my sister and I:

* **Matthew (Hardware):** I specialize in the hardware side, including the PCB design, production, programming, and case design.
* **Thalia (Software):** Thalia brings a rigorous background in software. She maintains a large repository of Brainfuck programs and deeply understands the Brainfuino software architecture.

## 🛠️ Documentation & Resources
If you have come here looking for documentation: Welcome! I hope you can find what you need here. 

*(Note: Documentation is currently being updated. Below are quick links to the project files)*

* [Quick Start (for those with a Brainfuino in Hand)](../docs/user-guide/quick-start.md)

* [Hardware BOM & Gerber Files](/brainfuino-PCB/Rev%201.1%20-%20KiCad/jlcpcb/production_files/)

* [Lattice FPGA Toolchain & State Machine Logic](/brainfuck_uP-FPGA-softprocessor/)

* [STM32 Coprocessor Firmware](/companion-STM32-firmware/Debug/BrainfuinoMCU.hex)

* [Brainfuck Program Repository](https://github.com/thaliaarchi/bfcorpus)


---
*Feel free to reach out if you have questions or would like help!*

```matthewarch314@gmail.com```