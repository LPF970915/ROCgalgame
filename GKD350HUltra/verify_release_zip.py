#!/usr/bin/env python3
import hashlib
import json
import pathlib
import re
import sys
import zipfile


APP_ROOT = "app/ROCgalgame"
EXPECTED_PACK = f"{APP_ROOT}/ui.pack"
EXPECTED_KRKRSDL2 = f"{APP_ROOT}/cores/krkr/krkrsdl2"
EXPECTED_KRKR2 = f"{APP_ROOT}/cores/krkr/krkr2"
EXPECTED_KRKR2_RESOURCES = f"{APP_ROOT}/cores/krkr/Resources/"
EXPECTED_KRKR2_GL = f"{APP_ROOT}/cores/krkr/lib_krkr2/libGL.so.1"
EXPECTED_KRKRSDL2_SHA256 = "cfff05da86ced8e8530f2cc08c478418dd2b2cd7a746e6ed161a15c6352efc3f"
EXPECTED_KRKR2_SHA256 = "4c633112890401d233ef3eae92c4472f171c7f3f370a8b91116394bf89b8f6c8"
EXPECTED_KRKR2_GL_SHA256 = "0e1d74952d5edcfd023c214a19f280a5248a256cbc179fbee2285b50bc3ec918"
EMPTY_RUNTIME_DIRS = ("games", "covers", "saves", "cache")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_release_zip.py <release.zip>", file=sys.stderr)
        return 2

    archive_path = pathlib.Path(sys.argv[1])
    version_match = re.match(
        r"^ROCgalgame ver([0-9]+\.[0-9]{2}) for GKD350H Ultra\.zip$",
        archive_path.name,
    )
    if not version_match:
        print(f"[package] ERROR: invalid release filename: {archive_path.name}", file=sys.stderr)
        return 1
    with zipfile.ZipFile(archive_path, "r") as archive:
        names = archive.namelist()
        if EXPECTED_PACK not in names:
            print(f"[package] ERROR: missing encrypted UI pack: {EXPECTED_PACK}", file=sys.stderr)
            return 1
        for required in (
            f"{APP_ROOT}/config.json",
            f"{APP_ROOT}/launch.sh",
            f"{APP_ROOT}/rocgalgame.png",
        ):
            if required not in names:
                print(f"[package] ERROR: missing IUX app file: {required}", file=sys.stderr)
                return 1
        expected_icon = (pathlib.Path(__file__).parent.parent / "ui" / "common" / "icon.png").read_bytes()
        packaged_icon = archive.read(f"{APP_ROOT}/rocgalgame.png")
        if packaged_icon != expected_icon:
            print("[package] ERROR: packaged IUX icon does not match ui/common/icon.png", file=sys.stderr)
            return 1
        if len(packaged_icon) < 26 or packaged_icon[:8] != b"\x89PNG\r\n\x1a\n" or packaged_icon[25] != 6:
            print("[package] ERROR: IUX icon must be an RGBA PNG (PNG color type 6)", file=sys.stderr)
            return 1
        if "roms/ports/ROCgalgame.sh" not in names:
            print("[package] ERROR: missing ES ports launcher", file=sys.stderr)
            return 1
        if any(name.startswith("roms/ports/ROCgalgame/") for name in names):
            print("[package] ERROR: duplicated ES runtime; ports must only contain the launcher", file=sys.stderr)
            return 1
        config = json.loads(archive.read(f"{APP_ROOT}/config.json").decode("utf-8"))
        expected_config = {
            "software_code": "rocgalgame",
            "title": "ROCgalgame",
            "exec": "launch.sh",
            "workdir": ".",
            "icon": "rocgalgame.png",
        }
        for key, expected in expected_config.items():
            if config.get(key) != expected:
                print(f"[package] ERROR: invalid config.json field {key}", file=sys.stderr)
                return 1
        if EXPECTED_KRKRSDL2 not in names:
            print(f"[package] ERROR: missing KRKRSDL2 core: {EXPECTED_KRKRSDL2}", file=sys.stderr)
            return 1
        if EXPECTED_KRKR2 not in names:
            print(f"[package] ERROR: missing KRKR2 core: {EXPECTED_KRKR2}", file=sys.stderr)
            return 1
        if not any(name.startswith(EXPECTED_KRKR2_RESOURCES) for name in names):
            print(
                f"[package] ERROR: missing KRKR2 resources: {EXPECTED_KRKR2_RESOURCES}",
                file=sys.stderr,
            )
            return 1
        if EXPECTED_KRKR2_GL not in names:
            print(f"[package] ERROR: missing KRKR2 private GLVND library: {EXPECTED_KRKR2_GL}", file=sys.stderr)
            return 1
        if hashlib.sha256(archive.read(EXPECTED_KRKRSDL2)).hexdigest() != EXPECTED_KRKRSDL2_SHA256:
            print("[package] ERROR: KRKRSDL2 core hash mismatch", file=sys.stderr)
            return 1
        if hashlib.sha256(archive.read(EXPECTED_KRKR2)).hexdigest() != EXPECTED_KRKR2_SHA256:
            print("[package] ERROR: KRKR2 core hash mismatch", file=sys.stderr)
            return 1
        if hashlib.sha256(archive.read(EXPECTED_KRKR2_GL)).hexdigest() != EXPECTED_KRKR2_GL_SHA256:
            print("[package] ERROR: KRKR2 private GLVND library hash mismatch", file=sys.stderr)
            return 1
        debug_cores = [
            name
            for name in names
            if name.startswith(f"{APP_ROOT}/cores/krkr/")
            and "debug" in pathlib.PurePosixPath(name).name.lower()
        ]
        if debug_cores:
            print(f"[package] ERROR: debug core leaked into archive: {debug_cores[0]}", file=sys.stderr)
            return 1
        unexpected_krkr2_variants = [
            name
            for name in names
            if pathlib.PurePosixPath(name).parent
            == pathlib.PurePosixPath(f"{APP_ROOT}/cores/krkr")
            and pathlib.PurePosixPath(name).name.startswith(("krkr2.", "krkr2-"))
        ]
        if unexpected_krkr2_variants:
            print(
                f"[package] ERROR: KRKR2 candidate/backup leaked into archive: {unexpected_krkr2_variants[0]}",
                file=sys.stderr,
            )
            return 1
        plaintext_ui = [
            name
            for name in names
            if name.startswith(f"{APP_ROOT}/ui/")
        ]
        if plaintext_ui:
            print(f"[package] ERROR: plaintext UI leaked into archive: {plaintext_ui[0]}", file=sys.stderr)
            return 1
        for directory in EMPTY_RUNTIME_DIRS:
            prefix = f"{APP_ROOT}/{directory}/"
            files = [name for name in names if name.startswith(prefix) and name != prefix]
            if files:
                print(f"[package] ERROR: {directory} is not empty: {files[0]}", file=sys.stderr)
                return 1
        installed_version = archive.read(f"{APP_ROOT}/version.txt").decode("utf-8").strip()
        if installed_version != version_match.group(1):
            print(
                f"[package] ERROR: version.txt is {installed_version}, expected {version_match.group(1)}",
                file=sys.stderr,
            )
            return 1
        if config.get("version") != version_match.group(1):
            print(
                f"[package] ERROR: config.json version is {config.get('version')}, expected {version_match.group(1)}",
                file=sys.stderr,
            )
            return 1

    print(f"[package] verified encrypted UI-only archive: {archive_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
