#!/usr/bin/env python3
import pathlib
import re
import sys
import zipfile


EXPECTED_PACK = "roms/ports/ROCgalgame/ui.pack"
EXPECTED_KRKR2 = "roms/ports/ROCgalgame/cores/krkr/krkr2"
EXPECTED_KRKR2_RESOURCES = "roms/ports/ROCgalgame/cores/krkr/Resources/"
EXPECTED_KRKR2_GL = "roms/ports/ROCgalgame/cores/krkr/lib_krkr2/libGL.so.1"


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
        debug_cores = [
            name
            for name in names
            if name.startswith("roms/ports/ROCgalgame/cores/krkr/")
            and "debug" in pathlib.PurePosixPath(name).name.lower()
        ]
        if debug_cores:
            print(f"[package] ERROR: debug core leaked into archive: {debug_cores[0]}", file=sys.stderr)
            return 1
        plaintext_ui = [
            name
            for name in names
            if name.startswith("roms/ports/ROCgalgame/ui/")
        ]
        if plaintext_ui:
            print(f"[package] ERROR: plaintext UI leaked into archive: {plaintext_ui[0]}", file=sys.stderr)
            return 1
        installed_version = archive.read("roms/ports/ROCgalgame/version.txt").decode("utf-8").strip()
        if installed_version != version_match.group(1):
            print(
                f"[package] ERROR: version.txt is {installed_version}, expected {version_match.group(1)}",
                file=sys.stderr,
            )
            return 1

    print(f"[package] verified encrypted UI-only archive: {archive_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
