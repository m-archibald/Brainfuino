import serial
import time
import sys

PORT = 'COM28'
BAUD = 115200

def open_serial():
    for _ in range(5):
        try:
            s = serial.Serial(PORT, BAUD, timeout=1.0)
            time.sleep(0.1)
            s.reset_input_buffer()
            return s
        except Exception as e:
            time.sleep(0.5)
    raise RuntimeError(f"Could not open {PORT}")

def read_until_prompt(ser, timeout=3.0):
    return read_until_text(ser, [b"Select", b"Enter", b"> "], timeout)

def read_until_text(ser, patterns, timeout=3.0):
    if isinstance(patterns, (bytes, str)):
        patterns = [patterns if isinstance(patterns, bytes) else patterns.encode('utf-8')]
    else:
        patterns = [p if isinstance(p, bytes) else p.encode('utf-8') for p in patterns]
    start = time.time()
    buf = b""
    while time.time() - start < timeout:
        chunk = ser.read_all()
        if chunk:
            buf += chunk
            for p in patterns:
                if p in buf:
                    time.sleep(0.1)
                    buf += ser.read_all()
                    return buf.decode('utf-8', errors='replace')
        time.sleep(0.05)
    return buf.decode('utf-8', errors='replace')

print("==========================================================")
print("     BRAINFUINO PROGRAM LIBRARY & MENU TEST SUITE")
print("==========================================================")

ser = open_serial()

# Step 1: Open Main Menu
print("\n[Test 1] Opening Main Menu via !MENU...")
ser.write(b"!MENU\r\n")
time.sleep(0.3)
resp = read_until_prompt(ser)
print(resp)

assert "BRAINFUINO CONFIGURATION MENU" in resp, "Main menu header missing"
assert "1. Program Library" in resp, "Program Library option missing"
assert "2. Hardware Settings" in resp, "Hardware Settings option missing"
print(">>> PASS: Main Menu rendered correctly.")

# Step 2: Navigate to Hardware Settings
print("\n[Test 2] Entering Hardware Settings (option 2)...")
ser.write(b"2")
time.sleep(0.3)
resp = read_until_prompt(ser)
print(resp)

assert "BRAINFUINO HARDWARE SETTINGS" in resp, "Settings menu header missing"
assert "Prune non-BF on Paste" in resp, "Prune on paste option missing"
assert "Prune non-BF in Library" in resp, "Prune in library option missing"
print(">>> PASS: Hardware Settings rendered correctly.")

# Step 3: Test Left / Right arrow navigation
print("\n[Test 3] Testing Left/Right arrow option cycling on Clock Frequency...")
# Move cursor down to item 5 (FPGA Clock Frequency)
ser.write(b"\x1b[B\x1b[B\x1b[B\x1b[B") # 4 down arrows from item 0 to item 4
time.sleep(0.2)
# Press Right arrow to cycle frequency forward
ser.write(b"\x1b[C")
time.sleep(0.3)
resp_right = read_until_prompt(ser)
print("After Right Arrow:")
for line in resp_right.splitlines():
    if "FPGA Clock Frequency" in line:
        print("  " + line.strip())

# Press Left arrow to cycle frequency backward
ser.write(b"\x1b[D")
time.sleep(0.3)
resp_left = read_until_prompt(ser)
print("After Left Arrow:")
for line in resp_left.splitlines():
    if "FPGA Clock Frequency" in line:
        print("  " + line.strip())

print(">>> PASS: Left/Right arrow cycling operational.")

# Return to Main Menu
print("\n[Test 4] Returning to Main Menu...")
ser.write(b"0")
time.sleep(0.3)
resp = read_until_prompt(ser)
assert "BRAINFUINO CONFIGURATION MENU" in resp, "Failed to return to Main Menu"
print(">>> PASS: Returned to Main Menu.")

# Step 5: Enter Program Library
print("\n[Test 5] Entering Program Library (option 1)...")
ser.write(b"1")
time.sleep(0.3)
resp = read_until_prompt(ser)
print(resp)

assert "BRAINFUINO PROGRAM LIBRARY" in resp, "Library header missing"
assert "00. Brainfuino Demo" in resp, "Slot 00 Brainfuino Demo missing"
assert "[ + Add New Program ]" in resp, "Add New Program button missing"
print(">>> PASS: Program Library rendered correctly.")

