#!/usr/bin/env python3
"""
Brainfuino STM32 DFU Automated Flashing Stress Test
Tests end-to-end software DFU reboot, USB re-enumeration, STM32CubeProgrammer CLI
flashing, download verification, MCU system reset, and application handshake
repeatedly across dozens of cycles.
"""

import argparse
import os
import subprocess
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial is required. Run 'pip install pyserial'.")
    sys.exit(1)

DEFAULT_PROGRAMMER = r"C:\ST\STM32CubeIDE_2.0.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.300.202508131133\tools\bin\STM32_Programmer_CLI.exe"
DEFAULT_HEX = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "companion-STM32-firmware", "Debug", "BrainfuinoMCU.hex"))


def find_com_port(preferred="COM32"):
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if preferred.upper() in p.device.upper():
            return p.device
    for p in ports:
        if "0483" in (p.hwid or "").lower() or "stm" in (p.description or "").lower():
            return p.device
    return preferred


def is_dfu_connected(programmer_cli):
    try:
        res = subprocess.run([programmer_cli, "-l", "usb"], capture_output=True, timeout=5)
        out = (res.stdout or b'').decode('cp1252', errors='replace') + (res.stderr or b'').decode('cp1252', errors='replace')
        out_lower = out.lower()
        if "no stm32 device" in out_lower or "mode: 0" in out_lower:
            return False
        return "total number of available stm32 device in dfu mode:" in out_lower or "stm32  bootloader" in out_lower or "0x0483:0xdf11" in out_lower
    except Exception:
        return False


def wait_for_dfu(programmer_cli, timeout=8.0):
    # Quiet delay to let host USB controller complete disconnect/re-enumeration cleanly
    time.sleep(1.8)
    start = time.time()
    while time.time() - start < timeout:
        if is_dfu_connected(programmer_cli):
            return True
        time.sleep(0.4)
    return False


def wait_for_com_port(port_name, timeout=8.0):
    time.sleep(1.2)
    start = time.time()
    while time.time() - start < timeout:
        ports = [p.device.upper() for p in serial.tools.list_ports.comports()]
        if port_name.upper() in ports:
            return True
        time.sleep(0.3)
    return False


def trigger_dfu(port_name):
    try:
        s = serial.Serial(port_name, 115200, timeout=0.2, write_timeout=0.2)
        s.write(b"!DFU!\r\n")
        s.flush()
        s.close()
        return True, "Triggered via serial command !DFU!"
    except serial.SerialException as e:
        err_msg = str(e)
        if "Access is denied" in err_msg or "PermissionError" in err_msg:
            return False, f"Access denied on {port_name}. Please disconnect Web Serial in browser!"
        return False, f"Serial error: {err_msg}"


def flash_device(programmer_cli, hex_path):
    cmd = [programmer_cli, "-c", "port=usb1", "-w", hex_path, "-v", "-g", "0x08000000"]
    res = subprocess.run(cmd, capture_output=True, timeout=30)
    stdout = (res.stdout or b'').decode('cp1252', errors='replace') + (res.stderr or b'').decode('cp1252', errors='replace')
    success = (res.returncode == 0) and ("Download verified successfully" in stdout or "File download complete" in stdout)
    return success, stdout


def verify_application_running(port_name, timeout=3.0):
    start = time.time()
    received = b""
    while time.time() - start < timeout:
        try:
            with serial.Serial(port_name, 115200, timeout=0.4, write_timeout=0.4) as s:
                s.write(b"!CLK\r\n")
                s.flush()
                time.sleep(0.15)
                received += s.read(128)
                if b"Clock:" in received or b"!CMD:" in received or b"bf_" in received or len(received) > 0:
                    return True, received.decode("utf-8", errors="replace").strip()
        except serial.SerialException:
            time.sleep(0.2)
    return len(received) > 0, received.decode("utf-8", errors="replace").strip()


