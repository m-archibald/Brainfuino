# Terminal Commands & Interactive Controls

The FPGA soft-processor (`brainfuck_uP`) is pure hardware. It doesn't have a USB stack, an operating system, or a serial terminal interface. If you ran the FPGA standalone, you would need an external programmer to burn ROM chips, a function generator to pump clock signals, and a logic analyzer just to see your program's output.

To make the system actually user-friendly, the Brainfuino includes an **STM32F072 ARM Cortex-M0 coprocessor**. The STM32 acts as the FPGA's dedicated digital butler: generating clock signals, managing USB Virtual COM communication, writing programs to Flash, and translating parallel FPGA pulses into readable serial characters.

---

## Single-Key Shortcuts

When you send an individual character (payload length under 3 bytes), the STM32 intercepts it and performs the following actions:

### Clock Frequency Selection (Keys `1` – `7`)

--8<-- "snippets/clock-speeds.md"

### System Commands (Prefix `!`)

| Command | Action | Description |
| :---: | :--- | :--- |
| `!RESET` / `!RST` | Reset FPGA | Pulses the FPGA soft-processor reset line (`BF_RST`) and announces `[bf_µP reset] <speed>`. |
| `!DFU!` | Software DFU Reboot | Reboots STM32 into built-in USB DFU Bootloader mode for firmware updates without moving the `BOOT` jumper. |
| `!MENU!` / `!CONFIG!` | Config Menu | Opens interactive configuration and clock tuning menu. |

---

### Manual Stepping Mode (Single-Stepping & Burst Stepping)

When **Manual Stepping Mode** is enabled via the [Configuration Menu](config-menu.md#7-manual-stepping-mode--disabled--enabled-), the FPGA master clock remains stopped until triggered by the configured keyboard shortcut:

* **Configurable Trigger Key:** Spacebar, Tab, or Enter.
* **Configurable Step Multiplier:** 1, 10, 100, 1,000, 10,000, or 100,000 clock ticks per keypress.
* **Burst Accumulation:** If you hold or spam the key rapidly, keystroke bursts accumulate in an internal queue and drain cleanly without dropping ticks, allowing frame-by-frame observation of Brainfuck execution.

---

## When the FPGA is "Too Fast": The Serial Bottleneck

??? warning "High Clock Speeds & Missed Characters"
    The Lattice MachXO2 FPGA soft-processor processes bytes with extreme speed. However, serial transmission over USB is fundamentally serialized. 
    
    When executing **write-intensive programs**—programs that spam the `.` output command in tight loops (such as Mandelbrot fractals, large ASCII art dumps, or matrix calculations)—the FPGA can produce parallel bytes on its output bus faster than the STM32 can packetize and transmit them over USB CDC.
    
    * **Zero Dropped Characters:** The firmware includes **Smart Hardware Clock-Pausing** ([read architecture details](../firmware/architecture.md#smart-hardware-clock-pausing-output-throttling)). The STM32 automatically freezes the FPGA master clock in 1 CPU instruction upon output strobe assertion, samples data with zero race conditions, and applies hardware backpressure if the USB buffer nears capacity.
    * **Operating Speeds:** Supported across all 13 clock frequencies from 62.5 kHz up to 12 MHz with 100% character fidelity.

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

## Uploading New Code (Dedicated Program Mode)

To upload new Brainfuck code into Flash ROM:

1. **Enter Program Mode:**
   * Press and hold the hardware button for **3 seconds**.
   * The **Red LED** turns on at the 3-second mark to confirm you can release the button.
   * The FPGA is held in reset (`BF_RST = 0`) to release the ROM bus, and the terminal displays:
     ```text
     === BRAINFUINO PROGRAM MODE ===
     Paste Brainfuck code now (up to 256 kB)...
     ```
2. **Paste Your Code:**
   * Simply paste your Brainfuck code into the terminal emulator.
   * **Programs $\le$ 4 kB:** The STM32 buffers the code in SRAM. When reception pauses for **100 ms**, it prints the total size, erases ROM, fast-writes to Flash while reporting live percentage progress (`Writing: 50% (2048 / 4096 bytes)...`), verifies against RAM, appends `[-]+[]`, and auto-launches.
   * **Programs > 4 kB (up to 256 kB):** If the 4 kB buffer fills, the firmware automatically announces:
     ```text
     Program larger than RAM buffer. Streaming directly to Flash...
     ```
     It erases ROM, writes the first 4 kB, and streams subsequent incoming packets straight into Flash using USB CDC hardware NAK flow control, displaying cumulative bytes written (`Wrote 5120 bytes...`, `Wrote 6144 bytes...`).
3. **Run Your Code:**
   * Flashing automatically appends an endless loop (`[-]+[]`) to halt the FPGA program counter cleanly at EOF, and auto-launches the new code immediately!
   * You can also give the button a **short press (< 3 seconds)** at any time to soft-reset the program.

---

## Hardware Button Behavior

The Brainfuino button utilizes a 35 ms hardware debounce filter to eliminate tactile contact chatter. The coprocessor measures the steady hold duration to provide intuitive 4-tier control:

| Action | Duration | Red LED Feedback | Result |
| :--- | :---: | :---: | :--- |
| **Short Press** (Run Mode) | < 3 seconds | **70 ms Blip** | Resets the running FPGA soft-processor (`BF_RST` pulse) and prints `[bf_µP reset] <speed>` (e.g. `[bf_µP reset] 500 kHz`). |
| **Short Press** (Config Mode) | < 3 seconds | Turns OFF | Exits configuration menu, saves settings, and resumes execution. |
| **Short Press** (Program Mode) | < 3 seconds | Turns OFF | Exits Program Mode and executes the program currently in ROM. |
| **Program Mode Hold** | 3 – 6 seconds | **Solid ON** | Enters **Dedicated Program Mode** (ready for Brainfuck code paste). |
| **Config Menu Hold** | 6 – 10 seconds | **Smooth Breathing / Pulse** | Enters **[Configuration Menu](config-menu.md)** (`STATE_CONFIG`) for tuning clock, hotkeys, and features. |
| **Factory Demo Restore** | $\ge$ 10 seconds | **Rapid Strobe** (50 ms) | Erases parallel ROM, restores official Brainfuino ASCII logo demo, and auto-launches it! |

