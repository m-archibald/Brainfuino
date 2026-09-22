"""
generate_pi_stream_looped.py - Generator for Unbounded Streaming Pi Spigot
using Runtime-Looped Slot Architecture in pure Brainfuck.
Code size is constant (~3-4 kB) independent of num_slots.
Supports arbitrary slot counts (32, 64, 128, etc.) on 128 kB SRAM tape.
"""

import os
import sys

class BFLoopedStreamBuilder:
    def __init__(self, num_slots=4, dynamic=True):
        self.num_slots = num_slots
        self.dynamic = dynamic
        self.header_size = 16
        self.W = 16


        # Slot offsets (0..15):
        self.OFF_FLAG = 0
        self.OFF_Q = 1
        self.OFF_R = 2
        self.OFF_T = 3
        self.OFF_TMP1 = 4
        self.OFF_TMP2 = 5
        self.OFF_CARRY = 6
        self.OFF_SCRATCH1 = 7
        self.OFF_SCRATCH2 = 8
        self.OFF_DM_C = 9
        self.OFF_DM_T1 = 10
        self.OFF_DM_T2 = 11
        self.OFF_DM_Z = 12
        self.OFF_SPARE1 = 13
        self.OFF_SPARE2 = 14
        self.OFF_SPARE3 = 15
        self.OFF_TMP3 = 15

        # Header offsets (0..15):
        self.H_ZERO = 0       # Boundary marker (always 0)
        self.H_TMP0 = 1
        self.H_TMP1 = 2
        self.H_TMP2 = 3
        self.H_TMP3 = 4
        self.H_K = 5
        self.H_L = 6
        self.H_N = 7
        self.H_RSIGN = 8
        self.H_COND = 9
        self.H_BRANCH_B = 10
        self.H_DOT_DONE = 11
        self.H_CHAR = 12
        self.H_LOOP = 13
        self.H_CMP_RES = 14
        self.H_SCRATCH = 15

        self.ptr = 0
        self.code = []

    def to_cell(self, target):
        dist = target - self.ptr
        if dist > 0: self.code.append('>' * dist)
        elif dist < 0: self.code.append('<' * (-dist))
        self.ptr = target

    def clear(self, cell):
        self.to_cell(cell)
        self.code.append('[-]')

    def add_const(self, cell, val):
        self.to_cell(cell)
        val = val & 0xFF
        if val == 0: return
        if val <= 128: self.code.append('+' * val)
        else: self.code.append('-' * (256 - val))

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
        return self.header_size + slot_idx * self.W + offset

    def emit_divmod_10_local(self, off_n, off_rem, off_quot, off_c, off_t1, off_t2, off_z):
        """Emits relative divmod_10 code where all offsets are relative to current slot base."""
        # Clear quot, t1, t2, z
        self.to_cell_rel(off_quot); self.code.append('[-]')
        self.to_cell_rel(off_t1); self.code.append('[-]')
        self.to_cell_rel(off_t2); self.code.append('[-]')
        self.to_cell_rel(off_z); self.code.append('[-]')
        self.to_cell_rel(off_c); self.code.append('[-]++++++++++')

        # Loop on N:
        self.to_cell_rel(off_n)
        self.code.append('[')
        # C -= 1
        self.to_cell_rel(off_c); self.code.append('-')

        # Copy C to t1, t2
        self.code.append('[-')
        self.to_cell_rel(off_t1); self.code.append('+')
        self.to_cell_rel(off_t2); self.code.append('+')
        self.to_cell_rel(off_c); self.code.append(']')

        # Restore C from t2
        self.to_cell_rel(off_t2)
        self.code.append('[-')
        self.to_cell_rel(off_c); self.code.append('+')
        self.to_cell_rel(off_t2); self.code.append(']')

        # is_zero = 1
        self.to_cell_rel(off_z); self.code.append('+')
        self.to_cell_rel(off_t1)
        self.code.append('[-')
        self.to_cell_rel(off_z); self.code.append('[-]')
        self.to_cell_rel(off_t1); self.code.append('[-]')
        self.code.append(']')

        # if is_zero: quot += 1, C += 10
        self.to_cell_rel(off_z)
        self.code.append('[-')
        self.to_cell_rel(off_quot); self.code.append('+')
        self.to_cell_rel(off_c); self.code.append('++++++++++')
        self.to_cell_rel(off_z); self.code.append(']')

        # N -= 1
        self.to_cell_rel(off_n)
        self.code.append('-]')

        # rem = 10 - C
        self.to_cell_rel(off_rem); self.code.append('[-]++++++++++')
        self.to_cell_rel(off_c)
        self.code.append('[-')
        self.to_cell_rel(off_rem); self.code.append('-')
        self.to_cell_rel(off_c); self.code.append(']')

    def to_cell_rel(self, target_offset):
        # We assume self.curr_rel_offset tracks current offset within slot
        dist = target_offset - self.curr_rel_offset
        if dist > 0: self.code.append('>' * dist)
        elif dist < 0: self.code.append('<' * (-dist))
        self.curr_rel_offset = target_offset

    def init_tape(self):
        """Initializes slot flags: slots 0..num_slots-1 have FLAG=1; slot num_slots has FLAG=0.
        Uses a compact Brainfuck loop to initialize arbitrary slot counts without ROM code bloat."""
        if self.dynamic or self.num_slots <= 16:
            self.to_cell(0)
            for i in range(self.num_slots):
                c = self.slot_cell(i, self.OFF_FLAG)
                self.set_const(c, 1)
            self.clear(self.slot_cell(self.num_slots, self.OFF_FLAG))
            self.to_cell(0)
            return

        # Factor num_slots into outer * inner, with both <= 255
        outer, inner = None, None
        for i in range(min(255, self.num_slots), 0, -1):
            if self.num_slots % i == 0 and (self.num_slots // i) <= 255:
                inner = i
                outer = self.num_slots // i
                break
        if outer is None:
            raise ValueError(f"num_slots={self.num_slots} cannot be factored into two 8-bit factors <= 255")

        OFF_FLAG = self.OFF_FLAG
        OFF_INNER = self.OFF_SCRATCH1   # 7
        OFF_OUTER = self.OFF_SCRATCH2   # 8
        OFF_SPARE1 = self.OFF_DM_C      # 9
        OFF_SPARE2 = self.OFF_DM_T1     # 10
        OFF_TEMP = self.OFF_DM_T2       # 11
        OFF_IS_ZERO = self.OFF_DM_Z     # 12
        OFF_CONT = self.OFF_SPARE1      # 13

        # Setup Slot 0:
        self.set_const(self.slot_cell(0, OFF_INNER), inner)
        self.set_const(self.slot_cell(0, OFF_OUTER), outer)
        self.set_const(self.slot_cell(0, OFF_CONT), 1)

        # Loop: while CONT
        self.code.append('[')
        # Pointer is at current slot's CONT (13).

        # 1. Set current slot FLAG = 1:
        self.code.append('<' * (OFF_CONT - OFF_FLAG) + '[-]+')  # at FLAG (0)

        # 2. Clear current slot CONT = 0:
        self.code.append('>' * (OFF_CONT - OFF_FLAG) + '[-]')   # at CONT (13)

        # 3. INNER -= 1:
        self.code.append('<' * (OFF_CONT - OFF_INNER) + '-')    # at INNER (7)

        # 4. Test if INNER == 0:
        # Set IS_ZERO (12) = 1, TEMP (11) = 0
        self.code.append('>' * (OFF_IS_ZERO - OFF_INNER) + '[-]+')  # at 12
        self.code.append('<' * (OFF_IS_ZERO - OFF_TEMP) + '[-]')    # at 11
        # INNER [ TEMP+, IS_ZERO[-], INNER- ]
        self.code.append('<' * (OFF_TEMP - OFF_INNER) + '[')        # at 7
        self.code.append('>' * (OFF_TEMP - OFF_INNER) + '+')        # at 11
        self.code.append('>' * (OFF_IS_ZERO - OFF_TEMP) + '[-]')    # at 12
        self.code.append('<' * (OFF_IS_ZERO - OFF_INNER) + '-')      # at 7
        self.code.append(']')
        # Restore INNER: TEMP [ INNER+, TEMP- ]
        self.code.append('>' * (OFF_TEMP - OFF_INNER) + '[')        # at 11
        self.code.append('<' * (OFF_TEMP - OFF_INNER) + '+')        # at 7
        self.code.append('>' * (OFF_TEMP - OFF_INNER) + '-')        # at 11
        self.code.append(']')

        # If IS_ZERO (12) is 1:
        self.code.append('>' * (OFF_IS_ZERO - OFF_TEMP) + '[')      # at 12
        # Reset INNER = inner
        self.code.append('<' * (OFF_IS_ZERO - OFF_INNER) + '+' * inner)  # at 7
        # OUTER -= 1
        self.code.append('>' * (OFF_OUTER - OFF_INNER) + '-')       # at 8
        # Clear IS_ZERO (12)
        self.code.append('>' * (OFF_IS_ZERO - OFF_OUTER) + '[-]')   # at 12
        self.code.append(']')  # end if INNER == 0

        # 5. Check if OUTER > 0:
        # Clear SPARE1 (9) = 0, SPARE2 (10) = 0
        self.code.append('<' * (OFF_IS_ZERO - OFF_SPARE1) + '[-]')  # at 9
        self.code.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '[-]')   # at 10
        # OUTER [ SPARE1+, SPARE2+, OUTER- ]
        self.code.append('<' * (OFF_SPARE2 - OFF_OUTER) + '[')      # at 8
        self.code.append('>' * (OFF_SPARE1 - OFF_OUTER) + '+')      # at 9
        self.code.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '+')     # at 10
        self.code.append('<' * (OFF_SPARE2 - OFF_OUTER) + '-')      # at 8
        self.code.append(']')
        # Restore OUTER: SPARE2 [ OUTER+, SPARE2- ]
        self.code.append('>' * (OFF_SPARE2 - OFF_OUTER) + '[')      # at 10
        self.code.append('<' * (OFF_SPARE2 - OFF_OUTER) + '+')      # at 8
        self.code.append('>' * (OFF_SPARE2 - OFF_OUTER) + '-')      # at 10
        self.code.append(']')

        # Normalize SPARE1 to 1:
        self.code.append('<' * (OFF_SPARE2 - OFF_SPARE1) + '[')     # at 9
        self.code.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '+')     # at 10
        self.code.append('<' * (OFF_SPARE2 - OFF_SPARE1) + '[-]')   # at 9
        self.code.append(']')
        self.code.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '[')     # at 10
        self.code.append('<' * (OFF_SPARE2 - OFF_SPARE1) + '+')     # at 9
        self.code.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '-')     # at 10
        self.code.append(']')

        # If SPARE1 (9) is 1: more slots remaining!
        self.code.append('<' * (OFF_SPARE2 - OFF_SPARE1) + '[')     # at 9
        # next_slot_CONT = 1
        self.code.append('>' * (OFF_CONT - OFF_SPARE1 + self.W) + '+')  # at next slot CONT
        self.code.append('<' * (OFF_CONT - OFF_SPARE1 + self.W))        # back to SPARE1 (9)
        # Move INNER (7) to next slot INNER (7 + self.W):
        self.code.append('<' * (OFF_SPARE1 - OFF_INNER))            # at 7
        self.code.append('[-' + '>' * self.W + '+' + '<' * self.W + ']')
        # Move OUTER (8) to next slot OUTER (8 + self.W):
        self.code.append('>' * (OFF_OUTER - OFF_INNER))            # at 8
        self.code.append('[-' + '>' * self.W + '+' + '<' * self.W + ']')
        # Clear SPARE1:
        self.code.append('>' * (OFF_SPARE1 - OFF_OUTER) + '-')      # at 9
        self.code.append(']')  # end if SPARE1

        # Clear any leftover INNER and OUTER:
        self.code.append('<' * (OFF_SPARE1 - OFF_INNER) + '[-]')    # clear 7
        self.code.append('>' * (OFF_OUTER - OFF_INNER) + '[-]')    # clear 8

        # ALWAYS advance pointer to NEXT slot's CONT:
        self.code.append('>' * (OFF_CONT - OFF_OUTER + self.W))

        # End of while CONT loop:
        self.code.append(']')

        # Pointer is at slot N's CONT (13). Move to slot N's FLAG (0):
        self.code.append('<' * OFF_CONT)

        # Return pass to cell 0:
        self.code.append('<' * self.W)
        self.code.append('[' + '<' * self.W + ']')
        self.ptr = 0


    def zero_reg(self, offset):
        self.to_cell(self.slot_cell(0, self.OFF_FLAG))
        self.code.append('[') # while OFF_FLAG
        self.curr_rel_offset = self.OFF_FLAG
        self.to_cell_rel(offset); self.code.append('[-]')
        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('>' * self.W) # move to next slot
        self.code.append(']')
        # Return pass to cell 0:
        self.code.append('<' * self.W + '[ ' + '<' * self.W + ']')
        self.ptr = 0

    def copy_reg(self, src_off, dst_off):
        self.to_cell(self.slot_cell(0, self.OFF_FLAG))
        self.code.append('[')
        self.curr_rel_offset = self.OFF_FLAG
        self.to_cell_rel(dst_off); self.code.append('[-]')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('[-]')
        self.to_cell_rel(src_off)
        self.code.append('[-')
        self.to_cell_rel(dst_off); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(src_off); self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append('[-')
        self.to_cell_rel(src_off); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append(']')
        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('>' * self.W)
        self.code.append(']')
        self.code.append('<' * self.W + '[ ' + '<' * self.W + ']')
        self.ptr = 0

    def add_reg(self, src_off, dst_off):
        # Clear carry at Slot 0
        self.clear(self.slot_cell(0, self.OFF_CARRY))
        self.to_cell(self.slot_cell(0, self.OFF_FLAG))
        self.code.append('[')
        self.curr_rel_offset = self.OFF_FLAG

        # dst += src (preserve src using SCRATCH1)
        self.to_cell_rel(src_off)
        self.code.append('[-')
        self.to_cell_rel(dst_off); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(src_off); self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append('[-')
        self.to_cell_rel(src_off); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append(']')

        # dst += cin
        self.to_cell_rel(self.OFF_CARRY)
        self.code.append('[-')
        self.to_cell_rel(dst_off); self.code.append('+')
        self.to_cell_rel(self.OFF_CARRY); self.code.append(']')

        # cout is at next slot's CARRY: rel_offset = self.W + self.OFF_CARRY!
        next_cout_rel = self.W + self.OFF_CARRY
        self.emit_divmod_10_local(dst_off, dst_off, next_cout_rel,
                                  self.OFF_DM_C, self.OFF_DM_T1, self.OFF_DM_T2, self.OFF_DM_Z)

        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('>' * self.W)
        self.code.append(']')

        if self.dynamic:
            # Dynamic expansion check:
            self.curr_rel_offset = self.OFF_FLAG
            self.to_cell_rel(self.OFF_CARRY)

            # CARRY [- dst+, FLAG+, CARRY]
            self.code.append('[-')
            self.to_cell_rel(dst_off); self.code.append('+')
            self.to_cell_rel(self.OFF_FLAG); self.code.append('+')
            self.to_cell_rel(self.OFF_CARRY); self.code.append(']')

            # Move to OFF_FLAG:
            self.to_cell_rel(self.OFF_FLAG)

            # Conditional advance if expanded: [ >>>>>>>>>>>>>>>> [-] ]
            self.code.append('[' + '>' * self.W + '[-]' + ']')

        self.code.append('<' * self.W + '[ ' + '<' * self.W + ']')
        self.ptr = 0

    def sub_reg(self, src_off, dst_off):
        self.clear(self.slot_cell(0, self.OFF_CARRY))
        self.to_cell(self.slot_cell(0, self.OFF_FLAG))
        self.code.append('[')
        self.curr_rel_offset = self.OFF_FLAG

        # dst += 10
        self.to_cell_rel(dst_off); self.code.append('++++++++++')

        # dst -= src (preserve src using SCRATCH1)
        self.to_cell_rel(src_off)
        self.code.append('[-')
        self.to_cell_rel(dst_off); self.code.append('-')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(src_off); self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append('[-')
        self.to_cell_rel(src_off); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append(']')

        # dst -= bin
        self.to_cell_rel(self.OFF_CARRY)
        self.code.append('[-')
        self.to_cell_rel(dst_off); self.code.append('-')
        self.to_cell_rel(self.OFF_CARRY); self.code.append(']')

        # divmod_10: quot into SCRATCH2
        self.emit_divmod_10_local(dst_off, dst_off, self.OFF_SCRATCH2,
                                  self.OFF_DM_C, self.OFF_DM_T1, self.OFF_DM_T2, self.OFF_DM_Z)

        # next_bout = 1 - quot
        next_bout_rel = self.W + self.OFF_CARRY
        self.to_cell_rel(next_bout_rel); self.code.append('[-] +')
        self.to_cell_rel(self.OFF_SCRATCH2)
        self.code.append('[-')
        self.to_cell_rel(next_bout_rel); self.code.append('-')
        self.to_cell_rel(self.OFF_SCRATCH2); self.code.append(']')

        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('>' * self.W)
        self.code.append(']')
        self.code.append('<' * self.W + '[ ' + '<' * self.W + ']')
        self.ptr = 0

    def mul_scalar_const(self, reg_off, factor):
        if factor == 1: return
        if factor == 0:
            self.zero_reg(reg_off)
            return
        self.clear(self.slot_cell(0, self.OFF_CARRY))
        self.to_cell(self.slot_cell(0, self.OFF_FLAG))
        self.code.append('[')
        self.curr_rel_offset = self.OFF_FLAG

        # SCRATCH1 = cin
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('[-]')
        self.to_cell_rel(self.OFF_CARRY)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(self.OFF_CARRY); self.code.append(']')

        # SCRATCH1 += cell * factor
        self.to_cell_rel(reg_off)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+' * factor)
        self.to_cell_rel(reg_off); self.code.append(']')

        # divmod_10 on SCRATCH1 -> rem in cell, quot in next_cout
        next_cout_rel = self.W + self.OFF_CARRY
        self.emit_divmod_10_local(self.OFF_SCRATCH1, reg_off, next_cout_rel,
                                  self.OFF_DM_C, self.OFF_DM_T1, self.OFF_DM_T2, self.OFF_DM_Z)

        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('>' * self.W)
        self.code.append(']')

        if self.dynamic:
            # Dynamic expansion check:
            self.curr_rel_offset = self.OFF_FLAG
            self.to_cell_rel(self.OFF_CARRY)

            # CARRY [ reg_off += CARRY, FLAG = 1, CARRY = 0 ]
            self.code.append('[')
            # reg_off += CARRY:
            self.to_cell_rel(reg_off)
            self.code.append('[-')
            self.to_cell_rel(self.OFF_DM_C); self.code.append('+')
            self.to_cell_rel(reg_off); self.code.append(']')
            self.to_cell_rel(self.OFF_CARRY)
            self.code.append('[-')
            self.to_cell_rel(reg_off); self.code.append('+')
            self.to_cell_rel(self.OFF_CARRY); self.code.append(']')
            self.to_cell_rel(self.OFF_FLAG); self.code.append('[-]+')
            self.to_cell_rel(self.OFF_CARRY); self.code.append('[-]')
            self.code.append(']')

            # Move to OFF_FLAG:
            self.to_cell_rel(self.OFF_FLAG)

            # Conditional advance if expanded:
            self.code.append('[' + '>' * self.W + '[-]' + ']')

        self.code.append('<' * self.W + '[ ' + '<' * self.W + ']')
        self.ptr = 0

    def mul_reg_by_scalar_cell(self, reg_off, scalar_cell, tmp_off):
        """Computes reg = reg * scalar_cell via repeated addition into tmp_off."""
        self.copy_reg(reg_off, tmp_off)
        self.zero_reg(reg_off)
        self.copy(scalar_cell, self.H_SCRATCH, self.H_TMP2)
        self.to_cell(self.H_SCRATCH)
        self.code.append('[')
        self.add_reg(tmp_off, reg_off)
        self.to_cell(self.H_SCRATCH)
        self.code.append('-]')

    def cmp_reg(self, a_off, b_off):
        """Forward pass comparison with return pass to cell 0 (landing in H_CMP_RES)."""
        OFF_CMP = self.OFF_SPARE2  # Offset 14 corresponds to H_CMP_RES (cell 14)
        
        # 0. Clear H_CMP_RES (cell 14)
        self.clear(self.H_CMP_RES)

        # 1. Clear cin at Slot 0
        self.clear(self.slot_cell(0, OFF_CMP))

        # 2. Forward loop: Slot 0 to Slot N-1
        self.to_cell(self.slot_cell(0, self.OFF_FLAG))
        self.code.append('[')
        self.curr_rel_offset = self.OFF_FLAG

        next_cmp_rel = self.W + OFF_CMP

        # Clear next slot's OFF_CMP:
        self.to_cell_rel(next_cmp_rel); self.code.append('[-]')

        # Clear local scratch:
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('[-]')
        self.to_cell_rel(self.OFF_SCRATCH2); self.code.append('[-]')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('[-]')
        self.to_cell_rel(self.OFF_DM_T1); self.code.append('[-]')
        self.to_cell_rel(self.OFF_DM_T2); self.code.append('[-]')
        self.to_cell_rel(self.OFF_DM_Z); self.code.append('[-]')
        self.to_cell_rel(self.OFF_SPARE1); self.code.append('[-]')  # is_diff = 0

        # Copy a to SCRATCH1, b to SCRATCH2 using DM_C
        self.to_cell_rel(a_off)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('+')
        self.to_cell_rel(a_off); self.code.append(']')
        self.to_cell_rel(self.OFF_DM_C)
        self.code.append('[-')
        self.to_cell_rel(a_off); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C); self.code.append(']')

        self.to_cell_rel(b_off)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SCRATCH2); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('+')
        self.to_cell_rel(b_off); self.code.append(']')
        self.to_cell_rel(self.OFF_DM_C)
        self.code.append('[-')
        self.to_cell_rel(b_off); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C); self.code.append(']')

        # ais523: A > B -> DM_Z
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH2)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('[-]')
        self.to_cell_rel(self.OFF_DM_T1); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH2)
        self.code.append(']')
        self.to_cell_rel(self.OFF_DM_C)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_DM_Z); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C)
        self.code.append(']')
        self.to_cell_rel(self.OFF_DM_T1)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SCRATCH2); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_T1)
        self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH2); self.code.append('-')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH2); self.code.append('[-]')

        # If DM_Z == 1: A > B -> next_cmp = 1, is_diff = 1
        self.to_cell_rel(self.OFF_DM_Z)
        self.code.append('[-')
        self.to_cell_rel(next_cmp_rel); self.code.append('[-] +')
        self.to_cell_rel(self.OFF_SPARE1); self.code.append('[-] +')
        self.to_cell_rel(self.OFF_DM_Z); self.code.append(']')

        # If is_diff == 0: check if B > A
        # Compute T2 = (OFF_SPARE1 == 0), preserving OFF_SPARE1 via OFF_SCRATCH1:
        self.to_cell_rel(self.OFF_DM_T2); self.code.append('[-] +')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('[-]')
        self.to_cell_rel(self.OFF_SPARE1)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_DM_T2); self.code.append('[-]')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(self.OFF_SPARE1)
        self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SPARE1); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append(']')

        self.to_cell_rel(self.OFF_DM_T2)
        self.code.append('[')

        # Copy a to SCRATCH1, b to SCRATCH2 again:
        self.to_cell_rel(a_off)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('+')
        self.to_cell_rel(a_off); self.code.append(']')
        self.to_cell_rel(self.OFF_DM_C)
        self.code.append('[-')
        self.to_cell_rel(a_off); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C); self.code.append(']')

        self.to_cell_rel(b_off)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SCRATCH2); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('+')
        self.to_cell_rel(b_off); self.code.append(']')
        self.to_cell_rel(self.OFF_DM_C)
        self.code.append('[-')
        self.to_cell_rel(b_off); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C); self.code.append(']')

        # ais523: B > A -> DM_Z
        self.to_cell_rel(self.OFF_SCRATCH2)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_DM_C); self.code.append('[-]')
        self.to_cell_rel(self.OFF_DM_T1); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append(']')
        self.to_cell_rel(self.OFF_DM_C)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_DM_Z); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_C)
        self.code.append(']')
        self.to_cell_rel(self.OFF_DM_T1)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(self.OFF_DM_T1)
        self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('-')
        self.to_cell_rel(self.OFF_SCRATCH2)
        self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('[-]')

        # If DM_Z == 1: B > A -> next_cmp = 2, is_diff = 1
        self.to_cell_rel(self.OFF_DM_Z)
        self.code.append('[-')
        self.to_cell_rel(next_cmp_rel); self.code.append('[-] ++')
        self.to_cell_rel(self.OFF_SPARE1); self.code.append('[-] +')
        self.to_cell_rel(self.OFF_DM_Z); self.code.append(']')

        self.to_cell_rel(self.OFF_DM_T2); self.code.append('[-]')
        self.to_cell_rel(self.OFF_DM_T2)
        self.code.append(']')

        # If is_diff == 0: digits are equal, inherit cin:
        self.to_cell_rel(self.OFF_DM_T2); self.code.append('[-] +')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('[-]')
        self.to_cell_rel(self.OFF_SPARE1)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_DM_T2); self.code.append('[-]')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(self.OFF_SPARE1)
        self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append('[-')
        self.to_cell_rel(self.OFF_SPARE1); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append(']')

        self.to_cell_rel(self.OFF_DM_T2)
        self.code.append('[-')
        # Copy cin (OFF_CMP) to next_cmp_rel using SCRATCH1:
        self.to_cell_rel(OFF_CMP)
        self.code.append('[-')
        self.to_cell_rel(next_cmp_rel); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append('+')
        self.to_cell_rel(OFF_CMP); self.code.append(']')
        self.to_cell_rel(self.OFF_SCRATCH1)
        self.code.append('[-')
        self.to_cell_rel(OFF_CMP); self.code.append('+')
        self.to_cell_rel(self.OFF_SCRATCH1); self.code.append(']')
        self.to_cell_rel(self.OFF_DM_T2)
        self.code.append(']')

        # Clear this slot's OFF_CMP:
        self.to_cell_rel(OFF_CMP); self.code.append('[-]')

        # Advance to next slot:
        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('>' * self.W)
        self.code.append(']')

        # 3. Return pass:
        # Move Slot[N].OFF_CMP into Slot[N-1].OFF_CMP:
        self.to_cell_rel(OFF_CMP)
        self.code.append('[-' + '<' * self.W + '+' + '>' * self.W + ']')
        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('<' * self.W)  # at Slot[N-1].OFF_FLAG

        # Return loop:
        self.code.append('[')
        self.curr_rel_offset = self.OFF_FLAG
        self.to_cell_rel(OFF_CMP)
        self.code.append('[-' + '<' * self.W + '+' + '>' * self.W + ']')
        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('<' * self.W)
        self.code.append(']')
        self.ptr = 0

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
        # 1. Initialize tape slot flags
        self.init_tape()

        # 2. Initialize state
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

        # 3. Main Infinite Streaming Loop:
        self.set_const(self.H_TMP0, 1)  # outer endless loop flag
        self.to_cell(self.H_TMP0)
        self.code.append('[')

        # Condition: 4q + r < (n + 1)t
        self.copy_reg(self.OFF_Q, self.OFF_TMP1)
        self.mul_scalar_const(self.OFF_TMP1, 4)

        self.copy_reg(self.OFF_T, self.OFF_TMP2)
        self.copy(self.H_N, self.H_TMP1, self.H_TMP2)
        self.add_const(self.H_TMP1, 1)
        self.mul_reg_by_scalar_cell(self.OFF_TMP2, self.H_TMP1, self.OFF_TMP3)

        self.copy(self.H_RSIGN, self.H_TMP1, self.H_TMP2)
        self.set_const(self.H_COND, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_COND)
        self.add_reg(self.OFF_R, self.OFF_TMP2)
        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.to_cell(self.H_COND)
        self.code.append('[-')
        self.add_reg(self.OFF_R, self.OFF_TMP1)
        self.to_cell(self.H_COND)
        self.code.append(']')

        self.cmp_reg(self.OFF_TMP1, self.OFF_TMP2)
        self.copy(self.H_CMP_RES, self.H_TMP1, self.H_TMP2)
        self.add_const(self.H_TMP1, -2)
        self.set_const(self.H_COND, 0)
        self.set_const(self.H_SCRATCH, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_SCRATCH)
        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
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
        self.copy(self.H_N, self.H_CHAR, self.H_TMP1)
        self.add_const(self.H_CHAR, 48)
        self.to_cell(self.H_CHAR)
        self.code.append('.')
        self.clear(self.H_CHAR)

        # Output '.' after first digit:
        self.copy(self.H_DOT_DONE, self.H_TMP1, self.H_TMP2)
        self.set_const(self.H_TMP3, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_TMP3)
        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.to_cell(self.H_TMP3)
        self.code.append('[-')
        self.set_const(self.H_CHAR, 46)  # '.'
        self.to_cell(self.H_CHAR)
        self.code.append('.')
        self.clear(self.H_CHAR)
        self.set_const(self.H_DOT_DONE, 1)
        self.to_cell(self.H_TMP3)
        self.code.append(']')

        # Update r: r_new = 10 * (r - n * t)
        self.copy_reg(self.OFF_T, self.OFF_TMP1)
        self.mul_reg_by_scalar_cell(self.OFF_TMP1, self.H_N, self.OFF_TMP2)

        self.copy(self.H_RSIGN, self.H_TMP1, self.H_TMP2)
        self.set_const(self.H_TMP3, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_TMP3)
        self.add_reg(self.OFF_TMP1, self.OFF_R)
        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.to_cell(self.H_TMP3)
        self.code.append('[-')
        # r_sign == 0: compare R with TMP1 (n*t)
        self.cmp_reg(self.OFF_R, self.OFF_TMP1)
        self.copy(self.H_CMP_RES, self.H_TMP1, self.H_TMP2)
        self.add_const(self.H_TMP1, -2)
        self.set_const(self.H_SCRATCH, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_SCRATCH)
        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.set_const(self.H_TMP1, 1)
        self.to_cell(self.H_SCRATCH)
        self.code.append('[-')
        self.clear(self.H_TMP1)
        # R < n*t -> R = n*t - R, r_sign = 1
        self.sub_reg(self.OFF_R, self.OFF_TMP1)
        self.copy_reg(self.OFF_TMP1, self.OFF_R)
        self.set_const(self.H_RSIGN, 1)
        self.to_cell(self.H_SCRATCH)
        self.code.append(']')

        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        # R >= n*t -> R = R - n*t, r_sign = 0
        self.sub_reg(self.OFF_TMP1, self.OFF_R)
        self.set_const(self.H_RSIGN, 0)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.to_cell(self.H_TMP3)
        self.code.append(']')

        self.mul_scalar_const(self.OFF_R, 10)

        # Update n: n_new = (30*q + r) // t
        self.copy_reg(self.OFF_Q, self.OFF_TMP1)
        self.mul_scalar_const(self.OFF_TMP1, 3)
        self.mul_scalar_const(self.OFF_TMP1, 10)

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
        self.mul_scalar_const(self.OFF_Q, 10)

        self.clear(self.H_COND)
        self.to_cell(self.H_COND)
        self.code.append(']')  # END BRANCH A

        # ----------------------------------------------------
        # BRANCH B: CONDITION IS FALSE (CONSUME TERM / STEP)
        # ----------------------------------------------------
        self.copy(self.H_CMP_RES, self.H_TMP1, self.H_TMP2)
        self.add_const(self.H_TMP1, -2)
        self.set_const(self.H_BRANCH_B, 0)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.set_const(self.H_BRANCH_B, 1)
        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.to_cell(self.H_BRANCH_B)
        self.code.append('[')

        # 1. t = t * l:
        self.mul_reg_by_scalar_cell(self.OFF_T, self.H_L, self.OFF_TMP1)

        # 2. r_new = (2q + r) * l:
        self.copy_reg(self.OFF_Q, self.OFF_TMP1)
        self.mul_scalar_const(self.OFF_TMP1, 2)

        self.copy(self.H_RSIGN, self.H_TMP1, self.H_TMP2)
        self.set_const(self.H_TMP3, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_TMP3)
        self.cmp_reg(self.OFF_TMP1, self.OFF_R)
        self.copy(self.H_CMP_RES, self.H_TMP1, self.H_TMP2)
        self.add_const(self.H_TMP1, -2)
        self.set_const(self.H_SCRATCH, 1)
        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        self.clear(self.H_SCRATCH)
        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.set_const(self.H_TMP1, 1)
        self.to_cell(self.H_SCRATCH)
        self.code.append('[-')
        self.clear(self.H_TMP1)
        # 2q < |r|: R = |r| - 2q, put into TMP1, r_sign = 1
        self.sub_reg(self.OFF_TMP1, self.OFF_R)
        self.copy_reg(self.OFF_R, self.OFF_TMP1)
        self.set_const(self.H_RSIGN, 1)
        self.to_cell(self.H_SCRATCH)
        self.code.append(']')

        self.to_cell(self.H_TMP1)
        self.code.append('[-')
        # 2q >= |r|: TMP1 = 2q - |r|, r_sign = 0
        self.sub_reg(self.OFF_R, self.OFF_TMP1)
        self.set_const(self.H_RSIGN, 0)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.clear(self.H_TMP1)
        self.to_cell(self.H_TMP1)
        self.code.append(']')

        self.to_cell(self.H_TMP3)
        self.code.append('[-')
        self.add_reg(self.OFF_R, self.OFF_TMP1)
        self.to_cell(self.H_TMP3)
        self.code.append(']')

        self.mul_reg_by_scalar_cell(self.OFF_TMP1, self.H_L, self.OFF_TMP2)
        self.copy_reg(self.OFF_TMP1, self.OFF_R)

        # 3. n_new = (3k*q + r) // t:
        # Compute Q * k, then multiply by 3 to prevent 8-bit scalar overflow:
        self.copy_reg(self.OFF_Q, self.OFF_TMP1)
        self.mul_reg_by_scalar_cell(self.OFF_TMP1, self.H_K, self.OFF_TMP2)
        self.mul_scalar_const(self.OFF_TMP1, 3)

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
        self.mul_reg_by_scalar_cell(self.OFF_Q, self.H_K, self.OFF_TMP1)

        # 5. l += 2, k += 1:
        self.add_const(self.H_L, 2)
        self.add_const(self.H_K, 1)

        self.clear(self.H_BRANCH_B)
        self.to_cell(self.H_BRANCH_B)
        self.code.append(']')  # END BRANCH B

        self.set_const(self.H_TMP0, 1)
        self.to_cell(self.H_TMP0)
        self.code.append(']')  # END INFINITE STREAMING LOOP

    def get_code(self):
        return "".join(self.code)


if __name__ == "__main__":
    num_slots = 4
    dynamic = True
    if len(sys.argv) > 1:
        if sys.argv[1] == "--fixed":
            dynamic = False
            num_slots = int(sys.argv[2]) if len(sys.argv) > 2 else 8000
        else:
            num_slots = int(sys.argv[1])
    builder = BFLoopedStreamBuilder(num_slots=num_slots, dynamic=dynamic)
    mode_str = f"dynamic tape (starting at {num_slots} slots)" if dynamic else f"fixed tape ({num_slots} slots)"
    print(f"Building looped streaming pi program with {mode_str}...")
    builder.build_pi_stream()
    code = builder.get_code()
    print(f"Generated looped streaming Brainfuck program: {len(code):,} bytes")
    out_file = os.path.join(os.path.dirname(__file__), "pi_stream_looped.b")
    with open(out_file, "w") as f:
        f.write(code)
    print(f"Saved to {out_file}")

