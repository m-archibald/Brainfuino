#!/usr/bin/env python3
"""
High-Speed Brainfuck Emulator & Performance Telemetry Harness for Brainfuino.

Executes Brainfuck programs using an optimized bytecode compiler and simulates
the Lattice MachXO2 soft-processor execution timing, providing cycle counts,
song percentage tracking, real-time speed simulation, and hardware clock recommendations.
"""

import sys
import time
import argparse
import os

# Opcode constants
OP_ADD = 1      # +
OP_SUB = 2      # -
OP_RIGHT = 3    # >
OP_LEFT = 4     # <
OP_CLEAR = 5    # [-] or [+]
OP_OUT = 6      # .
OP_IN = 7       # ,
OP_JZ = 8       # [ (Jump to target if *ptr == 0)
OP_JNZ = 9      # ] (Jump to target if *ptr != 0)

# Brainfuino 25 discrete hardware clock frequencies
CLOCK_FREQUENCIES = [
    (10, "10 Hz"), (25, "25 Hz"), (50, "50 Hz"), (100, "100 Hz"), (250, "250 Hz"),
    (500, "500 Hz"), (1000, "1 kHz"), (2000, "2 kHz"), (5000, "5 kHz"), (10000, "10 kHz"),
    (25000, "25 kHz"), (50000, "50 kHz"), (62500, "62.5 kHz"), (125000, "125 kHz"), (250000, "250 kHz"),
    (500000, "500 kHz"), (750000, "750 kHz"), (1000000, "1 MHz"), (1500000, "1.5 MHz"), (2000000, "2 MHz"),
    (3000000, "3 MHz"), (4000000, "4 MHz"), (6000000, "6 MHz"), (8000000, "8 MHz"), (12000000, "12 MHz")
]

TOTAL_SONG_DURATION = 219.15  # Bad Apple total seconds

def compile_bf(source_code):
    """
    Compiles raw Brainfuck code into an optimized bytecode instruction list.
    Performs instruction fusing, clear-loop folding, and jump target precomputation.
    """
    clean = [c for c in source_code if c in "+-><.,[]"]
    n = len(clean)
    i = 0
    bytecode = []
    # Each entry: [opcode, arg, target]

    while i < n:
        c = clean[i]
        
        # 1. Clear loop folding: [-] or [+]
        if c == '[' and i + 2 < n and clean[i+1] in ('-', '+') and clean[i+2] == ']':
            bytecode.append([OP_CLEAR, 0, 0])
            i += 3
            continue

        # 2. Arithmetic fusing: + and -
        if c in ('+', '-'):
            delta = 0
            while i < n and clean[i] in ('+', '-'):
                delta += 1 if clean[i] == '+' else -1
                i += 1
            delta = delta % 256
            if delta != 0:
                bytecode.append([OP_ADD, delta, 0])
            continue

        # 3. Pointer move fusing: > and <
        if c in ('>', '<'):
            shift = 0
            while i < n and clean[i] in ('>', '<'):
                shift += 1 if clean[i] == '>' else -1
                i += 1
            if shift > 0:
                bytecode.append([OP_RIGHT, shift, 0])
            elif shift < 0:
                bytecode.append([OP_LEFT, -shift, 0])
            continue

        # 4. Input / Output
        if c == '.':
            count = 0
            while i < n and clean[i] == '.':
                count += 1
                i += 1
            bytecode.append([OP_OUT, count, 0])
            continue

        if c == ',':
            bytecode.append([OP_IN, 1, 0])
            i += 1
            continue

        # 5. Jump brackets
        if c == '[':
            bytecode.append([OP_JZ, 0, 0])
            i += 1
            continue

        if c == ']':
            bytecode.append([OP_JNZ, 0, 0])
            i += 1
            continue

    # Precompute jump targets
    stack = []
    for idx, inst in enumerate(bytecode):
        if inst[0] == OP_JZ:
            stack.append(idx)
        elif inst[0] == OP_JNZ:
            if not stack:
                raise ValueError(f"Unmatched closing bracket at instruction {idx}")
            start_idx = stack.pop()
            bytecode[start_idx][2] = idx
            inst[2] = start_idx

    if stack:
        raise ValueError(f"Unmatched open bracket at instruction {stack[-1]}")

    return bytecode

