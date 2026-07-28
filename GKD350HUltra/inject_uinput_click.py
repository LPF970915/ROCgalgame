#!/usr/bin/env python3
import fcntl
import os
import struct
import sys
import time


IOC_WRITE = 1
EV_SYN = 0x00
EV_KEY = 0x01
EV_REL = 0x02
SYN_REPORT = 0
BTN_LEFT = 0x110
BTN_RIGHT = 0x111
REL_X = 0x00
REL_Y = 0x01
BUS_USB = 0x03


def ioc(direction, type_char, number, size):
    return (direction << 30) | (ord(type_char) << 8) | number | (size << 16)


def iow(type_char, number, size):
    return ioc(IOC_WRITE, type_char, number, size)


UI_DEV_CREATE = ioc(0, "U", 1, 0)
UI_DEV_DESTROY = ioc(0, "U", 2, 0)
UI_DEV_SETUP = iow("U", 3, 92)
UI_SET_EVBIT = iow("U", 100, 4)
UI_SET_KEYBIT = iow("U", 101, 4)
UI_SET_RELBIT = iow("U", 102, 4)


def emit(fd, event_type, code, value):
    os.write(fd, struct.pack("llHHi", 0, 0, event_type, code, value))


fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
try:
    button = BTN_RIGHT if len(sys.argv) > 1 and sys.argv[1] == "right" else BTN_LEFT
    for event_type in (EV_KEY, EV_REL):
        fcntl.ioctl(fd, UI_SET_EVBIT, event_type)
    fcntl.ioctl(fd, UI_SET_KEYBIT, button)
    fcntl.ioctl(fd, UI_SET_RELBIT, REL_X)
    fcntl.ioctl(fd, UI_SET_RELBIT, REL_Y)

    name = b"rocgalgame-krkr2-probe-mouse"
    setup = struct.pack("HHHH80sI", BUS_USB, 0x524F, 0x4347, 1, name, 0)
    fcntl.ioctl(fd, UI_DEV_SETUP, setup)
    fcntl.ioctl(fd, UI_DEV_CREATE)
    time.sleep(1.0)

    emit(fd, EV_KEY, button, 1)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(0.15)
    emit(fd, EV_KEY, button, 0)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(0.5)
finally:
    try:
        fcntl.ioctl(fd, UI_DEV_DESTROY)
    finally:
        os.close(fd)
