# Brainfuck Code Examples & Demos

These tested Brainfuck examples are verified for native execution on the Brainfuino. Copy and paste them into your serial terminal to run them!

---

## 1. Official Brainfuino Text Art Banner

[:material-download: Download `banner.b`](../programs/basic/banner.b){ .md-button .md-button--small }

This program outputs the official Brainfuino ASCII logo to the serial terminal. With the companion STM32's **Append Endless Loop** feature enabled (on by default), the hardware automatically halts the program counter at EOF without needing manual halt loops. Paste this code directly into your serial terminal (e.g. Tera Term or PuTTY) at 9600 or 115200 baud:

```brainfuck title="Brainfuino Text Art Banner Generator"
++++++++++[>+++++++++>++++++++++++>++++++>+++++>++++++++>+++++++++++>++++<<<<<<<-]>+>+++>>>>>--------..<<<<<<++++....>>>>>>............<<<<<<.>>>>>>........<<<<<<..>>>>>>.......<<<<<<.>>>>>>.............<<<<<<<+++++++++++++.---.>>>>>>>.<<<<<+.>>>>>.<<<<<<..>>>>>>.+++++++++.---------.<<<<<<.>>>>>>.<<<<<<..>>>>>>.<<<<<<..>>>>>>.<<<<<<.>>>>>>++++++++.<<<<<<.>>>>>>+.<<<<<<.>>>>>>---------.<<<<<<..>>>>>>..+++++++++++++++.---------------.<<<<<<.>.<.>>>>>>...<<<<<<.>>>>>>++++++++.<<<<<<.>>>>>>+.<<<<<<.>>>>>>---------.<<<<<<..>>>>>>...<<<<<<...>>>>>>..<<<<<<<+++.---.>>>>>>>.<<<<<.>>>>>..<<<<<<.>>>>>>.<<<<<<---.>.>>>>>.+++++++.<<<<<<+++..>>>>>>++++++++.---------------.<<<<<<.+.>>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.+++++++.<<<<<<-.>>>>>>-------.<<<<<<---.>.>>>>>.<<<<<.<+++.>.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.+++++++.<<<<<<.>>>>>>-------.<<<<<<---.>>>>>>.+++++++++++++++.---------------.<<<<<<+++.>>>>>>.<<<<<<---.>>>>>>.<<<<<<<+++.---.>>>>>>>.<<<<<.>>>>>.<<<<<.<+++.>>>>>>+++++++++.---------.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.++++++++.<<<<<<.>.>>>>>--------.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>..<<<<<<.>.>>>>>.<<<<<.<.>.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.<<<<<.>>>>>.++++++++.<<<<<<.>>>>>>+.---------.<<<<<.<<+++.---.>>>>>>>.<<<<<.<....>>>>>>+++++++++++++++.<<<<<.<.>.>>>>>---------------..<<<<<<---.+++..>>>>>>++++++++++++.<<<<<<.>.<.>.<.>.>>>>>------------.<<<<<.<.>.<.>.>>>>>..<<<<<<---.+++..>>>>>>++++++++++++.<<<<<<.>.<.>.<.>.>>>>>------------.<<<<<.<.>.<---.+++...>>>>>>+++++++++++++++.---------------.<<<<<<<+++.---.>>>>>>>..................................................<<<<<<<+++.---.
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

[:material-download: Download `hello.b`](../programs/basic/hello.b){ .md-button .md-button--small }

The quintessential Brainfuck program, taken from the [cwfitzgerald/brainfuck-benchmark](https://github.com/cwfitzgerald/brainfuck-benchmark/blob/master/benches/hello.b) suite. It sets up an initial counter to multiply values into cell targets before printing each ASCII character. Because none of the comment letters contain Brainfuck operator characters, the commented source can be pasted or flashed directly into memory.

```brainfuck title="Hello World (Benchmark Edition)"
+++++ +++++             initialize counter (cell #0) to 10
[                       use loop to set the next four cells to 70/100/30/10
    > +++++ ++              add  7 to cell #1
    > +++++ +++++           add 10 to cell #2 
    > +++                   add  3 to cell #3
    > +                     add  1 to cell #4
    <<<< -                  decrement counter (cell #0)
]                   
> ++ .                  print 'H'
> + .                   print 'e'
+++++ ++ .              print 'l'
.                       print 'l'
+++ .                   print 'o'
> ++ .                  print ' '
<< +++++ +++++ +++++ .  print 'W'
> .                     print 'o'
+++ .                   print 'r'
----- - .               print 'l'
----- --- .             print 'd'
> + .                   print '!'
> .                     print '\n'
```

*Source: [cwfitzgerald/brainfuck-benchmark](https://github.com/cwfitzgerald/brainfuck-benchmark/blob/master/benches/hello.b)*

**Terminal Output:**
```text
Hello World!
```

---

## 3. Interactive Character Echo

[:material-download: Download `echo.b`](../programs/basic/echo.b){ .md-button .md-button--small }

Reads an incoming character from the serial terminal and echoes it back immediately.

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

## 4. Sierpinski Triangle Generator

[:material-download: Download `sierpinski.b`](../programs/fractals/sierpinski.b){ .md-button .md-button--small }

An iconic Brainfuck fractal program authored by **NYYRIKKI** (2002) formatted in the shape of a Sierpinski triangle. It computes and renders a 32-line Sierpinski fractal pattern across an 80-column display (the EOF halt loop is automatically appended by the firmware):

```brainfuck title="Sierpinski Triangle by NYYRIKKI (2002)"
[ This program prints Sierpinski triangle on 80-column display. ]
                                >    
                               + +    
                              +   +    
                             [ < + +    
                            +       +    
                           + +     + +    
                          >   -   ]   >    
                         + + + + + + + +    
                        [               >    
                       + +             + +    
                      <   -           ]   >    
                     > + + >         > > + >    
                    >       >       +       <    
                   < <     < <     < <     < <    
                  <   [   -   [   -   >   +   <    
                 ] > [ - < + > > > . < < ] > > >    
                [                               [    
               - >                             + +    
              +   +                           +   +    
             + + [ >                         + + + +    
            <       -                       ]       >    
           . <     < [                     - >     + <    
          ]   +   >   [                   -   >   +   +    
         + + + + + + + +                 < < + > ] > . [    
        -               ]               >               ]    
       ] +             < <             < [             - [    
      -   >           +   <           ]   +           >   [    
     - < + >         > > - [         - > + <         ] + + >    
    [       -       <       -       >       ]       <       <    
   < ]     < <     < <     ] +     + +     + +     + +     + +    
  +   .   +   +   +   .   [   -   ]   <   ]   +   +   +   +   +    
 * * * * * M a d e * B y : * N Y Y R I K K I * 2 0 0 2 * * * * *    
```

---

---

## 5. Mandelbrot Fractal Viewer

[:material-download: Download `mandelbrot.b` (11.6 kB)](../programs/fractals/mandelbrot.b){ .md-button .md-button--primary }

Authored by **Erik Bosman** (2004), this is one of the most famous Brainfuck benchmark programs in existence. It computes and renders a 2D ASCII visualization of the Mandelbrot set directly over serial. It rigorously stresses nested loop pipelines, 16-bit register arithmetic, and high-frequency character transmission.

* **Author:** Erik Bosman (2004)
* **Source:** [kostya/benchmarks/mandel.b](https://github.com/kostya/benchmarks/blob/master/brainfuck/mandel.b) / [frerich/brainfuck samples/mandelbrot.bf](https://github.com/frerich/brainfuck/blob/master/samples/mandelbrot.bf)

**Terminal Output Preview:**
```text
                                                 ****                                   
                                               ********                                 
                                              **********                                
                                              **********                                
                                               ********                                 
                                           ***************                              
                                       ***********************                          
                                     ***************************                        
                                   *******************************                      
                                   *******************************                      
                                 ***********************************                    
                                *************************************                   
                                *************************************                   
...
```

??? example "View Brainfuck Source (`mandelbrot.b` - 11,669 bytes)"
    Because `mandelbrot.b` is 11.6 kB, it is recommended to download the file directly or run it via the Python runner:
    ```bash
    python programs/pi/run_pi.py programs/fractals/mandelbrot.b --count 200
    ```

---

## 6. Decimal Fibonacci Sequence Generator

[:material-download: Download `fibonacci.b`](../programs/math/fibonacci.b){ .md-button .md-button--small }

Authored by **Daniel B. Cristofani**, this program computes and outputs an unbounded stream of multi-precision decimal Fibonacci numbers (0, 1, 1, 2, 3, 5, 8, 13, 21, 34, ...) with newline separation. Unlike naive implementations that overflow 8-bit cells after the 13th number (233), this program maintains multi-cell decimal registers to scale indefinitely across the tape.

* **Author:** Daniel B. Cristofani
* **Source:** [Daniel B. Cristofani's `fib.b` (brainfuck.org)](http://www.brainfuck.org/fib.b)

```brainfuck title="Decimal Fibonacci Generator by Daniel B. Cristofani"
>++++++++++>+>+[
    [+++++[>++++++++<-]>.<++++++[>--------<-]+<<<]>.>>[
        [-]<[>+<-]>>[<<+>+>-]<[>+<-[>+<-[>+<-[>+<-[>+<-[>+<-
            [>+<-[>+<-[>+<-[>[-]>+>+<<<-[>+<-]]]]]]]]]]]+>>>
    ]<<<
]
```

**Terminal Output:**
```text
0
1
1
2
3
5
8
13
21
34
55
89
144
233
377
610
...
```

---

## 7. Streaming Golden Ratio Generator

[:material-download: Download `golden.b`](../programs/math/golden.b){ .md-button .md-button--small }

Authored by **Daniel B. Cristofani** (2019), this program computes and streams the decimal digits of the Golden Ratio (φ = (1 + √5)/2 ≈ 1.6180339887..., [OEIS A001622](https://oeis.org/A001622)) continuously in pure Brainfuck.

* **Author:** Daniel B. Cristofani (2019)
* **Source:** [Daniel B. Cristofani's `golden.b` (brainfuck.org)](http://www.brainfuck.org/golden.b)

```brainfuck title="Golden Ratio Generator by Daniel B. Cristofani (2019)"
+>>>>>>>++>+>+>+>++<[
    +[
        --[++>>--]->--[
            +[
                +<+[-<<+]++<<[-[->-[>>-]++<[<<]++<<-]+<<]>>>>-<<<<
                <++<-<<++++++[<++++++++>-]<.---<[->.[-]+++++>]>[[-]>>]
            ]+>>--
        ]+<+[-<+<+]++>>
    ]<<<<[[<<]>>[-[+++<<-]+>>-]++[<<]<<<<<+>]
    >[->>[[>>>[>>]+[-[->>+>>>>-[-[+++<<[-]]+>>-]++[<<]]+<<]<-]<]]>>>>>>>
]
```

**Terminal Output:**
```text
1.618033988749894848204586834365638117720309...
```

---

## 8. Arbitrary-Precision Factorials

[:material-download: Download `factorial.b`](../programs/math/factorial.b){ .md-button .md-button--small }

Authored by **Daniel B. Cristofani** (2019), this program computes and emits consecutive factorials (1!, 2!, 3!, 4!, ..., [OEIS A000142](https://oeis.org/A000142)) in multi-precision decimal format. It uses an optimized long-multiplication and carry algorithm.

* **Author:** Daniel B. Cristofani (2019)
* **Source:** [Daniel B. Cristofani's `factorial2.b` (brainfuck.org)](http://www.brainfuck.org/factorial2.b)

```brainfuck title="Arbitrary-Precision Factorials by Daniel B. Cristofani (2019)"
>>>>++>+[
    [
        >[>>]<[>+>]<<[>->>+<<<-]>+[
            [+>>[<<+>>-]>]+[-<<+<]>-[
                -[<+>>+<-]++++++[>++++++++<-]+>.[-]<<[
                    >>>[[<<+>+>-]>>>]<<<<[[>+<-]<-<<]>-
                ]>>>[
                    <<-[<<+>>-]<+++++++++<[
                        >[->+>]>>>[<<[<+>-]>>>+>>[-<]<[>]>+<]<<<<<<-
                    ]>[-]>+>>[<<<+>>>-]>>>
                ]<<<+[-[+>>]<<<]>[<<<]>
            ]>>>[<[>>>]<<<[[>>>+<<<-]<<<]>>>>>>>-[<]>>>[<<]<<[>+>]<]<<
        ]++>>
    ]<<++++++++.+
]
```

**Terminal Output:**
```text
1
1
2
6
24
120
720
5040
40320
362880
3628800
...
```

---

## 9. Interactive Conway's Game of Life

[:material-download: Download `life.b`](../programs/games/life.b){ .md-button .md-button--small }

John Horton Conway's Game of Life 2D cellular automaton running on a 10×10 toroidal grid. Authored by **Daniel B. Cristofani** (2021), this program duplicates the interactive interface of the classic program by **Linus Åkesson** (2007). Enter cell coordinates (e.g. `be` to toggle row `b`, column `e`), type `q` to quit, or send a bare newline (`\n`) to advance to the next generation!

* **Author:** Daniel B. Cristofani (2021) based on interface by Linus Åkesson (2007)
* **Source:** [Daniel B. Cristofani's `life.b` (brainfuck.org)](http://www.brainfuck.org/life.b) / [Linus Åkesson's Game of Life](http://www.linusakesson.net/programming/brainfuck/index.php)

```brainfuck title="Conway's Game of Life (10x10 Toroidal)"
>>>->+>+++++>++++++++++[[>>>+<<<-]>+++++>+>>+[<<+>>>>>+<<<-]<-]>>>>[
  [>>>+>+<<<<-]+++>>+[<+>>>+>+<<<-]>>[>[[>>>+<<<-]<]<<++>+>>>>>>-]<-
]+++>+>[[-]<+<[>+++++++++++++++++<-]<+]>>[
  [+++++++++.-------->>>]+[-<<<]>>>[>>,----------[>]<]<<[
    <<<[
      >--[<->>+>-<<-]<[[>>>]+>-[+>>+>-]+[<<<]<-]>++>[<+>-]
      >[[>>>]+[<<<]>>>-]+[->>>]<-[++>]>[------<]>+++[<<<]>
    ]<
  ]>[
    -[+>>+>-]+>>+>>>+>[<<<]>->+>[
      >[->+>+++>>++[>>>]+++<<<++<<<++[>>>]>>>]<<<[>[>>>]+>>>]
      <<<<<<<[<<++<+[-<<<+]->++>>>++>>>++<<<<]<<<+[-<<<+]+>->>->>
    ]<<+<<+<<<+<<-[+<+<<-]+<+[
      ->+>[-<-<<[<<<]>[>>[>>>]<<+<[<<<]>-]]
      <[<[<[<<<]>+>>[>>>]<<-]<[<<<]]>>>->>>[>>>]+>
    ]>+[-<<[-]<]-[
      [>>>]<[<<[<<<]>>>>>+>[>>>]<-]>>>[>[>>>]<<<<+>[<<<]>>-]>
    ]<<<<<<[---<-----[-[-[<->>+++<+++++++[-]]]]<+<+]>
  ]>>
]
```

**Terminal Output:**
```text
 abcdefghij
a----------
b----------
c----------
d----------
e----------
f----------
g----------
h----------
i----------
j----------
>
```

---

## 10. Unbounded Streaming π Spigot (128 kB SRAM)

[:material-download: Download `pi_stream.b` (20.2 kB)](../programs/pi/pi_stream.b){ .md-button .md-button--primary }
[:material-book-open-page-variant: Read Full Technical Deep-Dive →](pi-spigot.md){ .md-button }

An unbounded streaming π spigot executing directly on bare FPGA silicon. Based on Jeremy Gibbons' Linear Fractional Transformation (LFT) spigot algorithm, it features a **pure Brainfuck dynamic on-demand self-expanding tape engine**:
- **Instantaneous Boot**: Starts with only 4 active slots (64 bytes), emitting early digits in **< 0.05 seconds** (2,033× faster than fixed-allocation startup).
- **Dynamic Growth**: Dynamically activates new 16-byte register slots across the full 128 kB SRAM tape (8,000 slots) whenever arithmetic carries propagate.
- **Hardware Verified**: Emits continuous digits (`3.141592653...`) at 12 MHz on bare silicon.

* **Algorithm:** Jeremy Gibbons (2004), *Unbounded Spigot Algorithms for the Digits of Pi*
* **Architecture:** [Brainfuino Dynamic Tape Spigot Specification](pi-spigot.md)

**Terminal Output:**
```text
=======================================================
>>> FPGA SOFT-PROCESSOR LAUNCHED ON SILICON!
>>> Streaming Pi digits in real-time:
=======================================================
3.141592653...
```

??? example "View Brainfuck Code Preview (`pi_stream.b` - 20,261 bytes)"
    Because `pi_stream.b` is 20 kB, it is recommended to download the file directly or use the automated Python runner:
    ```bash
    python programs/pi/run_pi.py programs/pi/pi_stream.b --count 10
    ```
    For the complete source code, mathematical proofs, and architectural details, see the [Streaming π Spigot Deep-Dive](pi-spigot.md).

---

??? tip "Brainfuck Programs, Compilers & Formal Verification"
    Looking for more Brainfuck programs, compilers, or formal proofs?
    
    * **[Daniel B. Cristofani (brainfuck.org)](http://brainfuck.org/):** Archive of classic Brainfuck algorithms, quines, games, and reference implementations.
    * **[Linus Åkesson (linusakesson.net)](http://www.linusakesson.net/programming/brainfuck/index.php):** Creator of the original Brainfuck Game of Life and Mandelbrot viewer explorations.
    * **[bfcoq (Thalia Archibald)](https://github.com/thaliaarchi/bfcoq):** A formally verified Brainfuck compiler and Turing-completeness proof written in the Coq theorem prover.
    * **[Thalia's Research (thalia.dev)](https://thalia.dev/):** Compilers, programming language theory, and explorations of unexpected Turing completeness.
    * **[Brainfuck Community Archive](https://esolangs.org/wiki/Brainfuck):** Comprehensive reference collection of classic Brainfuck algorithms, games, fractals, and interpreters.

