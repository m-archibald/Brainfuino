"""
Trace BF execution to see where it is spending time or if a loop is infinite.
"""
from generate_pi_stream_looped import BFLoopedStreamBuilder
import time

def trace_bf():
    builder = BFLoopedStreamBuilder(num_slots=4)
    builder.build_pi_stream()
    code = builder.get_code()
    print(f"Code len: {len(code)}")

    bracket_map = {}
    stack = []
    for i, c in enumerate(code):
        if c == '[':
            stack.append(i)
        elif c == ']':
            start = stack.pop()
            bracket_map[start] = i
            bracket_map[i] = start

    tape = bytearray(1000)
    ptr = 0
    pc = 0
    steps = 0

    # Let's count loop executions:
    loop_counts = {}

    while pc < len(code) and steps < 200_000:
        cmd = code[pc]
        steps += 1

        if cmd == '>': ptr += 1
        elif cmd == '<': ptr -= 1
        elif cmd == '+': tape[ptr] = (tape[ptr] + 1) & 0xFF
        elif cmd == '-': tape[ptr] = (tape[ptr] - 1) & 0xFF
        elif cmd == '.':
            print(f"OUTPUT: {chr(tape[ptr])}")
        elif cmd == '[':
            loop_counts[pc] = loop_counts.get(pc, 0) + 1
            if tape[ptr] == 0:
                pc = bracket_map[pc]
        elif cmd == ']':
            if tape[ptr] != 0:
                pc = bracket_map[pc]

        pc += 1

    print(f"Executed {steps} steps. Current PC: {pc}, ptr: {ptr}")
    # Print top 5 hottest loops:
    top_loops = sorted(loop_counts.items(), key=lambda x: x[1], reverse=True)[:5]
    for l_pc, cnt in top_loops:
        sub = code[max(0, l_pc - 10):min(len(code), bracket_map[l_pc] + 10)]
        print(f"Loop at PC {l_pc} (end {bracket_map[l_pc]}): hit {cnt} times. Context: {sub}")

if __name__ == "__main__":
    trace_bf()