def run_emulator(bf_file, speed=1.0, fps=12, quiet=False, max_frames=None):
    if not os.path.exists(bf_file):
        print(f"Error: {bf_file} does not exist.")
        return

    with open(bf_file, "r", encoding="ascii", errors="ignore") as f:
        raw_code = f.read()

    file_size = len(raw_code)
    compile_start = time.time()
    bytecode = compile_bf(raw_code)
    compile_time = time.time() - compile_start

    # Memory state (128 kB RAM matching Brainfuino IS62WV1288BLL)
    MEM_SIZE = 131072
    memory = [0] * MEM_SIZE
    ptr = 0
    pc = 0
    total_inst = len(bytecode)

    # Telemetry
    total_ops_executed = 0
    total_cycles_modeled = 0
    frames_rendered = 0
    last_frame_time = time.time()
    frame_interval = (1.0 / fps) if fps > 0 else 0.0833
    output_buffer = []

    # Cycle cost model based on brainfuck_uP.v FSM
    CYCLE_COSTS = {
        OP_ADD: 3,
        OP_SUB: 3,
        OP_RIGHT: 2,
        OP_LEFT: 2,
        OP_CLEAR: 5,
        OP_OUT: 6,
        OP_IN: 10,
        OP_JZ: 4,
        OP_JNZ: 4
    }

    start_time = time.time()

    # Main execution loop
    try:
        while pc < total_inst:
            inst = bytecode[pc]
            op = inst[0]
            arg = inst[1]
            target = inst[2]

            total_ops_executed += 1
            total_cycles_modeled += CYCLE_COSTS[op]

            if op == OP_ADD:
                memory[ptr] = (memory[ptr] + arg) & 0xFF
                pc += 1
            elif op == OP_RIGHT:
                ptr = (ptr + arg) % MEM_SIZE
                pc += 1
            elif op == OP_LEFT:
                ptr = (ptr - arg) % MEM_SIZE
                pc += 1
            elif op == OP_CLEAR:
                memory[ptr] = 0
                pc += 1
            elif op == OP_OUT:
                ch = chr(memory[ptr])
                for _ in range(arg):
                    output_buffer.append(ch)
                    # Check for frame boundary: ESC [ H (cursor home) or ESC [ 2 J
                    if len(output_buffer) >= 3 and output_buffer[-3:] == ['\x1b', '[', 'H']:
                        frames_rendered += 1
                        if not quiet:
                            # Render frame to terminal
                            frame_text = "".join(output_buffer[:-3])
                            sys.stdout.write("\x1b[H" + frame_text)
                            sys.stdout.flush()

                        output_buffer = []

                        # Playback pacing if speed > 0
                        if speed > 0:
                            target_delay = (frame_interval / speed)
                            elapsed = time.time() - last_frame_time
                            if elapsed < target_delay:
                                time.sleep(target_delay - elapsed)
                            last_frame_time = time.time()

                        if max_frames and frames_rendered >= max_frames:
                            pc = total_inst
                            break

                pc += 1
            elif op == OP_JZ:
                if memory[ptr] == 0:
                    pc = target + 1
                else:
                    pc += 1
            elif op == OP_JNZ:
                if target == pc - 1 and memory[ptr] != 0:
                    # Intentional Brainfuck halt loop: []
                    break
                if memory[ptr] != 0:
                    pc = target + 1
                else:
                    pc += 1
            elif op == OP_IN:
                memory[ptr] = 0
                pc += 1
    except KeyboardInterrupt:
        if not quiet:
            sys.stdout.write("\n\n[Playback stopped by user]\n")
            sys.stdout.flush()

    # Final flush
    if output_buffer and not quiet:
        sys.stdout.write("".join(output_buffer))
        sys.stdout.flush()

    total_real_time = time.time() - start_time
    derived_song_duration = frames_rendered / fps if fps > 0 else 0
    percent_song_achieved = (derived_song_duration / TOTAL_SONG_DURATION) * 100.0
    budget_used_pct = (file_size / 262144.0) * 100.0

    avg_cycles_per_frame = (total_cycles_modeled / frames_rendered) if frames_rendered > 0 else 0
    ideal_freq = avg_cycles_per_frame * fps

    # Find best matching hardware clock frequency
    best_freq_idx = 0
    best_freq_diff = float('inf')
    for idx, (freq_val, name) in enumerate(CLOCK_FREQUENCIES):
        diff = abs(freq_val - ideal_freq)
        if diff < best_freq_diff:
            best_freq_diff = diff
            best_freq_idx = idx

    rec_freq_val, rec_freq_name = CLOCK_FREQUENCIES[best_freq_idx]
    real_speed_ratio = (rec_freq_val / ideal_freq * 100.0) if ideal_freq > 0 else 100.0

    # Print comprehensive performance report
    print("\n" + "=" * 70)
    print(">>> BAD APPLE EMULATOR: COMPREHENSIVE TELEMETRY REPORT <<<")
    print("=" * 70)
    print(f"Source Binary File       : {bf_file}")
    print(f"Binary File Size         : {file_size:,} bytes")
    print(f"Flash ROM Budget Used    : {file_size:,} / 262,144 bytes ({budget_used_pct:.2f}%)")
    print(f"Bytecode Compiler Time   : {compile_time*1000:.1f} ms ({total_inst:,} IR instructions)")
    print(f"Total Opcode Dispatches  : {total_ops_executed:,} ops")
    print(f"Modelled FPGA Cycles     : {total_cycles_modeled:,} cycles")
    print("-" * 70)
    print(f"Frames Rendered          : {frames_rendered:,} frames")
    print(f"Framerate Target         : {fps:.1f} fps")
    print(f"Derived Song Duration    : {derived_song_duration:.2f} seconds (out of {TOTAL_SONG_DURATION:.2f}s)")
    print(f"SONG COVERAGE ACHIEVED   : {percent_song_achieved:.2f}% of Bad Apple!!")
    print("-" * 70)
    print(">>> BRAINFUINO HARDWARE CLOCK CALIBRATION <<<")
    print(f"Average Cycles / Frame   : {int(avg_cycles_per_frame):,} cycles/frame")
    print(f"Exact Target Frequency   : {ideal_freq/1000000.0:.3f} MHz ({int(ideal_freq):,} Hz)")
    print(f"Recommended FPGA Clock   : {rec_freq_name} (Menu Index {best_freq_idx}, !SET:10={best_freq_idx})")
    print(f"Hardware Playback Ratio  : {real_speed_ratio:.1f}% of Real-Time Speed")
    if real_speed_ratio < 90.0:
        print(f"Notice: Will play {100.0 - real_speed_ratio:.1f}% slower on {rec_freq_name}. Consider next frequency up.")
    elif real_speed_ratio > 110.0:
        print(f"Notice: Will play {real_speed_ratio - 100.0:.1f}% faster on {rec_freq_name}. (Beat tempo increased).")
    else:
        print("PERFECT MATCH: Soft-processor will play at near 100% natural song tempo!")
    print("=" * 70 + "\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="High-Speed Brainfuino Brainfuck Video Emulator")
    parser.add_argument("file", nargs="?", default="scripts/bad_apple/bad_apple_76k.b", help="Path to Brainfuck .b file")
    parser.add_argument("--speed", type=float, default=0.0, help="Playback speed multiplier (0 = unthrottled benchmark, 1.0 = real-time, 2.0 = 2x, etc.)")
    parser.add_argument("--fps", type=int, default=12, help="Target framerate of the encoded file")
    parser.add_argument("--quiet", action="store_true", help="Suppress visual rendering for pure benchmark speed")
    parser.add_argument("--max-frames", type=int, default=None, help="Stop after N frames")

    args = parser.parse_args()
    run_emulator(args.file, speed=args.speed, fps=args.fps, quiet=args.quiet, max_frames=args.max_frames)
