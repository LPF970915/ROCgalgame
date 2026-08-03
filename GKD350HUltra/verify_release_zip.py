#!/usr/bin/env python3
import hashlib
import json
import pathlib
import re
import sys
import zipfile


APP_ROOT = "app/ROCgalgame"
EXPECTED_PACK = f"{APP_ROOT}/ui.pack"
EXPECTED_ONS = f"{APP_ROOT}/cores/ons/onsyuri"
EXPECTED_KRKRSDL2 = f"{APP_ROOT}/cores/krkr/krkrsdl2"
EXPECTED_KRKR2 = f"{APP_ROOT}/cores/krkr/krkr2"
EXPECTED_KRKR2_RESOURCES = f"{APP_ROOT}/cores/krkr/Resources/"
EXPECTED_KRKR2_GL = f"{APP_ROOT}/cores/krkr/lib_krkr2/libGL.so.1"
CORE_HASH_MANIFEST = pathlib.Path(__file__).with_name("release_core_hashes.sha256")
EMPTY_RUNTIME_DIRS = ("games", "covers", "saves", "cache")


def load_expected_core_hashes() -> dict[str, str]:
    hashes: dict[str, str] = {}
    for line_number, raw_line in enumerate(CORE_HASH_MANIFEST.read_text(encoding="ascii").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split(maxsplit=1)
        if len(fields) != 2 or not re.fullmatch(r"[0-9a-fA-F]{64}", fields[0]):
            raise ValueError(f"invalid core hash manifest line {line_number}")
        archive_name = f"app/{fields[1].lstrip('*')}"
        hashes[archive_name] = fields[0].lower()
    required_paths = {EXPECTED_ONS, EXPECTED_KRKRSDL2, EXPECTED_KRKR2, EXPECTED_KRKR2_GL}
    missing_paths = sorted(required_paths - hashes.keys())
    if missing_paths:
        raise ValueError(f"core hash manifest is missing {missing_paths[0]}")
    return hashes


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_release_zip.py <release.zip>", file=sys.stderr)
        return 2

    archive_path = pathlib.Path(sys.argv[1])
    try:
        expected_core_hashes = load_expected_core_hashes()
    except (OSError, ValueError) as error:
        print(f"[package] ERROR: unable to load core hash manifest: {error}", file=sys.stderr)
        return 1
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
        for core_path, expected_hash in expected_core_hashes.items():
            if core_path not in names:
                print(f"[package] ERROR: missing manifest artifact: {core_path}", file=sys.stderr)
                return 1
            actual_hash = hashlib.sha256(archive.read(core_path)).hexdigest()
            if actual_hash != expected_hash:
                print(f"[package] ERROR: manifest hash mismatch: {core_path}", file=sys.stderr)
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
