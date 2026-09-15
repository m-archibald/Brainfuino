# Terminal Commands & Interactive Controls

The FPGA soft-processor (`brainfuck_uP`) is pure hardware. It doesn't have a USB stack, an operating system, or a serial terminal interface. If you ran the FPGA standalone, you would need an external programmer to burn ROM chips, a function generator to pump clock signals, and a logic analyzer just to see your program's output.

To make the system actually user-friendly, the Brainfuino includes an **STM32F072 ARM Cortex-M0 coprocessor**. The STM32 acts as the FPGA's dedicated digital butler: generating clock signals, managing USB Virtual COM communication, writing programs to Flash, and translating parallel FPGA pulses into readable serial characters.

---

## Single-Key Shortcuts

When you send an individual character (payload length under 3 bytes), the STM32 intercepts it and performs the following actions:

### Clock Frequency Selection (Keys `1` – `7`)

--8<-- "snippets/clock-speeds.md"

### Buffer & Memory Inspection (Keys `!` and `@`)

| Key | Function | Description | Terminal Response |
| :---: | :--- | :--- | :--- |
| `!` | Dump Program | Reads back the program code currently residing in parallel Flash ROM. | Hex / ASCII dump |
| `@` | Retransmit Buffer | Re-sends the recent serial stream captured from the FPGA output port buffer. | Flushes output box |

![PuTTY Terminal showing clock frequency switches and Flash ROM code dump](../imgs/putty-brainfuino-logo.png)

---

## When the FPGA is "Too Fast": The Serial Bottleneck

??? warning "High Clock Speeds & Missed Characters"
    The Lattice MachXO2 FPGA soft-processor processes bytes with extreme speed. However, serial transmission over USB is fundamentally serialized. 
    
    When executing **write-intensive programs**—programs that spam the `.` output command in tight loops (such as Mandelbrot fractals, large ASCII art dumps, or matrix calculations)—the FPGA can produce parallel bytes on its output bus faster than the STM32 can packetize and transmit them over USB CDC.
    
    * **At 8 MHz and above:** Missed or dropped characters can occur if output generation is continuous.
    * **At lower clock speeds (500 kHz to 6 MHz):** Output is generally rock-solid, though extremely write-dense code loops can occasionally experience buffer saturation.
    
    **Roadmap Solution:**
    We are implementing a **smart variable clock rate** ([read roadmap details](../roadmap.md#smart-variable-clock-rate)). In this mode, the FPGA runs at full maximum speed (up to 48 MHz) for computation, but whenever the `.` instruction is executed, the STM32 will briefly hold or slow the clock pulses until its serial transmission buffer is emptied, guaranteeing 100% character fidelity.

---

## Interactive Program Input (`,` Command)

Brainfuck is not just a language for printing static text—it can take live input from your keyboard using the `,` command!

When your Brainfuck program reaches a `,` instruction:

1. The FPGA pauses and asserts a read request (`portRD`).
2. The STM32 places the ASCII key you type into the FPGA's 8-bit input port (`inPort[7:0]`) and asserts the `BF_INCMG` flag.
3. The FPGA accepts the byte and saves it directly into the memory tape cell pointed to by `p`.

### What Can You Build with Interactive Input?
* **Game of Life:** Use the keyboard to place cells, advance generations, or change rules.
* **Interactive Text Adventures:** Type direction commands (`n`, `s`, `e`, `w`) to navigate rooms and solve puzzles.
* **Terminal Mirror / Filter:** Build a program that transforms your typed input (e.g. converting lowercase to uppercase) and echoes it back in real time.

---

## Uploading New Code

When you paste Brainfuck code into the terminal emulator:

1. The USB CDC packet length is detected as **3 or more bytes**.
2. The STM32 automatically detects an incoming payload and enters `STATE_PROGRAM`:
    * It asserts the FPGA reset line low (`BF_RST = 0`), pausing soft-processor execution.
    * It illuminates the programming status LED.
    * It clears existing ROM contents.
    * It writes the new code byte-by-byte into consecutive parallel Flash ROM addresses.
3. Upon completion, the terminal displays:
   ```text
   Wrote 142 bytes
   ```
4. Press the hardware **Reset** button to start executing your new code.

---

## Extended Keys & The Future "Program Mode"

??? tip "Extended Keys & ASCII Gotcha"
    Because the firmware currently separates single-key commands from code uploads based on packet size (`len < 3`), pressing special keyboard keys can inadvertently trigger a program write:
    
    * Keys like ++insert++, ++page-up++, ++page-down++, or arrow keys transmit multi-byte ANSI escape sequences (e.g. `\x1b[2~`).
    * The STM32 can mistake this for a short code paste and write those bytes to address `0` in Flash ROM!
    * If this happens, simply re-paste your intended Brainfuck program and hit Reset.

??? note "Roadmap: Dedicated Program Mode"
    To eliminate this issue and allow interactive programs to freely use any keystrokes (including number keys) without accidentally switching clock frequencies or triggering code writes, we are updating the firmware with a **dedicated Program Mode** ([read roadmap details](../roadmap.md#dedicated-program-mode-3-second-button-hold)). 
    
    In this upcoming update, uploading code or adjusting clock speeds will require holding down a button for 3 seconds to enter Program Mode. Normal terminal keystrokes will pass cleanly through to the running Brainfuck application.
