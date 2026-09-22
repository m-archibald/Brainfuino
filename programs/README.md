# Brainfuino Example Programs Catalog

A collection of tested, hardware-verified Brainfuck programs for the **Brainfuino** native hardware Brainfuck computer (Lattice MachXO2 FPGA soft-processor + STM32 coprocessor).

---

## Directory Structure

```text
programs/
├── basic/
│   ├── banner.b          # Official Brainfuino ASCII text art banner (1.3 kB)
│   ├── hello.b           # Classic benchmark Hello World with comments (0.8 kB)
│   └── echo.b            # Interactive character echo via serial UART (5 bytes)
├── fractals/
│   ├── sierpinski.b      # 80-column Sierpinski triangle by NYYRIKKI (1.3 kB)
│   └── mandelbrot.b      # ASCII Mandelbrot set renderer by Erik Bosman (11.6 kB)
├── math/
│   ├── fibonacci.b       # Unbounded decimal Fibonacci generator by Daniel B. Cristofani (247 B)
│   ├── golden.b          # Infinite streaming Golden Ratio by Daniel B. Cristofani (375 B)
│   └── factorial.b       # Arbitrary-precision factorial generator by Daniel B. Cristofani (554 B)
├── games/
│   └── life.b            # Conway's Game of Life by Daniel B. Cristofani / Linus Åkesson (760 B)
└── pi/
    ├── pi_stream.b       # Dynamic self-expanding streaming Pi spigot (20.2 kB)
    ├── generate_pi_stream.py # Pure Brainfuck spigot code generator
    └── run_pi.py         # Hardware flasher & real-time digit validation runner
```

---

## How to Flash to Hardware

### Method 1: Paste directly into Serial Terminal
For short-to-medium programs (`banner.b`, `hello.b`, `echo.b`, `sierpinski.b`, `fibonacci.b`, `golden.b`, `factorial.b`, `life.b`):
1. Connect Brainfuino via USB CDC serial (`COM22` on Windows, `/dev/ttyACM0` on Linux) at **115200 baud**.
2. Copy and paste the raw Brainfuck code directly into your terminal emulator (Tera Term, PuTTY, minicom).
3. The companion STM32 automatically buffers the incoming code, flashes it to memory, and launches execution on the FPGA soft-processor.

### Method 2: Flash via Python Hardware Runner
For large programs like `pi_stream.b` or `mandelbrot.b`:
```bash
python programs/pi/run_pi.py programs/fractals/mandelbrot.b --count 20 --timeout 120
python programs/pi/run_pi.py programs/pi/pi_stream.b --count 10 --timeout 60
```

---

## Program Details & Verified Sources

### 1. Basic Demos (`programs/basic/`)
* **`banner.b`** (1,348 bytes)
    * **Description:** Emits the official Brainfuino ASCII text banner over serial.
    * **Author:** Brainfuino Project
* **`hello.b`** (779 bytes)
    * **Description:** Canonical benchmark edition of Hello World featuring comments that skip non-operator characters.
    * **Source:** [cwfitzgerald/brainfuck-benchmark](https://github.com/cwfitzgerald/brainfuck-benchmark/blob/master/benches/hello.b)
* **`echo.b`** (5 bytes)
    * **Description:** Character echo loop `, [ . , ]` testing bidirectional serial UART I/O.
    * **Author:** Urban Müller (1993)

### 2. Fractals (`programs/fractals/`)
* **`sierpinski.b`** (1,327 bytes)
    * **Description:** Computes and renders a 32-line Sierpinski fractal pattern across an 80-column display.
    * **Author:** NYYRIKKI (2002)
    * **Source:** [Brainfuck Community Archive](https://esolangs.org/wiki/Brainfuck#Sierpinski_triangle)
* **`mandelbrot.b`** (11,669 bytes)
    * **Description:** ASCII Mandelbrot set renderer testing deep nested loops, 16-bit register arithmetic, and high-frequency output throttling.
    * **Author:** Erik Bosman (2004)
    * **Source:** [kostya/benchmarks/mandel.b](https://github.com/kostya/benchmarks/blob/master/brainfuck/mandel.b) / [frerich/brainfuck](https://github.com/frerich/brainfuck/blob/master/samples/mandelbrot.bf)

### 3. Mathematical Sequences (`programs/math/`)
* **`fibonacci.b`** (247 bytes)
    * **Description:** Generates and outputs an unbounded stream of multi-precision decimal Fibonacci numbers ($0, 1, 1, 2, 3, 5, 8, 13, 21, 34, ...$).
    * **Author:** Daniel B. Cristofani
    * **Source:** [Daniel B. Cristofani's fib.b (brainfuck.org)](http://www.brainfuck.org/fib.b)
* **`golden.b`** (375 bytes)
    * **Description:** Computes and streams the digits of the Golden Ratio ($\phi = 1.6180339887...$) continuously in decimal.
    * **Author:** Daniel B. Cristofani (2019)
    * **Source:** [Daniel B. Cristofani's golden.b (brainfuck.org)](http://www.brainfuck.org/golden.b)
* **`factorial.b`** (554 bytes)
    * **Description:** Arbitrary-precision factorial generator emitting $1!, 2!, 3!, 4!, ...$ in decimal with newline separation.
    * **Author:** Daniel B. Cristofani (2019)
    * **Source:** [Daniel B. Cristofani's factorial2.b (brainfuck.org)](http://www.brainfuck.org/factorial2.b)

### 4. Interactive Games (`programs/games/`)
* **`life.b`** (760 bytes)
    * **Description:** John Horton Conway's Game of Life 2D cellular automaton with a 10×10 toroidal grid. Enter coordinates (e.g. `be`) to toggle cells, or send a bare newline to advance generations.
    * **Author:** Daniel B. Cristofani (2021) based on the interface by Linus Åkesson (2007)
    * **Source:** [Daniel B. Cristofani's life.b (brainfuck.org)](http://www.brainfuck.org/life.b) / [Linus Åkesson's Game of Life](http://www.linusakesson.net/programming/brainfuck/index.php)

### 5. Streaming $\pi$ Spigot (`programs/pi/`)
* **`pi_stream.b`** (20,261 bytes)
    * **Description:** Unbounded streaming $\pi$ spigot based on Jeremy Gibbons' Linear Fractional Transformation (LFT) matrix algorithm.
    * **Dynamic On-Demand Tape:** Starts with 4 active slots ($64$ bytes) for sub-millisecond boot and dynamically expands across 128 kB SRAM (up to 8,000 slots) on carry propagation.
    * **Source:** [Brainfuino Pi Spigot Architecture](../documentation/docs/brainfuck/pi-spigot.md)
