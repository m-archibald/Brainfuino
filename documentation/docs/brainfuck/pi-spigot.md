# Unbounded Streaming π Spigot on Bare Silicon

This document details the architecture, mathematics, and hardware verification of an unbounded streaming π spigot executing in pure Brainfuck on the Brainfuino FPGA soft-processor.

Operating directly on bare silicon with a 128 kB SRAM memory tape and 12 MHz clock, the program emits continuous decimal digits of π in real-time without external floating-point libraries, operating systems, or runtimes.

---

## Downloads & Source Files

| File | Description | Download |
| :--- | :--- | :--- |
| **`pi_stream.b`** | Pure Brainfuck streaming binary (20,261 bytes, dynamic tape) | [Download `pi_stream.b`](../programs/pi/pi_stream.b){ .md-button } |
| **`generate_pi_stream.py`** | Python generator that synthesizes the spigot Brainfuck code | [Download `generate_pi_stream.py`](../programs/pi/generate_pi_stream.py){ .md-button } |
| **`run_pi.py`** | Hardware flasher & real-time digit verification runner | [Download `run_pi.py`](../programs/pi/run_pi.py){ .md-button } |

---

## 1. Mathematical Spigot Foundation

The algorithm is based on **Jeremy Gibbons' streaming spigot algorithm** (2004) using 2×2 integer matrices to represent Linear Fractional Transformations (LFTs):

$$\begin{pmatrix} q & r \\ s & t \end{pmatrix}(x) = \frac{q x + r}{s x + t}$$

Using the Lambert / Euler continued fraction expansion for π:

$$\pi = \begin{pmatrix} 1 & 0 \\ 0 & 1 \end{pmatrix} \prod_{k=1}^\infty \begin{pmatrix} k & 4k+2 \\ 0 & 2k+1 \end{pmatrix}(0)$$

Since the lower-left element s remains 0 throughout execution, the transformation simplifies to an upper-triangular state matrix (q, r, 0, t).

### Spigot Step Logic

At each iteration k ≥ 1 with state matrix (q, r, t):

1. **Extract Digit Candidate**:

    $$
    n = \lfloor (3q + r) / t \rfloor
    $$

2. **Refinement Check**:
    Test whether the interval bounds produce the same integer digit:

    $$
    4q + 2r - 2t < n \cdot t
    $$

3. **Branch A (Digit Produced)**:
    If the condition holds, digit n is confirmed:
    - Output ASCII digit n (`'0' + n`)
    - If this is the first digit, also output decimal point `.`
    - Scale matrix for base-10 streaming:

        $$
        q \leftarrow 10q, \quad r \leftarrow 10(r - n \cdot t)
        $$

4. **Branch B (Ingest Next Matrix)**:
    If the digit cannot yet be confirmed:
    - Ingest next term (k, 4k+2, 2k+1) where l = 2k+1:

        $$
        r \leftarrow q(4k+2) + r \cdot l, \quad t \leftarrow t \cdot l, \quad q \leftarrow q \cdot k, \quad l \leftarrow l + 2, \quad k \leftarrow k + 1
        $$

---

## 2. Multi-Precision Tape Memory Architecture

To handle numbers growing to thousands of decimal digits, memory is partitioned into **16-byte register slots** across the 128 kB SRAM tape (8,000 total slots available).

```text
[ Tape Cell 0..15: Global State Header ]
  Cell 0 : Zero boundary marker (always 0)
  Cell 1 : Streaming loop condition
  Cell 2-4: Global arithmetic scratch cells
  Cell 5 : k (current iteration, 8-bit)
  Cell 6 : l (odd factor 2k+1, 8-bit)
  Cell 7 : n (extracted digit candidate)
  Cell 8 : Comparison sign flag
  Cell 9 : Branch condition result
  Cell 10: Inverted branch flag
  Cell 11: Decimal point emitted flag
  Cell 12: Output ASCII character buffer
  Cell 13: Loop control
  Cell 14: Return pass comparison accumulator
  Cell 15: Scratch / temporary

[ Slot 0..N-1: 16-byte Multi-Precision BCD Register Slots ]
  Offset 0 : Active Slot Flag (1 = active slot, 0 = tape boundary)
  Offset 1 : q (decimal digit, 0..9)
  Offset 2 : r (decimal digit, 0..9)
  Offset 3 : t (decimal digit, 0..9)
  Offset 4 : TMP1 (scratch register A)
  Offset 5 : TMP2 (scratch register B)
  Offset 6 : CARRY (arithmetic carry / borrow)
  Offset 7 : SCRATCH1 (local register preservation)
  Offset 8 : SCRATCH2 (local division scratch)
  Offset 9 : DM_C (divmod constant 10 scratch)
  Offset 10: DM_T1 (divmod temporary 1)
  Offset 11: DM_T2 (divmod temporary 2)
  Offset 12: DM_Z (divmod zero-detect flag)
  Offset 13-15: Spare auxiliary scratch cells
```

