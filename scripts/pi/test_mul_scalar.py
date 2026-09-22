"""
Test mul_reg_by_scalar_cell in BF.
"""
from generate_pi_stream_looped import BFLoopedStreamBuilder
from test_bf_looped import run_bf

def test_mul_scalar():
    builder = BFLoopedStreamBuilder(num_slots=4)
    builder.init_tape()

    # Put 25 in OFF_T (5 at slot 0, 2 at slot 1)
    builder.set_const(builder.slot_cell(0, builder.OFF_T), 5)
    builder.set_const(builder.slot_cell(1, builder.OFF_T), 2)

    # Set H_L = 29
    builder.set_const(builder.H_L, 29)

    # Multiply T by L (25 * 29 = 725)
    # Using repeated addition:
    builder.copy_reg(builder.OFF_T, builder.OFF_TMP1)
    builder.zero_reg(builder.OFF_T)
    builder.copy(builder.H_L, builder.H_SCRATCH, builder.H_TMP2)
    builder.to_cell(builder.H_SCRATCH)
    builder.code.append('[')
    builder.add_reg(builder.OFF_TMP1, builder.OFF_T)
    builder.to_cell(builder.H_SCRATCH)
    builder.code.append('-]')

    # Output slot 2, slot 1, slot 0 as digits (expected: 7, 2, 5)
    for s in [2, 1, 0]:
        builder.to_cell(builder.slot_cell(s, builder.OFF_T))
        builder.code.append('++++++++++++++++++++++++++++++++++++++++++++++++.')

    code = builder.get_code()
    print("Testing 25 * 29 (expected 725)...")
    out = run_bf(code, max_steps=1_000_000, max_outputs=3)
    print("Result:", out)
    assert out == "725", f"Expected '725', got '{out}'"
    print("PASSED!")

if __name__ == "__main__":
    test_mul_scalar()
