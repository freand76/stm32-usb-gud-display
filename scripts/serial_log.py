#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = ["pyserial"]
# ///

"""Print the log of the STM32 GUD display from the ST-LINK virtual COM port

The firmware prints what the host does with the display, and once a second how many
updates it received. Finds the ST-LINK's serial port by itself. Needs read access to
/dev/ttyACMn, which belongs to the dialout group.

Run with "uv run scripts/serial_log.py".
"""

import argparse
import sys
import time
from pathlib import Path

import serial

STLINK_VENDOR = "0483"
BAUD_RATE = 115200


def find_stlink_tty():
    """Device path of the first serial port of an ST-LINK, or None"""
    for tty in sorted(Path("/sys/class/tty").glob("ttyACM*")):
        # The tty belongs to an interface, whose parent is the USB device
        usb_device = (tty / "device").resolve().parent
        vendor = usb_device / "idVendor"
        if vendor.exists() and vendor.read_text().strip() == STLINK_VENDOR:
            return f"/dev/{tty.name}"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--device", help="serial port (default: the ST-LINK's)")
    parser.add_argument(
        "--no-time", action="store_true", help="do not prefix lines with the time"
    )
    args = parser.parse_args()

    device = args.device or find_stlink_tty()
    if device is None:
        sys.exit("No ST-LINK serial port found, is CN14 connected?")
    print(f"Reading {device} at {BAUD_RATE} baud, Ctrl-C to stop", file=sys.stderr)

    try:
        port = serial.Serial(device, BAUD_RATE, timeout=1)
    except serial.SerialException as error:
        sys.exit(
            f"{error}\nAdd yourself to the dialout group: sudo usermod -aG dialout $USER"
        )

    try:
        with port:
            while True:
                line = port.readline()
                if not line:
                    continue
                text = line.decode(errors="replace").rstrip("\r\n")
                if args.no_time:
                    print(text, flush=True)
                else:
                    print(f"{time.strftime('%H:%M:%S')} {text}", flush=True)
    except KeyboardInterrupt:
        pass
    except serial.SerialException as error:
        # The ST-LINK re-enumerates when it is programmed through its drive
        sys.exit(f"\n{error}")


if __name__ == "__main__":
    main()
