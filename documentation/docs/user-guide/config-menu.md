# Interactive Configuration Menu & Program Library

The Brainfuino coprocessor includes a rich, multi-page interactive text configuration menu (reminiscent of Raspberry Pi's `raspi-config`). It allows you to manage an onboard **Program Library** stored directly in the STM32's internal Flash, tune FPGA clock frequencies, customize upload thresholds and character pruning, configure runtime hotkeys, restore the default factory demo, or soft-reboot into the STM32 USB DFU bootloader without touching hardware jumpers.

---

## Accessing the Menu

You can enter the Configuration Menu at any time using either the hardware button or serial terminal:

=== "Physical Hardware Button"
    1. Press and hold the **Reset Button** on the Brainfuino.
    2. At **3 seconds**, the Red LED turns solid ON (Program Mode threshold).
    3. Keep holding! At **6 seconds**, the Red LED begins **smoothly breathing / pulsing**.
    4. **Release the button** during this pulsing window (between 6s and 10s).
    5. The soft-processor halts in reset, the Red LED pulses continuously, and the configuration menu appears in your terminal.

=== "Serial Terminal Command"
    * While running, send either of the following commands:
      ```text
      !MENU
      ```
      or
      ```text
      !CONFIG
      ```
    * The menu will immediately render on your screen.

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
|          BRAINFUINO PROGRAM LIBRARY             |
+-------------------------------------------------+
|  Select program to run, [D] to delete           |
+-------------------------------------------------+
| > 00. Brainfuino Demo        : [   1.4 kB ] < |
|   01. Mandelbrot             : [  11.2 kB ]   |
|   02. Game of Life           : [   4.8 kB ]   |
|   [ + Add New Program ]      : [  NEW PROG]   |
|   0. Back to Main Menu       : [   BACK   ]   |
+-------------------------------------------------+
|  2 / 63 Programs | Used: 16.0 / 76 kB           |
+-------------------------------------------------+
Select program [0-2], [A]dd, [D]elete: 
```

### Submenu 2: Hardware Settings
```text
+-------------------------------------------------+
|          BRAINFUINO HARDWARE SETTINGS           |
+-------------------------------------------------+
|  Use [Up/Down], [Left/Right], [Enter], or [1-9] |
+-------------------------------------------------+
| > 1. Auto-Program on Paste   : [ ENABLED  ] < |
|   2. Paste Upload Threshold  : [   16 B   ]   |
|   3. Auto-Reset after Flash  : [ ENABLED  ]   |
|   4. Append Endless Loop     : [  [-]+[]  ]   |
|   5. Prune non-BF on Paste   : [ ENABLED  ]   |
|   6. Prune non-BF in Library : [ ENABLED  ]   |
|   7. FPGA Clock Frequency    : [ 500 kHz  ]   |
|   8. Run Mode Speed Hotkeys  : [ PgUp/Dn  ]   |
|   9. Manual Stepping Mode    : [ DISABLED ]   |
|   0. Back to Main Menu       : [   BACK   ]   |
+-------------------------------------------------+
|  Hardware: STM32F072 | Parallel ROM: 256 kB     |
+-------------------------------------------------+
Select option or use arrows + Enter: 
```

*(When **Manual Stepping Mode** is set to `ENABLED`, options `A. Step Advance Ticks` and `B. Step Trigger Key` dynamically appear before option `0`).*

---

## Dual-Mode Navigation Controls

The menu architecture uses a hybrid parser supporting both modern rich terminal emulators and basic line-oriented serial monitors:

| Control | Modern Terminals (Tera Term, PuTTY, minicom) | Basic Terminals (Arduino Serial Monitor) |
| :--- | :--- | :--- |
| **Navigate Up / Down** | :material-arrow-up: Up Arrow / :material-arrow-down: Down Arrow | Type item number and press Send |
| **Cycle Option Forward** | :material-arrow-right: Right Arrow, :material-keyboard-space: Spacebar, or :material-keyboard-return: Enter | Type item number and press Send |
| **Cycle Option Backward** | :material-arrow-left: Left Arrow | — |
| **Direct Select / Enter** | Numbers `1` – `9`, `A` / `B` | Numbers `1` – `9`, `A` |
| **Delete Program (Library)**| `d` or `D` on highlighted slot | Type `d` and press Send |
| **Add Program (Library)** | `a` or `A`, or select `[ + Add New Program ]` | Type `a` and press Send |
| **Return / Cancel / Exit**| `0`, `q`, `Q`, or `ESC` | `0` or `q` |
| **Hardware Button Exit** | Quick tap (< 3s) on Reset Button | Quick tap (< 3s) on Reset Button |

---

## Onboard Program Library

The STM32 internal Flash features a dedicated **76 kB storage partition** and a **64-slot Table of Contents (TOC)** allowing you to upload, name, store, verify, and run multiple Brainfuck programs on demand.

### 1. Running a Stored Program
From the **Program Library** submenu:
1. Use Up/Down arrows to highlight the desired program (or type its two-digit slot number).
2. Press **Enter**.
3. The STM32 erases the 256 kB parallel Flash ROM, flashes the selected program from internal Flash, pulses the FPGA reset line, and begins execution immediately.

### 2. Adding a New Program
1. Select `[ + Add New Program ]` (or press `a` / `A`).
2. **Program Name:** Enter a custom name up to 15 characters (e.g. `Mandelbrot`) and press Enter.
3. **Paste Code:** The terminal prompts:
   ```text
   Preparing scratchpad... Ready.
   Paste Brainfuck code now (press Enter or pause 100ms when done)...
   ```
   Paste your Brainfuck code. If `Prune non-BF in Library` is enabled, all non-Brainfuck characters (comments, spaces, newlines) are stripped in real-time.
4. **Pre-Run Verification Prompt:**
   ```text
   Received 11264 valid bytes.
   Run on FPGA to verify before saving? [Y/n]: 
   ```
   * **Skip Verification (`n` or `N`):** Immediately commits the program to internal Flash without running.
   * **Verify on FPGA (`y`, `Y`, or Enter):** Immediately launches the code on the FPGA soft-processor. A 10-second countdown begins:
     * **Abort / Cancel:** Press the hardware Reset Button or send `!RST` within 10 seconds. The program is discarded and parallel ROM is restored to the factory demo.
     * **Confirm Early:** Press **Enter** to save immediately before 10 seconds.
     * **Auto-Save:** If no reset occurs within 10 seconds, the program auto-saves to internal Flash:
       ```text
       [10s Verification Elapsed: Auto-saving to Library...]
       [SUCCESS: Saved to Slot 01: 'Mandelbrot' (11264 bytes)]
       ```

### 3. Deleting Programs & Compaction
* **Tombstone Deletion:** Highlighting any user program (slots 01–63) and pressing `d` or `D` immediately marks its status as deleted (`0x0000`). This takes **0 page erasures**, preserving Flash endurance.
* **Slot 00 Protection:** Slot `00. Brainfuino Demo` is permanently burned-in and cannot be deleted or overwritten.
* **On-Demand Compaction:** When the 76 kB payload pool runs out of contiguous space at the end, the firmware automatically defragments/compacts active programs forward, reclaiming freed space without user intervention.

---

## Configurable Hardware Options

### 1. Auto-Program on Paste
* **Options:** `ENABLED` *(default)*, `DISABLED`
* **Description:** When enabled, pasting Brainfuck code in standard Run Mode automatically transitions to Program Mode and flashes the new code to parallel ROM. When disabled, code uploads can only occur after deliberately entering Dedicated Program Mode (via 3-second button hold).

### 2. Paste Upload Threshold
* **Options:** `4 B`, `8 B`, `16 B` *(default)*, `32 B`, `64 B`
* **Description:** The minimum byte burst size required to trigger auto-programming. Setting this to 16 bytes or higher ensures that terminal escape sequences (such as arrow keys `\x1b[A`, Home `\x1b[1~`, or function keys) are never misinterpreted as code uploads while interacting with running Brainfuck programs.

### 3. Auto-Reset after Flash
* **Options:** `ENABLED` *(default)*, `DISABLED`
* **Description:** When enabled, the STM32 automatically un-resets the FPGA soft-processor and launches execution immediately upon completion of code writing. When disabled, the firmware holds the soft-processor halted until you press the hardware Reset Button.

### 4. Append Endless Loop
* **Options:** `[-]+[]` *(default)*, `NONE`
* **Description:** Automatically appends the robust Brainfuck halt idiom (`[-]+[]`) to the end of your program. This prevents the FPGA Program Counter from marching through unprogrammed ROM space (`0xFF`) and restarting at address `0`.

### 5. Prune non-BF on Paste
* **Options:** `ENABLED` *(default)*, `DISABLED`
* **Description:** When enabled, non-Brainfuck characters (comments, tabs, spaces, newlines) are automatically filtered out during direct paste uploads in Run Mode and Dedicated Program Mode, ensuring only valid instructions (`+`, `-`, `<`, `>`, `[`, `]`, `.`, `,`) are written to parallel ROM.

### 6. Prune non-BF in Library
* **Options:** `ENABLED` *(default)*, `DISABLED`
* **Description:** When enabled, non-Brainfuck characters are stripped during uploads to the internal Flash **Program Library**, maximizing storage density across the 76 kB pool.

### 7. FPGA Clock Frequency
* **Options (25 Speeds):**
  * **Ultra-Low Frequencies (TIM1 PWM):** `10 Hz`, `25 Hz`, `50 Hz`, `100 Hz`, `250 Hz`, `500 Hz`, `1 kHz`, `2 kHz`, `5 kHz`, `10 kHz`, `25 kHz`, `50 kHz`
  * **Standard & High Frequencies (MCO):** `62.5 kHz`, `125 kHz`, `250 kHz`, `500 kHz` *(default)*, `750 kHz`, `1 MHz`, `1.5 MHz`, `2 MHz`, `3 MHz`, `4 MHz`, `6 MHz`, `8 MHz`, `12 MHz`
* **Description:** Selects the master clock frequency fed to the MachXO2 FPGA soft-processor. Clock signals $\le 50\text{ kHz}$ are generated by hardware timer `TIM1_CH1` (AF2 on PA8) in 50% PWM mode, providing clean square waves down to 10 Hz with 0% CPU overhead. Frequencies $\ge 62.5\text{ kHz}$ are generated via the STM32 Master Clock Output (`MCO`) pin using HSI/HSI48 clock dividers.
* **Silicon Timing Limit:** The parallel Flash ROM on Brainfuino (`SST39LF020-55`) has a maximum address access time of $55\text{ ns}$ ($18.18\text{ MHz}$ theoretical maximum). **12 MHz** ($83.3\text{ ns}$ cycle) is the maximum safe operating speed with $+28\text{ ns}$ timing margin.

### 8. Run Mode Speed Hotkeys
* **Options:** `PgUp/Dn` *(default)*, `Up/Down`, `+ / -`, `NONE`
* **Description:** Allows dynamically stepping clock speeds up or down across all 25 frequencies during live program execution in Run Mode without entering the configuration menu. When a speed change occurs, a transient dimmed HUD indicator is displayed:
  ```text
  [Clock: 50 kHz]
  ```
* **Wear-Leveling Delayed Save:** Changes made via hotkeys use a **3-second debounce timer** before saving to internal Flash Page 63, preventing flash wear during rapid speed adjustments.

### 9. Manual Stepping Mode
* **Options:** `DISABLED` *(default)*, `ENABLED`
* **Description:** When enabled, the FPGA master clock is **halted by default in Run Mode**. Toggling this setting to `ENABLED` dynamically reveals sub-options **A** and **B** in the hardware settings menu:
  * **A. Step Advance Ticks:** `1 Tick`, `10 Ticks`, `100 Ticks` *(default)*, `1k Ticks`, `10k Ticks`, `100k Ticks`
  * **B. Step Trigger Key:** `Spacebar` *(default)*, `Tab`, `Enter`
* **Queueing / Tick Piling Behavior:** If the trigger key is held down or spammed rapidly, keystroke bursts accumulate in a pending tick queue and drain continuously without dropping a single clock cycle. All generated program characters stream out to the terminal in real time.

---

## Main Menu Actions

### 3. Reboot to USB DFU
* **Description:** Soft-resets the STM32 directly into the factory system memory USB DFU bootloader without touching hardware boot jumpers. Firmware can then be updated using `dfu-util` or STM32CubeProgrammer.

### 4. Restore Factory Demo
* **Description:** Immediately erases parallel Flash ROM, writes the built-in official Brainfuino ASCII logo banner demo from internal Flash, and restarts execution.

### 5. Save & Exit
* **Description:** Commits all modified hardware settings to internal Flash Page 63, exits the configuration menu, turns off the pulsing Red LED, and pulses the FPGA reset line to resume execution cleanly. On reset, prints:
  ```text
  [bf_µP reset] 500 kHz
  ```

### 0. Exit without Saving
* **Description:** Discards any pending hardware setting modifications and resumes execution without touching Flash Page 63.

---

## STM32 Internal Memory Map

The STM32F072's 128 kB internal Flash is cleanly organized into non-overlapping partitions:

| Range | Pages | Size | Purpose |
| :--- | :---: | :---: | :--- |
| `0x08000000` – `0x0800B7FF` | 0 – 22 | 46 kB | STM32 Firmware Application Code |
| `0x0800B800` – `0x0800BFFF` | 23 | 2 kB | Firmware headroom / alignment |
| `0x0800C000` – `0x0800C7FF` | 24 | 2 kB | **Library Table of Contents (TOC)** (64 slots $\times$ 32 B) |
| `0x0800C800` – `0x0801F7FF` | 25 – 62 | 76 kB | **Library Program Payload Pool** (Dynamic allocation) |
| `0x0801F800` – `0x0801FFFF` | 63 | 2 kB | **Persistent Hardware Configuration** (Page 63) |

---

## Automated Hardware Verification Suite

The entire multi-page menu, program library partition, TOC engine, and navigation workflows are continuously validated on physical hardware using an automated Python test harness:

* **Script:** [`scripts/test_library_and_menu.py`](https://github.com/m-archibald/Brainfuino/blob/main/scripts/test_library_and_menu.py)
* **Execution:**
  ```powershell
  python scripts/test_library_and_menu.py
  ```
* **Coverage (10 Automated Verification Stages):**
  1. **Main Menu Rendering:** Verifies banner, framing, options 1–5, and 0.
  2. **Hardware Settings Submenu:** Validates all configuration entries, including both pruning toggles.
  3. **Left/Right Arrow Cycling:** Confirms bidirectional setting value cycling with VT100 escape codes.
  4. **Menu Navigation:** Tests back navigation (`0` key) and state persistence.
  5. **Program Library Listing:** Verifies TOC rendering, slot allocations, and capacity gauges.
  6. **Program Addition & Pruning:** Uploads code with comments, verifies comment stripping, and tests skipping pre-run verification (`n`).
  7. **Dynamic TOC Updating:** Confirms program name and byte size register in the Table of Contents.
  8. **Execution from Flash:** Loads the newly saved program into parallel ROM and verifies execution on the FPGA soft-processor.
  9. **Zero-Erase Tombstone Deletion:** Tests instant program deletion (`d` key) and confirms freed slot recovery.
  10. **Factory Restore:** Validates restoring the burned-in demo banner to parallel Flash.

