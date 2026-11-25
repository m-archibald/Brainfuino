# Interactive Configuration Menu & Program Library

The Brainfuino coprocessor includes a rich, multi-page interactive text configuration menu (reminiscent of Raspberry Pi's `raspi-config`). It allows you to manage an onboard **Program Library** stored directly in the STM32's internal Flash, tune FPGA clock frequencies, customize upload thresholds and character pruning, configure runtime hotkeys, customize terminal text output colors, restore the default factory demo, or soft-reboot into the STM32 USB DFU bootloader without touching hardware jumpers.

---

## Accessing the Menu

You can enter the Configuration Menu at any time using either the Web Terminal, serial commands, or the hardware button:

=== "Web Terminal & Serial Commands"
    * **Web Terminal:** Click the **Menu** button in the top toolbar.
    * **Serial Command:** Send either of the following commands from any terminal console:
      ```text
      !MENU
      ```
      or
      ```text
      !CONFIG
      ```
    * The menu will immediately render on your screen.

=== "Physical Hardware Button"
    1. Press and hold the **Reset Button** on the Brainfuino.
    2. At **1 second**, the Red LED turns solid ON (Dedicated Program Mode threshold).
    3. Keep holding! At **3 seconds**, the Red LED begins **smoothly breathing / pulsing** (Settings Menu threshold).
    4. **Release the button** during this breathing window (between 3s and 8s).
    5. The soft-processor halts in reset, the Red LED pulses continuously, and the configuration menu appears in your terminal.
    6. *(Holding past **8 seconds** triggers a rapid LED strobe and restores the burned-in factory demo).*

---

## Multi-Page Menu Architecture

The menu interface is divided into a **Main Menu** and dedicated submenus for **Program Library** management and **Hardware Settings**:

### Main Menu
```text
+-------------------------------------------------+
|          BRAINFUINO CONFIGURATION MENU          |
+-------------------------------------------------+
|  Use [Up/Down] & [Enter], or type [1-5, 0]      |
+-------------------------------------------------+
| > 1. Program Library         : [ ENTER -->] < |
|   2. Hardware Settings       : [ ENTER -->]   |
|   3. Reboot to USB DFU       : [   BOOT   ]   |
|   4. Restore Factory Demo    : [  FLASH   ]   |
|   5. Save & Exit             : [   EXIT   ]   |
|   0. Exit without Saving     : [ CANCEL   ]   |
+-------------------------------------------------+
|  Hardware: STM32F072 | Parallel ROM: 256 kB     |
+-------------------------------------------------+
Select option or use arrows + Enter: 
```

### Submenu 1: Program Library
```text
+-------------------------------------------------+
|            BRAINFUINO PROGRAM LIBRARY           |
+-------------------------------------------------+
| [Enter/L] Run [A] Add [D] Dump [X] Del [0] Back |
+-------------------------------------------------+
| > 00. Brainfuino Demo               [  1.6kB] < |
|   01. Mandelbrot                    [ 11.2kB]   |
|   02. Game of Life                  [  4.8kB]   |
|   [ + Add New Program ]                         |
|   0. Back to Main Menu             [   BACK   ] |
+-------------------------------------------------+
|  Library:  2 / 63 Programs | Used: 16.0 / 76 kB |
+-------------------------------------------------+
Select program [Enter/L=Run, A=Add, D=Dump, X=Del, 0=Back]: 
```

### Submenu 2: Hardware Settings
```text
+--------------------------------------------------+
|           BRAINFUINO HARDWARE SETTINGS           |
+--------------------------------------------------+
|     Use [Up/Down], [Left/Right], or [Enter]      |
+--------------------------------------------------+
| > 1. Auto-Program on Paste      : [ DISABLED ] < |
|   2. Auto-Reset after Flash     : [ ENABLED  ]   |
|   3. Append Endless Loop        : [  [-]+[]  ]   |
|   4. FPGA Clock Frequency       : [ 500 kHz  ]   |
|   5. Prune non-BF on Paste      : [ DISABLED ]   |
|   6. Prune non-BF in Library    : [ ENABLED  ]   |
|   7. Serial Input CR -> LF      : [ ENABLED  ]   |
|   8. Run Mode Speed Hotkeys     : [ PgUp/Dn  ]   |
|   C. Companion MCU Output Color : [ NONE     ]   |
|   A. Manual Stepping Mode       : [ DISABLED ]   |
|   S. Hardware Serial (PD5/PD6)  : [ ENABLED  ]   |
|   U. Serial Baud Rate           : [ 9600     ]   |
|   F. Serial Flow Control        : [ RTS/CTS  ]   |
|   R. In-Place Cursor Redraw     : [ ENABLED  ]   |
|   W. Old Terminal Mode (WY-50)  : [ DISABLED ]   |
|   0. Back to Main Menu          : [   BACK   ]   |
+--------------------------------------------------+
|    Hardware: STM32F072 | Parallel ROM: 256 kB    |
+--------------------------------------------------+
Select option or use arrows + Enter: 
```

*(When **Auto-Program on Paste** is `ENABLED`, the sub-option for Paste Upload Threshold dynamically appears at option #2. When **Companion MCU Output Color** is set to `CUSTOM`, sub-options for Red, Green, and Blue (0–5) dynamically expand. When **Manual Stepping Mode** is `ENABLED`, sub-options for Step Advance Ticks and Step Trigger Key appear. When **Hardware Serial** is `ENABLED`, Baud Rate and Flow Control options appear).*

---

## Navigation Controls

The menu supports standard ANSI / VT100 navigation across all serial terminals and the Web Terminal:

| Control | Key / Action | Description |
| :--- | :--- | :--- |
| **Navigate Up / Down** | :material-arrow-up: Up Arrow / :material-arrow-down: Down Arrow | Moves selection cursor |
| **Cycle Option Forward** | :material-arrow-right: Right Arrow, Spacebar, or Enter | Cycles through available values |
| **Cycle Option Backward**| :material-arrow-left: Left Arrow | Cycles backward through values |
| **Direct Option Select** | Numbers `1` – `9`, `C`, `A` | Jumps directly to corresponding option |
| **Dump Code (Library)**  | `d` or `D` on highlighted program | Streams raw Brainfuck code to console |
| **Delete Program (Library)** | `x`, `X`, or `Delete` key | Marks program deleted with 0 page erases |
| **Add Program (Library)**| `a` or `A` | Opens new program upload scratchpad |
| **Cancel / Exit**        | `0`, `q`, `Q`, or `ESC` | Prompts to save or discard changes |
| **Hardware Button Exit** | Quick tap (< 1s) on Reset Button | Saves pending changes and exits |

---

## Onboard Program Library

The STM32 internal Flash features a dedicated **76 kB storage partition** and a **64-slot Table of Contents (TOC)** allowing you to upload, name, store, verify, and run multiple Brainfuck programs on demand.

### 1. Running a Stored Program
From the **Program Library** submenu:
1. Use Up/Down arrows to highlight the desired program (or type its two-digit slot number).
2. Press **Enter** (or `l` / `L`).
3. The STM32 provides full visual feedback:
   * Displays program title and size.
   * Reports parallel Flash ROM erasure progress.
   * Streams 0–100% writing progress bar.
   * Performs readback verification (`Verifying ROM... OK!`).
   * Pulses the FPGA reset line and auto-launches program execution cleanly.

### 2. Adding a New Program
1. Select `[ + Add New Program ]` (or press `a` / `A`).
2. **Program Name:** Enter a custom name up to 15 characters (e.g. `Mandelbrot`) and press Enter.
3. **Paste Code:** The terminal prompts:
   ```text
   Preparing scratchpad... Ready.
   Paste Brainfuck code now (press Enter or pause 100ms when done)...
   ```
   Paste your Brainfuck code. If `Prune non-BF in Library` is enabled, all non-Brainfuck characters (comments, spaces, newlines) are stripped in real-time.
4. **Pruning Statistics & Pre-Run Verification Prompt:**
   Upon paste completion, the coprocessor displays exact pruning metrics:
   ```text
   Received 11264 valid bytes.
   [Prune Stats: Received 15300 B | Kept 11264 B | Saved 26.3% non-BF comments]
   Run on FPGA to verify before saving? [Y/n]: 
   ```
   * **Skip Verification (`n` or `N`):** Immediately commits the program to internal Flash without running.
   * **Verify on FPGA (`y`, `Y`, or Enter):** Immediately launches execution on the FPGA soft-processor. Program characters stream live to the terminal while a 10-second countdown runs:
     * **Abort / Cancel:** Press the hardware Reset Button or send `!RST` within 10 seconds. The program is discarded and parallel ROM is restored to the factory demo.
     * **Confirm Early:** Press **Enter** (or send `!COMMIT`) to save immediately before 10 seconds elapse.
     * **Auto-Save:** If no reset occurs within 10 seconds, the program auto-saves to internal Flash:
       ```text
       [10s Verification Elapsed: Auto-saving to Library...]
       [SUCCESS: Saved to Slot 01: 'Mandelbrot' (11264 bytes)]
       ```

### 3. Capacity Management & Flash Endurance
* **Scratchpad Architecture:** Incoming programs stream into the 256 kB external parallel Flash ROM as a fast scratchpad, allowing programs up to 256 kB to be pasted without exceeding STM32 internal RAM limits.
* **Tombstone Deletion (`x`):** Highlighting any user program (slots 01–63) and pressing `x`, `X`, or `Delete` immediately marks its status as deleted (`0x0000`). This takes **0 page erasures**, preserving Flash endurance.
* **Defragmentation / Compaction:** When active programs must be packed forward to create contiguous space, the coprocessor reports rewriting progress and packs active programs forward cleanly.
* **Slot 00 Protection:** Slot `00. Brainfuino Demo` is permanently burned-in and cannot be deleted or overwritten.

---

## Configurable Hardware Options

### 1. Auto-Program on Paste
* **Options:** `DISABLED` *(default)*, `ENABLED`
* **Description:** When enabled, pasting Brainfuck code in standard Run Mode automatically transitions to Program Mode and flashes the new code to parallel ROM. When disabled *(default)*, code uploads can only occur after deliberately entering Dedicated Program Mode (via 1-second button hold), and the **Paste Upload Threshold** option is hidden.

### 2. Paste Upload Threshold (when Auto-Program is ENABLED)
* **Options:** `4 B`, `8 B`, `16 B` *(default)*, `32 B`, `64 B`
* **Description:** The minimum byte burst size required to trigger auto-programming. Setting this to 16 bytes ensures that terminal escape sequences (such as arrow keys `\x1b[A` or function keys) are never misinterpreted as code uploads while interacting with running Brainfuck programs. Only visible when **Auto-Program on Paste** is `ENABLED`.

### 3. Auto-Reset after Flash
* **Options:** `ENABLED` *(default)*, `DISABLED`
* **Description:** When enabled, the STM32 automatically un-resets the FPGA soft-processor and launches execution immediately upon completion of code writing. When disabled, the firmware holds the soft-processor halted until you press the hardware Reset Button.

### 4. Append Endless Loop
* **Options:** `[-]+[]` *(default)*, `NONE`
* **Description:** Automatically appends the robust Brainfuck halt idiom (`[-]+[]`) to the end of your program before writing to parallel ROM. This prevents the FPGA Program Counter from marching through unprogrammed ROM space (`0xFF`) and restarting at address `0`.

### 5. FPGA Clock Frequency
* **Options (25 Speeds):**
  * **Ultra-Low Frequencies (TIM1 PWM):** `10 Hz`, `25 Hz`, `50 Hz`, `100 Hz`, `250 Hz`, `500 Hz`, `1 kHz`, `2 kHz`, `5 kHz`, `10 kHz`, `25 kHz`, `50 kHz`
  * **Standard & High Frequencies (MCO):** `62.5 kHz`, `125 kHz`, `250 kHz`, `500 kHz` *(default)*, `750 kHz`, `1 MHz`, `1.5 MHz`, `2 MHz`, `3 MHz`, `4 MHz`, `6 MHz`, `8 MHz`, `12 MHz`
* **Description:** Selects the master clock frequency fed to the MachXO2 FPGA soft-processor. Clock signals ≤ 50 kHz are generated by hardware timer `TIM1` in 50% PWM mode. Frequencies ≥ 62.5 kHz are generated via the STM32 Master Clock Output (`MCO`) pin using HSI/HSI48 clock dividers.
* **Silicon Timing Limit:** The parallel Flash ROM on Brainfuino (`SST39LF020-55`) has a maximum address access time of 55 ns (18.18 MHz theoretical limit). **12 MHz** (83.3 ns cycle) is the maximum safe operating speed with +28 ns timing margin.

### 6. Prune non-BF on Paste
* **Options:** `ENABLED` *(default)*, `DISABLED`
* **Description:** Filters out non-Brainfuck characters (comments, tabs, spaces, newlines) during direct paste uploads in Run Mode and Dedicated Program Mode, ensuring only valid instructions (`+`, `-`, `<`, `>`, `[`, `]`, `.`, `,`) are written to parallel ROM.

### 7. Prune non-BF in Library
* **Options:** `ENABLED` *(default)*, `DISABLED`
* **Description:** Strips comments and non-Brainfuck characters during uploads to the internal Flash **Program Library**, maximizing storage density across the 76 kB pool.

### 8. Serial Input CR -> LF
* **Options:** `ENABLED` *(default)*, `DISABLED`
* **Description:** Automatically translates incoming Carriage Return (`\r`) to Line Feed (`\n`) and collapses Windows CRLF pairs into a single newline. This ensures that interactive `,` input programs respond consistently regardless of whether the user presses Enter in Chrome, PuTTY, or minicom.

### 9. Run Mode Speed Hotkeys
* **Options:** `PgUp/Dn` *(default)*, `Up/Down`, `+ / -`, `NONE`
* **Description:** Configures the keyboard shortcut used to dynamically step clock speeds up or down across all 25 frequencies during live execution in Run Mode. Changes use a **3-second debounce timer** before saving to Flash to prevent wear during rapid speed adjustments.

### C. Companion MCU Output Color
* **Presets:** `NONE` *(default)*, `RED`, `HI-RED`, `ORANGE`, `YELLOW`, `HI-YEL`, `LIME`, `GREEN`, `HI-GRN`, `CYAN`, `HI-CYAN`, `BLUE`, `HI-BLUE`, `VIOLET`, `MAGENTA`, `HI-MAG`, `GRAY`, `CUSTOM`
* **Description:** Applies ANSI color escape sequences to all coprocessor system output (prompts, banners, menu borders, and HUD indicators). This visually distinguishes STM32 coprocessor messages from pure Brainfuck program output. The menu displays a live color preview as you cycle through options.
* **Custom 6×6×6 RGB Cube Mixer:** Selecting `CUSTOM` dynamically reveals three sub-options:
  * **Red (0–5):** Component intensity
  * **Green (0–5):** Component intensity
  * **Blue (0–5):** Component intensity
  This gives 216 distinct 256-color palette shades (`\x1b[38;5;<n>m`) for complete terminal personalization!

### A. Manual Stepping Mode
* **Options:** `DISABLED` *(default)*, `ENABLED`
* **Description:** When enabled, the FPGA master clock remains **halted by default in Run Mode**. Toggling this to `ENABLED` reveals sub-options **T** and **K**:
  * **T. Step Advance Ticks:** `1 Tick`, `10 Ticks`, `100 Ticks` *(default)*, `1k Ticks`, `10k Ticks`, `100k Ticks`
  * **K. Step Trigger Key:** `Spacebar` *(default)*, `Tab`, `Enter`
* **Keystroke Accumulator:** If the trigger key is held or pressed rapidly, keystrokes accumulate in an internal queue and drain continuously without dropping cycles, letting you watch program execution cycle-by-cycle.

---

## Main Menu Actions & Exit Confirmation

### 3. Reboot to USB DFU
* Soft-resets the STM32 directly into the factory system memory USB DFU bootloader without touching hardware boot jumpers. Firmware can then be updated using `dfu-util` or STM32CubeProgrammer.

### 4. Restore Factory Demo
* Erases parallel Flash ROM, writes the built-in official Brainfuino ASCII logo banner demo from internal Flash, and restarts execution.

### 5. Save & Exit
* Commits all modified hardware settings to internal Flash Page 63, exits the configuration menu, turns off the pulsing Red LED, and pulses the FPGA reset line to resume execution cleanly.

### 0. Exit without Saving
* Discards any pending hardware setting modifications and resumes execution without touching Flash Page 63.

### Unsaved Changes Confirmation Dialog
If you press `ESC` or `0` while settings have been modified, the coprocessor prompts with an exit confirmation dialog:

```text
+--------------------------------------------------+
|              UNSAVED CONFIGURATION               |
+--------------------------------------------------+
|  You have unsaved changes in settings.           |
|                                                  |
|  [Y]   Save changes and Exit                     |
|  [N]   Discard changes and Exit                  |
|  [ESC] Cancel (Stay in Menu)                     |
+--------------------------------------------------+
Save settings before exit? [y/n/ESC]: 
```

Upon exiting, the coprocessor cleanly clears the terminal screen using ANSI escape codes (`\x1b[2J\x1b[H`), returning you to a pristine console for running programs.

---

## STM32 Internal Memory Map

The STM32F072's 128 kB internal Flash is organized into non-overlapping partitions:

| Range | Pages | Size | Purpose |
| :--- | :---: | :---: | :--- |
| `0x08000000` – `0x08013FFF` | 0 – 39 | 80 kB | **STM32 Firmware Application Code** |
| `0x08014000` – `0x080147FF` | 40 | 2 kB | **Library Table of Contents (TOC)** (64 slots × 32 B) |
| `0x08014800` – `0x0801F7FF` | 41 – 62 | 44 kB | **Library Program Payload Pool** (Dynamic allocation) |
| `0x0801F800` – `0x0801FFFF` | 63 | 2 kB | **Persistent Hardware Configuration** (Page 63) |
