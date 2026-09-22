#!/usr/bin/env python3
"""
Brainfuino Pi Generator
Generates a verified spigot algorithm program in pure Brainfuck
tailored for the Brainfuino FPGA soft-processor.
"""

import sys
import os
import time

class BFBuilder:
    def __init__(self):
        self.code = []
        self.ptr = 0

    def to_cell(self, target):
        diff = target - self.ptr
        if diff > 0:
            self.code.append('>' * diff)
        elif diff < 0:
            self.code.append('<' * (-diff))
        self.ptr = target

    def add(self, cell, val):
        if val == 0:
            return
        self.to_cell(cell)
        if val > 0:
            self.code.append('+' * val)
        else:
            self.code.append('-' * (-val))

    def clear(self, cell):
        self.to_cell(cell)
        self.code.append('[-]')

    def set_const(self, cell, val):
        val = val & 0xFF
        self.clear(cell)
        if val == 0:
            return
        if val <= 12:
            self.add(cell, val)
        else:
            best_a, best_b, best_rem = 1, val, 0
            best_cost = val
            for a in range(2, 16):
                b = val // a
                rem = val % a
                cost = a + b + rem + 9
                if cost < best_cost:
                    best_cost = cost
                    best_a, best_b, best_rem = a, b, rem
            if best_a > 1:
                tmp = 15 if cell != 15 else 14
                self.clear(tmp)
                self.add(tmp, best_a)
                self.to_cell(tmp)
                self.code.append('[')
                self.to_cell(cell)
                self.add(cell, best_b)
                self.to_cell(tmp)
                self.code.append('-]')
                if best_rem > 0:
                    self.add(cell, best_rem)
            else:
                self.add(cell, val)

    def move(self, src, dst):
        self.clear(dst)
        self.to_cell(src)
        self.code.append('[')
        self.to_cell(dst)
        self.code.append('+')
        self.to_cell(src)
        self.code.append('-]')

    def copy(self, src, dst, tmp):
        self.clear(dst)
        self.clear(tmp)
        self.to_cell(src)
        self.code.append('[')
        self.to_cell(dst)
        self.code.append('+')
        self.to_cell(tmp)
        self.code.append('+')
        self.to_cell(src)
        self.code.append('-]')
        self.to_cell(tmp)
        self.code.append('[')
        self.to_cell(src)
        self.code.append('+')
        self.to_cell(tmp)
        self.code.append('-]')

    def add_mult(self, src, dst, factor=1):
        if factor == 0:
            return
        self.to_cell(src)
        self.code.append('[')
        self.to_cell(dst)
        if factor > 0:
            self.code.append('+' * factor)
        elif factor < 0:
            self.code.append('-' * (-factor))
        self.to_cell(src)
        self.code.append('-]')

    def emit_char(self, char):
        tmp = 14
        self.set_const(tmp, ord(char))
        self.to_cell(tmp)
        self.code.append('.')
        self.clear(tmp)

    def emit_dot(self, cell):
        self.to_cell(cell)
        self.code.append('.')

    def divmod_const(self, cell_n, d_val, cell_rem, cell_quot, scratch_base):
        """
        Deterministic, bounded divmod by compile-time constant d_val.
        Uses 4 scratch cells at scratch_base:
          scratch_base + 0: C (countdown counter, 1..d_val)
          scratch_base + 1: temp1
          scratch_base + 2: temp2
          scratch_base + 3: is_zero
        Zero unbounded seeking. Fully deterministic static pointer shifts.
        """
        assert d_val > 0, "Division by zero"
        c_cell = scratch_base + 0
        t1_cell = scratch_base + 1
        t2_cell = scratch_base + 2
        z_cell = scratch_base + 3

        self.clear(cell_quot)
        self.clear(t1_cell)
        self.clear(t2_cell)
        self.clear(z_cell)
        self.set_const(c_cell, d_val)

        # Loop on N:
        self.to_cell(cell_n)
        self.code.append('[')
        
        # C -= 1
        self.add(c_cell, -1)

        # Check if C == 0:
        # Copy C to t1, t2
        self.to_cell(c_cell)
        self.code.append('[-')
        self.to_cell(t1_cell)
        self.code.append('+')
        self.to_cell(t2_cell)
        self.code.append('+')
        self.to_cell(c_cell)
        self.code.append(']')

        # Restore C from t2
        self.to_cell(t2_cell)
        self.code.append('[-')
        self.to_cell(c_cell)
        self.code.append('+')
        self.to_cell(t2_cell)
        self.code.append(']')

        # is_zero = 1
        self.set_const(z_cell, 1)
        # if t1 > 0: is_zero = 0; t1 = 0
        self.to_cell(t1_cell)
        self.code.append('[-')
        self.clear(z_cell)
        self.clear(t1_cell)
        self.to_cell(t1_cell)
        self.code.append(']')

        # if is_zero == 1: Q += 1; C += d_val; is_zero = 0
        self.to_cell(z_cell)
        self.code.append('[-')
        self.to_cell(cell_quot)
        self.code.append('+')
        self.add(c_cell, d_val)
        self.to_cell(z_cell)
        self.code.append(']')

        # N -= 1
        self.to_cell(cell_n)
        self.code.append('-]')

        # Remainder R = d_val - C
        self.set_const(cell_rem, d_val)
        self.to_cell(c_cell)
        self.code.append('[-')
        self.to_cell(cell_rem)
        self.code.append('-')
        self.to_cell(c_cell)
        self.code.append(']')
        self.clear(c_cell)

    def get_code(self):
        return ''.join(self.code)