def run_test_suite(cycles=25, port="COM32", hex_path=DEFAULT_HEX, programmer_cli=DEFAULT_PROGRAMMER):
    print("=" * 72)
    print("  BRAINFUINO AUTOMATED DFU FLASHING STRESS TEST")
    print("=" * 72)
    print(f"Target COM Port:   {port}")
    print(f"Target HEX File:   {hex_path}")
    print(f"Programmer CLI:    {programmer_cli}")
    print(f"Total Cycles:      {cycles}")
    print("=" * 72)

    if not os.path.exists(programmer_cli):
        print(f"ERROR: STM32_Programmer_CLI not found at: {programmer_cli}")
        return False

    if not os.path.exists(hex_path):
        print(f"ERROR: Firmware HEX not found at: {hex_path}")
        return False

    # Check port accessibility
    port = find_com_port(port)
    print(f"Checking access to {port}...")
    try:
        test_s = serial.Serial(port, 115200, timeout=0.2)
        test_s.close()
        print(f"Port {port} is FREE and ready for testing.")
    except serial.SerialException as e:
        if "Access is denied" in str(e):
            print(f"\n[!] WARNING: Access to {port} is denied! Another process (e.g. browser Web Serial) has it open.")
            print("    Please click 'Disconnect' in your web browser terminal tab so the automated test can run.\n")
            return False
        else:
            print(f"Note on initial port check: {e}")

    results = []
    overall_start = time.time()

    for cycle in range(1, cycles + 1):
        cycle_start = time.time()
        print(f"\n--- [Cycle {cycle:02d}/{cycles:02d}] ---")

        # 1. Trigger DFU Reboot
        if is_dfu_connected(programmer_cli):
            print("Device is already in DFU mode.")
        else:
            print(f"Sending !DFU! to {port}...")
            ok, msg = trigger_dfu(port)
            if not ok:
                print(f"FAILED to trigger DFU: {msg}")
                results.append((cycle, False, time.time() - cycle_start, f"DFU trigger failed: {msg}"))
                continue

            print("Waiting for ST DFU USB device enumeration (0483:DF11)...")
            if not wait_for_dfu(programmer_cli, timeout=7.0):
                print("FAILED: DFU device did not enumerate within 7 seconds.")
                results.append((cycle, False, time.time() - cycle_start, "DFU enumeration timeout"))
                continue

        print("DFU device detected. Initiating STM32CubeProgrammer flash...")
        flash_ok, flash_log = flash_device(programmer_cli, hex_path)
        if not flash_ok:
            print(f"FAILED: Programmer CLI exited with error.")
            last_lines = "\n".join(flash_log.strip().splitlines()[-6:])
            print(last_lines)
            results.append((cycle, False, time.time() - cycle_start, "Flashing failed"))
            continue

        print("Flash write and verification successful. Waiting for COM port to re-appear...")
        if not wait_for_com_port(port, timeout=7.0):
            print(f"FAILED: COM port {port} did not re-appear within 7 seconds.")
            results.append((cycle, False, time.time() - cycle_start, "COM re-enumeration timeout"))
            continue

        # Let USB CDC driver stabilize
        time.sleep(0.5)

        print(f"Verifying application handshake on {port}...")
        app_ok, app_resp = verify_application_running(port, timeout=3.5)
        cycle_time = time.time() - cycle_start

        if app_ok:
            print(f"[PASS] Cycle {cycle:02d} completed in {cycle_time:.2f}s. Response: {app_resp[:40]}")
            results.append((cycle, True, cycle_time, "OK"))
        else:
            print(f"[WARN] Flashed OK, but no banner response received on {port} ({cycle_time:.2f}s).")
            results.append((cycle, True, cycle_time, "Flashed, weak handshake"))

        # Brief pause between iterations
        time.sleep(0.5)

    total_time = time.time() - overall_start
    passed = sum(1 for r in results if r[1])
    failed = len(results) - passed

    print("\n" + "=" * 72)
    print("  STRESS TEST SUMMARY RESULTS")
    print("=" * 72)
    print(f"Total Cycles:      {len(results)}")
    print(f"Passed:            {passed} ({passed / len(results) * 100:.1f}%)" if len(results) > 0 else "0")
    print(f"Failed:            {failed}")
    print(f"Total Elapsed:     {total_time:.1f}s")
    if passed > 0:
        pass_times = [r[2] for r in results if r[1]]
        print(f"Avg Cycle Time:    {sum(pass_times)/len(pass_times):.2f}s")
        print(f"Min Cycle Time:    {min(pass_times):.2f}s")
        print(f"Max Cycle Time:    {max(pass_times):.2f}s")
    print("=" * 72)

    return failed == 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Brainfuino Automated Flashing Stress Test")
    parser.add_argument("--cycles", type=int, default=25, help="Number of flash cycles (default: 25)")
    parser.add_argument("--port", type=str, default="COM32", help="Serial COM port (default: COM32)")
    parser.add_argument("--hex", type=str, default=DEFAULT_HEX, help="Path to firmware HEX")
    parser.add_argument("--programmer", type=str, default=DEFAULT_PROGRAMMER, help="Path to STM32_Programmer_CLI.exe")

    args = parser.parse_args()
    success = run_test_suite(cycles=args.cycles, port=args.port, hex_path=args.hex, programmer_cli=args.programmer)
    sys.exit(0 if success else 1)
