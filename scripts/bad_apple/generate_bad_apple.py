#!/usr/bin/env python3
"""
Next-Generation Bad Apple!! Brainfuck Generator for Brainfuino.

Features:
- Dual Modes:
    1. 'resolution': Locks (cols, rows, fps) and packs maximum song duration into ROM.
    2. 'duration': Locks desired time (up to 219.15s full song) and auto-solves the highest visual resolution.
- Knobs:
    --mode [resolution|duration]
    --cols INT, --rows INT
    --fps INT (e.g. 8, 10, 12, 15)
    --size BYTES (e.g. 262144 for 256 kB Flash, 76000 for Library Slot)
    --target-seconds FLOAT (e.g. 219.15 for 100% full song)
    --target-percent FLOAT (e.g. 100.0)
- Temporal Delta Compression with static frame collapse.
- Cost-Weighted Token Synthesizer (RLE loops vs raw dots).
- Adaptive Per-Frame Delay Loops (constant-framerate pacing).
- Hardware Clock Calibration & Speed % Recommendation.
"""

import os
import sys
import argparse
import subprocess
import time
import numpy as np

TOTAL_SONG_DURATION = 219.15  # seconds

CLOCK_FREQUENCIES = [
    (10, "10 Hz"), (25, "25 Hz"), (50, "50 Hz"), (100, "100 Hz"), (250, "250 Hz"),
    (500, "500 Hz"), (1000, "1 kHz"), (2000, "2 kHz"), (5000, "5 kHz"), (10000, "10 kHz"),
    (25000, "25 kHz"), (50000, "50 kHz"), (62500, "62.5 kHz"), (125000, "125 kHz"), (250000, "250 kHz"),
    (500000, "500 kHz"), (750000, "750 kHz"), (1000000, "1 MHz"), (1500000, "1.5 MHz"), (2000000, "2 MHz"),
    (3000000, "3 MHz"), (4000000, "4 MHz"), (6000000, "6 MHz"), (8000000, "8 MHz"), (12000000, "12 MHz")
]

def extract_frames_in_memory(video_path, cols, rows, fps):
    """
    Extracts raw grayscale video frames from MP4 using FFmpeg pipe directly into NumPy.
    Runs in ~1-2 seconds with zero disk I/O.
    """
    if not os.path.exists(video_path):
        raise FileNotFoundError(f"Video file not found: {video_path}")

    cmd = [
        'ffmpeg', '-y', '-i', video_path,
        '-f', 'rawvideo', '-pix_fmt', 'gray',
        '-vf', f'fps={fps},scale={cols}:{rows}',
        '-'
    ]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    raw = p.stdout.read()
    p.wait()

    frame_size = cols * rows
    num_frames = len(raw) // frame_size
    if num_frames == 0:
        raise ValueError(f"Failed to extract frames from {video_path}")

    # Binarize with threshold 128
    frames = (np.frombuffer(raw[:num_frames * frame_size], dtype=np.uint8).reshape(num_frames, rows, cols) > 128)
    return frames

def build_register_init():
    """
    Precomputes constant registers in Data RAM at startup:
      Cell 0: ' ' (32)
      Cell 1: Counter for Cell 0
      Cell 2: '#' (35)
      Cell 3: Counter for Cell 2
      Cell 4: '\n' (10)
      Cell 5: ESC (27)
      Cell 6: '[' (91)
      Cell 7: 'H' (72)
      Cell 8: Multiplier / Math scratch
      Cell 9: Pacing loop counter 1
      Cell 10: Pacing loop counter 2
    """
    targets = {0: 32, 2: 35, 4: 10, 5: 27, 6: 91, 7: 72}
    code = ">>>>>>>>" + ("+" * 10) + "["
    for cell, val in sorted(targets.items()):
        base = val // 10
        dist = 8 - cell
        code += ("<" * dist) + ("+" * base) + (">" * dist)
    code += "-]"
    for cell, val in sorted(targets.items()):
        rem = val % 10
        dist = 8 - cell
        code += ("<" * dist) + ("+" * rem) + (">" * dist)
    code += "<<<<<<<<"
    return code

