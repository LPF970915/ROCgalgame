#!/usr/bin/env python3
import os
import sys
import zipfile


def add_directory(archive: zipfile.ZipFile, root: str, relative: str) -> None:
    archive_name = relative.replace(os.sep, "/").rstrip("/") + "/"
    info = zipfile.ZipInfo.from_file(os.path.join(root, relative), archive_name)
    archive.writestr(info, b"")


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: create_release_zip.py <staging-root> <output.zip> <top-level-dir>")
        return 2

    staging_root = os.path.abspath(sys.argv[1])
    output_path = os.path.abspath(sys.argv[2])
    top_level = sys.argv[3].strip("/\\")
    source_root = os.path.join(staging_root, top_level)
    if not os.path.isdir(source_root):
        print(f"missing release directory: {source_root}")
        return 1

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with zipfile.ZipFile(
        output_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as archive:
        for current_root, directories, files in os.walk(source_root):
            directories.sort()
            files.sort()
            relative_root = os.path.relpath(current_root, staging_root)
            add_directory(archive, staging_root, relative_root)
            for filename in files:
                source_path = os.path.join(current_root, filename)
                archive_name = os.path.relpath(source_path, staging_root).replace(os.sep, "/")
                archive.write(source_path, archive_name)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
