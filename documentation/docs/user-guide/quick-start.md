# Quick Start Guide

Getting started with Brainfuino is remarkably simple. Because the board uses standard USB CDC (Virtual COM Port) emulation, no proprietary drivers or software suites are needed. You can connect, upload code, and control the hardware directly from your browser using the built-in **Web Serial Terminal**!

---

## 1. Connect Hardware

1. Plug a standard **USB-C data cable** into your computer and the Brainfuino.
2. The onboard power LEDs will illuminate, and your operating system will automatically recognize the board as a USB Virtual COM Port (e.g. `COM3` on Windows, `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem...` on macOS). No driver installation is required.

---

## 2. Launch the Web Serial Terminal

The easiest way to interact with Brainfuino is the zero-install **Web Serial Terminal** running natively in your browser:

<div class="grid cards" markdown="1">

-   [:octicons-link-external-16: **Launch Web Terminal**](../web-terminal.html){ .md-button .md-button--primary target="_blank" }

    ---

    Open the full-screen terminal in Google Chrome, Microsoft Edge, Opera, or Brave.

</div>

1. Click **Launch Web Terminal** above (or open the standalone terminal).
2. Click the **Connect Brainfuino** button in the top toolbar.
3. Select your Brainfuino from the browser's device list and click **Connect**.
4. You are live! The terminal window will immediately receive output streamed from the FPGA.

??? info "Prefer a Classic Serial Terminal? (PuTTY, Tera Term, screen, minicom)"
    Brainfuino uses standard USB CDC serial, so any serial terminal works right out of the box!
    
    * **Windows:** [PuTTY](https://putty.org/) or [Tera Term](https://ttssh2.osdn.jp/)
    * **Linux:** `screen /dev/ttyACM0 115200` or `minicom -D /dev/ttyACM0`
    * **macOS:** `screen /dev/cu.usbmodem* 115200`
    
    **Connection Settings:**
    
    | Setting | Value |
    | :--- | :--- |
    | **Port** | Assigned Virtual COM Port |
    | **Baud Rate** | Any (Hardware USB CDC ignores baud settings) |
    | **Data Bits** | 8 |
    | **Parity** | None |
    | **Stop Bits** | 1 |
    | **Flow Control** | None |
    
    *Tip for Tera Term / PuTTY:* If incoming text steps diagonally across your screen without returning to the left margin, enable **Implicit CR in every LF** (PuTTY: *Terminal* settings) or set **New-Line Receive** to **AUTO** (Tera Term: *Setup → Terminal*).

---

## 3. Run the Installed Program

1. Ensure the small **BOOT jumper** on the board is set to normal run mode.
2. Tap the hardware **Reset Button** on the Brainfuino board (or click the **`!RST`** button in the Web Terminal toolbar).
3. The FPGA soft-processor resets its Program Counter (`pc = 0`), resets its Data Pointer (`p = 0`), and begins executing the program stored in parallel Flash ROM.
4. The board immediately computes in hardware and streams ASCII characters directly to your terminal!

---

## 4. Uploading a New Brainfuck Program

The companion STM32 coprocessor continuously monitors incoming serial data. Uploading code requires zero programming dongles—you can use any of these methods:

=== "Drag & Drop (Web Terminal)"
    1. Simply drag any `.b` or `.bf` file from your computer and drop it anywhere onto the Web Terminal window.
    2. The terminal streams the file directly into parallel Flash ROM and auto-launches it immediately!

=== "Write Code File Button"
    1. Click the **Write Code File** button in the Web Terminal toolbar.
    2. Select any Brainfuck source file (`.b` or `.bf`) from your file browser.
    3. The code is written into Flash ROM and begins executing right away.

=== "Direct Copy & Paste"
    1. Open any Brainfuck source file in your text editor and copy the code to your clipboard.
    2. Click inside your terminal window and **paste** the code (`Ctrl+V` or right-click).
    3. The STM32 automatically detects the incoming Brainfuck characters, halts the FPGA in reset, erases the parallel Flash ROM sectors, writes the new program byte-by-byte, and auto-launches it!

```brainfuck title="Try this Simple Hello World Example"
+[-->-[>>+>-----<<]<--<---]>-.>>>+.>>..+++[.>]<<<<.+++------.<<<.>>>>+.
```

---

## 5. Live Interactive Controls

While your program is running, the coprocessor provides live keyboard controls:

* **Dynamic Clock Speed Stepping:** Press **`PageUp`** to speed up or **`PageDown`** to slow down the FPGA clock in real time (stepping through 25 precision frequencies from 10 Hz up to 12 MHz). You can also send `!clk+` or `!clk-`.
* **Interactive Menu:** Click the **Menu** button in the Web Terminal (or send `!MENU`) to open the VT100 configuration interface. Here you can load programs from the onboard Flash library, tune settings, or pick terminal text colors.
* **Soft Reset:** Click **`!RST`** (or send `!RESET`) to restart execution from address 0 without power cycling.

---

## 6. Helpful Tips & Program Halting

??? tip "Automatic Program Halting (Append Endless Loop)"
    **Automatic Halting Enabled by Default:**
    
    The FPGA soft-processor fetches and executes instructions from ROM continuously. Without a halt loop, once execution reaches the end of your program, it would march through unprogrammed ROM (`0xFF`) until address 262,143, wrap back around to address 0, and restart the program.
    
    Brainfuino solves this automatically! The companion firmware includes an **Append Endless Loop** configuration option (enabled by default as `[-]+[]` in the [Configuration Menu](config-menu.md)). Whenever you upload code or load from the Program Library, the STM32 automatically appends the robust halt idiom to the end of your code before flashing.
    
    **Recommendation:** Keep **Append Endless Loop** enabled (`[-]+[]`). You do not need to add manual halt loops to your Brainfuck programs—any standard Brainfuck code will halt cleanly at EOF!
    
    **Behind the Scenes: The Robust `[-]+[]` Idiom:**
    Many online tutorials suggest ending programs with `+[]`. However, if your program finishes with the current tape cell containing **255**, adding 1 wraps to **0** (`255 + 1 = 0`). The loop `[]` checks if the cell is non-zero, sees `0`, skips right past the loop, and restarts anyway! Brainfuino's auto-injected `[-]+[]` idiom zeroes the cell first, adds 1, and enters an unbreakable infinite halt loop.
