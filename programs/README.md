# Brainfuino Program Catalog

A collection of tested, hardware-verified Brainfuck programs for the **Brainfuino** native hardware computer. All programs run in raw ASCII on the Lattice MachXO2 FPGA soft-processor and 128 kB parallel tape SRAM.

---

## How to Run on Hardware

* **Web Serial Terminal (1-Click):** Open the [Web Serial Terminal](https://m-archibald.github.io/Brainfuino/web-terminal.html), pick any program from the **Program Suite** sidebar, and click **Run**. You can also drag and drop any `.b` file straight into the terminal!
* **Direct Paste:** Copy the raw Brainfuck code from any file below and paste it into the Web Terminal, PuTTY, or Tera Term.

---

## 🎮 Interactive Games

| File | Program | Author / Provenance | Mode | Description |
| :--- | :--- | :--- | :---: | :--- |
| [`games/tictactoe_ai.b`](games/tictactoe_ai.b) | **Tic-Tac-Toe (AI)** | Mitxela | Interactive | Play against an AI opponent with ASCII grid updates (`X`, `O`, `-`). |
| [`games/tictactoe.b`](games/tictactoe.b) | **Tic-Tac-Toe** | Daniel B. Cristofani | Interactive | Two-player or solo game using a standard 1–9 numeric keypad grid. |
| [`games/guess.b`](games/guess.b) | **Guess the Number** | Mitxela | Interactive | Secret number guessing game with interactive "Higher" / "Lower" feedback. |
| [`games/life.b`](games/life.b) | **Conway's Game of Life** | Cristofani / Åkesson | Interactive | 2D cellular automaton with coordinate cell toggling and generation stepping. |
| [`games/hanoi.b`](games/hanoi.b) | **Towers of Hanoi** | Clifford Wolf | Autonomous | Animates the recursive disk movements of Towers of Hanoi using ANSI terminal escapes. |
| [`games/bounce.b`](games/bounce.b) | **Particle Bounce** | Daniel B. Cristofani | Autonomous | 2D particle simulation and physics automaton in ASCII space. |

---

## 🎨 Visuals & ASCII Art

| File | Program | Author / Provenance | Mode | Description |
| :--- | :--- | :--- | :---: | :--- |
| [`fractals/mandelbrot.b`](fractals/mandelbrot.b) | **Mandelbrot Fractal** | Erik Bosman | Autonomous | Famous ASCII Mandelbrot set renderer testing deep loops and register math. |
| [`fractals/sierpinski.b`](fractals/sierpinski.b) | **Sierpinski Triangle** | Daniel B. Cristofani | Autonomous | Ultra-compact recursive fractal triangle generator. |
| [`fractals/xmastree.b`](fractals/xmastree.b) | **Christmas Tree** | Daniel B. Cristofani | Autonomous | Renders a decorated holiday pine tree in ASCII art. |
| [`fractals/pyramid.b`](fractals/pyramid.b) | **Text Pyramid** | Antosser | Autonomous | Generates a centered, symmetrical expanding character pyramid. |
| [`fractals/twinkle.b`](fractals/twinkle.b) | **Twinkle Twinkle** | C.W. Fitzgerald | Autonomous | Renders the full lyrics and rhythmic punctuation of the classic song. |

---

## 🔢 Math, Sequences & Calculators

| File | Program | Author / Provenance | Mode | Description |
| :--- | :--- | :--- | :---: | :--- |
| [`math/prime.b`](math/prime.b) | **Prime Sieve** | Urban Müller | Autonomous | Generates and prints prime numbers in decimal text continuously. |
| [`math/calculator.b`](math/calculator.b) | **Calculator** | Antosser | Interactive | Interactive four-function calculator (type `12+34=` to evaluate). |
| [`math/factor.b`](math/factor.b) | **Prime Factorizer** | Brian Raiter | Interactive | Enter any decimal number to compute and display its prime factors. |
| [`math/fibonacci.b`](math/fibonacci.b) | **Fibonacci Sequence** | Daniel B. Cristofani | Autonomous | Generates an unbounded decimal Fibonacci sequence using multi-precision tape math. |
| [`math/golden.b`](math/golden.b) | **Golden Ratio ($\phi$)** | Daniel B. Cristofani | Autonomous | Computes continued fraction digits of the Golden Ratio (`1.6180339887...`). |
| [`math/e.b`](math/e.b) | **Euler's Constant ($e$)** | Daniel B. Cristofani | Autonomous | Computes digits of $e$ (`2.718281828459...`) via Taylor series expansion. |
| [`math/fizzbuzz.b`](math/fizzbuzz.b) | **FizzBuzz** | none1 | Autonomous | Solves the classic programmer challenge (`1, 2, Fizz, 4, Buzz, ...`) up to 100 in ASCII. |
| [`math/collatz.b`](math/collatz.b) | **Collatz Conjecture** | Daniel B. Cristofani | Autonomous | Computes the $3n + 1$ hailstone trajectory until reaching 1. |
| [`math/factorial.b`](math/factorial.b) | **Factorials** | Daniel B. Cristofani | Autonomous | Arbitrary-precision factorial generator emitting 1!, 2!, 3!, ... in decimal. |
| [`math/kolakoski.b`](math/kolakoski.b) | **Kolakoski Sequence** | none1 | Autonomous | Generates the infinite self-generating run-length sequence of 1s and 2s. |
| [`math/thuemorse.b`](math/thuemorse.b) | **Thue-Morse Sequence**| Daniel B. Cristofani | Autonomous | Generates the infinite fair-sharing binary sequence (`01101001...`). |
| [`pi/pi_stream.b`](pi/pi_stream.b) | **Streaming $\pi$ Spigot** | Brainfuino Project | Autonomous | Infinite streaming $\pi$ spigot dynamically expanding across 128 kB SRAM. |

---

## 🔤 Text Utilities & Ciphers

| File | Program | Author / Provenance | Mode | Description |
| :--- | :--- | :--- | :---: | :--- |
| [`ciphers/rot13.b`](ciphers/rot13.b) | **ROT13 Cipher** | Daniel B. Cristofani | Interactive | Real-time Caesar-13 letter substitution cipher: type text to encode/decode. |
| [`ciphers/atbash.b`](ciphers/atbash.b) | **Atbash Cipher** | Keymaker | Interactive | Biblical substitution cipher (A $\leftrightarrow$ Z, B $\leftrightarrow$ Y) translating keystrokes live. |
| [`ciphers/morse.b`](ciphers/morse.b) | **Morse Code Translator**| Keymaker | Interactive | Translates typed English text into Morse code dots and dashes (`.... .-..`). |
| [`ciphers/dvorak.b`](ciphers/dvorak.b) | **Dvorak Keymapper** | Daniel B. Cristofani | Interactive | Translates QWERTY keystrokes into the Dvorak keyboard layout on the fly. |
| [`ciphers/wc.b`](ciphers/wc.b) | **Word Counter (`wc`)** | Daniel B. Cristofani | Interactive | UNIX `wc` utility clone: counts characters, words, and newlines from input. |
| [`ciphers/piglatin.b`](ciphers/piglatin.b) | **Pig Latin Translator**| Thalia Archibald | Interactive | Translates input words into Pig Latin. |

---

## 🍺 Classics & Humor

| File | Program | Author / Provenance | Mode | Description |
| :--- | :--- | :--- | :---: | :--- |
| [`basic/banner.b`](basic/banner.b) | **Brainfuino Banner** | Brainfuino Project | Autonomous | Official ASCII logo text banner and hardware diagnostic check. |
| [`basic/hello.b`](basic/hello.b) | **Hello World** | C.W. Fitzgerald | Autonomous | Canonical benchmark edition of Hello World with non-interfering comments. |
| [`basic/echo.b`](basic/echo.b) | **Serial Echo** | Urban Müller | Interactive | Single-cell echo test `, [ . , ]` validating bidirectional USB serial I/O. |
| [`classics/bottles.b`](classics/bottles.b) | **99 Bottles of Beer** | C.W. Fitzgerald | Autonomous | Generates all 99 verses of the recursive ballad with correct pluralization. |

---

## 🪞 Quines (Self-Reproducing Programs)

| File | Program | Author / Provenance | Mode | Description |
| :--- | :--- | :--- | :---: | :--- |
| [`quines/quine_raiter.b`](quines/quine_raiter.b) | **Brian Raiter Quine** | Brian Raiter | Autonomous | The classic 822-byte quine that prints its own exact Brainfuck source code. |
| [`quines/quine_400.b`](quines/quine_400.b) | **Daniel Cristofani Quine** | Daniel B. Cristofani | Autonomous | Ultra-compact 400-byte self-reproducing quine. |
| [`quines/quine_kusnery.b`](quines/quine_kusnery.b) | **Ryan Kusnery Quine** | Ryan Kusnery | Autonomous | Elegantly formatted self-replicating program. |
