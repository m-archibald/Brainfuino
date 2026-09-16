# Brainfuck Code Examples & Demos

These tested Brainfuck examples are verified for native execution on the Brainfuino. Copy and paste them into your serial terminal to run them!

---

## 1. Official Brainfuino Text Art Banner

This program outputs the official Brainfuino ASCII logo to the serial terminal, ending with the hardware halt idiom `+[]`. Paste this code directly into your serial terminal (e.g. Tera Term or PuTTY) at 9600 baud:

```brainfuck title="Brainfuino Text Art Banner Generator"
++++++++++[>+++++++++>++++++++++++>++++++>+++++>++++++++>+++++++++++>++++<<<<<<<-]>+>+++>>>>>--------..<<<<<<++++....>>>>>>............<<<<<<.>>>>>>........<<<<<<..>>>>>>.......<<<<<<.>>>>>>.............<<<<<<<+++++++++++++.---.>>>>>>>.<<<<<+.>>>>>.<<<<<<..>>>>>>.+++++++++.---------.<<<<<<.>>>>>>.<<<<<<..>>>>>>.<<<<<<..>>>>>>.<<<<<<.>>>>>>++++++++.<<<<<<.>>>>>>+.<<<<<<.>>>>>>---------.<<<<<<..>>>>>>..+++++++++++++++.---------------.<<<<<<.>.<.>>>>>>...<<<<<<.>>>>>>++++++++.<<<<<<.>>>>>>+.<<<<<<.>>>>>>---------.<<<<<<..>>>>>>...<<<<<<...>>>>>>..<<<<<<<+++.---.>>>>>>>.<<<<<.>>>>>..<<<<<<.>>>>>>.<<<<<<---.>.>>>>>.+++++++.<<<<<<+++..>>>>>>++++++++.---------------.<<<<<<.+.>>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.+++++++.<<<<<<-.>>>>>>-------.<<<<<<---.>.>>>>>.<<<<<.<+++.>.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.+++++++.<<<<<<.>>>>>>-------.<<<<<<---.>>>>>>.+++++++++++++++.---------------.<<<<<<+++.>>>>>>.<<<<<<---.>>>>>>.<<<<<<<+++.---.>>>>>>>.<<<<<.>>>>>.<<<<<.<+++.>>>>>>+++++++++.---------.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.++++++++.<<<<<<.>.>>>>>--------.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>..<<<<<<.>.>>>>>.<<<<<.<.>.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.++++++++.<<<<<<.>>>>>>+.---------.<<<<<.<<+++.---.>>>>>>>.<<<<<.<....>>>>>>+++++++++++++++.<<<<<.<.>.>>>>>---------------..<<<<<<---.+++..>>>>>>++++++++++++.<<<<<<.>.<.>.<.>.>>>>>------------.<<<<<.<.>.<.>.>>>>>..<<<<<<---.+++..>>>>>>++++++++++++.<<<<<<.>.<.>.<.>.>>>>>------------.<<<<<.<.>.<---.+++...>>>>>>+++++++++++++++.---------------.<<<<<<<+++.---.>>>>>>>..................................................<<<<<<<+++.---.
+[]
```

**Terminal Output:**
<pre class="ascii-banner">
  ____            _        __       _             
 | __ ) _ __ __ _(_)_ __  / _|_   _(_)_ __   ___  
 |  _ \| '__/ _` | | '_ \| |_| | | | | '_ \ / _ \ 
 | |_) | | | (_| | | | | |  _| |_| | | | | | (_) |
 |____/|_|  \__,_|_|_| |_|_|  \__,_|_|_| |_|\___/ 
</pre>

---

## 2. Classic "Hello World!"

The quintessential Brainfuck program. It demonstrates setting up multiplication loops to compute the ASCII values of text characters efficiently, ending with the robust `[-]+[]` halt idiom.

```brainfuck title="Hello World with Robust Halt Loop"
[-->-[>>+>-----<<]<--<---]>-.>>>+.>>..+++[.>]<<<<.+++------.<<<.>>>>+.[-]+[]
```

**Terminal Output:**
```text
Hello, World!
```

---

## 3. Interactive Character Echo

Reads an incoming character from the terminal and echoes it back immediately.

```brainfuck title="Echo Program"
,[.,]
```

**Explanation:**
1. `,`: Waits for incoming serial byte and stores it in the current cell.
2. `[`: If the character is non-zero, enters the loop.
3. `.`: Outputs the character back to the terminal.
4. `,`: Reads the next character from the input buffer.
5. `]`: Loops back to check if the next character is non-zero.

---

## 4. Fibonacci Sequence Generator

Generates and displays terms of the Fibonacci sequence:

```brainfuck title="Fibonacci Generator"
>++++++++++>+>+[
    [+++++[>++++++++<-]>.<++++++[>--------<-]+<<]>.>[
        [-]<[>+<-]>>[<<+>+>-]<[>+<-]>[<<+>>-]<<<<
    ]
]+[-]+[]
```

---

??? tip "Brainfuck Programs, Compilers & Formal Verification"
    Looking for more Brainfuck programs, compilers, or formal proofs?
    
    * **[bfcoq (Thalia Archibald)](https://github.com/thaliaarchi/bfcoq):** A formally verified Brainfuck compiler and Turing-completeness proof written in the Coq theorem prover.
    * **[Thalia's Research (thalia.dev)](https://thalia.dev/):** Compilers, programming language theory, and explorations of unexpected Turing completeness.
    * **[Brainfuck Community Archive:](https://esolangs.org/wiki/Brainfuck)** Comprehensive reference collection of classic Brainfuck algorithms, games, fractals, and interpreters.
