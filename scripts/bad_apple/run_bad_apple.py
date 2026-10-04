import serial
import serial.tools.list_ports
import time
import sys
import os
import argparse

BAUD = 115200

def find_brainfuino_port():
    """Auto-detect STMicroelectronics Virtual COM Port if available."""
    for p in serial.tools.list_ports.comports():
        if "STM" in p.description or "STMicroelectronics" in p.description or "Virtual COM" in p.description:
            return p.device
    return "COM32"

def flash_and_play(bf_file, port=None, clock_idx=None):
    if not os.path.exists(bf_file):
        print(f"Error: File '{bf_file}' not found")
        return

    if port is None:
        port = find_brainfuino_port()

    with open(bf_file, "r", encoding="ascii", errors="replace") as f:
        code = f.read()

    size = len(code)
    print("\n" + "=" * 65)
    print(f"Loaded: {bf_file} ({size:,} bytes / {size*100/262144:.1f}% of Flash ROM)")
    print(f"Target Serial Port: {port} @ {BAUD} baud")
    print("=" * 65 + "\n")

    try:
        ser = serial.Serial(port, BAUD, timeout=0.1)
    except serial.SerialException as e:
        err = str(e)
        if "PermissionError" in err or "Access is denied" in err:
            print(f"\n[ERROR] Access denied to {port}!")
            print("Another program (PuTTY, Tera Term, Arduino Serial Monitor, etc.) currently has this port open.")
            print("Please close or disconnect that serial terminal/monitor and run this command again.\n")
        else:
            print(f"[ERROR] Failed to open {port}: {e}")
        return

    # Clean any menu state and exit to Run Mode
    ser.write(b"\x1b\x1b0\r\n")
    time.sleep(0.3)
    ser.reset_input_buffer()

    # Set clock speed if requested
    if clock_idx is not None:
        print(f"Configuring FPGA clock speed: Menu Index {clock_idx} (!SET:10={clock_idx})...")
        ser.write(f"!SET:10={clock_idx}\r\n".encode('ascii'))
        time.sleep(0.1)
        ser.reset_input_buffer()

    print("Streaming Brainfuck program to parallel ROM (Pasting)...")
    chunk_size = 512
    t0 = time.time()
    sent = 0
    for i in range(0, size, chunk_size):
        chunk = code[i:i+chunk_size].encode('ascii')
        ser.write(chunk)
        sent += len(chunk)
        pct = (sent / size) * 100.0
        sys.stdout.write(f"\rStreaming progress: {sent:,} / {size:,} bytes ({pct:.1f}%)")
        sys.stdout.flush()
        time.sleep(0.005)
        # Flush any incoming responses
        _ = ser.read_all()

    dt = time.time() - t0
    rate = (sent / dt / 1024) if dt > 0 else 0
    print(f"\n\nStream complete: {sent:,} bytes in {dt:.2f}s ({rate:.1f} kB/s).")
    print("Waiting for STM32 to finish burning ROM and auto-launch...")

    # Wait for execution / auto-launch
    start = time.time()
    while time.time() - start < 15.0:
        chunk = ser.read_all().decode('utf-8', errors='replace')
        if chunk:
            sys.stdout.write(chunk)
            sys.stdout.flush()
            if "Resuming Brainfuino" in chunk or "Auto-launching" in chunk:
                break
        time.sleep(0.05)

    print("\n" + "=" * 65)
    print(">>> BAD APPLE IS NOW PLAYING LIVE ON BRAINFUINO! <<<")
    print("=" * 65 + "\n")

    # Read and display live frames from FPGA until interrupted
    try:
        while True:
            chunk = ser.read(ser.in_waiting or 1).decode('utf-8', errors='replace')
            if chunk:
                sys.stdout.write(chunk)
                sys.stdout.flush()
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\n\n[Live display stopped by user]")
    finally:
        ser.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Flash Bad Apple Brainfuck animation to Brainfuino hardware")
    parser.add_argument("file", nargs="?", default="scripts/bad_apple/bad_apple_100pct_4fps.b", help="Path to Brainfuck .b file")
    parser.add_argument("--port", default=None, help="Serial COM port (default: auto-detect STM32 or COM32)")
    parser.add_argument("--clock-idx", type=int, default=None, help="FPGA Clock speed index (0..24), e.g. 8 for 5 kHz")

    args = parser.parse_args()
    flash_and_play(args.file, port=args.port, clock_idx=args.clock_idx)
