"""
test_init_tape.py - Proper is_zero using [-] inside the loop.
"""

def build_init_tape_bf(outer_val=3, inner_val=4, W=16):
    OFF_FLAG = 0
    OFF_INNER = 7
    OFF_OUTER = 8
    OFF_SPARE1 = 9
    OFF_SPARE2 = 10
    OFF_TEMP = 11
    OFF_IS_ZERO = 12
    OFF_CONT = 13

    bf = []
    curr = [0]
    def goto(target):
        diff = target - curr[0]
        if diff > 0: bf.append('>' * diff)
        elif diff < 0: bf.append('<' * (-diff))
        curr[0] = target

    # Setup Slot 0:
    goto(16 + OFF_INNER)
    bf.append('+' * inner_val)
    goto(16 + OFF_OUTER)
    bf.append('+' * outer_val)
    goto(16 + OFF_CONT)
    bf.append('+') # CONT = 1

    # Loop: while CONT
    bf.append('[')
    # Pointer is at current slot's CONT (13).

    # 1. Set current slot FLAG = 1:
    bf.append('<' * (OFF_CONT - OFF_FLAG) + '[-]+') # at FLAG (0)

    # 2. Clear current slot CONT = 0:
    bf.append('>' * (OFF_CONT - OFF_FLAG) + '[-]')   # at CONT (13)

    # 3. INNER -= 1:
    bf.append('<' * (OFF_CONT - OFF_INNER) + '-')   # at INNER (7)

    # 4. Test if INNER == 0:
    # Set IS_ZERO (12) = 1, TEMP (11) = 0
    bf.append('>' * (OFF_IS_ZERO - OFF_INNER) + '[-]+') # at 12
    bf.append('<' * (OFF_IS_ZERO - OFF_TEMP) + '[-]')   # at 11
    # INNER [ TEMP+, IS_ZERO[-], INNER- ]
    bf.append('<' * (OFF_TEMP - OFF_INNER) + '[')       # at 7
    bf.append('>' * (OFF_TEMP - OFF_INNER) + '+')       # at 11
    bf.append('>' * (OFF_IS_ZERO - OFF_TEMP) + '[-]')   # at 12 (clear to 0)
    bf.append('<' * (OFF_IS_ZERO - OFF_INNER) + '-')     # at 7
    bf.append(']')
    # Restore INNER: TEMP [ INNER+, TEMP- ]
    bf.append('>' * (OFF_TEMP - OFF_INNER) + '[')       # at 11
    bf.append('<' * (OFF_TEMP - OFF_INNER) + '+')       # at 7
    bf.append('>' * (OFF_TEMP - OFF_INNER) + '-')       # at 11
    bf.append(']')

    # Now at TEMP (11).
    # If IS_ZERO (12) is 1:
    bf.append('>' * (OFF_IS_ZERO - OFF_TEMP) + '[')     # at 12
    # Reset INNER = inner_val
    bf.append('<' * (OFF_IS_ZERO - OFF_INNER) + '+' * inner_val) # at 7
    # OUTER -= 1
    bf.append('>' * (OFF_OUTER - OFF_INNER) + '-')      # at 8
    # Clear IS_ZERO (12)
    bf.append('>' * (OFF_IS_ZERO - OFF_OUTER) + '[-]')  # at 12
    bf.append(']') # end if INNER == 0

    # 5. Check if OUTER > 0:
    # Test if OUTER != 0.
    # Clear SPARE1 (9) = 0, SPARE2 (10) = 0
    bf.append('<' * (OFF_IS_ZERO - OFF_SPARE1) + '[-]') # at 9
    bf.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '[-]')  # at 10
    # OUTER [ SPARE1+, SPARE2+, OUTER- ]
    bf.append('<' * (OFF_SPARE2 - OFF_OUTER) + '[')     # at 8
    bf.append('>' * (OFF_SPARE1 - OFF_OUTER) + '+')     # at 9
    bf.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '+')    # at 10
    bf.append('<' * (OFF_SPARE2 - OFF_OUTER) + '-')     # at 8
    bf.append(']')
    # Restore OUTER: SPARE2 [ OUTER+, SPARE2- ]
    bf.append('>' * (OFF_SPARE2 - OFF_OUTER) + '[')     # at 10
    bf.append('<' * (OFF_SPARE2 - OFF_OUTER) + '+')     # at 8
    bf.append('>' * (OFF_SPARE2 - OFF_OUTER) + '-')     # at 10
    bf.append(']')

    # Normalize SPARE1 to 1:
    bf.append('<' * (OFF_SPARE2 - OFF_SPARE1) + '[')    # at 9
    bf.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '+')    # at 10
    bf.append('<' * (OFF_SPARE2 - OFF_SPARE1) + '[-]')  # at 9
    bf.append(']')
    bf.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '[')    # at 10
    bf.append('<' * (OFF_SPARE2 - OFF_SPARE1) + '+')    # at 9
    bf.append('>' * (OFF_SPARE2 - OFF_SPARE1) + '-')    # at 10
    bf.append(']')

    # If SPARE1 (9) is 1: more slots remaining!
    bf.append('<' * (OFF_SPARE2 - OFF_SPARE1) + '[')    # at 9
    # next_slot_CONT = 1
    bf.append('>' * (OFF_CONT - OFF_SPARE1 + 16) + '+') # at next slot CONT
    bf.append('<' * (OFF_CONT - OFF_SPARE1 + 16))       # back to SPARE1 (9)
    # Move INNER (7) to next slot INNER (7 + 16):
    bf.append('<' * (OFF_SPARE1 - OFF_INNER))           # at 7
    bf.append('[-' + '>' * 16 + '+' + '<' * 16 + ']')
    # Move OUTER (8) to next slot OUTER (8 + 16):
    bf.append('>' * (OFF_OUTER - OFF_INNER))           # at 8
    bf.append('[-' + '>' * 16 + '+' + '<' * 16 + ']')
    # Clear SPARE1:
    bf.append('>' * (OFF_SPARE1 - OFF_OUTER) + '-')     # at 9
    bf.append(']') # end if SPARE1

    # Now at SPARE1 (9).
    # Clear any leftover INNER and OUTER (if this was the last slot):
    bf.append('<' * (OFF_SPARE1 - OFF_INNER) + '[-]')   # clear 7
    bf.append('>' * (OFF_OUTER - OFF_INNER) + '[-]')   # clear 8

    # ALWAYS advance pointer to NEXT slot's CONT (13 + 16):
    bf.append('>' * (OFF_CONT - OFF_OUTER + 16))

    # End of while CONT loop:
    bf.append(']')

    # Pointer is at slot N's CONT (13).
    # Move pointer to slot N's FLAG (0):
    bf.append('<' * OFF_CONT)

    # Return pass to cell 0:
    bf.append('<' * W)
    bf.append('[' + '<' * W + ']')

    return "".join(bf)

