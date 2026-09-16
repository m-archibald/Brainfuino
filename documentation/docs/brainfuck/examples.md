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

An iconic Brainfuck fractal program authored by **NYYRIKKI** (2002) formatted in the shape of a Sierpinski triangle. It computes and renders a 32-line Sierpinski fractal pattern across an 80-column display, then enters an infinite halt loop `[]`:

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
[]
```

**Terminal Output:**
```text
                                *    
                               * *    
                              *   *    
                             * * * *    
                            *       *    
                           * *     * *    
                          *   *   *   *    
                         * * * * * * * *    
                        *               *    
                       * *             * *    
                      *   *           *   *    
                     * * * *         * * * *    
                    *       *       *       *    
                   * *     * *     * *     * *    
                  *   *   *   *   *   *   *   *    
                 * * * * * * * * * * * * * * * *    
                *                               *    
               * *                             * *    
              *   *                           *   *    
             * * * *                         * * * *    
            *       *                       *       *    
           * *     * *                     * *     * *    
          *   *   *   *                   *   *   *   *    
         * * * * * * * *                 * * * * * * * *    
        *               *               *               *    
       * *             * *             * *             * *    
      *   *           *   *           *   *           *   *    
     * * * *         * * * *         * * * *         * * * *    
    *       *       *       *       *       *       *       *    
   * *     * *     * *     * *     * *     * *     * *     * *    
  *   *   *   *   *   *   *   *   *   *   *   *   *   *   *   *    
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *    
```

---

??? tip "Brainfuck Programs, Compilers & Formal Verification"
    Looking for more Brainfuck programs, compilers, or formal proofs?
    
    * **[bfcoq (Thalia Archibald)](https://github.com/thaliaarchi/bfcoq):** A formally verified Brainfuck compiler and Turing-completeness proof written in the Coq theorem prover.
    * **[Thalia's Research (thalia.dev)](https://thalia.dev/):** Compilers, programming language theory, and explorations of unexpected Turing completeness.
    * **[Brainfuck Community Archive:](https://esolangs.org/wiki/Brainfuck)** Comprehensive reference collection of classic Brainfuck algorithms, games, fractals, and interpreters.
