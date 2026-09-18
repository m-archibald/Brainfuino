# Quick Start Guide

Getting started with Brainfuino is remarkably simple. Because the board uses standard USB CDC (Virtual COM Port) emulation, no proprietary drivers or software suites are needed—just a serial terminal like PuTTY, Tera Term, minicom, or screen.

---

## 1. Connect Hardware

1. Plug a **USB-C data cable** into your computer and the Brainfuino.
2. The onboard power LEDs will illuminate, and your computer will detect an **STM32 Virtual COM Port** (e.g., `COM3` on Windows, `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem...` on macOS).

---

## 2. Open a Serial Terminal

Open your serial terminal emulator of choice:

* **Windows:** [PuTTY](https://putty.org/) or [Tera Term](https://ttssh2.osdn.jp/)
* **Linux:** `screen /dev/ttyACM0 115200` or `minicom -D /dev/ttyACM0`
* **macOS:** `screen /dev/cu.usbmodem* 115200`

### Recommended Settings

| Setting | Value |
| :--- | :--- |
| **Port** | Assigned Virtual COM Port |
| **Baud Rate** | Any (Hardware USB CDC ignores baud settings) |
| **Data Bits** | 8 |
| **Parity** | None |
| **Stop Bits** | 1 |
| **Flow Control** | None |

??? tip "Preventing 'Stair-Stepping' in Serial Terminals"
    If incoming text steps diagonally across your terminal screen without returning to the left margin on newlines:
    * **In Tera Term:** Go to **Setup → Terminal → New-Line → Receive** and select **AUTO**.
    * **In PuTTY:** Under **Terminal**, check **Implicit CR in every LF**.
    
    ![Tera Term New-Line Receive Auto Setting](../imgs/teraterm-newline-settings.png)

---

## 3. Run the Installed Program

1. Ensure the **BOOT jumper** is moved back to normal run mode so the STM32 can boot.
2. Press the **Reset Button** on the Brainfuino board.
3. The FPGA soft-processor resets its Program Counter (`pc = 0`), resets its Data Pointer (`p = 0`), and begins executing the program stored in parallel Flash ROM.
4. The board immediately executes the program and streams output directly to your serial console!

---

## 4. Uploading a New Brainfuck Program

The companion STM32 coprocessor monitors incoming serial packets: any pasted text will automatically be written to the parallel Flash memory.

1. Open any Brainfuck source file in your text editor and copy the code to your clipboard.
2. **Right-click** in your terminal emulator (e.g. Tera Term or PuTTY) to paste the code in.
3. When the incoming packet length is **3 or more bytes**, the STM32 automatically:
    * Pulls the FPGA reset line low (`BF_RST = 0`), pausing the soft-processor.
    * Turns on the status LED.
    * Erases the parallel Flash ROM sectors.
    * Writes the new code byte-by-byte into the parallel ROM.
    * Responds in the terminal: `Wrote <N> bytes`.

    ![Pasting Brainfuck code into Tera Term](../imgs/teraterm-paste-code.png)

4. Press the hardware **Reset Button** on the Brainfuino to boot the FPGA into your new program!

```brainfuck title="Simple Hello World Example"
+[-->-[>>+>-----<<]<--<---]>-.>>>+.>>..+++[.>]<<<<.+++------.<<<.>>>>+.
```

---

## 5. Helpful Tips & Program Halting

??? tip "Automatic Program Halting (Append Endless Loop)"
    **Automatic Halting Enabled by Default:**
    
    The FPGA soft-processor fetches and executes instructions from ROM continuously. Without a halt loop, once execution reaches the end of your program, it would march through unprogrammed ROM (`0xFF`) until address 262,143, wrap back around to address 0, and restart the program.
    
    Brainfuino solves this automatically! The companion firmware includes an **Append Endless Loop** configuration option (enabled by default as `[-]+[]` in the [Configuration Menu](config-menu.md)). Whenever you upload code or load from the Program Library, the STM32 automatically appends the robust halt idiom to the end of your code before flashing.
    
    **Recommendation:** Keep **Append Endless Loop** enabled (`[-]+[]`). You do not need to add manual halt loops to your Brainfuck programs—any standard Brainfuck code will halt cleanly at EOF!
    
    **Behind the Scenes: The Robust `[-]+[]` Idiom:**
    Many online tutorials suggest ending programs with `+[]`. However, if your program finishes with the current tape cell containing **255**, adding 1 wraps to **0** (`255 + 1 = 0`). The loop `[]` checks if the cell is non-zero, sees `0`, skips right past the loop, and restarts anyway! Brainfuino's auto-injected `[-]+[]` idiom zeroes the cell first, adds 1, and enters an unbreakable infinite halt loop.

??? note "Single-Key Commands"
    When you press a single key (packet length under 3 bytes), the STM32 interprets hotkeys:
    
    * **Keys `1` through `7`:** Change FPGA clock frequency (500 kHz to 48 MHz).
    * **Key `!`:** Dump the entire program currently stored in ROM.
    * **Key `@`:** Retransmit buffered serial data from the FPGA output buffer.
    
    See the [Terminal Commands Reference](terminal-commands.md) for full details.