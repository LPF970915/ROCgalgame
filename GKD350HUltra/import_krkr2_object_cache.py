#!/usr/bin/env python3
"""Import verified KRKR2 objects into a freshly configured fixed-path tree."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import shlex
import sys


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def normalized_flags(
    path: Path,
    old_source: str,
    new_source: str,
    old_workspace: str,
    new_workspace: str,
) -> str:
    value = path.read_text(encoding="utf-8")
    replacements = (
        (old_source, "${SOURCE}"),
        (new_source, "${SOURCE}"),
        (old_workspace, "${WORKSPACE}"),
        (new_workspace, "${WORKSPACE}"),
    )
    for source, target in replacements:
        value = value.replace(source, target)
    value = value.replace(
        "${WORKSPACE}/build/gkd350h-glibc234/krkr2/vcpkg_installed/"
        "arm64-linux-gkd-glibc234/lib/pkgconfig/../../include",
        "${SYSROOT}/usr/include",
    )
    value = value.replace(
        "${WORKSPACE}/build/gkd350h-glibc234/sysroot/usr/include",
        "${SYSROOT}/usr/include",
    )
    value = value.replace(
        "${WORKSPACE}/build/gkd350h-glibc234/krkr2/vcpkg_installed/"
        "arm64-linux-gkd-glibc234/lib/pkgconfig/../../lib",
        "${SYSROOT}/usr/lib/aarch64-linux-gnu",
    )
    value = value.replace(
        "${WORKSPACE}/build/gkd350h-glibc234/sysroot/usr/lib/aarch64-linux-gnu",
        "${SYSROOT}/usr/lib/aarch64-linux-gnu",
    )
    value = value.replace(
        "${WORKSPACE}/build/gkd350h-glibc234/krkr2/vcpkg_installed/"
        "arm64-linux-gkd-glibc234/include/freetype2",
        "${WORKSPACE}/build/gkd350h-glibc234/krkr2/vcpkg_installed/"
        "arm64-linux-gkd-glibc234/include",
    )

    normalized_lines: list[str] = []
    for line in value.splitlines():
        if line.startswith(("C_FLAGS = ", "CXX_FLAGS = ")):
            name, raw_flags = line.split(" = ", 1)
            tokens = [
                token
                for token in shlex.split(raw_flags)
                if token not in ("-pthread", "-DXML_STATIC")
            ]
            normalized_lines.append(f"{name} = {' '.join(tokens)}")
            continue
        if not line.startswith(("C_INCLUDES = ", "CXX_INCLUDES = ")):
            normalized_lines.append(line)
            continue
        name, raw_flags = line.split(" = ", 1)
        tokens = shlex.split(raw_flags)
        includes: set[str] = set()
        index = 0
        while index < len(tokens):
            token = tokens[index]
            if token == "-isystem" and index + 1 < len(tokens):
                includes.add(os.path.normpath(tokens[index + 1]))
                index += 2
            elif token.startswith("-I"):
                includes.add(os.path.normpath(token[2:]))
                index += 1
            else:
                raise RuntimeError(f"unexpected include flag in {path}: {token}")
        includes.discard("${SYSROOT}/usr/include")
        canonical = " ".join(sorted(includes))
        normalized_lines.append(f"{name} = {canonical}")
    return "\n".join(normalized_lines) + "\n"


def flags_file(object_path: Path, build_root: Path) -> Path:
    current = object_path.parent
    while current != build_root:
        if current.name.endswith(".dir"):
            candidate = current / "flags.make"
            if candidate.is_file():
                return candidate
        current = current.parent
    raise RuntimeError(f"cannot locate flags.make for {object_path}")


def rewrite_depfile(
    value: str,
    old_source: str,
    new_source: str,
    old_workspace: str,
    new_workspace: str,
) -> str:
    return (
        value.replace(old_source, new_source)
        .replace(old_workspace, new_workspace)
    )


def import_objects(args: argparse.Namespace) -> None:
    source_build = args.source_build.resolve()
    destination_build = args.destination_build.resolve()
    if source_build == destination_build:
        raise RuntimeError("source and destination build roots must differ")
    if not (destination_build / "CMakeCache.txt").is_file():
        raise RuntimeError("destination must be freshly configured before import")
    if any(destination_build.rglob("*.o")):
        raise RuntimeError("destination already contains object files")

    objects = sorted(
        path
        for path in source_build.rglob("*.o")
        if path.relative_to(source_build).parts[0] == "cpp"
    )
    if len(objects) != args.expected_count:
        raise RuntimeError(
            f"object cache count mismatch: expected {args.expected_count}, found {len(objects)}"
        )

    flags_verified: set[Path] = set()
    for source_object in objects:
        source_flags = flags_file(source_object, source_build)
        target_flags = destination_build / source_flags.relative_to(source_build)
        if not target_flags.is_file():
            raise RuntimeError(f"new build target has no flags file: {target_flags}")
        if source_flags not in flags_verified:
            old_flags = normalized_flags(
                source_flags,
                args.old_source,
                args.new_source,
                args.old_workspace,
                args.new_workspace,
            )
            new_flags = normalized_flags(
                target_flags,
                args.old_source,
                args.new_source,
                args.old_workspace,
                args.new_workspace,
            )
            if old_flags != new_flags:
                raise RuntimeError(
                    f"compile flags changed; refusing object import: "
                    f"{source_flags.relative_to(source_build)}"
                )
            flags_verified.add(source_flags)

    records: list[dict[str, object]] = []
    depfiles = 0
    for source_object in objects:
        relative = source_object.relative_to(source_build)
        target_object = destination_build / relative

        target_object.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_object, target_object)
        object_hash = sha256(target_object)
        os.utime(target_object, None)

        source_depfile = source_object.with_name(source_object.name + ".d")
        if source_depfile.is_file():
            target_depfile = target_object.with_name(target_object.name + ".d")
            rewritten = rewrite_depfile(
                source_depfile.read_text(encoding="utf-8"),
                args.old_source,
                args.new_source,
                args.old_workspace,
                args.new_workspace,
            )
            target_depfile.write_text(rewritten, encoding="utf-8", newline="\n")
            os.utime(target_depfile, None)
            depfiles += 1

        records.append(
            {
                "path": relative.as_posix(),
                "sha256": object_hash,
                "size": target_object.stat().st_size,
            }
        )

    manifest = {
        "schema": 1,
        "source_commit": args.source_commit,
        "object_count": len(records),
        "depfile_count": depfiles,
        "verified_flags_files": len(flags_verified),
        "source_build": str(source_build),
        "destination_build": str(destination_build),
        "objects": records,
    }
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    os.replace(temporary, output)
    print(
        f"[krkr2_object_cache] imported={len(records)} depfiles={depfiles} "
        f"flags={len(flags_verified)} manifest={output}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-build", type=Path, required=True)
    parser.add_argument("--destination-build", type=Path, required=True)
    parser.add_argument("--old-source", required=True)
    parser.add_argument("--new-source", required=True)
    parser.add_argument("--old-workspace", required=True)
    parser.add_argument("--new-workspace", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--expected-count", type=int, default=576)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    import_objects(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
