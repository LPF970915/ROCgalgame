#!/usr/bin/env python3
import argparse
import fcntl
import os
import struct
import time


IOC_NRBITS = 8
IOC_TYPEBITS = 8
IOC_SIZEBITS = 14
IOC_NRSHIFT = 0
IOC_TYPESHIFT = IOC_NRSHIFT + IOC_NRBITS
IOC_SIZESHIFT = IOC_TYPESHIFT + IOC_TYPEBITS
IOC_DIRSHIFT = IOC_SIZESHIFT + IOC_SIZEBITS
IOC_WRITE = 1


def ioctl_code(direction, ioctl_type, number, size=0):
    return ((direction << IOC_DIRSHIFT) | (ioctl_type << IOC_TYPESHIFT) |
            (number << IOC_NRSHIFT) | (size << IOC_SIZESHIFT))


def iow(ioctl_type, number):
    return ioctl_code(IOC_WRITE, ioctl_type, number, struct.calcsize("i"))


UI_TYPE = ord("U")
UI_DEV_CREATE = ioctl_code(0, UI_TYPE, 1)
UI_DEV_DESTROY = ioctl_code(0, UI_TYPE, 2)
UI_SET_EVBIT = iow(UI_TYPE, 100)
UI_SET_KEYBIT = iow(UI_TYPE, 101)
UI_SET_ABSBIT = iow(UI_TYPE, 103)

EV_SYN = 0
EV_KEY = 1
EV_ABS = 3
SYN_REPORT = 0
ABS_X = 0
ABS_Y = 1
BTN_SOUTH = 304
BTN_EAST = 305
AXIS_MAX = 900


def emit(fd, event_type, code, value):
    os.write(fd, struct.pack("llHHi", 0, 0, event_type, code, value))


def sync(fd):
    emit(fd, EV_SYN, SYN_REPORT, 0)


def set_axis(fd, x, y):
    emit(fd, EV_ABS, ABS_X, round(max(-1.0, min(1.0, x)) * AXIS_MAX))
    emit(fd, EV_ABS, ABS_Y, round(max(-1.0, min(1.0, y)) * AXIS_MAX))
    sync(fd)


def tap(fd, code, duration):
    emit(fd, EV_KEY, code, 1)
    sync(fd)
    time.sleep(duration)
    emit(fd, EV_KEY, code, 0)
    sync(fd)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--settle", type=float, default=1.0)
    parser.add_argument("--axis-x", type=float, default=0.0)
    parser.add_argument("--axis-y", type=float, default=0.0)
    parser.add_argument("--axis-seconds", type=float, default=0.0)
    parser.add_argument("--tap", choices=("none", "a", "b"), default="none")
    parser.add_argument("--tap-seconds", type=float, default=0.12)
    args = parser.parse_args()

    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    try:
        fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
        fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
        fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_EAST)
        fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_SOUTH)
        fcntl.ioctl(fd, UI_SET_ABSBIT, ABS_X)
        fcntl.ioctl(fd, UI_SET_ABSBIT, ABS_Y)

        abs_max = [0] * 64
        abs_min = [0] * 64
        abs_fuzz = [0] * 64
        abs_flat = [0] * 64
        for axis in (ABS_X, ABS_Y):
            abs_max[axis] = AXIS_MAX
            abs_min[axis] = -AXIS_MAX
            abs_flat[axis] = 80
        descriptor = struct.pack(
            "80sHHHHi" + "i" * 256,
            b"gkd_atom_joypad_dialog_smoke",
            0x03,
            0x1209,
            0x3500,
            1,
            0,
            *(abs_max + abs_min + abs_fuzz + abs_flat),
        )
        os.write(fd, descriptor)
        fcntl.ioctl(fd, UI_DEV_CREATE)
        time.sleep(args.settle)

        if args.axis_seconds > 0.0:
            set_axis(fd, args.axis_x, args.axis_y)
            time.sleep(args.axis_seconds)
            set_axis(fd, 0.0, 0.0)
            time.sleep(0.15)
        if args.tap != "none":
            tap(fd, BTN_EAST if args.tap == "a" else BTN_SOUTH,
                args.tap_seconds)
            time.sleep(0.25)
    finally:
        try:
            set_axis(fd, 0.0, 0.0)
            fcntl.ioctl(fd, UI_DEV_DESTROY)
        finally:
            os.close(fd)


if __name__ == "__main__":
    main()
