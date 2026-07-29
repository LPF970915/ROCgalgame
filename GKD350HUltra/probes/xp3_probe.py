#!/usr/bin/env python3
"""List or selectively extract files from a standard XP3 archive."""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import struct
import sys
import zlib


XP3_MAGIC = b"XP3\r\n \n\x1a\x8bg\x01"
INDEX_COMPRESSED = 0x01
INDEX_CONTINUES = 0x80
SEGMENT_COMPRESSED = 0x01


@dataclasses.dataclass(frozen=True)
class Segment:
    flags: int
    offset: int
    original_size: int
    archive_size: int


@dataclasses.dataclass(frozen=True)
class Entry:
    name: str
    original_size: int
    archive_size: int
    segments: tuple[Segment, ...]


def unpack_from(fmt: str, data: bytes, offset: int) -> tuple[tuple[int, ...], int]:
    size = struct.calcsize(fmt)
    end = offset + size
    if end > len(data):
        raise ValueError("truncated XP3 index")
    return struct.unpack_from(fmt, data, offset), end


def read_index_blocks(archive) -> bytes:
    magic = archive.read(len(XP3_MAGIC))
    if magic != XP3_MAGIC:
        raise ValueError("not an XP3 archive")
    raw_offset = archive.read(8)
    if len(raw_offset) != 8:
        raise ValueError("truncated XP3 header")
    archive.seek(struct.unpack("<Q", raw_offset)[0])

    blocks: list[bytes] = []
    while True:
        raw_flag = archive.read(1)
        if len(raw_flag) != 1:
            raise ValueError("truncated XP3 index header")
        flags = raw_flag[0]
        raw_size = archive.read(8)
        if len(raw_size) != 8:
            raise ValueError("truncated XP3 index size")
        archive_size = struct.unpack("<Q", raw_size)[0]
        if flags & INDEX_COMPRESSED:
            raw_original_size = archive.read(8)
            if len(raw_original_size) != 8:
                raise ValueError("truncated XP3 original index size")
            original_size = struct.unpack("<Q", raw_original_size)[0]
        else:
            original_size = archive_size

        payload = archive.read(archive_size)
        if len(payload) != archive_size:
            raise ValueError("truncated XP3 index payload")
        if flags & INDEX_COMPRESSED:
            payload = zlib.decompress(payload)
            if len(payload) != original_size:
                raise ValueError("XP3 index size mismatch")
        blocks.append(payload)
        if not flags & INDEX_CONTINUES:
            return b"".join(blocks)


def parse_entry(data: bytes) -> Entry:
    offset = 0
    name: str | None = None
    original_size = 0
    archive_size = 0
    segments: list[Segment] = []

    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError("truncated XP3 File chunk")
        tag = data[offset : offset + 4]
        (chunk_size,), payload_offset = unpack_from("<Q", data, offset + 4)
        payload_end = payload_offset + chunk_size
        if payload_end > len(data):
            raise ValueError("invalid XP3 subchunk size")
        payload = data[payload_offset:payload_end]

        if tag == b"info":
            if len(payload) < 22:
                raise ValueError("truncated XP3 info chunk")
            _, original_size, archive_size, name_length = struct.unpack_from(
                "<IQQH", payload
            )
            name_bytes = payload[22 : 22 + name_length * 2]
            if len(name_bytes) != name_length * 2:
                raise ValueError("truncated XP3 filename")
            name = name_bytes.decode("utf-16le")
        elif tag == b"segm":
            if len(payload) % 28:
                raise ValueError("invalid XP3 segment chunk")
            for segment_offset in range(0, len(payload), 28):
                flags, file_offset, original, archived = struct.unpack_from(
                    "<IQQQ", payload, segment_offset
                )
                segments.append(Segment(flags, file_offset, original, archived))
        offset = payload_end

    if name is None or not segments:
        raise ValueError("XP3 entry is missing info or segments")
    return Entry(name, original_size, archive_size, tuple(segments))


def read_entries(archive) -> list[Entry]:
    index = read_index_blocks(archive)
    entries: list[Entry] = []
    offset = 0
    while offset < len(index):
        if offset + 12 > len(index):
            raise ValueError("truncated XP3 top-level chunk")
        tag = index[offset : offset + 4]
        (chunk_size,), payload_offset = unpack_from("<Q", index, offset + 4)
        payload_end = payload_offset + chunk_size
        if payload_end > len(index):
            raise ValueError("invalid XP3 top-level chunk size")
        if tag == b"File":
            entries.append(parse_entry(index[payload_offset:payload_end]))
        offset = payload_end
    return entries


def safe_output_path(root: pathlib.Path, entry_name: str) -> pathlib.Path:
    normalized = entry_name.replace("\\", "/").lstrip("/")
    parts = pathlib.PurePosixPath(normalized).parts
    if not parts or any(part in ("", ".", "..") for part in parts):
        raise ValueError(f"unsafe XP3 entry path: {entry_name!r}")
    return root.joinpath(*parts)


def extract_entry(archive, entry: Entry, destination: pathlib.Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    written = 0
    with destination.open("wb") as output:
        for segment in entry.segments:
            archive.seek(segment.offset)
            payload = archive.read(segment.archive_size)
            if len(payload) != segment.archive_size:
                raise ValueError(f"truncated segment for {entry.name}")
            if segment.flags & SEGMENT_COMPRESSED:
                payload = zlib.decompress(payload)
            if len(payload) != segment.original_size:
                raise ValueError(f"segment size mismatch for {entry.name}")
            output.write(payload)
            written += len(payload)
    if written != entry.original_size:
        raise ValueError(f"entry size mismatch for {entry.name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=pathlib.Path)
    parser.add_argument("--match", default="", help="case-insensitive name substring")
    parser.add_argument("--extract", type=pathlib.Path, help="extract matches below this directory")
    args = parser.parse_args()

    needle = args.match.casefold()
    try:
        with args.archive.open("rb") as archive:
            entries = read_entries(archive)
            matched = [entry for entry in entries if needle in entry.name.casefold()]
            for entry in matched:
                print(
                    f"{entry.original_size}\t{entry.archive_size}\t"
                    f"{len(entry.segments)}\t{entry.name}"
                )
                if args.extract is not None:
                    extract_entry(
                        archive, entry, safe_output_path(args.extract, entry.name)
                    )
        print(f"matched={len(matched)} total={len(entries)}", file=sys.stderr)
        return 0
    except (OSError, ValueError, zlib.error) as error:
        print(f"xp3_probe: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
