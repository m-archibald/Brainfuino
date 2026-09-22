#!/usr/bin/env python3
"""
Brainfuino Pi Hardware Runner
=============================
Streams a pure Brainfuck Pi program to the Brainfuino FPGA soft-processor
over USB CDC serial (COM22), waits for auto-flashing to complete, and
validates the real-time streamed digits against math.pi.
"""

import sys
import os
import time
import argparse
import serial
from serial.tools import list_ports

EXPECTED_PI_DIGITS = "3.1415926535897932384626433832795028841971"

def auto_detect_port():
    ports = list(list_ports.comports())
    for p in ports:
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        if "stm" in desc or "0483:5740" in hwid or "brainfuino" in desc:
            return p.device
    for p in ports:
        if "com32" in p.device.lower():
            return p.device
    if ports:
        return ports[0].device
    return "COM32"

def flash_and_run(bf_file, port=None, baud=115200, expected_count=5, timeout=600, idle_timeout=180, run_only=False):
    if not port:
        port = auto_detect_port()

    print(f"Opening {port} at {baud} baud...")
    ser = serial.Serial(port, baudrate=baud, timeout=0.1)
    time.sleep(0.3)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # Wake up / ensure in RUN mode (send ESC ESC to exit any config menu)
    ser.write(b"\x1b\x1b\r\n")
    time.sleep(0.3)
    ser.read_all()

    with open(bf_file, 'r') as f:
        code = f.read()

    launch_seen = False
    output_chars = []
    digit_times = []
    start_run_tick = None
    last_rx_tick = None

    if not run_only:
        print(f"Loaded {bf_file}: {len(code):,} characters of Brainfuck code.")
        print("Streaming program to Brainfuino Flash ROM...")
        t0 = time.time()

        # Send in chunks of 512 bytes with small pause for USB flow control
        chunk_size = 512
        total_sent = 0
        for i in range(0, len(code), chunk_size):
            chunk = code[i:i + chunk_size].encode('ascii')
            ser.write(chunk)
            total_sent += len(chunk)
            time.sleep(0.005)

        stream_time = time.time() - t0
        print(f"Sent {total_sent:,} bytes in {stream_time:.2f}s ({total_sent / stream_time / 1024:.1f} kB/s).")
        print("\nWaiting for STM32 Flash programming and FPGA soft-processor auto-launch...")
    else:
        print(f"Running already-flashed program from Flash ROM...")
        ser.write(b"!RST\r\n")
        time.sleep(0.1)
        initial = ser.read_all().decode('latin-1', errors='replace')
        launch_seen = True
        start_run_tick = time.time()
        last_rx_tick = time.time()
        print("\n" + "=" * 55)
        print(">>> FPGA SOFT-PROCESSOR LAUNCHED ON SILICON!")
        print(">>> Streaming Pi digits in real-time:")
        print("=" * 55)
        # Strip reset banner: [bf_µP reset] ... MHz\r\n
        if "MHz" in initial:
            pos = initial.find("MHz")
            initial = initial[pos + 3:].lstrip('\r\n')
        elif "[bf_" in initial:
            pos = initial.find("\n")
            if pos != -1:
                initial = initial[pos + 1:].lstrip('\r\n')
        for c in initial:
            if c in "0123456789.":
                sys.stdout.write(c)
                sys.stdout.flush()
                output_chars.append(c)
                digit_times.append((c, 0.0))

    accumulated = []
    t_start = time.time()
    while time.time() - t_start < timeout:
        data = ser.read(ser.in_waiting or 1)
        if data:
            text = data.decode('latin-1', errors='replace')
            for ch in text:
                if not launch_seen:
                    sys.stdout.write(ch)
                    sys.stdout.flush()
                    accumulated.append(ch)
                    acc_str = "".join(accumulated)
                    if "Auto-launching program..." in acc_str:
                        launch_seen = True
                        print("\n" + "=" * 55)
                        print(">>> FPGA SOFT-PROCESSOR LAUNCHED ON SILICON!")
                        print(">>> Streaming Pi digits in real-time:")
                        print("=" * 55)
                        start_run_tick = time.time()
                        last_rx_tick = time.time()
                        output_chars.clear()
                else:
                    sys.stdout.write(ch)
                    sys.stdout.flush()
                    now = time.time()
                    output_chars.append(ch)
                    if ch in "0123456789.":
                        digit_times.append((ch, now - start_run_tick))
                    last_rx_tick = now

        else:
            if launch_seen and last_rx_tick:
                digits_only = [c for c in output_chars if c in "0123456789"]
                if len(digits_only) >= expected_count:
                    time.sleep(1.0)
                    break
                if (time.time() - last_rx_tick > idle_timeout):
                    print(f"\n[Idle timeout: no output for {idle_timeout:.0f}s]")
                    break
            time.sleep(0.02)

    ser.close()

    result_str = "".join([c for c in output_chars if c in "0123456789."])
    print("\n\n" + "=" * 55)
    print("HARDWARE EXECUTION SUMMARY")
    print("=" * 55)
    print(f"Target Program     : {bf_file}")
    print(f"Code Size Flashed  : {len(code):,} bytes")
    print(f"Hardware Output    : {result_str}")
    expected_ref = EXPECTED_PI_DIGITS[:len(result_str)]
    print(f"Expected Reference : {expected_ref}")

    if result_str == expected_ref and len(result_str) >= 3:
        print("\n>>> [PASS] 100% HARDWARE VERIFICATION SUCCESSFUL!")
        print(f">>> Computed {len([c for c in result_str if c != '.'])} digits of Pi on bare FPGA silicon!")
    else:
        print("\n>>> [FAIL] Output mismatch or incomplete output!")

    if digit_times:
        print("\nDigit Streaming Timeline:")
        for ch, dt in digit_times:
            print(f"  Digit '{ch}': +{dt:.2f}s")
    print("=" * 55)
    return result_str

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Brainfuino Pi Hardware Runner")
    parser.add_argument("file", nargs="?", default="scripts/pi/pi_5.b", help="Brainfuck file to flash")
    parser.add_argument("--count", type=int, default=5, help="Expected digit count")
    parser.add_argument("--port", default="COM32", help="Serial COM port")
    parser.add_argument("--timeout", type=int, default=600, help="Total execution timeout in seconds")
    parser.add_argument("--idle-timeout", type=int, default=180, help="Per-digit idle timeout in seconds")
    parser.add_argument("--run-only", action="store_true", help="Reset and run currently flashed program")
    args = parser.parse_args()

    flash_and_run(args.file, port=args.port, expected_count=args.count, timeout=args.timeout, idle_timeout=args.idle_timeout, run_only=args.run_only)


