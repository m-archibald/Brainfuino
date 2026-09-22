"""
Verify ais523 logic for all digits 0..9.
"""
def test_ais523_sim(a_val, b_val):
    tape = [0] * 10
    # tape[0]: t_a, tape[1]: t_b, tape[2]: s_tmp0, tape[3]: s_tmp1, tape[4]: s_tmp3
    tape[0] = a_val
    tape[1] = b_val
    # run BF:
    # to_cell(t_a) [ -
    #   s_tmp0 += 1
    #   to_cell(t_b) [ -
    #     s_tmp0 = 0
    #     s_tmp1 += 1
    #   ]
    #   to_cell(s_tmp0) [ -
    #     s_tmp3 += 1
    #   ]
    #   to_cell(s_tmp1) [ -
    #     t_b += 1
    #   ]
    #   t_b -= 1
    #   to_cell(t_a)
    # ]
    # clear t_b
    while tape[0] > 0:
        tape[0] -= 1
        tape[2] += 1
        while tape[1] > 0:
            tape[1] -= 1
            tape[2] = 0
            tape[3] += 1
        while tape[2] > 0:
            tape[2] -= 1
            tape[4] += 1
        while tape[3] > 0:
            tape[3] -= 1
            tape[1] += 1
        tape[1] = (tape[1] - 1) & 0xFF
    tape[1] = 0
    return tape[4] > 0 # True if a > b

# Test all pairs 0..9:
errors = 0
for a in range(10):
    for b in range(10):
        expected = (a > b)
        got = test_ais523_sim(a, b)
        if got != expected:
            print(f"Mismatch for a={a}, b={b}: expected {expected}, got {got}")
            errors += 1

print(f"Total errors: {errors} / 100")
