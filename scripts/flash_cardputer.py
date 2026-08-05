#!/usr/bin/env python3
"""Flash Cardputer ADV — device must be in download mode (see docs)."""
import glob
import subprocess
import sys
import time

FW = ".pio/build/cardputer-adv/firmware.bin"
ESPTOOL = ["python3", "-m", "esptool"]

def find_port():
    for pattern in ("/dev/cu.usbmodem*", "/dev/tty.usbmodem*", "/dev/cu.usbserial*", "/dev/tty.usbserial*"):
        ports = sorted(glob.glob(pattern))
        if ports:
            return ports[0]
    return None

def main():
    port = find_port()
    if not port:
        print("No USB serial port found. Plug in Cardputer ADV.")
        sys.exit(1)
    print(f"Port: {port}")
    print("Download mode: power OFF -> hold G0 -> power ON -> release G0")
    for attempt in range(20):
        r = subprocess.run(
            ESPTOOL + ["--chip", "esp32s3", "--port", port, "--baud", "460800",
                       "write_flash", "-z", "0x10000", FW],
            capture_output=True,
            text=True,
        )
        if r.returncode == 0:
            print(r.stdout)
            print("Flash OK")
            return
        time.sleep(0.5)
    print("Flash failed. Enter download mode and retry:")
    print(f"  ~/.platformio/penv/bin/pio run -e cardputer-adv -t upload")
    sys.exit(1)

if __name__ == "__main__":
    main()