class BFVideoEncoder:
    def __init__(self, cols, rows, fps, target_bytes=262144, enable_pacing=True):
        self.cols = cols
        self.rows = rows
        self.fps = fps
        self.target_bytes = target_bytes
        self.enable_pacing = enable_pacing

        self.curr_cell = 0
        self.init_code = build_register_init()
        self.bf_chunks = [self.init_code]
        self.curr_size = len(self.init_code)

        # Telemetry & Pacing stats
        self.frame_costs = []
        self.target_frame_cycles = 1000  # Will be dynamically estimated

    def move_to(self, target):
        diff = target - self.curr_cell
        self.curr_cell = target
        return (">" if diff > 0 else "<") * abs(diff)

    def synthesize_run(self, char, count):
        """
        Cost-weighted emission of 'count' repetitions of 'char'.
        Uses adjacent counter cell (Cell 1 for Cell 0 ' ', Cell 3 for Cell 2 '#')
        for minimal 1-byte pointer shifts '>' and '<'.
        """
        if count <= 0:
            return ""

        char_cell = 0 if char == ' ' else 2
        counter_cell = char_cell + 1
        res = self.move_to(char_cell)

        # Baseline: raw dots
        best = "." * count

        # Test factored loops: count = a * b + r
        # Move to adjacent counter: '>'
        # Loop body: '<' + ('.' * b) + '>' + '-'
        # Remainder: '<' + ('.' * r)
        for b in range(2, 16):
            a = count // b
            r = count % b
            if a >= 2:
                candidate = (
                    ">" + ("+" * a) +
                    "[<" + ("." * b) + ">-]" +
                    "<" + ("." * r)
                )
                if len(candidate) < len(best):
                    best = candidate

        res += best
        return res

    def synthesize_pacing_delay(self, deficit_cycles):
        """
        Synthesizes a minimal-byte factored delay loop on scratch cells 9 and 10
        to equalize frame execution time.
        """
        if deficit_cycles <= 20:
            return ""

        # Factor deficit into a * b ~= deficit / 5
        iters = max(1, deficit_cycles // 5)
        if iters <= 50:
            return self.move_to(9) + ("+" * iters) + "[-]" + self.move_to(0)

        b = int(np.sqrt(iters))
        a = iters // b
        rem = iters % b

        loop = (
            self.move_to(9) + ("+" * a) +
            "[>" + ("+" * b) + "[-]<-]"
        )
        if rem > 0:
            loop += ("+" * rem) + "[-]"
        loop += self.move_to(0)
        return loop

    def encode_frames(self, frames, max_frames=None):
        total_available = len(frames) if max_frames is None else min(len(frames), max_frames)
        prev_frame = None
        frames_encoded = 0

        # Typical cycle cost per drawing operation
        DRAWING_CYCLE_ESTIMATE = 4

        for fidx in range(total_available):
            curr_frame = frames[fidx]
            frame_bf = []
            frame_cycles = 0

            # 1. Cursor Home: ESC [ H (cells 5, 6, 7)
            cursor_home = self.move_to(5) + "." + self.move_to(6) + "." + self.move_to(7) + "." + self.move_to(0)
            frame_bf.append(cursor_home)
            frame_cycles += 24

            # 2. Check if frame is 100% identical to previous frame
            if prev_frame is not None and np.array_equal(curr_frame, prev_frame):
                # Static frame collapse: do not repaint any characters!
                # Emit frame boundary and pacing delay
                if self.enable_pacing and self.frame_costs:
                    avg_c = np.mean(self.frame_costs) if self.frame_costs else 1000
                    pacing = self.synthesize_pacing_delay(int(avg_c))
                    frame_bf.append(pacing)
                
                frame_str = "".join(frame_bf)
                if self.curr_size + len(frame_str) + 10 > self.target_bytes:
                    break

                self.bf_chunks.append(frame_str)
                self.curr_size += len(frame_str)
                frames_encoded += 1
                self.frame_costs.append(frame_cycles)
                continue

            # 3. Render scanlines
            for y in range(self.rows):
                # Run-Length encode row
                row_runs = []
                curr_c = None
                curr_len = 0

                for x in range(self.cols):
                    c = '#' if curr_frame[y, x] else ' '
                    if c == curr_c:
                        curr_len += 1
                    else:
                        if curr_c is not None:
                            row_runs.append((curr_c, curr_len))
                        curr_c = c
                        curr_len = 1
                if curr_len > 0:
                    row_runs.append((curr_c, curr_len))

                for c, length in row_runs:
                    run_code = self.synthesize_run(c, length)
                    frame_bf.append(run_code)
                    frame_cycles += (length * DRAWING_CYCLE_ESTIMATE)

                # Row newline
                frame_bf.append(self.move_to(4) + ".")
                frame_cycles += 4

            # 4. Optional adaptive pacing delay
            if self.enable_pacing and self.frame_costs:
                target_c = max(1000, int(np.mean(self.frame_costs)))
                deficit = target_c - frame_cycles
                if deficit > 30:
                    pacing = self.synthesize_pacing_delay(deficit)
                    frame_bf.append(pacing)

            frame_str = "".join(frame_bf)

            # Check ROM capacity budget
            if self.curr_size + len(frame_str) + 10 > self.target_bytes:
                break

            self.bf_chunks.append(frame_str)
            self.curr_size += len(frame_str)
            self.frame_costs.append(frame_cycles)
            frames_encoded += 1
            prev_frame = curr_frame

        # Append halt loop: [-]+[]
        self.bf_chunks.append("[-]+[]")
        full_code = "".join(self.bf_chunks)
        return full_code, frames_encoded

def optimize_auto_resolution(video_path, target_seconds, fps, target_bytes, aspect_ratio=2.4):
    """
    Auto-Resolution Solver (Knob 2):
    Binary-searches and solves for the highest visual resolution (W, H)
    that completely fits the requested target duration inside target_bytes.
    """
    print(f"\n[AUTO-RESOLUTION OPTIMIZER] Targeting {target_seconds:.2f}s ({target_seconds*100/TOTAL_SONG_DURATION:.1f}% song) @ {fps} fps | Budget: {target_bytes:,} bytes")
    
    candidate_resolutions = [
        (48, 20), (44, 18), (40, 16), (36, 15), (32, 14),
        (30, 12), (28, 12), (26, 11), (24, 10), (22, 9),
        (20, 8), (18, 8), (16, 7), (14, 6)
    ]

    best_res = candidate_resolutions[-1]
    best_code = ""
    best_frames = 0
    target_frames = int(target_seconds * fps)

    for cols, rows in candidate_resolutions:
        print(f"Testing candidate resolution: {cols:2d}x{rows:2d} ...", end=" ", flush=True)
        try:
            frames = extract_frames_in_memory(video_path, cols, rows, fps)
            enc = BFVideoEncoder(cols, rows, fps, target_bytes=target_bytes)
            code, encoded_count = enc.encode_frames(frames, max_frames=target_frames)
            
            coverage_pct = (encoded_count / target_frames) * 100.0
            print(f"Result: {encoded_count}/{target_frames} frames ({coverage_pct:.1f}%) | Size: {len(code):,} B")

            if encoded_count > best_frames or (encoded_count == best_frames and (cols * rows) > (best_res[0] * best_res[1])):
                best_res = (cols, rows)
                best_code = code
                best_frames = encoded_count

            if encoded_count >= target_frames:
                print(f"--> SUCCESS: Resolution {cols}x{rows} completely fits 100% of the target duration!")
                break
        except Exception as e:
            print(f"Error: {e}")

    return best_res[0], best_res[1], best_code, best_frames

def generate(args):
    video_path = args.video
    if not os.path.exists(video_path):
        print(f"Error: Video file not found at {video_path}")
        return

    cols = args.cols
    rows = args.rows
    fps = args.fps
    budget = args.size
    output_file = args.output

    start_time = time.time()

    if args.mode == "duration":
        target_sec = args.target_seconds
        if args.target_percent < 100.0:
            target_sec = TOTAL_SONG_DURATION * (args.target_percent / 100.0)

        cols, rows, code, frames_count = optimize_auto_resolution(video_path, target_sec, fps, budget)
    else:
        print(f"\n[TARGET RESOLUTION MODE] Grid: {cols}x{rows} | FPS: {fps} | Budget: {budget:,} bytes")
        print("Extracting frames via FFmpeg pipe directly into memory...")
        frames = extract_frames_in_memory(video_path, cols, rows, fps)
        print(f"Extracted {len(frames)} frames. Encoding to Brainfuck...")
        enc = BFVideoEncoder(cols, rows, fps, target_bytes=budget)
        code, frames_count = enc.encode_frames(frames)

    with open(output_file, "w", encoding="ascii") as f:
        f.write(code)

    elapsed = time.time() - start_time
    duration_achieved = frames_count / fps if fps > 0 else 0
    percent_achieved = (duration_achieved / TOTAL_SONG_DURATION) * 100.0

    print("\n" + "=" * 70)
    print(">>> BAD APPLE GENERATION COMPLETE <<<")
    print("=" * 70)
    print(f"Output File              : {output_file}")
    print(f"Binary Output Size       : {len(code):,} bytes (Target Budget: {budget:,} B)")
    print(f"ROM Capacity Used        : {len(code)*100/budget:.2f}%")
    print(f"Terminal Grid Resolution : {cols} columns x {rows} rows")
    print(f"Framerate                : {fps} fps")
    print(f"Frames Encoded           : {frames_count:,} frames")
    print(f"Song Duration Encoded    : {duration_achieved:.2f} seconds (out of {TOTAL_SONG_DURATION:.2f}s)")
    print(f"SONG COVERAGE ACHIEVED   : {percent_achieved:.2f}% of Bad Apple!!")
    print(f"Generation Pipeline Time : {elapsed:.2f} seconds")
    print("-" * 70)

    # Brainfuino Hardware Clock Recommendation
    avg_bytes_per_frame = len(code) / frames_count if frames_count > 0 else 0
    est_cycles_per_frame = avg_bytes_per_frame * 3.5
    target_freq = est_cycles_per_frame * fps

    best_idx = 0
    best_diff = float('inf')
    for idx, (f_val, name) in enumerate(CLOCK_FREQUENCIES):
        diff = abs(f_val - target_freq)
        if diff < best_diff:
            best_diff = diff
            best_idx = idx

    rec_freq, rec_name = CLOCK_FREQUENCIES[best_idx]
    speed_ratio = (rec_freq / target_freq) * 100.0 if target_freq > 0 else 100.0

    print(">>> BRAINFUINO HARDWARE CLOCK CALIBRATION <<<")
    print(f"Estimated Cycles / Frame : {int(est_cycles_per_frame):,} cycles/frame")
    print(f"Ideal Target Frequency   : {target_freq/1000000.0:.3f} MHz ({int(target_freq):,} Hz)")
    print(f"Recommended FPGA Clock   : {rec_name} (Menu Index {best_idx}, !SET:10={best_idx})")
    print(f"Hardware Playback Ratio  : {speed_ratio:.1f}% of Real-Time Speed")
    print("=" * 70 + "\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Next-Gen Bad Apple Brainfuck Generator for Brainfuino")
    parser.add_argument("--mode", choices=["resolution", "duration"], default="resolution",
                        help="'resolution' = fixed grid, maximize duration; 'duration' = fixed time, auto-solve highest grid")
    parser.add_argument("--cols", type=int, default=36, help="Terminal columns (default: 36)")
    parser.add_argument("--rows", type=int, default=15, help="Terminal rows (default: 15)")
    parser.add_argument("--fps", type=int, default=12, help="Framerate (default: 12)")
    parser.add_argument("--size", type=int, default=262144, help="ROM byte budget (default: 262144 for 256 kB Flash)")
    parser.add_argument("--target-seconds", type=float, default=219.15, help="Target duration in seconds for --mode duration")
    parser.add_argument("--target-percent", type=float, default=100.0, help="Target percentage of song for --mode duration")
    parser.add_argument("--video", default="scripts/bad_apple/bad_apple.mp4", help="Input video MP4 path")
    parser.add_argument("-o", "--output", default="scripts/bad_apple/bad_apple_optimized.b", help="Output .b file path")

    args = parser.parse_args()
    generate(args)
