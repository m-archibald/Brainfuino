"""
Test looped 16-bit tape initialization in BF.
"""
def generate_looped_init(num_slots=8000, W=16):
    code = []
    # Start at cell 0.
    # Move to Slot 0: cell 16.
    code.append('>' * W)
    # Put counter in Slot 0's OFF_SCRATCH1 (high) and OFF_SCRATCH2 (low):
    # num_slots = 8000 = 31 * 256 + 64
    high = num_slots >> 8
    low = num_slots & 0xFF
    code.append('>>>>>>>' + '+' * high) # at offset 7
    code.append('>' + '+' * low)       # at offset 8
    # Active flag is in cell 9 (OFF_DM_C) = 1
    code.append('>+') # at offset 9

    # While active:
    code.append('[')
    # 1. Set OFF_FLAG (offset 0) = 1
    code.append('<<<<<<<<<+>>>>>>>>>') # offset 9 -> offset 0 -> offset 9

    # 2. Decrement counter: (high at 7, low at 8)
    # Check if low is 0:
    # Use offset 10 (DM_T1) as flag: is_low_zero = 1
    code.append('>+<') # at 10 set 1, back to 9
    code.append('<')   # at 8 (low)
    code.append('[')
    code.append('>>-<<') # clear is_low_zero at 10
    code.append('-')     # low -= 1
    code.append(']')     # if low was > 0, low decremented by 1, is_low_zero = 0.
                         # Wait: if low decremented to 0, the loop emptied it!
                         # That's not what we want! We just want to decrement low by 1!

    return "".join(code)

if __name__ == "__main__":
    pass
