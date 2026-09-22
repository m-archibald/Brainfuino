import sys
sys.path.append('scripts/pi')
from generate_pi_stream_looped import BFLoopedStreamBuilder

builder = BFLoopedStreamBuilder(num_slots=8)
builder.build_pi_stream()
code = builder.get_code()

bracket_map = {}
stack = []
for i, c in enumerate(code):
    if c == '[': stack.append(i)
    elif c == ']':
        start = stack.pop()
        bracket_map[start] = i
        bracket_map[i] = start

tape = bytearray(2000)
ptr = 0
pc = 0
steps = 0
pc_outer = 607
it = 0

while pc < len(code) and steps < 6_000_000:
    cmd = code[pc]
    steps += 1
    if pc == pc_outer:
        it += 1
    if cmd == '>': ptr += 1
    elif cmd == '<': ptr -= 1
    elif cmd == '+': tape[ptr] = (tape[ptr] + 1) & 0xFF
    elif cmd == '-': tape[ptr] = (tape[ptr] - 1) & 0xFF
    elif cmd == '.':
        q_v = sum(tape[16 + s*16 + 1] * (10**s) for s in range(8))
        r_v = sum(tape[16 + s*16 + 2] * (10**s) for s in range(8))
        if tape[8] == 1: r_v = -r_v
        t_v = sum(tape[16 + s*16 + 3] * (10**s) for s in range(8))
        print(f"OUTPUT '{chr(tape[ptr])}' at Step {it}: q={q_v}, r={r_v}, t={t_v}, k={tape[5]}, l={tape[6]}, n={tape[7]}")
    elif cmd == '[':
        if tape[ptr] == 0: pc = bracket_map[pc]
    elif cmd == ']':
        if tape[ptr] != 0: pc = bracket_map[pc]
    pc += 1
