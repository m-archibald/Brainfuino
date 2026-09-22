"""
generate_pi_stream.py - Generator for Unbounded Streaming Pi Spigot in pure Brainfuck.
Implements Jeremy Gibbons' Linear Fractional Transformation (LFT) spigot algorithm
using interleaved fixed-bank decimal registers on the tape with local slot scratch windows.
"""

import os
import sys

class BFStreamBuilder:
    def __init__(self, num_slots=20):
        self.num_slots = num_slots
        self.header_size = 16
        self.slot_width = 12

        # Offsets within each slot:
        self.OFF_Q = 0
        self.OFF_R = 1
        self.OFF_T = 2
        self.OFF_TMP1 = 3
        self.OFF_TMP2 = 4
        self.OFF_CARRY = 5
        self.OFF_SCRATCH1 = 6
        self.OFF_SCRATCH2 = 7
        self.OFF_DM_C = 8
        self.OFF_DM_T1 = 9
        self.OFF_DM_T2 = 10
        self.OFF_DM_Z = 11

        # Header cell offsets:
        self.H_TMP0 = 0
        self.H_TMP1 = 1
        self.H_TMP2 = 2
        self.H_TMP3 = 3
        self.H_SCALAR = 4
        self.H_K = 5
        self.H_L = 6
        self.H_N = 7
        self.H_RSIGN = 8
        self.H_COND = 9
        self.H_DOT_DONE = 10
        self.H_CHAR = 11
        self.H_LOOP = 12
        self.H_CMP_RES = 13
        self.H_BRANCH_B = 14
        self.H_SCRATCH = 15

        self.ptr = 0
        self.code = []

    def to_cell(self, target):
        dist = target - self.ptr
        if dist > 0:
            self.code.append('>' * dist)
        elif dist < 0:
            self.code.append('<' * (-dist))
        self.ptr = target

    def clear(self, cell):
        self.to_cell(cell)
        self.code.append('[-]')

    def add_const(self, cell, val):
        self.to_cell(cell)
        val = val & 0xFF
        if val == 0:
            return
        if val <= 128:
            self.code.append('+' * val)
        else:
            self.code.append('-' * (256 - val))

    def set_const(self, cell, val):
        self.clear(cell)
        self.add_const(cell, val)

    def move(self, src, dst):
        self.clear(dst)
        self.to_cell(src)
        self.code.append('[-')
        self.to_cell(dst)
        self.code.append('+')
        self.to_cell(src)
        self.code.append(']')

    def copy(self, src, dst, tmp):
        self.clear(dst)
        self.clear(tmp)
        self.to_cell(src)
        self.code.append('[-')
        self.to_cell(dst)
        self.code.append('+')
        self.to_cell(tmp)
        self.code.append('+')
        self.to_cell(src)
        self.code.append(']')
        self.to_cell(tmp)
        self.code.append('[-')
        self.to_cell(src)
        self.code.append('+')
        self.to_cell(tmp)
        self.code.append(']')

    def slot_cell(self, slot_idx, offset):
        return self.header_size + slot_idx * self.slot_width + offset

    def divmod_10(self, cell_n, cell_rem, cell_quot, scratch_base):
        c_cell = scratch_base + 0
        t1_cell = scratch_base + 1
        t2_cell = scratch_base + 2
        z_cell = scratch_base + 3

        self.clear(cell_quot)
        self.clear(t1_cell)
        self.clear(t2_cell)
        self.clear(z_cell)
        self.set_const(c_cell, 10)

        self.to_cell(cell_n)
        self.code.append('[')
        self.add_const(c_cell, -1)

        self.to_cell(c_cell)
        self.code.append('[-')
        self.to_cell(t1_cell)
        self.code.append('+')
        self.to_cell(t2_cell)
        self.code.append('+')
        self.to_cell(c_cell)
        self.code.append(']')

        self.to_cell(t2_cell)
        self.code.append('[-')
        self.to_cell(c_cell)
        self.code.append('+')
        self.to_cell(t2_cell)
        self.code.append(']')

        self.set_const(z_cell, 1)
        self.to_cell(t1_cell)
        self.code.append('[-')
        self.clear(z_cell)
        self.clear(t1_cell)
        self.to_cell(t1_cell)
        self.code.append(']')

        self.to_cell(z_cell)
        self.code.append('[-')
        self.to_cell(cell_quot)
        self.code.append('+')
        self.add_const(c_cell, 10)
        self.to_cell(z_cell)
        self.code.append(']')

        self.to_cell(cell_n)
        self.code.append('-]')

        self.set_const(cell_rem, 10)
        self.to_cell(c_cell)
        self.code.append('[-')
        self.to_cell(cell_rem)
        self.code.append('-')
        self.to_cell(c_cell)
        self.code.append(']')
        self.clear(c_cell)

    def zero_reg(self, offset):
        for i in range(self.num_slots):
            self.clear(self.slot_cell(i, offset))

    def copy_reg(self, src_off, dst_off):
        for i in range(self.num_slots):
            s = self.slot_cell(i, src_off)
            d = self.slot_cell(i, dst_off)
            t = self.slot_cell(i, self.OFF_SCRATCH1)
            self.copy(s, d, t)

    def add_reg(self, src_off, dst_off):
        self.clear(self.slot_cell(0, self.OFF_CARRY))
        for i in range(self.num_slots):
            dst = self.slot_cell(i, dst_off)
            src = self.slot_cell(i, src_off)
            cin = self.slot_cell(i, self.OFF_CARRY)
            cout = self.slot_cell(i + 1, self.OFF_CARRY) if i + 1 < self.num_slots else self.H_TMP3
            t = self.slot_cell(i, self.OFF_SCRATCH1)
            sb = self.slot_cell(i, self.OFF_DM_C)

            self.to_cell(src)
            self.code.append('[-')
            self.to_cell(dst)
            self.code.append('+')
            self.to_cell(t)
            self.code.append('+')
            self.to_cell(src)
            self.code.append(']')
            self.move(t, src)

            self.to_cell(cin)
            self.code.append('[-')
            self.to_cell(dst)
            self.code.append('+')
            self.to_cell(cin)
            self.code.append(']')

            self.divmod_10(dst, dst, cout, sb)

    def sub_reg(self, src_off, dst_off):
        self.clear(self.slot_cell(0, self.OFF_CARRY))
        for i in range(self.num_slots):
            dst = self.slot_cell(i, dst_off)
            src = self.slot_cell(i, src_off)
            bin_cell = self.slot_cell(i, self.OFF_CARRY)
            bout_cell = self.slot_cell(i + 1, self.OFF_CARRY) if i + 1 < self.num_slots else self.H_TMP3
            t = self.slot_cell(i, self.OFF_SCRATCH1)
            sb = self.slot_cell(i, self.OFF_DM_C)
            q_tmp = self.slot_cell(i, self.OFF_SCRATCH2)

            self.add_const(dst, 10)
            self.to_cell(src)
            self.code.append('[-')
            self.to_cell(dst)
            self.code.append('-')
            self.to_cell(t)
            self.code.append('+')
            self.to_cell(src)
            self.code.append(']')
            self.move(t, src)

            self.to_cell(bin_cell)
            self.code.append('[-')
            self.to_cell(dst)
            self.code.append('-')
            self.to_cell(bin_cell)
            self.code.append(']')

            self.divmod_10(dst, dst, q_tmp, sb)

            self.set_const(bout_cell, 1)
            self.to_cell(q_tmp)
            self.code.append('[-')
            self.to_cell(bout_cell)
            self.code.append('-')
            self.to_cell(q_tmp)
            self.code.append(']')

    def mul_scalar_const(self, reg_off, factor):
        if factor == 1:
            return
        if factor == 0:
            self.zero_reg(reg_off)
            return

        self.clear(self.slot_cell(0, self.OFF_CARRY))
        for i in range(self.num_slots):
            cell = self.slot_cell(i, reg_off)
            cin = self.slot_cell(i, self.OFF_CARRY)
            cout = self.slot_cell(i + 1, self.OFF_CARRY) if i + 1 < self.num_slots else self.H_TMP3
            t = self.slot_cell(i, self.OFF_SCRATCH1)
            sb = self.slot_cell(i, self.OFF_DM_C)

            self.move(cin, t)
            self.to_cell(cell)
            self.code.append('[-')
            self.to_cell(t)
            self.code.append('+' * factor)
            self.to_cell(cell)
            self.code.append(']')

            self.divmod_10(t, cell, cout, sb)

    def mul_scalar_cell(self, reg_off, scalar_cell):
        self.clear(self.slot_cell(0, self.OFF_CARRY))
        for i in range(self.num_slots):
            cell = self.slot_cell(i, reg_off)
            cin = self.slot_cell(i, self.OFF_CARRY)
            cout = self.slot_cell(i + 1, self.OFF_CARRY) if i + 1 < self.num_slots else self.H_TMP3
            t = self.slot_cell(i, self.OFF_SCRATCH1)
            s_copy = self.slot_cell(i, self.OFF_SCRATCH2)
            sb = self.slot_cell(i, self.OFF_DM_C)

            self.move(cin, t)
            self.to_cell(cell)
            self.code.append('[')
            self.copy(scalar_cell, s_copy, self.slot_cell(i, self.OFF_DM_T1))
            self.to_cell(s_copy)

            self.code.append('[-')
            self.to_cell(t)
            self.code.append('+')
            self.to_cell(s_copy)
            self.code.append(']')
            self.to_cell(cell)
            self.code.append('-]')

            self.divmod_10(t, cell, cout, sb)

    def cmp_reg(self, a_off, b_off):
        self.clear(self.H_CMP_RES)
        for i in range(self.num_slots - 1, -1, -1):
            a_cell = self.slot_cell(i, a_off)
            b_cell = self.slot_cell(i, b_off)
            t_a = self.slot_cell(i, self.OFF_SCRATCH1)
            t_b = self.slot_cell(i, self.OFF_SCRATCH2)
            s_tmp0 = self.slot_cell(i, self.OFF_DM_C)
            s_tmp1 = self.slot_cell(i, self.OFF_DM_T1)
            s_tmp2 = self.slot_cell(i, self.OFF_DM_T2)
            s_tmp3 = self.slot_cell(i, self.OFF_DM_Z)

            # Check if H_CMP_RES == 0 using slot scratch:
            self.copy(self.H_CMP_RES, s_tmp0, s_tmp1)
            self.set_const(s_tmp2, 1)
            self.to_cell(s_tmp0)
            self.code.append('[-')
            self.clear(s_tmp2)
            self.clear(s_tmp0)
            self.to_cell(s_tmp0)
            self.code.append(']')

            # If H_CMP_RES was 0:
            self.to_cell(s_tmp2)
            self.code.append('[')

            self.copy(a_cell, t_a, s_tmp0)
            self.copy(b_cell, t_b, s_tmp0)

            # ais523: z = t_a > t_b -> s_tmp3
            self.clear(s_tmp0)
            self.clear(s_tmp1)
            self.clear(s_tmp3)
            self.to_cell(t_a)
            self.code.append('[-')
            self.add_const(s_tmp0, 1)
            self.to_cell(t_b)
            self.code.append('[-')
            self.clear(s_tmp0)
            self.add_const(s_tmp1, 1)
            self.to_cell(t_b)
            self.code.append(']')
            self.to_cell(s_tmp0)
            self.code.append('[-')
            self.add_const(s_tmp3, 1)
            self.to_cell(s_tmp0)
            self.code.append(']')
            self.to_cell(s_tmp1)
            self.code.append('[-')
            self.to_cell(t_b)
            self.code.append('+')
            self.to_cell(s_tmp1)
            self.code.append(']')
            self.add_const(t_b, -1)
            self.to_cell(t_a)
            self.code.append(']')
            self.clear(t_b)

            # If s_tmp3 == 1: set H_CMP_RES = 1
            self.to_cell(s_tmp3)
            self.code.append('[-')
            self.set_const(self.H_CMP_RES, 1)
            self.to_cell(s_tmp3)
            self.code.append(']')

            # Now check if t_b > t_a (if H_CMP_RES is still 0)
            self.copy(self.H_CMP_RES, s_tmp0, s_tmp1)
            self.set_const(s_tmp3, 1)
            self.to_cell(s_tmp0)
            self.code.append('[-')
            self.clear(s_tmp3)
            self.clear(s_tmp0)
            self.to_cell(s_tmp0)
            self.code.append(']')

            self.to_cell(s_tmp3)
            self.code.append('[')
            self.copy(a_cell, t_a, s_tmp0)
            self.copy(b_cell, t_b, s_tmp0)
            self.clear(s_tmp0)
            self.clear(s_tmp1)
            self.clear(s_tmp2)
            self.to_cell(t_b)
            self.code.append('[-')
            self.add_const(s_tmp0, 1)
            self.to_cell(t_a)
            self.code.append('[-')
            self.clear(s_tmp0)
            self.add_const(s_tmp1, 1)
            self.to_cell(t_a)
            self.code.append(']')
            self.to_cell(s_tmp0)
            self.code.append('[-')
            self.add_const(s_tmp2, 1)
            self.to_cell(s_tmp0)
            self.code.append(']')
            self.to_cell(s_tmp1)
            self.code.append('[-')
            self.to_cell(t_a)
            self.code.append('+')
            self.to_cell(s_tmp1)
            self.code.append(']')
            self.add_const(t_a, -1)
            self.to_cell(t_b)
            self.code.append(']')
            self.clear(t_a)

            self.to_cell(s_tmp2)
            self.code.append('[-')
            self.set_const(self.H_CMP_RES, 2)
            self.to_cell(s_tmp2)
            self.code.append(']')

            self.clear(s_tmp3)
            self.to_cell(s_tmp3)
            self.code.append(']')

            self.clear(s_tmp2)
            self.to_cell(s_tmp2)
            self.code.append(']')

    def div_single(self, num_off, den_off, quot_cell):
        self.clear(quot_cell)
        self.set_const(self.H_LOOP, 1)

        self.to_cell(self.H_LOOP)
        self.code.append('[')

        self.cmp_reg(num_off, den_off)
        self.copy(self.H_CMP_RES, self.H_TMP2, self.H_TMP3)
        self.add_const(self.H_TMP2, -2)
        self.set_const(self.H_SCRATCH, 1)
        self.to_cell(self.H_TMP2)
        self.code.append('[-')
        self.clear(self.H_SCRATCH)
        self.clear(self.H_TMP2)
        self.to_cell(self.H_TMP2)
        self.code.append(']')

        self.set_const(self.H_COND, 1)
        self.to_cell(self.H_SCRATCH)
        self.code.append('[-')
        self.clear(self.H_LOOP)
        self.clear(self.H_COND)
        self.to_cell(self.H_SCRATCH)
        self.code.append(']')

        self.to_cell(self.H_COND)
        self.code.append('[-')
        self.sub_reg(den_off, num_off)
        self.add_const(quot_cell, 1)
        self.to_cell(self.H_COND)
        self.code.append(']')

        self.to_cell(self.H_LOOP)
        self.code.append(']')

    def build_pi_stream(self):
        # 1. Initialization
        self.set_const(self.H_K, 1)
        self.set_const(self.H_L, 3)
        self.set_const(self.H_N, 3)
        self.set_const(self.H_RSIGN, 0)
        self.set_const(self.H_DOT_DONE, 0)
        self.zero_reg(self.OFF_Q)
        self.zero_reg(self.OFF_R)
        self.zero_reg(self.OFF_T)
        self.set_const(self.slot_cell(0, self.OFF_Q), 1)  # Q = 1
        self.set_const(self.slot_cell(0, self.OFF_T), 1)  # T = 1

        # 2. Main Infinite Streaming Loop:
        self.set_const(self.H_SCALAR, 1)
        self.to_cell(self.H_SCALAR)
        self.code.append('[')

        # Condition: 4q + r < (n + 1)t
        self.copy_reg(self.OFF_Q, self.OFF_TMP1)
        self.mul_scalar_const(self.OFF_TMP1, 4)

        self.copy_reg(self.OFF_T, self.OFF_TMP2)
        self.copy(self.H_N, self.H_TMP0, self.H_TMP1)
        self.add_const(self.H_TMP0, 1)
        self.mul_scalar_cell(self.OFF_TMP2, self.H_TMP0)

        self.copy(self.H_RSIGN, self.H_TMP0, self.H_TMP1)
        self.set_const(self.H_COND, 1)
        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        self.clear(self.H_COND)
        self.add_reg(self.OFF_R, self.OFF_TMP2)
        self.clear(self.H_TMP0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.to_cell(self.H_COND)
        self.code.append('[-')
        self.add_reg(self.OFF_R, self.OFF_TMP1)
        self.to_cell(self.H_COND)
        self.code.append(']')

        self.cmp_reg(self.OFF_TMP1, self.OFF_TMP2)
        self.copy(self.H_CMP_RES, self.H_TMP0, self.H_TMP1)
        self.add_const(self.H_TMP0, -2)
        self.set_const(self.H_COND, 0)
        self.set_const(self.H_SCRATCH, 1)
        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        self.clear(self.H_SCRATCH)
        self.clear(self.H_TMP0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.to_cell(self.H_SCRATCH)
        self.code.append('[-')
        self.set_const(self.H_COND, 1)
        self.to_cell(self.H_SCRATCH)
        self.code.append(']')

        # ----------------------------------------------------
        # BRANCH A: CONDITION IS TRUE (EMIT DIGIT)
        # ----------------------------------------------------
        self.to_cell(self.H_COND)
        self.code.append('[')

        # Output digit N ('0' + N):
        self.copy(self.H_N, self.H_CHAR, self.H_TMP0)
        self.add_const(self.H_CHAR, 48)
        self.to_cell(self.H_CHAR)
        self.code.append('.')
        self.clear(self.H_CHAR)

        # Output '.' after first digit:
        self.copy(self.H_DOT_DONE, self.H_TMP0, self.H_TMP1)
        self.set_const(self.H_TMP2, 1)
        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        self.clear(self.H_TMP2)
        self.clear(self.H_TMP0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.to_cell(self.H_TMP2)
        self.code.append('[-')
        self.set_const(self.H_CHAR, 46)  # '.'
        self.to_cell(self.H_CHAR)
        self.code.append('.')
        self.clear(self.H_CHAR)
        self.set_const(self.H_DOT_DONE, 1)
        self.to_cell(self.H_TMP2)
        self.code.append(']')

        # Update r: r_new = 10 * (r - n * t)
        self.copy_reg(self.OFF_T, self.OFF_TMP1)
        self.mul_scalar_cell(self.OFF_TMP1, self.H_N)

        self.copy(self.H_RSIGN, self.H_TMP0, self.H_TMP1)
        self.set_const(self.H_TMP2, 1)
        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        self.clear(self.H_TMP2)
        self.add_reg(self.OFF_TMP1, self.OFF_R)
        self.clear(self.H_TMP0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.to_cell(self.H_TMP2)
        self.code.append('[-')
        # r_sign == 0: compare R with TMP1 (n*t)
        self.cmp_reg(self.OFF_R, self.OFF_TMP1)
        self.copy(self.H_CMP_RES, self.H_TMP0, self.H_TMP1)
        self.add_const(self.H_TMP0, -2)
        self.set_const(self.H_TMP3, 1)
        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        self.clear(self.H_TMP3)
        self.clear(self.H_TMP0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.set_const(self.H_TMP0, 1)
        self.to_cell(self.H_TMP3)
        self.code.append('[-')
        self.clear(self.H_TMP0)
        # R < n*t -> R = n*t - R, r_sign = 1
        self.sub_reg(self.OFF_R, self.OFF_TMP1)
        self.copy_reg(self.OFF_TMP1, self.OFF_R)
        self.set_const(self.H_RSIGN, 1)
        self.to_cell(self.H_TMP3)
        self.code.append(']')

        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        # R >= n*t -> R = R - n*t, r_sign = 0
        self.sub_reg(self.OFF_TMP1, self.OFF_R)
        self.set_const(self.H_RSIGN, 0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.to_cell(self.H_TMP2)
        self.code.append(']')

        self.mul_scalar_const(self.OFF_R, 10)

        # Update n: n_new = (30*q + r) // t
        self.copy_reg(self.OFF_Q, self.OFF_TMP1)
        self.mul_scalar_const(self.OFF_TMP1, 30)

        self.copy(self.H_RSIGN, self.H_TMP0, self.H_TMP1)
        self.set_const(self.H_TMP2, 1)
        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        self.clear(self.H_TMP2)
        self.sub_reg(self.OFF_R, self.OFF_TMP1)
        self.clear(self.H_TMP0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.to_cell(self.H_TMP2)
        self.code.append('[-')
        self.add_reg(self.OFF_R, self.OFF_TMP1)
        self.to_cell(self.H_TMP2)
        self.code.append(']')

        self.div_single(self.OFF_TMP1, self.OFF_T, self.H_N)
        self.mul_scalar_const(self.OFF_Q, 10)

        self.clear(self.H_COND)
        self.to_cell(self.H_COND)
        self.code.append(']')  # END BRANCH A

        # ----------------------------------------------------
        # BRANCH B: CONDITION IS FALSE (CONSUME TERM / STEP)
        # ----------------------------------------------------
        # Condition was False iff H_CMP_RES != 2:
        self.copy(self.H_CMP_RES, self.H_TMP0, self.H_TMP1)
        self.add_const(self.H_TMP0, -2)
        self.set_const(self.H_BRANCH_B, 0)
        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        self.set_const(self.H_BRANCH_B, 1)
        self.clear(self.H_TMP0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.to_cell(self.H_BRANCH_B)
        self.code.append('[')

        # 1. t = t * l:
        self.mul_scalar_cell(self.OFF_T, self.H_L)

        # 2. r_new = (2q + r) * l:
        self.copy_reg(self.OFF_Q, self.OFF_TMP1)
        self.mul_scalar_const(self.OFF_TMP1, 2)

        self.copy(self.H_RSIGN, self.H_TMP1, self.H_TMP2)
        self.set_const(self.H_TMP3, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_TMP3)
        # r_sign == 1: compare 2q (TMP1) with |r| (R)
        self.cmp_reg(self.OFF_TMP1, self.OFF_R)
        self.copy(self.H_CMP_RES, self.H_TMP0, self.H_TMP2)
        self.add_const(self.H_TMP0, -2)
        self.set_const(self.H_SCRATCH, 1)
        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        self.clear(self.H_SCRATCH)
        self.clear(self.H_TMP0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.set_const(self.H_TMP0, 1)
        self.to_cell(self.H_SCRATCH)
        self.code.append('[-')
        self.clear(self.H_TMP0)
        # 2q < |r|: R = |r| - 2q, put into TMP1, r_sign = 1
        self.sub_reg(self.OFF_TMP1, self.OFF_R)
        self.copy_reg(self.OFF_R, self.OFF_TMP1)
        self.set_const(self.H_RSIGN, 1)
        self.to_cell(self.H_SCRATCH)
        self.code.append(']')

        self.to_cell(self.H_TMP0)
        self.code.append('[-')
        # 2q >= |r|: TMP1 = 2q - |r|, r_sign = 0
        self.sub_reg(self.OFF_R, self.OFF_TMP1)
        self.set_const(self.H_RSIGN, 0)
        self.to_cell(self.H_TMP0)
        self.code.append(']')

        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.to_cell(self.H_TMP3)
        self.code.append('[-')
        # r_sign == 0: 2q + r
        self.add_reg(self.OFF_R, self.OFF_TMP1)
        self.to_cell(self.H_TMP3)
        self.code.append(']')

        self.mul_scalar_cell(self.OFF_TMP1, self.H_L)
        self.copy_reg(self.OFF_TMP1, self.OFF_R)



        # 3. n_new = (3k*q + r) // t:
        self.copy(self.H_K, self.H_TMP1, self.H_TMP2)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.add_const(self.H_TMP2, 3)
        self.to_cell(self.H_TMP1)
        self.code.append(']')
        self.move(self.H_TMP2, self.H_TMP1)

        self.copy_reg(self.OFF_Q, self.OFF_TMP1)
        self.mul_scalar_cell(self.OFF_TMP1, self.H_TMP1)

        self.copy(self.H_RSIGN, self.H_TMP1, self.H_TMP2)
        self.set_const(self.H_TMP3, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_TMP3)
        self.sub_reg(self.OFF_R, self.OFF_TMP1)
        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.to_cell(self.H_TMP3)
        self.code.append('[-')
        self.add_reg(self.OFF_R, self.OFF_TMP1)
        self.to_cell(self.H_TMP3)
        self.code.append(']')

        self.div_single(self.OFF_TMP1, self.OFF_T, self.H_N)

        # 4. q = q * k:
        self.mul_scalar_cell(self.OFF_Q, self.H_K)

        # 5. l += 2, k += 1:
        self.add_const(self.H_L, 2)
        self.add_const(self.H_K, 1)

        self.clear(self.H_BRANCH_B)
        self.to_cell(self.H_BRANCH_B)
        self.code.append(']')  # END BRANCH B


        self.to_cell(self.H_SCALAR)
        self.code.append(']')  # END INFINITE STREAMING LOOP

    def get_code(self):
        return "".join(self.code)


def simulate_bf(code, max_steps=50000000, tape_size=4096):
    tape = [0] * tape_size
    ptr = 0
    pc = 0
    bracket_map = {}
    stack = []
    for i, c in enumerate(code):
        if c == '[': stack.append(i)
        elif c == ']':
            j = stack.pop()
            bracket_map[i] = j
            bracket_map[j] = i

    steps = 0
    output = []
    while pc < len(code) and steps < max_steps:
        c = code[pc]
        if c == '>': ptr = (ptr + 1) % tape_size
        elif c == '<': ptr = (ptr - 1) % tape_size
        elif c == '+': tape[ptr] = (tape[ptr] + 1) & 0xFF
        elif c == '-': tape[ptr] = (tape[ptr] - 1) & 0xFF
        elif c == '.': output.append(chr(tape[ptr]))
        elif c == '[':
            if tape[ptr] == 0: pc = bracket_map[pc]
        elif c == ']':
            if tape[ptr] != 0: pc = bracket_map[pc]
        pc += 1
        steps += 1
    return "".join(output), tape, steps


if __name__ == "__main__":
    builder = BFStreamBuilder(num_slots=20)
    print("Building streaming pi program with local slot scratch...")
    builder.build_pi_stream()
    code = builder.get_code()
    print(f"Generated streaming Brainfuck program: {len(code)} bytes")
    out_file = os.path.join(os.path.dirname(__file__), "pi_stream.b")
    with open(out_file, "w") as f:
        f.write(code)
    print(f"Saved to {out_file}")
