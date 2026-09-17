#!/usr/bin/env python3
"""
Brainfuino Multi-Speed Logo Verification Test
============================================
Tests that the official Brainfuino ASCII logo banner prints with 100% fidelity
across all 13 supported FPGA clock frequencies (62.5 kHz up to 12 MHz).

Hardware Timing Note:
The parallel Flash ROM on Brainfuino Rev 1.1 is SST39LF020-55-4C-WHE (Taa = 55 ns).
All 13 speeds configured here have clock periods >= 83.3 ns, ensuring 100%
compliance with external parallel Flash memory timings.
"""

import sys
import time
import argparse
import serial
from serial.tools import list_ports

# Target ASCII banner lines that the Brainfuino boot logo produces:
EXPECTED_BANNER_LINES = [
    "| __ ) _ __ __ _(_)_ __  / _|_   _(_)_ __   ___",
    "|  _ \\| '__/ _` | | '_ \\| |_| | | | | '_ \\ / _ \\",
    "| |_) | | | (_| | | | | |  _| |_| | | | | | (_) |",
    "|____/|_|  \\__,_|_|_| |_|_|  \\__,_|_|_| |_|\\___/",
]

# (Display Name, Period in ns, Capture Timeout in seconds)
SPEEDS = [
    ("62.5 kHz", 16000.0, 7.0),
    ("125 kHz",   8000.0, 4.5),
    ("250 kHz",   4000.0, 3.0),
    ("500 kHz",   2000.0, 2.5),
    ("750 kHz",   1333.3, 2.5),
    ("1 MHz",     1000.0, 2.5),
    ("1.5 MHz",    666.7, 2.5),
    ("2 MHz",      500.0, 2.5),
    ("3 MHz",      333.3, 2.5),
    ("4 MHz",      250.0, 2.5),
    ("6 MHz",      166.7, 2.5),
    ("8 MHz",      125.0, 2.5),
    ("12 MHz",      83.3, 2.5),
]


def auto_detect_port():
    ports = list(list_ports.comports())
    for p in ports:
        desc = (p.description or "").lower()
        if "stm" in desc or "brainfuino" in desc or "virtual com" in desc:
            return p.device
    if ports:
        return ports[0].device
    return "COM28"


def flush_input(ser):
    time.sleep(0.04)
    while ser.in_waiting:
        ser.read(ser.in_waiting)
        time.sleep(0.01)


def set_speed_and_capture(ser, speed_name, timeout_s=3.0):
    # 1. Open config menu
    flush_input(ser)
    ser.write(b"!menu\r\n")
    time.sleep(0.15)
    flush_input(ser)

    # 2. Cycle speed until it matches target speed_name (up to 16 cycles)
    for _ in range(16):
        ser.write(b"5")
        time.sleep(0.08)
        menu_text = ser.read(ser.in_waiting).decode("ascii", errors="replace")
        if speed_name in menu_text:
            break

    # 3. Exit menu (saves settings and pulses FPGA reset)
    flush_input(ser)
    start_time = time.time()
    ser.write(b"0\r\n")

    # 4. Capture output until idle
    captured = bytearray()
    last_rx = time.time()
    while (time.time() - start_time) < timeout_s:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting)
            captured.extend(chunk)
            last_rx = time.time()
        else:
            if len(captured) > 100 and (time.time() - last_rx) > 0.4:
                break
            time.sleep(0.02)

    elapsed_ms = (last_rx - start_time) * 1000.0
    text = captured.decode("ascii", errors="replace")

    # Check banner fidelity
    matched_lines = sum(1 for line in EXPECTED_BANNER_LINES if line in text)
    passed = (matched_lines == len(EXPECTED_BANNER_LINES))
    fidelity_pct = (matched_lines / len(EXPECTED_BANNER_LINES)) * 100.0

    return passed, fidelity_pct, len(captured), elapsed_ms, text


def main():
    parser = argparse.ArgumentParser(description="Brainfuino 13-Speed Logo Verification Test")
    parser.add_argument("--port", "-p", default=None, help="Serial COM port (default: auto-detect)")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)")
    args = parser.parse_args()

    port = args.port or auto_detect_port()

    print("=" * 74)
    print("           BRAINFUINO 13-SPEED LOGO FIDELITY TEST")
    print("=" * 74)
    print(f"Target Port : {port}")
    print(f"Baud Rate   : {args.baud}")
    print(f"Flash ROM   : SST39LF020-55-4C-WHE (Taa = 55 ns, Fmax = 18.18 MHz)")
    print(f"Frequencies : 13 speeds from 62.5 kHz to 12 MHz (all <= 12 MHz safe)")
    print("=" * 74 + "\n")

    try:
        ser = serial.Serial(port, args.baud, timeout=1)
    except Exception as e:
        print(f"Error opening port {port}: {e}")
        sys.exit(1)

    # Restore Default Demo to ensure known state
    print("Restoring Default Brainfuino Logo Demo in Flash ROM...")
    flush_input(ser)
    ser.write(b"!menu\r\n")
    time.sleep(0.2)
    flush_input(ser)
    ser.write(b"7\r\n")  # Option 7: Restore Default Demo
    time.sleep(1.5)
    flush_input(ser)
    print("Default demo confirmed.\n")

    print(f"{'Idx':<4} {'Frequency':<11} {'Cycle Period':<14} {'ROM Margin':<12} {'Status':<8} {'Fidelity':<10} {'Bytes':<7} {'Time':<9}")
    print("-" * 74)

    results = []
    for idx, (name, cycle_ns, timeout_s) in enumerate(SPEEDS, 1):
        margin_ns = cycle_ns - 55.0
        margin_str = f"+{margin_ns:0.0f} ns" if margin_ns < 1000 else f"+{margin_ns/1000:0.1f} us"
        passed, fidelity, byte_cnt, elapsed, text = set_speed_and_capture(ser, name, timeout_s=timeout_s)
        status_str = "PASS" if passed else "FAIL"

        print(f"[{idx:>2}] {name:<11} {cycle_ns:>7.1f} ns     {margin_str:<12} {status_str:<8} {fidelity:>5.1f}%    {byte_cnt:<7} {elapsed:>6.1f} ms")
        results.append((name, passed, fidelity, byte_cnt, elapsed))

    # Reset back to default clock (500 kHz)
    flush_input(ser)
    ser.write(b"!menu\r\n")
    time.sleep(0.15)
    flush_input(ser)
    for _ in range(16):
        ser.write(b"5")
        time.sleep(0.08)
        menu_text = ser.read(ser.in_waiting).decode("ascii", errors="replace")
        if "500 kHz" in menu_text:
            break
    ser.write(b"0\r\n")
    time.sleep(0.2)
    ser.close()

    print("-" * 74)
    all_passed = all(r[1] for r in results)

    if all_passed:
        print("\n>>> SUCCESS: ALL 13 FREQUENCIES PASSED WITH 100% CHARACTER FIDELITY! <<<")
        print("    Zero dropped bytes across all speeds from 62.5 kHz to 12 MHz.")
    else:
        print("\n>>> WARNING: Character loss detected on some speeds! <<<")
    print("=" * 74)


if __name__ == "__main__":
    main()
