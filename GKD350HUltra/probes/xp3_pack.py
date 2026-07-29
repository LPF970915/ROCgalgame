#!/usr/bin/env python3
"""Create a small, standard XP3 archive from a directory tree."""

from __future__ import annotations

import argparse
import pathlib
import struct
import zlib


XP3_MAGIC = b"XP3\r\n \n\x1a\x8bg\x01"


def chunk(tag: bytes, payload: bytes) -> bytes:
    return tag + struct.pack("<Q", len(payload)) + payload


def archive_name(root: pathlib.Path, path: pathlib.Path) -> str:
    return path.relative_to(root).as_posix().replace("/", "\\")


def create_archive(source: pathlib.Path, output: pathlib.Path) -> None:
    files = sorted(path for path in source.rglob("*") if path.is_file())
    if not files:
        raise ValueError(f"source directory has no files: {source}")

    payloads: list[tuple[pathlib.Path, str, bytes, int]] = []
    offset = len(XP3_MAGIC) + 8
    for path in files:
        data = path.read_bytes()
        payloads.append((path, archive_name(source, path), data, offset))
        offset += len(data)

    index_parts: list[bytes] = []
    for _, name, data, data_offset in payloads:
        encoded_name = name.encode("utf-16le")
        info = struct.pack("<IQQH", 0, len(data), len(data), len(name)) + encoded_name
        segment = struct.pack("<IQQQ", 0, data_offset, len(data), len(data))
        checksum = struct.pack("<I", zlib.adler32(data) & 0xFFFFFFFF)
        index_parts.append(
            chunk(
                b"File",
                chunk(b"info", info)
                + chunk(b"segm", segment)
                + chunk(b"adlr", checksum),
            )
        )

    index = b"".join(index_parts)
    compressed_index = zlib.compress(index, 9)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as archive:
        archive.write(XP3_MAGIC)
        archive.write(struct.pack("<Q", offset))
        for _, _, data, _ in payloads:
            archive.write(data)
        archive.write(b"\x01")
        archive.write(struct.pack("<Q", len(compressed_index)))
        archive.write(struct.pack("<Q", len(index)))
        archive.write(compressed_index)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    create_archive(args.source.resolve(), args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
