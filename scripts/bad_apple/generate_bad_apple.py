import os
import sys
import glob
from PIL import Image

def make_init(targets):
    # targets: dict of cell_idx -> value
    # cell 8 is multiplier
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

def generate_bad_apple(target_bytes=76000, cols=36, rows=15, fps=12, delay_level=0, output_file="bad_apple.b"):
    frames_dir = os.path.join(os.path.dirname(__file__), "frames")
    frame_files = sorted(glob.glob(os.path.join(frames_dir, "frame_*.png")))
    if not frame_files:
        print(f"Error: No frame images found in {frames_dir}")
        return

    print(f"Found {len(frame_files)} frames in {frames_dir}")
    print(f"Target size: {target_bytes} bytes | Grid: {cols}x{rows} | FPS: {fps} | Delay Level: {delay_level}")

    # Register allocation:
    # Cell 0: ' ' (32)
    # Cell 1: Counter for Cell 0 (0)
    # Cell 2: '#' (35)
    # Cell 3: Counter for Cell 2 (0)
    # Cell 4: '\n' (10)
    # Cell 5: ESC (27)
    # Cell 6: '[' (91)
    # Cell 7: 'H' (72)
    # Cell 8, 9: Scratch / Delay

    targets = {0: 32, 2: 35, 4: 10, 5: 27, 6: 91, 7: 72}
    init_code = make_init(targets)

    bf = [init_code]
    curr_cell = 0

    def move_to(target):
        nonlocal curr_cell
        diff = target - curr_cell
        curr_cell = target
        return (">" if diff > 0 else "<") * abs(diff)

    def emit_run(char, count):
        nonlocal curr_cell
        if count == 0:
            return ""
        
        char_cell = 0 if char == ' ' else 2
        res = move_to(char_cell)
        
        direct = "." * count
        best = direct
        
        for b in range(2, 10):
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

    frames_encoded = 0
    curr_size = len(init_code)

    for fidx, fpath in enumerate(frame_files):
        im = Image.open(fpath).convert('L')
        if im.size != (cols, rows):
            im = im.resize((cols, rows), Image.Resampling.BOX)

        frame_bf = []
        
        # 1. Cursor Home: ESC [ H (cells 5, 6, 7)
        frame_bf.append(move_to(5) + "." + move_to(6) + "." + move_to(7) + "." + move_to(0))

        # 2. Render rows
        for y in range(rows):
            row_runs = []
            curr_c = None
            curr_len = 0
            for x in range(cols):
                lum = im.getpixel((x, y))
                c = '#' if lum > 128 else ' '
                if c == curr_c:
                    curr_len += 1
                else:
                    if curr_c is not None:
                        row_runs.append((curr_c, curr_len))
                    curr_c = c
                    curr_len = 1
            if curr_len > 0:
                row_runs.append((curr_c, curr_len))

            # Emit runs for this row
            for c, length in row_runs:
                frame_bf.append(emit_run(c, length))

            # Emit newline (cell 4)
            frame_bf.append(move_to(4) + ".")

        # Optional delay loop between frames
        if delay_level > 0:
            # 2-level loop on cell 8 and 9
            cnt = min(delay_level, 200)
            frame_bf.append(move_to(8) + ("+" * cnt) + "[>++++[<-]>-]" + move_to(0))

        frame_str = "".join(frame_bf)
        
        # Check size budget (leaving 10 bytes for halt loop)
        if curr_size + len(frame_str) + 10 > target_bytes:
            print(f"Target size reached at frame {fidx} / {len(frame_files)}")
            break

        bf.append(frame_str)
        curr_size += len(frame_str)
        frames_encoded += 1

    # Append halt loop: [-]+[]
    bf.append("[-]+[]")
    full_code = "".join(bf)

    with open(output_file, "w", encoding="ascii") as f:
        f.write(full_code)

    duration = frames_encoded / fps
    print(f"\nSuccessfully generated '{output_file}'!")
    print(f"Total size    : {len(full_code)} bytes")
    print(f"Frames encoded: {frames_encoded} frames")
    print(f"Duration      : {duration:.2f} seconds @ {fps} fps")
    print(f"Budget used   : {len(full_code)} / {target_bytes} ({len(full_code)*100/target_bytes:.1f}%)")

if __name__ == "__main__":
    target = 76000
    out = "bad_apple_76k.b"
    if len(sys.argv) > 1:
        arg = sys.argv[1].lower()
        if arg in ["256k", "256kb", "rom"]:
            target = 256000
            out = "bad_apple_256k.b"
        elif arg in ["76k", "76kb", "lib", "library"]:
            target = 76000
            out = "bad_apple_76k.b"
        else:
            try:
                target = int(sys.argv[1])
                out = "bad_apple.b"
            except ValueError:
                out = sys.argv[1]

    generate_bad_apple(target_bytes=target, output_file=out)
