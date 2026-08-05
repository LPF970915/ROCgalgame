#!/usr/bin/env python3
"""Validate imported KRKR2 objects and rebuild their static archives."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run(*command: str, cwd: Path | None = None) -> str:
    return subprocess.check_output(command, cwd=cwd, text=True).strip()


def validate_source_delta(source: Path, old_commit: str, new_commit: str) -> list[str]:
    head = run("git", "-c", f"safe.directory={source}", "-C", str(source), "rev-parse", "HEAD")
    if head != new_commit:
        raise RuntimeError(f"source HEAD mismatch: expected {new_commit}, found {head}")
    changed = run(
        "git", "-c", f"safe.directory={source}", "-C", str(source),
        "diff", "--name-only", f"{old_commit}..{new_commit}",
    ).splitlines()
    invalid = [path for path in changed if Path(path).name != "CMakeLists.txt"]
    if invalid:
        raise RuntimeError(f"source changes invalidate cached objects: {invalid[:10]}")
    return changed


def restore_objects(build: Path, reference: Path, manifest: dict[str, object]) -> None:
    for record in manifest["objects"]:
        relative = Path(record["path"])
        source = reference / relative
        target = build / relative
        if not source.is_file() or sha256(source) != record["sha256"]:
            raise RuntimeError(f"reference object hash mismatch: {source}")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        os.utime(target, None)


def validate_objects(build: Path, manifest: dict[str, object]) -> set[Path]:
    verified: set[Path] = set()
    for record in manifest["objects"]:
        path = build / record["path"]
        if not path.is_file():
            raise RuntimeError(f"imported object is missing: {path}")
        if sha256(path) != record["sha256"]:
            raise RuntimeError(f"imported object hash mismatch: {path}")
        verified.add(path.resolve())
    return verified


def refresh_generated_objects(build: Path, reference: Path, verified: set[Path]) -> list[str]:
    generated = ("tjs.tab", "tjsdate.tab", "tjspp.tab")
    refreshed: list[str] = []
    for stem in generated:
        for suffix in ("cpp", "hpp"):
            relative = Path("cpp/core/tjs2/gen") / f"{stem}.{suffix}"
            current = build / relative
            old = reference / relative
            if not current.is_file() or not old.is_file() or sha256(current) != sha256(old):
                raise RuntimeError(f"generated source changed: {relative}")
        relative_object = Path("cpp/core/tjs2/CMakeFiles/tjs2.dir/gen") / f"{stem}.cpp.o"
        obj = (build / relative_object).resolve()
        if obj not in verified:
            raise RuntimeError(f"generated object is not in verified manifest: {relative_object}")
        os.utime(obj, None)
        depfile = obj.with_name(obj.name + ".d")
        if depfile.is_file():
            os.utime(depfile, None)
        refreshed.append(relative_object.as_posix())
    return refreshed


def rebuild_archives(build: Path, verified: set[Path]) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    for link_file in sorted(build.rglob("link.txt")):
        lines = [line.strip() for line in link_file.read_text(encoding="utf-8").splitlines() if line.strip()]
        if not lines or "-gnu-ar" not in lines[0]:
            continue
        tokens = shlex.split(lines[0])
        if len(tokens) < 4 or tokens[1] != "qc":
            raise RuntimeError(f"unexpected archive command: {link_file}")
        target_directory = link_file.parents[2]
        archive = target_directory / tokens[2]
        members = [(target_directory / token).resolve() for token in tokens[3:] if token.endswith(".o")]
        unverified = [str(path) for path in members if path not in verified]
        if unverified:
            raise RuntimeError(f"archive references unverified objects: {unverified[:10]}")
        archive.unlink(missing_ok=True)
        for line in lines:
            subprocess.run(shlex.split(line), cwd=target_directory, check=True)
        records.append({
            "path": archive.relative_to(build).as_posix(),
            "sha256": sha256(archive),
            "member_count": len(members),
        })
    if len(records) != 25:
        raise RuntimeError(f"archive count mismatch: expected 25, rebuilt {len(records)}")
    return records


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--reference-build", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--current-commit", required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--restore-objects", action="store_true")
    args = parser.parse_args()

    build = args.build.resolve()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    old_commit = manifest["source_commit"]
    changed = validate_source_delta(args.source.resolve(), old_commit, args.current_commit)
    if args.restore_objects:
        restore_objects(build, args.reference_build.resolve(), manifest)
    verified = validate_objects(build, manifest)
    for path in verified:
        os.utime(path, None)
    refreshed = refresh_generated_objects(build, args.reference_build.resolve(), verified)
    archives = rebuild_archives(build, verified)
    result = {
        "schema": 1,
        "object_source_commit": old_commit,
        "current_source_commit": args.current_commit,
        "source_delta": changed,
        "verified_object_count": len(verified),
        "refreshed_generated_objects": refreshed,
        "archives": archives,
    }
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"[krkr2_object_cache] verified={len(verified)} archives={len(archives)} output={args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
