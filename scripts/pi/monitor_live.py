import sys
import time
import serial

ser = serial.Serial('COM22', 115200, timeout=0.1)
print("Opened COM22. Resetting FPGA soft-processor...", flush=True)

ser.write(b"!RST\r\n")
time.sleep(0.1)

t0 = time.time()
print(f"Monitoring output from t=0.00s onwards...", flush=True)

digits = []
while True:
    data = ser.read(ser.in_waiting or 1)
    if data:
        t = time.time() - t0
        text = data.decode('latin-1', errors='replace')
        for c in text:
            sys.stdout.write(c)
            sys.stdout.flush()
            if c in "0123456789.":
                digits.append((c, t))
    time.sleep(0.01)
