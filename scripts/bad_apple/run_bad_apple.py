import serial
import time
import sys
import os

PORT = "COM22"
BAUD = 115200

def flash_and_play(bf_file):
    if not os.path.exists(bf_file):
        print(f"Error: {bf_file} not found")
        return

    with open(bf_file, "r", encoding="ascii") as f:
        code = f.read()

    print(f"Loaded {bf_file} ({len(code)} bytes)")
    ser = serial.Serial(PORT, BAUD, timeout=0.1)

    # Clean any menu state and exit to Run Mode
    ser.write(b"\x1b\x1b0\r\n")
    time.sleep(0.3)
    ser.read_all()

    print("Sending program to Brainfuino (Pasting to parallel ROM)...")
    # Stream in 64-byte chunks
    chunk_size = 256
    for i in range(0, len(code), chunk_size):
        chunk = code[i:i+chunk_size].encode('ascii')
        ser.write(chunk)
        time.sleep(0.01)
        # Read back progress if available
        resp = ser.read_all().decode('utf-8', errors='replace')
        if resp:
            sys.stdout.write(resp)
            sys.stdout.flush()

    print("\nWaiting for Flash completion and Auto-Launch...")
    # Read until auto-launch
    start = time.time()
    while time.time() - start < 15.0:
        chunk = ser.read_all().decode('utf-8', errors='replace')
        if chunk:
            sys.stdout.write(chunk)
            sys.stdout.flush()
            if "Resuming Brainfuino" in chunk or "Auto-launching" in chunk:
                break
        time.sleep(0.05)

    print("\n=======================================================")
    print(">>> BAD APPLE IS NOW PLAYING LIVE ON FPGA! <<<")
    print("=======================================================\n")

    # Read and display frames live for 6 seconds
    start = time.time()
    while time.time() - start < 6.0:
        chunk = ser.read_all().decode('utf-8', errors='replace')
        if chunk:
            sys.stdout.write(chunk)
            sys.stdout.flush()
        time.sleep(0.02)

    ser.close()
    print("\n[Done reading live demo]")

if __name__ == "__main__":
    bf = "bad_apple_76k.b"
    if len(sys.argv) > 1:
        bf = sys.argv[1]
    flash_and_play(bf)
