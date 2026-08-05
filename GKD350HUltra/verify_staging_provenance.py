#!/usr/bin/env python3
import hashlib
import pathlib
import re
import sys


def read_fields(path: pathlib.Path) -> dict[str, str]:
    if not path.is_file():
        raise ValueError(f"missing metadata or lock file: {path}")
    fields: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f"invalid field in {path}: {line}")
        key, value = line.split("=", 1)
        fields[key] = value
    return fields


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_commit(label: str, value: str) -> None:
    if not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError(f"{label} is not a full Git commit: {value}")


def verify_core(
    label: str,
    binary: pathlib.Path,
    metadata: pathlib.Path,
    lock: pathlib.Path,
) -> tuple[str, str]:
    if not binary.is_file():
        raise ValueError(f"missing {label} binary: {binary}")
    meta = read_fields(metadata)
    locked = read_fields(lock)
    source_commit = meta.get("source_commit", "")
    locked_commit = locked.get("source_commit", "")
    require_commit(f"{label} source_commit", source_commit)
    if source_commit != locked_commit:
        raise ValueError(
            f"{label} source commit {source_commit} does not match lock {locked_commit}"
        )
    if meta.get("source_dirty") != "0":
        raise ValueError(f"{label} metadata is missing source_dirty=0")
    actual_hash = sha256(binary)
    recorded_hash = meta.get("artifact_sha256") or meta.get("binary_sha256")
    if recorded_hash != actual_hash:
        raise ValueError(
            f"{label} artifact hash {actual_hash} does not match metadata {recorded_hash}"
        )
    print(f"[provenance] {label} commit={source_commit} sha256={actual_hash}")
    return source_commit, actual_hash


def main() -> int:
    if len(sys.argv) not in (1, 2):
        print("usage: verify_staging_provenance.py [dist-root]", file=sys.stderr)
        return 2
    script_dir = pathlib.Path(__file__).resolve().parent
    dist_root = (
        pathlib.Path(sys.argv[1]).resolve()
        if len(sys.argv) == 2
        else script_dir / "dist_glibc234"
    )
    runtime = dist_root / "ROCgalgame"
    try:
        verify_core(
            "onsyuri",
            runtime / "cores/ons/onsyuri",
            runtime / "cores/ons/onsyuri.build-meta",
            script_dir / "onsyuri-port.lock",
        )
        krkr_meta = runtime / "cores/krkr/krkrsdl2.build-meta"
        verify_core(
            "krkrsdl2",
            runtime / "cores/krkr/krkrsdl2",
            krkr_meta,
            script_dir / "krkrsdl2-port.lock",
        )
        ffmpeg_commit = read_fields(krkr_meta).get("ffmpeg_headers_commit", "")
        locked_ffmpeg = read_fields(script_dir / "ffmpeg-headers.lock").get(
            "source_commit", ""
        )
        require_commit("ffmpeg_headers_commit", ffmpeg_commit)
        if ffmpeg_commit != locked_ffmpeg:
            raise ValueError(
                f"FFmpeg headers commit {ffmpeg_commit} does not match lock {locked_ffmpeg}"
            )
        verify_core(
            "krkr2",
            runtime / "cores/krkr/krkr2",
            runtime / "cores/krkr/krkr2.build-meta",
            script_dir / "krkr2-port.lock",
        )
    except (OSError, ValueError) as error:
        print(f"[provenance] ERROR: {error}", file=sys.stderr)
        return 1
    print(f"[provenance] staging verified: {runtime}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