---

## 3. Dynamic On-Demand Tape Expansion

Fixed-length tapes suffer from a fundamental trade-off: small tapes run quickly but run out of digits, while large tapes (like 8,000 slots) waste millions of clock cycles sweeping across empty memory.

The Brainfuino spigot solves this with **pure Brainfuck self-expansion**:

1. **Minimal Boot Allocation**:
   Execution starts with only **4 active slots** (64 bytes). Slot 4 has `FLAG = 0`.
2. **Carry Out Detection**:
   In `add_reg` and `mul_scalar_const`, when the forward pass exits the last active slot, the pointer lands on `slot_{last+1}`:
   - If `CARRY > 0`: an arithmetic overflow occurred.
   - `slot_{last+1}.FLAG` is set to 1.
   - The carry is moved into the destination register of `slot_{last+1}`.
   - `slot_{last+2}.FLAG` is cleared to 0.
3. **The 20-Byte Pure Brainfuck Conditional Advance Idiom**:
   ```brainfuck
   [ >>>>>>>>>>>>>>>> [-] ]
   ```
   - **If expansion occurred (`*ptr == 1`)**: loop enters, steps right 16 cells to the new inactive slot boundary, clears it to 0, and exits cleanly.
   - **If no expansion occurred (`*ptr == 0`)**: loop is skipped entirely.
   - In both cases, the pointer lands on the first inactive slot, allowing the exact same return sweep (`<16 [ <16 ]`) to step back to `Cell 0`.

This ensures **0% wasted cycles on inactive slots** while dynamically scaling up to the full 128 kB SRAM boundary.

---

## 4. Bare Silicon Hardware Benchmarks (`COM22` at 12 MHz)

Verified on physical Brainfuino hardware with the companion STM32 clock generator set to 12 MHz:

```text
=======================================================
HARDWARE EXECUTION SUMMARY
=======================================================
Target Program     : programs/pi/pi_stream.b (Dynamic Tape)
Code Size Flashed  : 20,261 bytes (Flash ROM: 256 kB)
Hardware Output    : 3.141592653
Expected Reference : 3.141592653

>>> [PASS] 100% HARDWARE VERIFICATION SUCCESSFUL!
>>> Computed 10 digits of Pi on bare FPGA silicon!

Digit Streaming Timeline:
  Digit '3': +0.00s (sub-millisecond boot!)
  Digit '.': +0.00s
  Digit '1': +0.05s
  Digit '4': +1.03s
  Digit '1': +1.46s
  Digit '5': +4.32s
  Digit '9': +10.89s
  Digit '2': +14.27s
  Digit '6': +21.33s
  Digit '5': +33.05s
  Digit '3': +40.67s
=======================================================
```

### Comparative Architecture Performance

| Metric / Digit | Fixed 8,000 Slots (128 kB) | Fixed 256 Slots (4 kB) | Dynamic Self-Expanding (4 → N) |
| :--- | :--- | :--- | :--- |
| **Startup / Boot Latency** | 42.84 s | 1.35 s | **< 0.05 s (Instantaneous)** |
| **Digit 1 (`3.1`)** | 101.67 s | 3.81 s | **0.05 s (2,033× faster!)** |
| **Digit 3 (`3.141`)** | ~220 s | 13.23 s | **1.46 s (150× faster!)** |
| **Digit 5 (`3.14159`)** | ~500 s | 50.26 s | **10.89 s (45× faster!)** |
| **Digit 9 (`3.141592653`)**| > 1,000 s | 120.54 s | **40.67 s (25× faster!)** |
| **Tape Capacity** | Fixed at 128 kB | Fixed at 4 kB | **Scales on demand up to 128 kB** |
| **Wasted Cycles** | 99.9% on early digits | 95% on early digits | **0% (Only active slots are swept)** |

---

## 5. Running on Your Brainfuino

### Using the Python Hardware Runner (Automated)

The automated runner connects to Brainfuino over USB CDC serial, streams the 20 kB code into SPI Flash ROM, resets the processor, and monitors the real-time digit stream:

```bash
# Flash and validate first 10 digits
python programs/pi/run_pi.py programs/pi/pi_stream.b --count 10 --timeout 60

# Run already-flashed program from Flash ROM without re-uploading
python programs/pi/run_pi.py programs/pi/pi_stream.b --run-only --count 15
```

### Using a Serial Terminal (Manual)

1. Connect Brainfuino to your PC via USB CDC serial (`COM22` on Windows, `/dev/ttyACM0` on Linux) at **115200 baud**.
2. If using Tera Term or PuTTY, send the contents of [`pi_stream.b`](../programs/pi/pi_stream.b) using the **Send File** feature.
3. Once transmission finishes, the companion STM32 automatically detects the end of the file, flashes the program into SPI Flash ROM, and resets the FPGA soft-processor to begin streaming digits.
