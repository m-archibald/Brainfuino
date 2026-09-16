import serial
import time
import sys

def main():
    port = "COM28"
    baud = 115200
    ser = serial.Serial(port, baud, timeout=0.5)
    time.sleep(0.2)
    ser.reset_input_buffer()

    print("\n--- ADVANCED TEST 1: Open Menu and Test Up Arrow Wrapping ---")
    ser.write(b"!menu\r\n")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 2048).decode('latin1', errors='replace')
    print("Initial Menu:")
    print(resp)

    # From Item 0, pressing Up Arrow should wrap to Item 8 (0. Save & Exit)
    print("\n--- Sending Up Arrow (\\x1b[A) to test wrapping to Item 8 ---")
    ser.write(b"\x1b[A")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 2048).decode('latin1', errors='replace')
    print(resp)
    if "> 0. Save & Exit" in resp:
        print("PASS: Up Arrow correctly wrapped to bottom item (Save & Exit)!")
    else:
        print("FAIL: Wrap did not land on Save & Exit!")

    # Pressing Down Arrow should wrap back to Item 0 (1. Auto-Program on Paste)
    print("\n--- Sending Down Arrow (\\x1b[B) to test wrapping to Item 0 ---")
    ser.write(b"\x1b[B")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 2048).decode('latin1', errors='replace')
    print(resp)
    if "> 1. Auto-Program" in resp:
        print("PASS: Down Arrow correctly wrapped to top item (Auto-Program)!")
    else:
        print("FAIL: Wrap did not land on Auto-Program!")

    # Test Spacebar to toggle Item 0
    print("\n--- Testing Spacebar to toggle Item 0 ---")
    ser.write(b" ")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 2048).decode('latin1', errors='replace')
    print(resp)

    # Test cycling Clock frequency through all speeds using key '5'
    print("\n--- Testing Clock Frequency cycling (keys '5' repeated) ---")
    for _ in range(6):
        ser.write(b"5")
        time.sleep(0.2)
        resp = ser.read(ser.in_waiting or 2048).decode('latin1', errors='replace')
        # Print just the clock line
        for line in resp.splitlines():
            if "FPGA Clock Frequency" in line:
                print("   ->", line.strip())

    # Test cycling Speed Hotkeys using key '6'
    print("\n--- Testing Speed Hotkey mode cycling (keys '6' repeated) ---")
    for _ in range(4):
        ser.write(b"6")
        time.sleep(0.2)
        resp = ser.read(ser.in_waiting or 2048).decode('latin1', errors='replace')
        for line in resp.splitlines():
            if "Run Mode Speed Hotkeys" in line:
                print("   ->", line.strip())

    # Test cycling Paste Upload Threshold using key '2'
    print("\n--- Testing Paste Upload Threshold cycling (keys '2' repeated) ---")
    for _ in range(5):
        ser.write(b"2")
        time.sleep(0.2)
        resp = ser.read(ser.in_waiting or 2048).decode('latin1', errors='replace')
        for line in resp.splitlines():
            if "Paste Upload Threshold" in line:
                print("   ->", line.strip())

    # Test Exit using 'q'
    print("\n--- Testing Quick Exit using 'q' ---")
    ser.write(b"q")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 2048).decode('latin1', errors='replace')
    print("Response after 'q':", repr(resp))
    if "[Config Saved]" in resp:
        print("PASS: 'q' exited configuration menu cleanly!")
    else:
        print("FAIL: 'q' did not exit cleanly!")

    ser.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
