#!/usr/bin/env python3
import fcntl
import glob
import os
import re
import signal
import struct
import subprocess
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
    return ((direction << IOC_DIRSHIFT) | (ioctl_type << IOC_TYPESHIFT) |
            (number << IOC_NRSHIFT) | (size << IOC_SIZESHIFT))


def iow(ioctl_type, number):
    return ioctl_code(IOC_WRITE, ioctl_type, number, struct.calcsize("i"))


UI_TYPE = ord("U")
UI_DEV_CREATE = ioctl_code(0, UI_TYPE, 1)
UI_DEV_DESTROY = ioctl_code(0, UI_TYPE, 2)
UI_SET_EVBIT = iow(UI_TYPE, 100)
UI_SET_KEYBIT = iow(UI_TYPE, 101)
EV_SYN = 0
EV_KEY = 1
SYN_REPORT = 0
KEY_VOLUMEDOWN = 114
KEY_VOLUMEUP = 115
BTN_EAST = 305
BTN_SOUTH = 304
BTN_MODE = 316
BTN_DPAD_DOWN = 545
BTN_DPAD_LEFT = 546
BTN_DPAD_RIGHT = 547


def emit(fd, event_type, code, value):
    os.write(fd, struct.pack("llHHi", 0, 0, event_type, code, value))


def press(fd, code, settle=0.25):
    emit(fd, EV_KEY, code, 1)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(0.12)
    emit(fd, EV_KEY, code, 0)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(settle)


def master_volume_percent():
    output = subprocess.check_output(["amixer", "sget", "Master"], text=True)
    values = [int(value) for value in re.findall(r"\[(\d+)%\]", output)]
    if not values:
        raise RuntimeError("Master volume percentage unavailable")
    return values[0]


def brightness_values():
    values = {}
    for path in glob.glob("/sys/class/backlight/*/brightness"):
        try:
            with open(path, "r", encoding="ascii") as stream:
                values[path] = int(stream.read().strip())
        except (OSError, ValueError):
            pass
    return values


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: gkd_system_controls_smoke.py APP_BINARY TEST_ROOT")
    binary, test_root = sys.argv[1:]
    config_path = os.path.join(test_root, "native_config.ini")
    with open(config_path, "r", encoding="utf-8") as stream:
        config_text = stream.read()
    config_text = re.sub(r"(?m)^system_volume_percent=.*$",
                         "system_volume_percent=50", config_text)
    config_text = re.sub(r"(?m)^brightness_level=.*$",
                         "brightness_level=4", config_text)
    with open(config_path, "w", encoding="utf-8") as stream:
        stream.write(config_text)
    env = os.environ.copy()
    env.update({
        "ROCGALGAME_ROOT": test_root,
        "ROCGALGAME_SCREEN_PROFILE": "1600x1440",
        "ROCGALGAME_DEVICE_MODEL": "gkd350h-ultra",
        "ROCREADER_SYSTEM_VOLUME_LEVELS": "20",
        "ROCREADER_FULL_INPUT_LOG": "1",
        "ROCREADER_SYSTEM_CONTROL_LOG": "1",
        "SDL_AUDIODRIVER": "alsa",
        "SDL_NOMOUSE": "1",
        "SDL_VIDEODRIVER": "wayland",
        "XDG_RUNTIME_DIR": "/var/run/0-runtime-dir",
        "WAYLAND_DISPLAY": "wayland-1",
        "LD_LIBRARY_PATH": os.path.join(os.path.dirname(binary), "lib_system_sdl") +
                           ":/usr/lib32:/usr/lib:/lib:/mnt/vendor/lib",
    })

    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    process = None
    original_volume = master_volume_percent()
    original_brightness = brightness_values()
    log_path = "/tmp/rocgalgame-stage4-system-controls.log"
    try:
        fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
        for code in (KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_EAST, BTN_SOUTH, BTN_MODE,
                     BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT):
            fcntl.ioctl(fd, UI_SET_KEYBIT, code)
        descriptor = struct.pack(
            "80sHHHHi" + "i" * 256,
            b"gkd_atom_joypad_system_smoke", 0x03, 0x1209, 0x3501, 1, 0,
            *([0] * 256),
        )
        os.write(fd, descriptor)
        fcntl.ioctl(fd, UI_DEV_CREATE)
        time.sleep(0.8)

        log = open(log_path, "w", encoding="utf-8")
        process = subprocess.Popen(
            ["stdbuf", "-oL", "-eL", binary], cwd=test_root, env=env, stdout=log,
            stderr=subprocess.STDOUT, start_new_session=True,
        )
        time.sleep(8.0)
        if process.poll() is not None:
            raise RuntimeError("frontend exited before input smoke test")

        volume_initial = master_volume_percent()
        press(fd, KEY_VOLUMEUP, 5.0)
        volume_physical_up = master_volume_percent()
        press(fd, KEY_VOLUMEDOWN, 5.0)
        volume_physical_restored = master_volume_percent()

        press(fd, BTN_MODE, 1.0)
        press(fd, BTN_DPAD_DOWN, 1.0)
        press(fd, BTN_DPAD_DOWN, 1.0)
        press(fd, BTN_EAST, 1.0)
        press(fd, BTN_DPAD_RIGHT, 5.0)
        volume_menu_up = master_volume_percent()
        press(fd, BTN_DPAD_LEFT, 5.0)
        volume_menu_restored = master_volume_percent()

        press(fd, BTN_DPAD_DOWN, 1.0)
        brightness_initial = brightness_values()
        press(fd, BTN_DPAD_LEFT, 5.0)
        brightness_down = brightness_values()
        press(fd, BTN_DPAD_RIGHT, 5.0)
        brightness_restored = brightness_values()

        press(fd, BTN_SOUTH, 1.0)
        for _ in range(5):
            press(fd, BTN_DPAD_DOWN, 0.5)
        press(fd, BTN_EAST, 1.0)
        process.wait(timeout=5)

        print("volume_initial={}".format(volume_initial), flush=True)
        print("volume_physical_up={}".format(volume_physical_up), flush=True)
        print("volume_physical_restored={}".format(volume_physical_restored), flush=True)
        print("volume_menu_up={}".format(volume_menu_up), flush=True)
        print("volume_menu_restored={}".format(volume_menu_restored), flush=True)
        print("brightness_initial={}".format(brightness_initial), flush=True)
        print("brightness_down={}".format(brightness_down), flush=True)
        print("brightness_restored={}".format(brightness_restored), flush=True)

        if volume_physical_up <= volume_initial:
            raise RuntimeError("physical Volume+ did not increase Master")
        if volume_physical_restored >= volume_physical_up:
            raise RuntimeError("physical Volume- did not reverse Volume+")
        if volume_menu_up <= volume_menu_restored:
            raise RuntimeError("System Settings volume controls did not form one sequence")
        if brightness_initial and brightness_down == brightness_initial:
            raise RuntimeError("System Settings brightness decrement had no hardware effect")
        if brightness_initial and brightness_restored != brightness_initial:
            raise RuntimeError("System Settings brightness did not restore hardware state")

        print("gkd system controls smoke passed")
    finally:
        if process is not None and process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait(timeout=3)
        try:
            fcntl.ioctl(fd, UI_DEV_DESTROY)
        finally:
            os.close(fd)
        subprocess.run(["amixer", "-q", "sset", "Master",
                        "{}%".format(original_volume), "unmute"], check=False)
        for path, value in original_brightness.items():
            try:
                with open(path, "w", encoding="ascii") as stream:
                    stream.write(str(value))
            except OSError:
                pass


if __name__ == "__main__":
    main()
