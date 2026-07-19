#!/usr/bin/env python3
import pathlib
import re
import sys
import zipfile


EXPECTED_PACK = "roms/ports/ROCgalgame/ui.pack"


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
        plaintext_ui = [name for name in names if "/ui/" in "/" + name.strip("/") + "/"]
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
