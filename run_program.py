#!/usr/bin/env python3
"""
Brainfuino Program Runner & Terminal
===================================
A unified CLI utility to auto-detect, flash, and interact with the
Brainfuino native Brainfuck development board.

Usage:
  python run_program.py                        # Interactive menu of available programs
  python run_program.py <path/to/program.b>     # Flash and run specified program
  python run_program.py --run-only             # Connect to currently flashed program
  python run_program.py --list                 # List all available sample programs
"""

import sys
import os
import time
import argparse
import threading
import serial
from serial.tools import list_ports

# Platform-specific non-blocking keyboard input
try:
    import msvcrt
    WINDOWS = True
except ImportError:
    WINDOWS = False
    import select
    import tty
    import termios

PROGRAMS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "programs")

def auto_detect_port():
    """Locate the Brainfuino Virtual COM Port."""
    ports = list(list_ports.comports())
    
    # Check for ST CDC / Brainfuino identifiers
    for p in ports:
        desc = (p.description or "").lower()
        hwid = (p.hwid or "").lower()
        mfg = (p.manufacturer or "").lower()
        if "0483:5740" in hwid or "brainfuino" in desc or "stm" in desc or "stmicroelectronics" in mfg:
            return p.device, p.description

    # Fallback to single available port
    if len(ports) == 1:
        return ports[0].device, ports[0].description

    if ports:
        port_list = ", ".join([f"{p.device} ({p.description})" for p in ports])
        raise RuntimeError(f"Could not uniquely identify Brainfuino.\nAvailable ports: {port_list}\nPlease specify with --port <PORT>.")
    else:
        raise RuntimeError("No COM ports found. Please ensure Brainfuino is connected via USB.")

def list_available_programs():
    """Discover all .b programs in the programs/ directory grouped by folder."""
    discovered = []
    if not os.path.isdir(PROGRAMS_DIR):
        return discovered

    for root, dirs, files in os.walk(PROGRAMS_DIR):
        for f in sorted(files):
            if f.endswith(".b"):
                full_path = os.path.join(root, f)
                rel_path = os.path.relpath(full_path, os.getcwd())
                category = os.path.basename(root)
                discovered.append((category, f, full_path, rel_path))
    return discovered

def interactive_pick_program():
    """Prompt user to choose a program from programs/."""
    programs = list_available_programs()
    if not programs:
        print(f"No .b programs found in {PROGRAMS_DIR}!")
        return None

    print("\n" + "=" * 60)
    print("       BRAINFUINO PROGRAM SELECTOR")
    print("=" * 60)
    
    current_cat = None
    for idx, (cat, name, _, rel) in enumerate(programs, 1):
        if cat != current_cat:
            current_cat = cat
            print(f"\n  [{cat.upper()}]")
        size = os.path.getsize(rel)
        print(f"    {idx:2d}) {name:<20} ({size:,} bytes) -> {rel}")

    print("\n" + "=" * 60)
    while True:
        try:
            choice = input(f"Select program number (1-{len(programs)}) or 'q' to quit: ").strip()
            if choice.lower() in ('q', 'exit', 'quit'):
                return None
            idx = int(choice)
            if 1 <= idx <= len(programs):
                return programs[idx - 1][3]
            print(f"Please enter a number between 1 and {len(programs)}.")
        except ValueError:
            print("Invalid input. Please enter a number.")
        except (KeyboardInterrupt, EOFError):
            print("\nAborted.")
            return None

def open_serial(port, baud=115200):
    """Open serial port with friendly diagnostics for locked or missing ports."""
    try:
        ser = serial.Serial(port, baudrate=baud, timeout=0.05)
        return ser
    except serial.SerialException as e:
        err = str(e)
        if "PermissionError" in err or "Access is denied" in err:
            print(f"\n[ERROR] Access denied to {port}!")
            print("Another program (PuTTY, Tera Term, VS Code, etc.) has this port open.")
            print("Please disconnect that terminal and try again.\n")
        elif "FileNotFoundError" in err or "cannot find the file" in err:
            print(f"\n[ERROR] Port {port} not found! Is the Brainfuino USB cable connected?\n")
        else:
            print(f"\n[ERROR] Failed to open {port}: {e}\n")
        sys.exit(1)