def simulate_bf(code, max_steps=500000000):
    tape = [0] * 30000
    ptr = 0
    pc = 0
    out = []
    stack = []
    jumps = {}
    for i, c in enumerate(code):
        if c == '[': stack.append(i)
        elif c == ']':
            s = stack.pop()
            jumps[s] = i
            jumps[i] = s
    steps = 0
    while pc < len(code) and steps < max_steps:
        steps += 1
        c = code[pc]
        if c == '>': ptr += 1
        elif c == '<': ptr -= 1
        elif c == '+': tape[ptr] = (tape[ptr] + 1) & 0xFF
        elif c == '-': tape[ptr] = (tape[ptr] - 1) & 0xFF
        elif c == '.':
            ch = chr(tape[ptr])
            out.append(ch)
            print(ch, end='', flush=True)
        elif c == '[':
            if tape[ptr] == 0: pc = jumps[pc]
        elif c == ']':
            if tape[ptr] != 0: pc = jumps[pc]
        pc += 1
    return ''.join(out), steps


def generate_pi_bf(num_digits=10):
    """
    Generates optimized Brainfuck code to compute num_digits of Pi.
    Uses base-10 bounded spigot algorithm.
    """
    c_init = max(14, int(num_digits * 3.5) + 3)
    bf = BFBuilder()

    # Tape Layout:
    # 0: H (High byte of d, <= 5)
    # 1: L (Low byte of d, 0..255)
    # 2: Q (Quotient)
    # 3: e (Previous digit carry)
    # 4: flag
    # 5..9: divmod scratch block (base 5)
    # 10..13: scratch
    # 16 + b: f[b]
    C_H = 0
    C_L = 1
    C_Q = 2
    C_E = 3
    C_FLAG = 4
    C_DIV_BASE = 5
    C_TMP0 = 11
    C_TMP1 = 12
    C_TMP2 = 13
    C_ARRAY_BASE = 16

    # 1. Print banner / intro
    intro = "Pi = 3."
    # We will let the algorithm print the '3', and we print '.' after '3'
    # Initialize array f[1..c_init] = 2
    for b in range(1, c_init + 1):
        bf.set_const(C_ARRAY_BASE + b, 2)

    c = c_init
    digit_count = 0

    while c > 0 and digit_count < num_digits:
        g = c * 2
        bf.clear(C_H)
        bf.clear(C_L)

        b = c
        while b > 0:
            # 1. Add 10 * f[b] to (H, L)
            # Copy f[b] to C_TMP0
            bf.copy(C_ARRAY_BASE + b, C_TMP0, C_TMP1)
            # Loop C_TMP0 times:
            bf.to_cell(C_TMP0)
            bf.code.append('[')
            # L += 10
            bf.add(C_L, 10)
            # Check if L < 10 (which means it wrapped):
            bf.copy(C_L, C_TMP1, C_TMP2)
            bf.divmod_const(C_TMP1, 10, C_TMP1, C_FLAG, C_DIV_BASE)
            # C_FLAG has L // 10. If C_FLAG == 0, carry = 1:
            bf.set_const(C_TMP1, 1)
            bf.to_cell(C_FLAG)
            bf.code.append('[-')
            bf.clear(C_TMP1)
            bf.clear(C_FLAG)
            bf.to_cell(C_FLAG)
            bf.code.append(']')
            # If C_TMP1 is 1: H += 1
            bf.to_cell(C_TMP1)
            bf.code.append('[-')
            bf.to_cell(C_H)
            bf.code.append('+')
            bf.to_cell(C_TMP1)
            bf.code.append(']')

            bf.to_cell(C_TMP0)
            bf.code.append('-]')

            # 2. Divmod (256 * H + L) by g:
            g -= 1

            # Step 2a: reduce L by g
            bf.copy(C_L, C_TMP0, C_TMP1)
            bf.divmod_const(C_TMP0, g, C_L, C_Q, C_DIV_BASE)
            # Now C_L has L % g, and C_Q has L // g

            # Step 2b: for each unit of H
            q256 = 256 // g
            r256 = 256 % g

            bf.to_cell(C_H)
            bf.code.append('[')
            bf.add(C_Q, q256)
            bf.add(C_L, r256)
            # Reduce L if L >= g
            bf.copy(C_L, C_TMP0, C_TMP1)
            bf.divmod_const(C_TMP0, g, C_L, C_TMP2, C_DIV_BASE)
            # C_TMP2 has extra quotient
            bf.move(C_TMP2, C_TMP0)
            bf.add_mult(C_TMP0, C_Q, 1)

            bf.to_cell(C_H)
            bf.code.append('-]')

            # Remainder is in C_L: update f[b]
            bf.copy(C_L, C_ARRAY_BASE + b, C_TMP0)

            # 3. Next multiplier (b - 1)
            g -= 1
            b -= 1
            if b > 0:
                # (H, L) = Q * b
                # b is constant!
                bf.clear(C_H)
                bf.clear(C_L)
                # For each unit of Q:
                bf.to_cell(C_Q)
                bf.code.append('[')
                # L += b
                bf.add(C_L, b)
                # Check if L < b (which means it wrapped):
                bf.copy(C_L, C_TMP1, C_TMP2)
                bf.divmod_const(C_TMP1, b, C_TMP1, C_FLAG, C_DIV_BASE)
                # C_FLAG has L // b. If C_FLAG == 0, carry = 1:
                bf.set_const(C_TMP1, 1)
                bf.to_cell(C_FLAG)
                bf.code.append('[-')
                bf.clear(C_TMP1)
                bf.clear(C_FLAG)
                bf.to_cell(C_FLAG)
                bf.code.append(']')
                # If C_TMP1 is 1: H += 1
                bf.to_cell(C_TMP1)
                bf.code.append('[-')
                bf.to_cell(C_H)
                bf.code.append('+')
                bf.to_cell(C_TMP1)
                bf.code.append(']')
                bf.to_cell(C_Q)
                bf.code.append('-]')
            else:
                # b == 0: Q is final quotient for this pass!
                # Copy Q to (H, L)
                bf.clear(C_H)
                bf.move(C_Q, C_L)

        c -= 1

        # 4. Output digit
        # total = 256 * H + L (H is 0 since Q <= 30)
        # digit = e + L // 10
        # e = L % 10
        bf.copy(C_L, C_TMP0, C_TMP1)
        bf.divmod_const(C_TMP0, 10, C_TMP0, C_Q, C_DIV_BASE)
        # C_Q has L // 10, C_TMP0 has L % 10

        # digit = e + C_Q
        bf.add_mult(C_Q, C_E, 1)
        # Print digit as ASCII ('0' + digit):
        bf.add(C_E, 48)
        bf.emit_dot(C_E)
        bf.clear(C_E)
        # Next e = C_TMP0
        bf.move(C_TMP0, C_E)

        if digit_count == 0:
            # Print decimal point '.' after first digit ('3')
            bf.emit_char('.')

        digit_count += 1

    # Clean halt loop (+[]) so soft-processor idles cleanly after the last digit
    bf.code.append('+[]')

    return bf.get_code()


if __name__ == '__main__':
    digits = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    print(f"Generating Brainfuck code for {digits} digits of Pi...")
    t0 = time.time()
    code = generate_pi_bf(digits)
    gen_time = time.time() - t0
    print(f"Generated {len(code):,} characters in {gen_time:.2f}s")

    out_file = f"scripts/pi/pi_{digits}.b"
    with open(out_file, 'w') as f:
        f.write(code)
    print(f"Saved to {out_file}")

    print(f"Simulating in Python BF interpreter...")
    t0 = time.time()
    out, steps = simulate_bf(code)
    sim_time = time.time() - t0
    print(f"Simulation output: {out}")
    print(f"Total instructions executed: {steps:,} in {sim_time:.2f}s")
