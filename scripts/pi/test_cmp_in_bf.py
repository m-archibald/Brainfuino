"""
Test cmp_reg implementation in BF thoroughly across multiple cases.
"""
from generate_pi_stream_looped import BFLoopedStreamBuilder
from test_bf_looped import run_bf

def run_cmp_test(val_a, val_b, expected):
    builder = BFLoopedStreamBuilder(num_slots=4)
    builder.init_tape()
    
    # Set val_a (little endian decimal digits):
    for i in range(4):
        builder.set_const(builder.slot_cell(i, builder.OFF_TMP1), (val_a // (10**i)) % 10)
        builder.set_const(builder.slot_cell(i, builder.OFF_TMP2), (val_b // (10**i)) % 10)

    builder.cmp_reg(builder.OFF_TMP1, builder.OFF_TMP2)
    builder.copy(builder.H_CMP_RES, builder.H_CHAR, builder.H_TMP0)
    builder.add_const(builder.H_CHAR, 48)
    builder.to_cell(builder.H_CHAR)
    builder.code.append('.')

    code = builder.get_code()
    out = run_bf(code, max_steps=500_000, max_outputs=1)
    assert out == str(expected), f"Failed for {val_a} vs {val_b}: got '{out}', expected '{expected}'"
    print(f"PASSED: {val_a} vs {val_b} -> {out}")

if __name__ == "__main__":
    run_cmp_test(145, 139, 1)  # 145 > 139 -> 1
    run_cmp_test(139, 145, 2)  # 139 < 145 -> 2
    run_cmp_test(145, 145, 0)  # 145 == 145 -> 0
    run_cmp_test(500, 499, 1)  # 500 > 499 -> 1
    run_cmp_test(499, 500, 2)  # 499 < 500 -> 2
    run_cmp_test(0, 0, 0)      # 0 == 0 -> 0
    print("ALL CMP_REG TESTS PASSED!")
