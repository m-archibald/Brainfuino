import sys
sys.path.append('scripts/pi')
from generate_pi_stream_looped import BFLoopedStreamBuilder

builder = BFLoopedStreamBuilder(num_slots=16)
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

tape = bytearray(4000)
ptr = 0
pc = 0
steps = 0
pc_outer = 607
it = 0

outputs = []
while pc < len(code) and steps < 15_000_000:
    cmd = code[pc]
    steps += 1
    if pc == pc_outer:
        it += 1
    if cmd == '>': ptr += 1
    elif cmd == '<': ptr -= 1
    elif cmd == '+': tape[ptr] = (tape[ptr] + 1) & 0xFF
    elif cmd == '-': tape[ptr] = (tape[ptr] - 1) & 0xFF
    elif cmd == '.':
        ch = chr(tape[ptr])
        outputs.append(ch)
        print(f"OUTPUT '{ch}' at step {steps:,} (total: {''.join(outputs)})")
        if len(outputs) >= 6:
            break
    elif cmd == '[':
        if tape[ptr] == 0: pc = bracket_map[pc]
    elif cmd == ']':
        if tape[ptr] != 0: pc = bracket_map[pc]
    pc += 1

print("FINAL OUTPUT:", "".join(outputs))
