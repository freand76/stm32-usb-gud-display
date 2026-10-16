#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = []
# ///

"""Check that Linux has found the STM32 GUD display and can use it

Looks for the ST-LINK and the GUD display on USB, checks the USB speed, that the gud
driver is bound, the DRM connector, the framebuffer and its access rights, and whether
GNOME has been told to leave the display alone. Prints a hint for everything that is
missing. Needs no special rights.

Run with "uv run scripts/gud_check.py".
"""

import grp
import os
import subprocess
import sys
from pathlib import Path

GUD_ID = ("1d50", "614d")
STLINK_VENDOR = "0483"
STLINK_PRODUCTS = {"3748", "374b", "3752", "374e", "374f", "3753", "3754", "3757"}
USB_DEVICES = Path("/sys/bus/usb/devices")

ok_count = 0
problem_count = 0


def ok(text):
    global ok_count
    ok_count += 1
    print(f"  ok    {text}")


def problem(text, hint):
    global problem_count
    problem_count += 1
    print(f"  FAIL  {text}")
    for line in hint.splitlines():
        print(f"        {line}")


def note(text):
    print(f"  info  {text}")


def read(path, default=""):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return default


def usb_devices(vendor, products):
    """sysfs directories of the USB devices with the vendor and one of the products"""
    found = []
    for dev in sorted(USB_DEVICES.iterdir()):
        if ":" in dev.name:
            continue
        if read(dev / "idVendor") == vendor and read(dev / "idProduct") in products:
            found.append(dev)
    return found


def check_stlink():
    print("ST-LINK (CN14)")
    devices = usb_devices(STLINK_VENDOR, STLINK_PRODUCTS)
    if not devices:
        problem(
            "no ST-LINK found",
            "Connect CN14 (USB ST-LINK) to the PC, it powers the board too.",
        )
        return
    dev = devices[0]
    node = Path(
        f"/dev/bus/usb/{int(read(dev / 'busnum')):03}/{int(read(dev / 'devnum')):03}"
    )
    ok(f"{read(dev / 'product')} {read(dev / 'idVendor')}:{read(dev / 'idProduct')}")
    if os.access(node, os.W_OK):
        ok(f"{node} is writable, OpenOCD can program the board")
    else:
        problem(
            f"no write access to {node}, OpenOCD fails with LIBUSB_ERROR_ACCESS",
            "Install the udev rule and plug the board in again:\n"
            "  make install-udev\n"
            "Until then, 'make flash-msd' programs through the ST-LINK drive.",
        )
    ttys = sorted(dev.glob("*:*/tty/tty*"))
    if ttys:
        tty = Path("/dev") / ttys[0].name
        if os.access(tty, os.R_OK | os.W_OK):
            ok(f"log on {tty}, read it with 'uv run scripts/serial_log.py'")
        else:
            problem(
                f"no access to {tty}",
                "Add yourself to the dialout group and log in again:\n"
                "  sudo usermod -aG dialout $USER",
            )


def drm_card(interface):
    """The DRM card created for a USB interface, or None"""
    cards = sorted((interface / "drm").glob("card[0-9]*"))
    return cards[0].name if cards else None


def check_display():
    print("GUD display (CN12 for USB=HS, CN13 for USB=FS)")
    if not Path("/sys/module/gud").exists():
        result = subprocess.run(
            ["modinfo", "gud"], check=False, capture_output=True, text=True
        )
        if result.returncode != 0:
            problem(
                "the kernel has no gud driver",
                "The gud driver is in Linux 5.13 and later (CONFIG_DRM_GUD).",
            )
            return

    devices = usb_devices(*GUD_ID[:1], {GUD_ID[1]})
    if not devices:
        problem(
            "no GUD display (1d50:614d) on USB",
            "- Is the firmware flashed? The LCD shows 'USB GUD Display' when it runs.\n"
            "- Is the cable in the connector of the build: CN12 for USB=HS, CN13 for\n"
            "  USB=FS? The splash screen on the LCD names the connector.\n"
            "- Is it a data cable? A charge only cable powers the USB PHY, but the\n"
            "  PC never sees the device. Check 'journalctl -k -f' while plugging in.",
        )
        return
    dev = devices[0]
    speed = read(dev / "speed")
    names = {"12": "Full Speed", "480": "High Speed"}
    ok(
        f"{read(dev / 'product')} at {names.get(speed, speed + ' Mbit/s')}, port {dev.name}"
    )

    interface = dev / f"{dev.name}:1.0"
    driver = (
        Path(os.path.realpath(interface / "driver")).name
        if (interface / "driver").exists()
        else None
    )
    if driver != "gud":
        problem(
            f"the interface is bound to {driver or 'no driver'}, not gud",
            "Load the driver: sudo modprobe gud",
        )
        return
    ok("bound to the gud driver")

    card = drm_card(interface)
    if card is None:
        problem("no DRM device", "See 'journalctl -k' for gud errors.")
        return
    connectors = sorted(Path("/sys/class/drm").glob(f"{card}-*"))
    for connector in connectors:
        status = read(connector / "status")
        modes = ", ".join(read(connector / "modes").split()) or "none"
        if status == "connected":
            ok(f"/dev/dri/{card}, connector {connector.name}: {status}, modes {modes}")
        else:
            problem(
                f"/dev/dri/{card}, connector {connector.name}: {status}",
                "The display reports its connector disconnected for 5 seconds after\n"
                "it is plugged in, and the kernel polls it every 10 seconds. Wait\n"
                "15 seconds and run the check again.",
            )

    fbs = sorted(interface.glob("graphics/fb[0-9]*"))
    if not fbs:
        note("no framebuffer yet, it appears when the connector is connected")
    else:
        fb = Path("/dev") / fbs[0].name
        if os.access(fb, os.W_OK):
            ok(f"framebuffer {fb} is writable")
        else:
            group = grp.getgrgid(os.stat(fb).st_gid).gr_name
            problem(
                f"no write access to {fb}",
                f"Add yourself to the {group} group and log in again:\n"
                f"  sudo usermod -aG {group} $USER",
            )

    check_mutter(card)


def check_mutter(card):
    """Whether GNOME (mutter) has been told to ignore the display"""
    result = subprocess.run(
        ["udevadm", "info", "--query=property", f"/sys/class/drm/{card}"],
        check=False,
        capture_output=True,
        text=True,
    )
    tags = ""
    for line in result.stdout.splitlines():
        if line.startswith(("CURRENT_TAGS=", "TAGS=")):
            tags += line
    desktop = os.environ.get("XDG_CURRENT_DESKTOP", "")
    if "mutter-device-ignore" in tags:
        ok(
            "GNOME ignores the display (71-gud-mutter-ignore.rules), draw on it yourself"
        )
    elif "GNOME" in desktop:
        note(
            "GNOME uses the display as a monitor. Plug it in once per session, or\n"
            "        install udev/71-gud-mutter-ignore.rules, see the README."
        )
    else:
        note("not tagged mutter-device-ignore, the desktop may use the display")


def main():
    if not USB_DEVICES.exists():
        sys.exit("No /sys/bus/usb, this script needs Linux")
    check_stlink()
    print()
    check_display()
    print()
    if problem_count:
        print(f"{problem_count} problem(s) found")
        sys.exit(1)
    print("Everything looks fine. Draw on the display with:")
    print("  uv run scripts/fb_animation.py")


if __name__ == "__main__":
    main()
