import sys

with open("scripts/pi/pi_stream_dynamic.b") as f:
    code = f.read()

tape = [0] * 100000
ptr = 0
pc = 0

bracket_map = {}
stack = []
for i, c in enumerate(code):
    if c == '[': stack.append(i)
    elif c == ']':
        s = stack.pop()
        bracket_map[s] = i
        bracket_map[i] = s

steps = 0
out = []
max_active_slot = 4

print(f"Starting simulation of dynamic spigot ({len(code):,} bytes)...", flush=True)

try:
    while pc < len(code) and len(out) < 8 and steps < 20000000:
        c = code[pc]
        if c == '>': ptr += 1
        elif c == '<': ptr -= 1
        elif c == '+': tape[ptr] = (tape[ptr] + 1) & 0xFF
        elif c == '-': tape[ptr] = (tape[ptr] - 1) & 0xFF
        elif c == '.':
            out.append(chr(tape[ptr]))
            active_slots = sum(1 for s in range(500) if tape[16 + s*16] != 0)
            print(f"OUTPUT: {chr(tape[ptr])} | Stream so far: {''.join(out)} | Active tape slots: {active_slots} | Steps: {steps:,}", flush=True)
        elif c == '[':
            if tape[ptr] == 0: pc = bracket_map[pc]
        elif c == ']':
            if tape[ptr] != 0: pc = bracket_map[pc]
        pc += 1
        steps += 1
except IndexError:
    print(f"IndexError! pc={pc}, ptr={ptr}, steps={steps}")
    print(f"Code context: {code[max(0, pc-30):pc+30]}")


print(f"\nFinal result: {''.join(out)}, total steps: {steps:,}", flush=True)
