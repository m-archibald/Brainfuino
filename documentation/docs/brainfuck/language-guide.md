# Brainfuck Language & Hardware Semantics

Brainfuck is an esoteric programming language created in 1993 by Urban Müller. Despite having only **eight commands**, the language is fully Turing-complete.

On the Brainfuino, these eight commands are executed directly in silicon by the `brainfuck_uP` soft-processor.

---

## The 8 Native Commands

| Command | Verilog Action | Equivalent in C | Description |
| :---: | :--- | :--- | :--- |
| `>` | `p <= p + 1; pc <= pc + 1;` | `p++;` | Increment the data pointer (points to the next memory cell). |
| `<` | `p <= p - 1; pc <= pc + 1;` | `p--;` | Decrement the data pointer (points to the previous memory cell). |
| `+` | `acc <= pData + 1;` | `(*p)++;` | Increment the byte at the data pointer. |
| `-` | `acc <= pData - 1;` | `(*p)--;` | Decrement the byte at the data pointer. |
| `.` | `outPort <= pData; portWR <= 0;` | `putchar(*p);` | Output the byte at the data pointer as an ASCII character. |
| `,` | `acc <= inPort; portRD <= 0;` | `*p = getchar();` | Accept one byte of input and store its value in the byte at the data pointer. |
| `[` | *Branch forward if `*p == 0`* | `while (*p) {` | If the byte at the data pointer is zero, jump the program counter forward to the command after the matching `]`. |
| `]` | *Branch backward if `*p != 0`* | `}` | If the byte at the data pointer is nonzero, jump the program counter back to the command after the matching `[`. |

---

## Hardware Execution Specifics

Because the Brainfuino runs on real physical chips (parallel SRAM and Flash ROM) rather than an emulator, keep these physical parameters in mind:

### 1. Memory Cell Size & Wraparound
* **Cell Size:** Exactly 8 bits (1 byte, unsigned 0 to 255).
* **Arithmetic Wraparound:**
  - `255 + 1` wraps cleanly to `0`.
  - `0 - 1` wraps cleanly to `255`.
* **Zeroing a Cell:** The standard idiom `[-]` decrements the cell until it reaches `0`. On Brainfuino, this takes N loop iterations, where N is the cell's initial value.

### 2. Memory Tape Bounds
* **Data RAM:** 128 kB total (131,072 cells).
* The 17-bit data pointer `p[16:0]` wraps around: decrementing below address `0` wraps around to address `131,071`.

### 3. Comments and Whitespace
* Any character in your source file that is not one of the eight tokens (`+`, `-`, `<`, `>`, `[`, `]`, `.`, `,`) is treated as a no-op (NOP).
* The FPGA simply increments the program counter (`pc <= pc + 1`) and continues.
* You can write descriptive comments, indentations, and notes directly in your code without a preprocessor!

### 4. Halting & Program Termination
* Standard Brainfuck specifications do not define an explicit "exit" instruction.
* In hardware, the FPGA will keep stepping through Flash ROM until address `262,143`, wrap back to `0`, and re-execute.
* **Best Practice:** End programs with an intentional bulletproof infinite loop:
  ```brainfuck
  [-]+[]
  ```
  *(Zeroes the cell first to guarantee it is non-zero when entering `+[]`, preventing accidental loop exits on wraparound).*
