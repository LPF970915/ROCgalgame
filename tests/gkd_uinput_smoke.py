#!/usr/bin/env python3
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
BTN_MODE = 316
BTN_DPAD_RIGHT = 547


def emit(fd, event_type, code, value):
    os.write(fd, struct.pack("llHHi", 0, 0, event_type, code, value))


def sync(fd):
    emit(fd, EV_SYN, SYN_REPORT, 0)


fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
try:
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
    fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_MODE)
    fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT)
    fcntl.ioctl(fd, UI_SET_ABSBIT, ABS_X)
    fcntl.ioctl(fd, UI_SET_ABSBIT, ABS_Y)

    abs_max = [0] * 64
    abs_min = [0] * 64
    abs_fuzz = [0] * 64
    abs_flat = [0] * 64
    for axis in (ABS_X, ABS_Y):
        abs_max[axis] = 32767
        abs_min[axis] = -32768
        abs_flat[axis] = 4096
    descriptor = struct.pack(
        "80sHHHHi" + "i" * 256,
        b"gkd_atom_joypad_smoke",
        0x03,
        0x1209,
        0x3500,
        1,
        0,
        *(abs_max + abs_min + abs_fuzz + abs_flat),
    )
    os.write(fd, descriptor)
    fcntl.ioctl(fd, UI_DEV_CREATE)
    print("uinput-ready", flush=True)
    time.sleep(1.2)

    for axis in (ABS_X, ABS_Y):
        emit(fd, EV_ABS, axis, 32767)
        sync(fd)
        time.sleep(0.12)
        emit(fd, EV_ABS, axis, 0)
        sync(fd)
        time.sleep(0.12)

    emit(fd, EV_KEY, BTN_DPAD_RIGHT, 1)
    sync(fd)
    time.sleep(0.12)
    emit(fd, EV_KEY, BTN_DPAD_RIGHT, 0)
    sync(fd)
    time.sleep(0.12)

    emit(fd, EV_KEY, BTN_MODE, 1)
    sync(fd)
    time.sleep(0.12)
    emit(fd, EV_KEY, BTN_MODE, 0)
    sync(fd)
    print("uinput-events-sent", flush=True)
    time.sleep(1.0)
finally:
    try:
        fcntl.ioctl(fd, UI_DEV_DESTROY)
    finally:
        os.close(fd)
