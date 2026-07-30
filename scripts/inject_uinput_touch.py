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
UI_SET_PROPBIT = iow(UI_TYPE, 110)

EV_SYN = 0
EV_KEY = 1
EV_ABS = 3
SYN_REPORT = 0
ABS_X = 0
ABS_Y = 1
ABS_MT_SLOT = 0x2F
ABS_MT_POSITION_X = 0x35
ABS_MT_POSITION_Y = 0x36
ABS_MT_TRACKING_ID = 0x39
BTN_TOUCH = 0x14A
INPUT_PROP_DIRECT = 1
BUS_USB = 0x03


def emit(fd, event_type, code, value):
    os.write(fd, struct.pack("llHHi", 0, 0, event_type, code, value))


def sync(fd):
    emit(fd, EV_SYN, SYN_REPORT, 0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("x", type=int)
    parser.add_argument("y", type=int)
    parser.add_argument("--width", type=int, default=960)
    parser.add_argument("--height", type=int, default=640)
    parser.add_argument("--hold", type=float, default=0.12)
    parser.add_argument("--settle", type=float, default=1.0)
    args = parser.parse_args()

    if args.width <= 1 or args.height <= 1:
        parser.error("width and height must be greater than one")
    x = max(0, min(args.x, args.width - 1))
    y = max(0, min(args.y, args.height - 1))

    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    try:
        fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
        fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
        fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH)
        for axis in (ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_POSITION_X,
                     ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID):
            fcntl.ioctl(fd, UI_SET_ABSBIT, axis)
        try:
            fcntl.ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT)
        except OSError:
            pass

        abs_max = [0] * 64
        abs_min = [0] * 64
        abs_fuzz = [0] * 64
        abs_flat = [0] * 64
        abs_max[ABS_X] = args.width - 1
        abs_max[ABS_Y] = args.height - 1
        abs_max[ABS_MT_SLOT] = 0
        abs_max[ABS_MT_POSITION_X] = args.width - 1
        abs_max[ABS_MT_POSITION_Y] = args.height - 1
        abs_max[ABS_MT_TRACKING_ID] = 65535
        abs_min[ABS_MT_TRACKING_ID] = -1
        descriptor = struct.pack(
            "80sHHHHi" + "i" * 256,
            b"rocgalgame-test-touch",
            BUS_USB,
            0x524F,
            0x4347,
            1,
            0,
            *(abs_max + abs_min + abs_fuzz + abs_flat),
        )
        os.write(fd, descriptor)
        fcntl.ioctl(fd, UI_DEV_CREATE)
        time.sleep(args.settle)

        emit(fd, EV_ABS, ABS_MT_SLOT, 0)
        emit(fd, EV_ABS, ABS_MT_TRACKING_ID, 1)
        emit(fd, EV_ABS, ABS_MT_POSITION_X, x)
        emit(fd, EV_ABS, ABS_MT_POSITION_Y, y)
        emit(fd, EV_ABS, ABS_X, x)
        emit(fd, EV_ABS, ABS_Y, y)
        emit(fd, EV_KEY, BTN_TOUCH, 1)
        sync(fd)
        time.sleep(args.hold)
        emit(fd, EV_ABS, ABS_MT_SLOT, 0)
        emit(fd, EV_ABS, ABS_MT_TRACKING_ID, -1)
        emit(fd, EV_KEY, BTN_TOUCH, 0)
        sync(fd)
        time.sleep(0.25)
    finally:
        try:
            fcntl.ioctl(fd, UI_DEV_DESTROY)
        finally:
            os.close(fd)


if __name__ == "__main__":
    main()