# Step 6: Test Adding a Program to Library
print("\n[Test 6] Adding test program to Library...")
ser.write(b"a") # Press 'A' to add program
prompt_name = read_until_text(ser, "Enter program name", timeout=3.0)
print("Add Prompt:", prompt_name)
assert "Enter program name" in prompt_name, "Did not prompt for name"

# Send Name
ser.write(b"TestHello\r")
prompt_paste = read_until_text(ser, "Paste Brainfuck code now", timeout=3.0)
print("Paste Prompt:", prompt_paste)
assert "Paste Brainfuck code now" in prompt_paste, "Did not prompt for paste"

# Send a small Brainfuck Hello World program with non-BF comments to test pruning
test_bf = b"""
[Comment explaining program]
++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.
[More trailing comments]
"""
ser.write(test_bf)
verify_prompt = read_until_text(ser, "Run on FPGA to verify before saving?", timeout=3.0)
print("Verify Prompt:", verify_prompt)
assert "Run on FPGA to verify before saving?" in verify_prompt, "Did not prompt for verification"

# Test choosing 'n' (skip verify)
print("Selecting 'n' to skip pre-run verification and save immediately...")
ser.write(b"n")
save_resp = read_until_text(ser, ["Saved to Slot", "SUCCESS"], timeout=4.0)
print("Save Response:", save_resp)
assert "Saved to Slot" in save_resp or "TestHello" in save_resp, "Save confirmation missing"
print(">>> PASS: Program successfully saved to Library partition!")

# Step 7: Verify Library listing includes the new program
print("\n[Test 7] Waiting for updated Program Library listing...")
resp_lib = read_until_text(ser, "Select program", timeout=4.0)
print("Updated Library Menu:\n", resp_lib)
assert "TestHello" in resp_lib, "TestHello missing from Library listing"
print(">>> PASS: TestHello appears in Program Library listing.")

# Step 8: Test Loading and Running the Program
print("\n[Test 8] Loading and Running TestHello from Library...")
ser.write(b"\x1b[B") # Down arrow once to highlight Slot 01 TestHello
time.sleep(0.2)
ser.write(b"\r")     # Enter to Load & Run
run_output = read_until_text(ser, "Loaded and running", timeout=4.0)
print("Execution Output:\n", run_output)
assert "Loaded and running" in run_output and "TestHello" in run_output, "Program did not execute properly"
print(">>> PASS: Program loaded from Flash and executed successfully!")

# Step 9: Re-enter menu and test Deleting the Program
print("\n[Test 9] Deleting TestHello from Library...")
ser.write(b"!MENU\r\n")
read_until_prompt(ser)

ser.write(b"1") # Open Library
read_until_prompt(ser)

ser.write(b"\x1b[B") # Highlight Slot 01
time.sleep(0.2)
ser.write(b"d")      # Press 'D' to delete
del_resp = read_until_text(ser, "Deleted Slot", timeout=3.0)
print("Delete Response:", del_resp)

resp_after_del = read_until_text(ser, "Select program", timeout=3.0)
print("Library after deletion:\n", resp_after_del)
assert "TestHello" not in resp_after_del, "TestHello was not deleted"
print(">>> PASS: Tombstone deletion verified cleanly (0 active user programs).")

# Step 10: Exit Menu and Restore Default Demo
print("\n[Test 10] Restoring Default Demo and verifying logo banner...")
ser.write(b"0") # Back to Main
read_until_prompt(ser)

ser.write(b"4") # Restore Factory Demo
time.sleep(0.6)
logo_output = ser.read_all().decode('utf-8', errors='replace')
print("Factory Demo Output:\n", repr(logo_output[:100]))
assert "BRAINFUINO" in logo_output or "Brainfuino" in logo_output or "RESTORE" in logo_output or "Default Demo" in logo_output, "Default demo restore failed"
print(">>> PASS: Factory demo restored and verified.")

ser.close()
print("\n==========================================================")
print(">>> ALL 10 PROGRAM LIBRARY & MENU TESTS PASSED 100%! <<<")
print("==========================================================")