def run_bf(code, tape_size=2000):
    tape = [0] * tape_size
    ptr = 0
    pc = 0
    n = len(code)

    bracket_map = {}
    stack = []
    for i, c in enumerate(code):
        if c == '[': stack.append(i)
        elif c == ']':
            start = stack.pop()
            bracket_map[start] = i
            bracket_map[i] = start

    steps = 0
    while pc < n:
        c = code[pc]
        if c == '>': ptr += 1
        elif c == '<': ptr -= 1
        elif c == '+': tape[ptr] = (tape[ptr] + 1) & 0xFF
        elif c == '-': tape[ptr] = (tape[ptr] - 1) & 0xFF
        elif c == '[':
            if tape[ptr] == 0: pc = bracket_map[pc]
        elif c == ']':
            if tape[ptr] != 0: pc = bracket_map[pc]
        pc += 1
        steps += 1
    return tape, ptr, steps

if __name__ == "__main__":
    for total_slots, outer, inner in [(1, 1, 1), (12, 3, 4), (64, 8, 8), (8000, 80, 100)]:
        code = build_init_tape_bf(outer_val=outer, inner_val=inner)
        print(f"\n--- Testing total_slots={total_slots} ({outer}x{inner}) ---")
        print(f"BF code length: {len(code)} bytes")
        tape_size = (total_slots + 2) * 16
        tape, final_ptr, steps = run_bf(code, tape_size=tape_size)
        print(f"Final ptr: {final_ptr} (expected 0), steps: {steps}")
        non_zero = [(i, tape[i]) for i in range(len(tape)) if tape[i] != 0]
        expected_flags = [(16 + i*16, 1) for i in range(total_slots)]
        if non_zero == expected_flags and final_ptr == 0:
            print(f"SUCCESS! Exactly {total_slots} slots initialized to 1, all others 0, final ptr = 0!")
        else:
            print(f"MISMATCH! Non-zero count: {len(non_zero)}, expected: {len(expected_flags)}")
            if len(non_zero) < 20:
                print(f"Non-zero cells: {non_zero}")
            break
