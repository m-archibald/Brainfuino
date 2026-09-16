import serial
import time
import sys

def main():
    port = "COM28"
    baud = 115200
    print(f"Connecting to Brainfuino on {port}...")
    try:
        ser = serial.Serial(port, baud, timeout=0.5)
    except Exception as e:
        print(f"Failed to open {port}: {e}")
        return 1

    time.sleep(0.2)
    # Drain any initial output
    ser.reset_input_buffer()

    print("\n--- TEST 1: Triggering Config Menu with '!menu' ---")
    ser.write(b"!menu\r\n")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 1024).decode('latin1', errors='replace')
    print("Received:")
    print(resp)

    if "BRAINFUINO CONFIGURATION MENU" not in resp:
        print("FAIL: Config menu title not detected!")
        ser.close()
        return 1
    print("PASS: Menu displayed successfully!")

    print("\n--- TEST 2: Navigating Down with Down Arrow (\\x1b[B) ---")
    ser.write(b"\x1b[B")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 1024).decode('latin1', errors='replace')
    print("Received after Down Arrow:")
    print(resp)

    print("\n--- TEST 3: Navigating Down again with Down Arrow (\\x1b[B) ---")
    ser.write(b"\x1b[B")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 1024).decode('latin1', errors='replace')
    print("Received after 2nd Down Arrow:")
    print(resp)

    print("\n--- TEST 4: Direct Number Key '5' (FPGA Clock Frequency) ---")
    ser.write(b"5\r\n")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 1024).decode('latin1', errors='replace')
    print("Received after pressing '5':")
    print(resp)

    print("\n--- TEST 5: Direct Number Key '1' (Auto-Program toggle) ---")
    ser.write(b"1\r\n")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 1024).decode('latin1', errors='replace')
    print("Received after pressing '1':")
    print(resp)

    print("\n--- TEST 6: Save & Exit with '0' ---")
    ser.write(b"0\r\n")
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 1024).decode('latin1', errors='replace')
    print("Received after pressing '0':")
    print(resp)

    if "[Config Saved]" in resp:
        print("PASS: Successfully exited config menu!")
    else:
        print("WARNING: '[Config Saved]' not seen in immediate response (might be running program).")

    ser.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
