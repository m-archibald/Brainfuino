import subprocess
import numpy as np

for fps in [8, 10, 12]:
    for cols, rows in [(28, 12), (30, 12), (32, 14), (36, 15)]:
        cmd = ['ffmpeg', '-i', 'scripts/bad_apple/bad_apple.mp4', '-f', 'rawvideo', '-pix_fmt', 'gray', '-vf', f'fps={fps},scale={cols}:{rows}', '-']
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        raw = p.stdout.read()
        frames = (np.frombuffer(raw, dtype=np.uint8).reshape(-1, rows, cols) > 128)

        total_pixel_diffs = 0
        total_segments = 0
        zero_diff_frames = 0
        n_frames = len(frames) - 1

        for i in range(1, len(frames)):
            prev = frames[i-1]
            curr = frames[i]
            diff = (curr != prev)
            diff_count = np.sum(diff)
            if diff_count == 0:
                zero_diff_frames += 1
            total_pixel_diffs += diff_count
            
            segs = 0
            for y in range(rows):
                diff_row = diff[y]
                if not np.any(diff_row):
                    continue
                in_run = False
                for x in range(cols):
                    if diff_row[x]:
                        if not in_run:
                            segs += 1
                            in_run = True
                    else:
                        in_run = False
            total_segments += segs

        avg_diffs = total_pixel_diffs / n_frames
        avg_segs = total_segments / n_frames
        zero_pct = (zero_diff_frames / n_frames) * 100.0
        print(f"FPS: {fps:2d} | Grid: {cols:2d}x{rows:2d} | Frames: {n_frames:4d} | Identical: {zero_pct:4.1f}% | Avg Diffs: {avg_diffs:4.1f} px | Avg Segs: {avg_segs:4.1f}")
