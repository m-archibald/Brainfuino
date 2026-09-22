"""
test_bf_looped.py - Fast Brainfuck interpreter to test looped streaming pi generator.
"""
import sys
import time

def run_bf(code, max_steps=50_000_000, max_outputs=6):
    # Precompute bracket jumps
    bracket_map = {}
    stack = []
    for i, c in enumerate(code):
        if c == '[':
            stack.append(i)
        elif c == ']':
            start = stack.pop()
            bracket_map[start] = i
            bracket_map[i] = start

    # Tape: 128 kB
    tape = bytearray(131072)
    ptr = 0
    pc = 0
    code_len = len(code)
    outputs = []
    steps = 0
    t0 = time.time()

    while pc < code_len and steps < max_steps:
        cmd = code[pc]
        steps += 1

        if cmd == '>':
            ptr = (ptr + 1) & 0x1FFFF
        elif cmd == '<':
            ptr = (ptr - 1) & 0x1FFFF
        elif cmd == '+':
            tape[ptr] = (tape[ptr] + 1) & 0xFF
        elif cmd == '-':
            tape[ptr] = (tape[ptr] - 1) & 0xFF
        elif cmd == '.':
            ch = chr(tape[ptr])
            outputs.append(ch)
            print(ch, end="", flush=True)
            if len(outputs) >= max_outputs:
                break
        elif cmd == '[':
            if tape[ptr] == 0:
                pc = bracket_map[pc]
        elif cmd == ']':
            if tape[ptr] != 0:
                pc = bracket_map[pc]

        pc += 1

    t1 = time.time()
    out_str = "".join(outputs)
    print(f"\nDone in {steps:,} steps ({t1 - t0:.2f}s). Output: '{out_str}'")
    return out_str

if __name__ == "__main__":
    from generate_pi_stream_looped import BFLoopedStreamBuilder
    builder = BFLoopedStreamBuilder(num_slots=24)
    builder.build_pi_stream()
    code = builder.get_code()
    print(f"Testing generated BF code ({len(code):,} bytes)...")
    run_bf(code, max_steps=20_000_000, max_outputs=6)
