# Brainfuck Program Examples & Catalog

A curated collection of tested, hardware-verified Brainfuck programs for the **Brainfuino**. All programs are 100% ASCII-based and execute natively on the Lattice MachXO2 FPGA soft-processor and 128 kB parallel tape SRAM.

---

## Quick Run

The fastest way to run any of these programs is via the browser:

<div class="grid cards" markdown="1">

-   [:octicons-link-external-16: **Launch Web Terminal**](../web-terminal.html){ .md-button .md-button--primary target="_blank" }

    ---

    **1-Click Execution**
    
    Open the Web Terminal, select any program from the **Program Suite** sidebar, and click **Run**. You can also drag and drop any `.b` file straight into the terminal!

</div>

---

## 🎮 Interactive Games

These programs use the `,` input command to respond to your keyboard in real time:

| Program | Source File | Author / Origin | Description |
| :--- | :--- | :--- | :--- |
| **Tic-Tac-Toe (AI)** | [`games/tictactoe_ai.b`](../programs/games/tictactoe_ai.b) | Mitxela | Play against an AI opponent with ASCII grid updates (`X`, `O`, `-`). |
| **Tic-Tac-Toe** | [`games/tictactoe.b`](../programs/games/tictactoe.b) | Daniel B. Cristofani | Two-player or solo game using a standard 1–9 numeric keypad grid. |
| **Guess the Number** | [`games/guess.b`](../programs/games/guess.b) | Mitxela | Secret number guessing game with interactive "Higher" / "Lower" feedback. |
| **Conway's Game of Life** | [`games/life.b`](../programs/games/life.b) | Cristofani / Åkesson | 2D cellular automaton with coordinate cell toggling and generation stepping. |
| **Towers of Hanoi** | [`games/hanoi.b`](../programs/games/hanoi.b) | Clifford Wolf | Animates the recursive disk movements of Towers of Hanoi using ANSI terminal escapes. |
| **Particle Bounce** | [`games/bounce.b`](../programs/games/bounce.b) | Daniel B. Cristofani | 2D particle simulation and physics automaton in ASCII space. |

---

## 🎨 Fractals & ASCII Art

Autonomous visualization programs that render complex patterns in terminal text:

| Program | Source File | Author / Origin | Description |
| :--- | :--- | :--- | :--- |
| **Mandelbrot Fractal** | [`fractals/mandelbrot.b`](../programs/fractals/mandelbrot.b) | Erik Bosman | Famous ASCII Mandelbrot set renderer testing deep loops and register math. |
| **Sierpinski Triangle** | [`fractals/sierpinski.b`](../programs/fractals/sierpinski.b) | Daniel B. Cristofani | Ultra-compact recursive fractal triangle generator. |
| **Christmas Tree** | [`fractals/xmastree.b`](../programs/fractals/xmastree.b) | Daniel B. Cristofani | Renders a decorated holiday pine tree in ASCII art. |
| **Text Pyramid** | [`fractals/pyramid.b`](../programs/fractals/pyramid.b) | Antosser | Generates a centered, symmetrical expanding character pyramid. |
| **Twinkle Twinkle** | [`fractals/twinkle.b`](../programs/fractals/twinkle.b) | C.W. Fitzgerald | Renders the full lyrics and rhythmic punctuation of the classic song. |

---

## 🔢 Math, Sequences & Calculators

High-precision mathematical algorithms executing multi-precision arithmetic on the hardware tape:

