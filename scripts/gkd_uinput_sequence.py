#!/usr/bin/env python3
import argparse
import fcntl
import os
import struct
import sys
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
    return (
        (direction << IOC_DIRSHIFT)
        | (ioctl_type << IOC_TYPESHIFT)
        | (number << IOC_NRSHIFT)
        | (size << IOC_SIZESHIFT)
    )


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
BTN_NORTH = 307
BTN_WEST = 308
BTN_TL = 310
BTN_TR = 311
BTN_TL2 = 312
BTN_TR2 = 313
BTN_SELECT = 314
BTN_START = 315
BTN_MODE = 316
BTN_DPAD_UP = 544
BTN_DPAD_DOWN = 545
BTN_DPAD_LEFT = 546
BTN_DPAD_RIGHT = 547

AXIS_MAX = 900

BUTTONS = {
    "a": BTN_EAST,
    "b": BTN_SOUTH,
    "x": BTN_NORTH,
    "y": BTN_WEST,
    "l1": BTN_TL,
    "r1": BTN_TR,
    "l2": BTN_TL2,
    "r2": BTN_TR2,
    "select": BTN_SELECT,
    "start": BTN_START,
    "menu": BTN_MODE,
    "up": BTN_DPAD_UP,
    "down": BTN_DPAD_DOWN,
    "left": BTN_DPAD_LEFT,
    "right": BTN_DPAD_RIGHT,
}


def emit(fd, event_type, code, value):
    os.write(fd, struct.pack("llHHi", 0, 0, event_type, code, value))


def sync(fd):
    emit(fd, EV_SYN, SYN_REPORT, 0)


def set_axis(fd, x, y):
    emit(fd, EV_ABS, ABS_X, round(max(-1.0, min(1.0, x)) * AXIS_MAX))
    emit(fd, EV_ABS, ABS_Y, round(max(-1.0, min(1.0, y)) * AXIS_MAX))
    sync(fd)


def set_button(fd, name, down):
    code = BUTTONS.get(name.lower())
    if code is None:
        raise ValueError(f"unknown button: {name}")
    emit(fd, EV_KEY, code, 1 if down else 0)


def tap(fd, name, duration):
    set_button(fd, name, True)
    sync(fd)
    time.sleep(duration)
    set_button(fd, name, False)
    sync(fd)


def hold(fd, names, duration):
    parsed = [name.strip().lower() for name in names.split("+") if name.strip()]
    for name in parsed:
        set_button(fd, name, True)
    sync(fd)
    time.sleep(duration)
    for name in reversed(parsed):
        set_button(fd, name, False)
    sync(fd)


def run_action(fd, action, default_tap):
    parts = [part.strip() for part in action.split(":")]
    if not parts or not parts[0]:
        return
    op = parts[0].lower()
    if op == "sleep":
        time.sleep(float(parts[1]))
    elif op == "tap":
        duration = float(parts[2]) if len(parts) > 2 and parts[2] else default_tap
        tap(fd, parts[1], duration)
    elif op == "hold":
        hold(fd, parts[1], float(parts[2]))
    elif op == "axis":
        set_axis(fd, float(parts[1]), float(parts[2]))
        time.sleep(float(parts[3]))
        set_axis(fd, 0.0, 0.0)
    else:
        raise ValueError(f"unknown action: {action}")


def create_device(fd, ready_file, device_name):
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
    for code in sorted(set(BUTTONS.values())):
        fcntl.ioctl(fd, UI_SET_KEYBIT, code)
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
        device_name.encode("utf-8")[:79],
        0x03,
        0x1209,
        0x3500,
        1,
        0,
        *(abs_max + abs_min + abs_fuzz + abs_flat),
    )
    os.write(fd, descriptor)
    fcntl.ioctl(fd, UI_DEV_CREATE)
    if ready_file:
        with open(ready_file, "w", encoding="utf-8") as out:
            out.write("ready\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ready-file")
    parser.add_argument("--device-name", default="gkd_atom_joypad_rocgalgame_probe")
    parser.add_argument("--sequence", required=True)
    parser.add_argument("--tap-seconds", type=float, default=0.12)
    parser.add_argument("--tail-seconds", type=float, default=0.0)
    args = parser.parse_args()

    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    try:
        create_device(fd, args.ready_file, args.device_name)
        time.sleep(0.25)
        set_axis(fd, 0.0, 0.0)
        for raw_action in args.sequence.split(","):
            run_action(fd, raw_action.strip(), args.tap_seconds)
        if args.tail_seconds > 0:
            time.sleep(args.tail_seconds)
    finally:
        try:
            set_axis(fd, 0.0, 0.0)
            fcntl.ioctl(fd, UI_DEV_DESTROY)
        finally:
            os.close(fd)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"[uinput_sequence] {exc}", file=sys.stderr)
        sys.exit(1)
