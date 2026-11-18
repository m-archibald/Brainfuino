# Terminal Commands & Interactive Controls

The FPGA soft-processor (`brainfuck_uP`) is pure hardware. It doesn't have a USB stack, an operating system, or a serial terminal interface. If you ran the FPGA standalone, you would need an external programmer to burn ROM chips, a function generator to pump clock signals, and a logic analyzer just to see your program's output.

To make the system user-friendly, the Brainfuino includes an **STM32F072 ARM Cortex-M0 coprocessor**. The STM32 acts as the FPGA's dedicated digital butler: synthesizing precision clock frequencies, managing USB Virtual COM communication, writing programs to Flash, and translating parallel FPGA pulses into serial terminal characters.

---

## Runtime Hotkeys & Clock Control

While a Brainfuck program is executing in standard Run Mode, the coprocessor listens for real-time control hotkeys:

### Dynamic Clock Speed Stepping
You can adjust the FPGA master clock dynamically while code is actively computing:

* **`PageUp`:** Step clock speed up to the next higher frequency.
* **`PageDown`:** Step clock speed down to the next lower frequency.
* **Alternative Hotkey Modes:** Configurable via the [Configuration Menu](config-menu.md) to use **`Up`/`Down` Arrow keys** or **`+`/`-` keys** instead.

The coprocessor provides 25 precision hardware clock speeds spanning 6 orders of magnitude (from 10 Hz for human-speed register observation up to 12.0 MHz maximum parallel ROM speed):

--8<-- "snippets/clock-speeds.md"

---

## System Commands (Prefix `!`)

You can send commands directly over the serial connection (via the Web Terminal command bar or any serial console) at any time:

### Core Hardware Controls

| Command | Action | Description |
| :--- | :--- | :--- |
| **`!RESET`** or **`!RST`** | Soft Reset FPGA | Pulses the FPGA soft-processor reset line (`BF_RST`) and restarts execution from address 0. |
| **`!MENU`** or **`!CONFIG`** | Configuration Menu | Enters the interactive multi-page VT100 configuration menu. |
| **`!CLK+`** / **`!CLOCK+`** | Clock Step Up | Steps the FPGA clock up one frequency tier silently. |
| **`!CLK-`** / **`!CLOCK-`** | Clock Step Down | Steps the FPGA clock down one frequency tier silently. |
| **`!CLK`** / **`!CLOCK`** | Clock Speed Query | Prints the active clock frequency (e.g. `[Clock: 12.0 MHz]`). |
| **`!DUMP`** | Dump Program ROM | Streams the raw Brainfuck program currently stored in parallel Flash ROM over USB serial in non-blocking 64-byte chunks. |
| **`!DFU!`** | Software DFU Reboot | Reboots the STM32 into its built-in USB DFU Bootloader mode for firmware updates without touching hardware jumpers. |

### Scriptable Program Library Commands
The onboard Program Library can also be queried and controlled directly via commands:

| Command | Action | Description |
| :--- | :--- | :--- |
| **`!LIB*`** or **`!LIB:LIST`** | Query Library | Outputs the list of all installed programs, slots, and Flash memory usage. |
| **`!LIB:RUN:<slot>`** | Run Program | Loads the program from the specified slot (e.g. `!LIB:RUN:1`) into parallel Flash and auto-executes. |
| **`!LIB:DUMP:<slot>`** | Dump Program | Streams the raw Brainfuck code of the specified library slot over serial. |
| **`!LIB:ADD:<name>`** | Add Program | Enters library upload mode with 10-second FPGA execution verification before committing to Flash. |
| **`!LIB:ADD_NOVERIFY:<name>`**| Direct Add | Uploads code directly to the library without verification countdown. |
| **`!LIB:DEL:<slot>`** | Delete Program | Deletes the program at the specified slot using zero-erase tombstone marking. |
| **`!COMMIT`** | Early Commit | Commits an uploaded library program immediately during verification. |
| **`!CANCEL`** | Abort Add | Cancels library program upload and restores normal run state. |

---

## Manual Stepping Mode (Single-Stepping & Burst Stepping)

When **Manual Stepping Mode** is enabled via the [Configuration Menu](config-menu.md), the FPGA master clock remains frozen until triggered by the configured keyboard shortcut:

* **Configurable Trigger Key:** Spacebar, Tab, or Enter.
* **Configurable Step Multiplier:** 1, 10, 100, 1,000, 10,000, or 100,000 clock ticks per keypress.
* **Burst Accumulation:** Keystroke bursts accumulate in an internal queue and drain cleanly without dropping ticks, allowing frame-by-frame observation of Brainfuck execution.

---

## When the FPGA is "Too Fast": The Serial Bottleneck

??? warning "High Clock Speeds & Missed Characters"
    The Lattice MachXO2 FPGA soft-processor executes instructions with extreme speed in silicon. However, serial transmission over USB is fundamentally serialized. 
    
    When executing **write-intensive programs**—programs that spam the `.` output command in tight loops (such as Mandelbrot fractals, large ASCII art dumps, or matrix calculations)—the FPGA can produce parallel bytes on its output bus faster than the STM32 can packetize and transmit them over USB CDC.
    
    * **Zero Dropped Characters:** The firmware includes **Smart Hardware Clock-Pausing** ([read architecture details](../firmware/architecture.md#smart-hardware-clock-pausing-output-throttling)). The STM32 automatically freezes the FPGA master clock in 1 CPU instruction upon output strobe assertion, samples data with zero race conditions, and applies hardware backpressure if the USB buffer nears capacity.
    * **Operating Speeds:** Supported across all 25 clock frequencies from 10 Hz up to 12 MHz with 100% character fidelity.

---

## Interactive Program Input (`,` Command)

Brainfuck can take live input from your keyboard using the `,` command:

When your Brainfuck program reaches a `,` instruction:

1. The FPGA pauses execution and asserts a read request (`portRD`).
2. The STM32 places the ASCII key you type into the FPGA's 8-bit input port (`inPort[7:0]`) and asserts the `BF_INCMG` flag.
3. The FPGA accepts the byte and saves it directly into the memory tape cell pointed to by `p`.
4. **Automatic Newline Translation:** The STM32 translates Windows `\r\n` pairs into standard `\n` (`0x0A`), ensuring consistent behavior across Windows, Linux, and macOS.

---

## Hardware Button Hold Thresholds

The physical button on the Brainfuino uses a 35 ms hardware debounce filter. The coprocessor measures the steady hold duration to provide intuitive 4-tier control:

| Action | Duration | Red LED Feedback | Result |
| :--- | :---: | :---: | :--- |
| **Short Tap** (Run Mode) | < 1 second | **70 ms Blip** | Resets the running FPGA soft-processor (`BF_RST` pulse) and restarts execution. |
| **Short Tap** (Config Mode) | < 1 second | Turns OFF | Exits configuration menu, saves settings, and resumes execution. |
| **Short Tap** (Program Mode) | < 1 second | Turns OFF | Exits Program Mode and executes the program currently in ROM. |
| **Program Mode Hold** | 1 – 3 seconds | **Solid ON** | Enters **Dedicated Program Mode** (ready for Brainfuck code paste). |
| **Config Menu Hold** | 3 – 8 seconds | **Smooth Breathing / Pulse** | Enters **[Configuration Menu](config-menu.md)** (`STATE_CONFIG`) for tuning clock, hotkeys, and features. |
| **Factory Demo Restore** | ≥ 8 seconds | **Rapid Strobe** (50 ms) | Erases parallel ROM, restores official Brainfuino ASCII logo demo, and auto-launches it! |
