#!/usr/bin/env python3
import fcntl
import json
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
KEY_POWER = 116
BTN_DPAD_RIGHT = 547


def emit(fd, event_type, code, value):
    os.write(fd, struct.pack("llHHi", 0, 0, event_type, code, value))


def press(fd, code):
    emit(fd, EV_KEY, code, 1)
    emit(fd, EV_SYN, SYN_REPORT, 0)
    time.sleep(0.15)
    emit(fd, EV_KEY, code, 0)
    emit(fd, EV_SYN, SYN_REPORT, 0)


def sway_env():
    env = os.environ.copy()
    env.update({
        "XDG_RUNTIME_DIR": "/var/run/0-runtime-dir",
        "WAYLAND_DISPLAY": "wayland-1",
        "SWAYSOCK": "/var/run/0-runtime-dir/sway-ipc.0.sock",
    })
    return env


def output_power():
    output = subprocess.check_output(
        ["swaymsg", "-t", "get_outputs", "-r"], env=sway_env(), text=True)
    for item in json.loads(output):
        if item.get("name") == "DSI-1":
            return bool(item.get("power", item.get("dpms", item.get("active", False))))
    raise RuntimeError("DSI-1 output unavailable")


def force_output_on():
    subprocess.run(["swaymsg", "output", "DSI-1", "power", "on"],
                   env=sway_env(), check=False, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)


def current_volume_percent():
    output = subprocess.check_output(["amixer", "sget", "Master"], text=True)
    values = re.findall(r"\[(\d+)%\]", output)
    return int(values[0]) if values else 50


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: gkd_power_lifecycle_smoke.py APP_BINARY TEST_ROOT")
    binary, test_root = sys.argv[1:]
    config_path = os.path.join(test_root, "native_config.ini")
    with open(config_path, "r", encoding="utf-8") as stream:
        config_text = stream.read()
    config_text = re.sub(r"(?m)^system_volume_percent=.*$",
                         "system_volume_percent={}".format(current_volume_percent()),
                         config_text)
    config_text = re.sub(r"(?m)^lid_close_screen_off=.*$",
                         "lid_close_screen_off=1", config_text)
    config_text = re.sub(r"(?m)^auto_sleep_interval_index=.*$",
                         "auto_sleep_interval_index=0", config_text)
    with open(config_path, "w", encoding="utf-8") as stream:
        stream.write(config_text)

    app_dir = os.path.dirname(binary)
    env = sway_env()
    env.update({
        "ROCGALGAME_ROOT": test_root,
        "ROCGALGAME_SCREEN_PROFILE": "1600x1440",
        "ROCGALGAME_DEVICE_MODEL": "gkd350h-ultra",
        "ROCREADER_SYSTEM_VOLUME_LEVELS": "20",
        "ROCREADER_FULL_INPUT_LOG": "1",
        "SDL_AUDIODRIVER": "alsa",
        "SDL_NOMOUSE": "1",
        "SDL_VIDEODRIVER": "wayland",
        "LD_LIBRARY_PATH": os.path.join(app_dir, "lib_system_sdl") +
                           ":/usr/lib32:/usr/lib:/lib:/mnt/vendor/lib",
    })

    force_output_on()
    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    process = None
    try:
        fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
        fcntl.ioctl(fd, UI_SET_KEYBIT, KEY_POWER)
        fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT)
        descriptor = struct.pack(
            "80sHHHHi" + "i" * 256,
            b"gkd_atom_joypad_power_smoke", 0x03, 0x1209, 0x3502, 1, 0,
            *([0] * 256),
        )
        os.write(fd, descriptor)
        fcntl.ioctl(fd, UI_DEV_CREATE)
        time.sleep(0.8)

        log = open("/tmp/rocgalgame-stage4-power.log", "w", encoding="utf-8")
        process = subprocess.Popen(
            ["stdbuf", "-oL", "-eL", binary], cwd=test_root, env=env,
            stdout=log, stderr=subprocess.STDOUT, start_new_session=True,
        )
        time.sleep(8.0)
        if process.poll() is not None or not output_power():
            raise RuntimeError("frontend did not reach awake state")

        for cycle in range(3):
            press(fd, KEY_POWER)
            time.sleep(2.0)
            if output_power():
                raise RuntimeError("manual screen off failed at cycle {}".format(cycle + 1))
            press(fd, KEY_POWER)
            time.sleep(2.5)
            if not output_power():
                raise RuntimeError("manual wake failed at cycle {}".format(cycle + 1))

        time.sleep(31.0)
        if output_power():
            raise RuntimeError("30 second auto sleep did not turn output off")
        press(fd, BTN_DPAD_RIGHT)
        time.sleep(2.0)
        if not output_power():
            raise RuntimeError("non-power input did not wake auto sleep")
        if process.poll() is not None:
            raise RuntimeError("frontend exited during power lifecycle test")
        print("manual_cycles=3")
        print("auto_sleep_seconds=30")
        print("non_power_wake=passed")
        print("gkd power lifecycle smoke passed")
    finally:
        force_output_on()
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
        force_output_on()


if __name__ == "__main__":
    main()