| Program | Source File | Author / Origin | Description |
| :--- | :--- | :--- | :--- |
| **Prime Sieve** | [`math/prime.b`](../programs/math/prime.b) | Urban Müller | Computes and prints prime numbers in decimal text continuously. |
| **Calculator** | [`math/calculator.b`](../programs/math/calculator.b) | Antosser | Interactive four-function calculator (type `12+34=` to evaluate). |
| **Prime Factorizer** | [`math/factor.b`](../programs/math/factor.b) | Brian Raiter | Enter any decimal number to compute and display its prime factors. |
| **Fibonacci Sequence** | [`math/fibonacci.b`](../programs/math/fibonacci.b) | Daniel B. Cristofani | Generates an unbounded decimal Fibonacci sequence using multi-precision tape math. |
| **Golden Ratio ($\phi$)** | [`math/golden.b`](../programs/math/golden.b) | Daniel B. Cristofani | Computes continued fraction digits of the Golden Ratio (`1.6180339887...`). |
| **Euler's Constant ($e$)** | [`math/e.b`](../programs/math/e.b) | Daniel B. Cristofani | Computes digits of $e$ (`2.718281828459...`) via Taylor series expansion. |
| **FizzBuzz** | [`math/fizzbuzz.b`](../programs/math/fizzbuzz.b) | none1 | Solves the classic programmer challenge (`1, 2, Fizz, 4, Buzz, ...`) up to 100 in ASCII. |
| **Collatz Conjecture** | [`math/collatz.b`](../programs/math/collatz.b) | Daniel B. Cristofani | Computes the $3n + 1$ hailstone trajectory until reaching 1. |
| **Factorials** | [`math/factorial.b`](../programs/math/factorial.b) | Daniel B. Cristofani | Arbitrary-precision factorial generator emitting 1!, 2!, 3!, ... in decimal. |
| **Kolakoski Sequence** | [`math/kolakoski.b`](../programs/math/kolakoski.b) | none1 | Generates the infinite self-generating run-length sequence of 1s and 2s. |
| **Thue-Morse Sequence**| [`math/thuemorse.b`](../programs/math/thuemorse.b) | Daniel B. Cristofani | Generates the infinite fair-sharing binary sequence (`01101001...`). |
| **Streaming $\pi$ Spigot** | [`pi/pi_stream.b`](../programs/pi/pi_stream.b) | Brainfuino Project | Dynamic self-expanding $\pi$ spigot ([read full architectural guide](pi-spigot.md)). |

---

## 🔤 Text Utilities & Ciphers

Interactive utilities demonstrating string parsing and algorithmic text conversion:

| Program | Source File | Author / Origin | Description |
| :--- | :--- | :--- | :--- |
| **ROT13 Cipher** | [`ciphers/rot13.b`](../programs/ciphers/rot13.b) | Daniel B. Cristofani | Real-time Caesar-13 letter substitution cipher: type text to encode/decode. |
| **Atbash Cipher** | [`ciphers/atbash.b`](../programs/ciphers/atbash.b) | Keymaker | Biblical substitution cipher (A $\leftrightarrow$ Z, B $\leftrightarrow$ Y) translating keystrokes live. |
| **Morse Code Translator**| [`ciphers/morse.b`](../programs/ciphers/morse.b) | Keymaker | Translates typed English text into Morse code dots and dashes (`.... .-..`). |
| **Dvorak Keymapper** | [`ciphers/dvorak.b`](../programs/ciphers/dvorak.b) | Daniel B. Cristofani | Translates QWERTY keystrokes into the Dvorak keyboard layout on the fly. |
| **Word Counter (`wc`)** | [`ciphers/wc.b`](../programs/ciphers/wc.b) | Daniel B. Cristofani | UNIX `wc` utility clone: counts characters, words, and newlines from input. |
| **Pig Latin Translator**| [`ciphers/piglatin.b`](../programs/ciphers/piglatin.b) | Thalia Archibald | Translates input words into Pig Latin. |

---

## 🍺 Classics & Humor

| Program | Source File | Author / Origin | Description |
| :--- | :--- | :--- | :--- |
| **Brainfuino Banner** | [`basic/banner.b`](../programs/basic/banner.b) | Brainfuino Project | Official ASCII logo text banner and hardware diagnostic check. |
| **Hello World** | [`basic/hello.b`](../programs/basic/hello.b) | C.W. Fitzgerald | Canonical benchmark edition of Hello World with non-interfering comments. |
| **Serial Echo** | [`basic/echo.b`](../programs/basic/echo.b) | Urban Müller | Single-cell echo test `, [ . , ]` validating bidirectional USB serial I/O. |
| **99 Bottles of Beer** | [`classics/bottles.b`](../programs/classics/bottles.b) | C.W. Fitzgerald | Generates all 99 verses of the recursive ballad with correct pluralization. |

---

## 🪞 Quines (Self-Reproducing Programs)

These programs output their own exact Brainfuck source code character-by-character:

| Program | Source File | Author / Origin | Description |
| :--- | :--- | :--- | :--- |
| **Brian Raiter Quine** | [`quines/quine_raiter.b`](../programs/quines/quine_raiter.b) | Brian Raiter | The classic 822-byte quine that prints its own exact Brainfuck source code. |
| **Daniel Cristofani Quine**| [`quines/quine_400.b`](../programs/quines/quine_400.b) | Daniel B. Cristofani | Ultra-compact 400-byte self-reproducing quine. |
| **Ryan Kusnery Quine** | [`quines/quine_kusnery.b`](../programs/quines/quine_kusnery.b) | Ryan Kusnery | Elegantly structured self-replicating program. |