def flash_program(ser, file_path):
    """Stream Brainfuck code to STM32 coprocessor for parallel ROM flashing."""
    with open(file_path, "r", encoding="latin-1", errors="replace") as f:
        code = f.read()

    size = len(code)
    print(f"\nLoaded '{file_path}': {size:,} characters.")
    print("Streaming to Brainfuino Flash ROM...")

    # Ensure in RUN mode first
    ser.write(b"\x1b\x1b\r\n")
    time.sleep(0.2)
    ser.reset_input_buffer()

    t0 = time.time()
    chunk_size = 512
    sent = 0
    for i in range(0, size, chunk_size):
        chunk = code[i:i + chunk_size].encode("latin-1")
        ser.write(chunk)
        sent += len(chunk)
        time.sleep(0.005)

    dt = time.time() - t0
    rate = (sent / dt / 1024) if dt > 0 else 0
    print(f"Sent {sent:,} bytes in {dt:.2f}s ({rate:.1f} kB/s).")
    print("Waiting for STM32 to erase and burn ROM...\n")

def run_interactive_terminal(ser):
    """Full-duplex non-blocking interactive serial console."""
    stop_event = threading.Event()

    def rx_thread_func():
        """Read data from Brainfuino and print to console."""
        while not stop_event.is_set():
            try:
                data = ser.read(ser.in_waiting or 1)
                if data:
                    text = data.decode("latin-1", errors="replace")
                    sys.stdout.write(text)
                    sys.stdout.flush()
            except serial.SerialException:
                break
            except Exception:
                pass

    rx_thread = threading.Thread(target=rx_thread_func, daemon=True)
    rx_thread.start()

    # Main thread: poll keyboard input and transmit
    try:
        while not stop_event.is_set():
            if WINDOWS:
                if msvcrt.kbhit():
                    ch = msvcrt.getch()
                    # Check for Ctrl+C
                    if ch == b"\x03":
                        print("\n[Exiting terminal...]")
                        break
                    # Check for Ctrl+R (Reset FPGA)
                    elif ch == b"\x12":
                        ser.write(b"!RST\r\n")
                    else:
                        ser.write(ch)
                else:
                    time.sleep(0.01)
            else:
                # POSIX fallback
                r, _, _ = select.select([sys.stdin], [], [], 0.02)
                if r:
                    ch = sys.stdin.read(1)
                    if ch == '\x03':
                        break
                    ser.write(ch.encode('latin-1'))
    except KeyboardInterrupt:
        print("\n[Exiting terminal...]")
    finally:
        stop_event.set()
        rx_thread.join(timeout=0.5)

def main():
    parser = argparse.ArgumentParser(description="Brainfuino Program Runner & Terminal")
    parser.add_argument("program", nargs="?", default=None, help="Brainfuck file to flash (.b)")
    parser.add_argument("--port", default=None, help="Serial port (auto-detected if omitted)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--run-only", action="store_true", help="Connect & reset without flashing new code")
    parser.add_argument("--list", action="store_true", help="List all available programs and exit")
    args = parser.parse_args()

    if args.list:
        programs = list_available_programs()
        print("\nAvailable Brainfuino Programs:")
        for cat, name, _, rel in programs:
            print(f"  {cat:<10} : {rel}")
        return

    # 1. Resolve Target Program
    file_to_run = args.program
    if not args.run_only:
        if not file_to_run:
            file_to_run = interactive_pick_program()
            if not file_to_run:
                return
        if not os.path.isfile(file_to_run):
            print(f"[ERROR] File not found: {file_to_run}")
            sys.exit(1)

    # 2. Port Detection
    if args.port:
        port = args.port
        print(f"Using specified port: {port}")
    else:
        port, desc = auto_detect_port()
        print(f"Auto-detected Brainfuino on {port} ({desc})")

    # 3. Connect
    ser = open_serial(port, args.baud)

    # 4. Flash or Reset
    if not args.run_only:
        flash_program(ser, file_to_run)
    else:
        print("\n[Run-Only Mode] Resetting FPGA soft-processor...")
        ser.write(b"!RST\r\n")

    # 5. Interactive Terminal
    print("=" * 60)
    print("  INTERACTIVE TERMINAL ACTIVE  (Press Ctrl+C to quit)")
    print("  Shortcuts: 1-7: Change clock speed | Ctrl+R: Reset FPGA")
    print("=" * 60 + "\n")

    try:
        run_interactive_terminal(ser)
    finally:
        ser.close()
        print("\nDisconnected from Brainfuino.")

if __name__ == "__main__":
    main()
