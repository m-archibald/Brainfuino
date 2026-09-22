"""
test_dynamic_tape.py - Verify dynamic tape expansion on Gibbons' LFT spigot.
"""

import sys
import os

from generate_pi_stream_looped import BFLoopedStreamBuilder

class DynamicStreamBuilder(BFLoopedStreamBuilder):
    def __init__(self, initial_slots=4):
        super().__init__(num_slots=initial_slots)
        self.initial_slots = initial_slots

    def init_tape(self):
        """Initializes just the initial small number of slots (e.g. 4 slots)."""
        self.to_cell(0)
        for i in range(self.initial_slots):
            c = self.slot_cell(i, self.OFF_FLAG)
            self.set_const(c, 1)
        self.clear(self.slot_cell(self.initial_slots, self.OFF_FLAG))
        self.to_cell(0)

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

        # cout is at next slot's CARRY
        next_cout_rel = self.W + self.OFF_CARRY
        self.emit_divmod_10_local(dst_off, dst_off, next_cout_rel,
                                  self.OFF_DM_C, self.OFF_DM_T1, self.OFF_DM_T2, self.OFF_DM_Z)

        self.to_cell_rel(self.OFF_FLAG)
        self.code.append('>' * self.W)
        self.code.append(']')

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

        # Return pass to cell 0:
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

        # Return pass to cell 0:
        self.code.append('<' * self.W + '[ ' + '<' * self.W + ']')
        self.ptr = 0

if __name__ == "__main__":
    builder = DynamicStreamBuilder(initial_slots=4)
    print("Building dynamic stream...")
    builder.build_pi_stream()
    code = builder.get_code()
    print(f"Generated dynamic Brainfuck code: {len(code):,} bytes")
    out_file = "scripts/pi/pi_stream_dynamic.b"
    with open(out_file, "w") as f:
        f.write(code)
    print(f"Saved to {out_file}")
