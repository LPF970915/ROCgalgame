#!/usr/bin/env python3
"""Create and materialize a locked vcpkg dependency set from binary archives."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile
import re


SCHEMA_VERSION = 1


def parse_paragraphs(value: str) -> list[dict[str, str]]:
    paragraphs: list[dict[str, str]] = []
    current: dict[str, str] = {}
    key = ""
    for raw_line in value.splitlines() + [""]:
        if not raw_line:
            if current:
                paragraphs.append(current)
                current = {}
                key = ""
            continue
        if raw_line[0].isspace() and key:
            current[key] += "\n" + raw_line
            continue
        key, field_value = raw_line.split(":", 1)
        current[key] = field_value.lstrip()
    return paragraphs


def render_paragraphs(paragraphs: list[dict[str, str]]) -> str:
    return (
        "\n\n".join(
            "\n".join(f"{key}: {value}" for key, value in paragraph.items())
            for paragraph in paragraphs
        )
        + "\n"
    )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def primary_control(archive: Path) -> tuple[list[dict[str, str]], dict[str, str]]:
    with zipfile.ZipFile(archive) as package:
        bad_file = package.testzip()
        names = set(package.namelist())
        if bad_file is not None:
            raise RuntimeError(f"corrupt archive {archive}: {bad_file}")
        if "CONTROL" not in names or "BUILD_INFO" not in names:
            raise RuntimeError(f"archive lacks vcpkg metadata: {archive}")
        paragraphs = parse_paragraphs(package.read("CONTROL").decode("utf-8"))
    primary = [item for item in paragraphs if "Feature" not in item]
    if len(primary) != 1:
        raise RuntimeError(f"archive must contain one primary CONTROL paragraph: {archive}")
    return paragraphs, primary[0]


def version_string(primary: dict[str, str]) -> str:
    for key in ("Version", "Version-Date", "Version-Semver", "Version-String"):
        if key in primary:
            return primary[key]
    raise RuntimeError(f"package has no version: {primary.get('Package', '<unknown>')}")


def load_status_selection(status_path: Path, triplet: str) -> dict[str, str]:
    paragraphs = parse_paragraphs(status_path.read_text(encoding="utf-8"))
    selected: dict[str, str] = {}
    for item in paragraphs:
        if item.get("Architecture") != triplet or "Feature" in item:
            continue
        name = item.get("Package", "")
        abi = item.get("Abi", "")
        if not name or len(abi) != 64:
            raise RuntimeError(f"status lacks locked ABI for target package: {name}")
        if name in selected and selected[name] != abi:
            raise RuntimeError(f"status contains conflicting ABIs for {name}")
        selected[name] = abi
    if not selected:
        raise RuntimeError(f"status contains no packages for {triplet}")
    return selected


def create_lock(
    cache_root: Path,
    status_path: Path,
    output_path: Path,
    triplet: str,
) -> None:
    selected = load_status_selection(status_path, triplet)
    archives: dict[tuple[str, str], tuple[Path, list[dict[str, str]], dict[str, str]]] = {}
    for archive in sorted(cache_root.glob("*/*.zip")):
        paragraphs, primary = primary_control(archive)
        if primary.get("Architecture") != triplet:
            continue
        name = primary.get("Package", "")
        abi = primary.get("Abi", "")
        if not name or archive.stem != abi:
            raise RuntimeError(f"cache path and CONTROL ABI disagree: {archive}")
        key = (name, abi)
        if key in archives:
            raise RuntimeError(f"duplicate cache archive for {name} ABI {abi}")
        archives[key] = (archive, paragraphs, primary)

    packages: list[dict[str, object]] = []
    for name, abi in sorted(selected.items()):
        match = archives.get((name, abi))
        if match is None:
            raise RuntimeError(f"locked archive is missing: {name} ABI {abi}")
        archive, paragraphs, primary = match
        with zipfile.ZipFile(archive) as package:
            file_count = sum(
                1
                for member in package.infolist()
                if not member.is_dir() and member.filename not in ("CONTROL", "BUILD_INFO")
            )
        packages.append(
            {
                "name": name,
                "version": version_string(primary),
                "port_version": int(primary.get("Port-Version", "0")),
                "abi": abi,
                "archive": archive.relative_to(cache_root).as_posix(),
                "archive_sha256": sha256(archive),
                "features": sorted(
                    item["Feature"] for item in paragraphs if "Feature" in item
                ),
                "file_count": file_count,
            }
        )

    lock = {
        "schema": SCHEMA_VERSION,
        "triplet": triplet,
        "package_count": len(packages),
        "selection": "vcpkg-status-abi",
        "packages": packages,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_suffix(output_path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(lock, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    os.replace(temporary, output_path)
    print(f"[vcpkg_materialize] lock={output_path} packages={len(packages)}")


def safe_member_path(member_name: str) -> Path:
    relative = Path(member_name)
    if relative.is_absolute() or ".." in relative.parts:
        raise RuntimeError(f"unsafe cache member: {member_name}")
    return relative


def normalize_text_artifact(
    path: Path, triplet_root: Path, final_triplet_root: Path | None = None
) -> bool:
    """Relocate generated text metadata away from the extraction machine paths."""
    if path.name == "vcpkg_abi_info.txt":
        return False
    text_suffixes = {
        ".cmake",
        ".conf",
        ".cfg",
        ".h",
        ".hpp",
        ".in",
        ".json",
        ".la",
        ".pc",
        ".py",
        ".sh",
        ".txt",
        ".xml",
    }
    is_config_script = path.parent.name == "bin" and "config" in path.name
    if path.suffix.lower() not in text_suffixes and not is_config_script:
        return False
    try:
        original = path.read_bytes()
        text = original.decode("utf-8")
    except (UnicodeDecodeError, OSError):
        return False
    final_root = final_triplet_root or triplet_root
    normalized = text
    normalized = normalized.replace(
        "/mnt/d/Works/ROCgalgame-krkr2-port", "/sources/krkr2"
    )
    normalized = normalized.replace("/mnt/d/Works/ROCgalgame", "/workspace")
    normalized = normalized.replace(
        "/workspace/build/gkd350h-glibc234/krkr2/vcpkg_installed/"
        "arm64-linux-gkd-glibc234",
        str(final_root),
    )
    normalized = re.sub(
        r"/workspace/build/gkd350h-glibc234/[^/\\\s]+/"
        r"vcpkg_installed(?:\.materializing)?/arm64-linux-gkd-glibc234",
        str(final_root),
        normalized,
    )
    normalized = re.sub(
        r"/workspace/build/gkd350h-glibc234/vcpkg/packages/[^/\\\s]+",
        str(final_root),
        normalized,
    )
    if normalized == text:
        return False
    path.write_text(normalized, encoding="utf-8", newline="\n")
    return True


def generate_recovery_metadata(stage: Path, triplet: str) -> dict[str, str]:
    """Generate deterministic package metadata omitted by artifact-only recovery."""
    triplet_root = stage / triplet
    ffmpeg_share = triplet_root / "share" / "ffmpeg"
    ffmpeg_share.mkdir(parents=True, exist_ok=True)
    ffmpeg_config = ffmpeg_share / "FFMPEGConfig.cmake"
    ffmpeg_config.write_text(
        "# Generated by ROCgalgame's locked vcpkg cache materializer.\n"
        "get_filename_component(_ROCGALGAME_FFMPEG_PREFIX \"${CMAKE_CURRENT_LIST_DIR}/../..\" ABSOLUTE)\n"
        "set(FFMPEG_INCLUDE_DIRS \"${_ROCGALGAME_FFMPEG_PREFIX}/include\")\n"
        "set(FFMPEG_LIBRARY_DIRS \"${_ROCGALGAME_FFMPEG_PREFIX}/lib\")\n"
        "set(FFMPEG_LIBRARIES\n"
        "  \"${_ROCGALGAME_FFMPEG_PREFIX}/lib/libavfilter.a\"\n"
        "  \"${_ROCGALGAME_FFMPEG_PREFIX}/lib/libavformat.a\"\n"
        "  \"${_ROCGALGAME_FFMPEG_PREFIX}/lib/libavcodec.a\"\n"
        "  \"${_ROCGALGAME_FFMPEG_PREFIX}/lib/libswresample.a\"\n"
        "  \"${_ROCGALGAME_FFMPEG_PREFIX}/lib/libswscale.a\"\n"
        "  \"${_ROCGALGAME_FFMPEG_PREFIX}/lib/libavutil.a\"\n"
        "  xcb-shm xcb SDL2 lzma bz2 z -pthread m)\n"
        "set(FFMPEG_VERSION \"3.3.9\")\n"
        "set(FFMPEG_FOUND TRUE)\n",
        encoding="utf-8",
        newline="\n",
    )
    info_files = list((stage / "vcpkg" / "info").glob(f"ffmpeg_*_{triplet}.list"))
    if len(info_files) != 1:
        raise RuntimeError("cannot identify FFmpeg vcpkg info list")
    entry = f"{triplet}/share/ffmpeg/FFMPEGConfig.cmake"
    existing = info_files[0].read_text(encoding="utf-8").splitlines()
    if entry not in existing:
        existing.append(entry)
        existing.sort()
        info_files[0].write_text("\n".join(existing) + "\n", encoding="utf-8", newline="\n")
    generated = {entry: sha256(ffmpeg_config)}

    boost_dir = triplet_root / "lib" / "cmake" / "boost_type_traits-1.91.0"
    boost_dir.mkdir(parents=True, exist_ok=True)
    boost_config = boost_dir / "boost_type_traits-config.cmake"
    boost_config.write_text(
        "# Generated from Boost.TypeTraits 1.91.0 package metadata.\n"
        "include(CMakeFindDependencyMacro)\n"
        "if(NOT boost_config_FOUND)\n"
        "  find_dependency(boost_config 1.91.0 EXACT HINTS \"${CMAKE_CURRENT_LIST_DIR}/..\")\n"
        "endif()\n"
        "if(NOT TARGET Boost::type_traits)\n"
        "  add_library(Boost::type_traits INTERFACE IMPORTED)\n"
        "  get_filename_component(_BOOST_TYPE_TRAITS_PREFIX \"${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)\n"
        "  set_target_properties(Boost::type_traits PROPERTIES\n"
        "    INTERFACE_INCLUDE_DIRECTORIES \"${_BOOST_TYPE_TRAITS_PREFIX}/include\"\n"
        "    INTERFACE_LINK_LIBRARIES \"Boost::config\")\n"
        "  unset(_BOOST_TYPE_TRAITS_PREFIX)\n"
        "endif()\n"
        "set(boost_type_traits_VERSION \"1.91.0\")\n"
        "set(boost_type_traits_FOUND TRUE)\n",
        encoding="utf-8",
        newline="\n",
    )
    boost_version = boost_dir / "boost_type_traits-config-version.cmake"
    boost_version.write_text(
        "set(PACKAGE_VERSION \"1.91.0\")\n"
        "if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)\n"
        "  set(PACKAGE_VERSION_EXACT TRUE)\n"
        "  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
        "elseif(PACKAGE_FIND_VERSION VERSION_LESS PACKAGE_VERSION AND PACKAGE_FIND_VERSION_MAJOR EQUAL 1)\n"
        "  set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
        "else()\n"
        "  set(PACKAGE_VERSION_COMPATIBLE FALSE)\n"
        "endif()\n",
        encoding="utf-8",
        newline="\n",
    )
    boost_info = list(
        (stage / "vcpkg" / "info").glob(f"boost-type-traits_*_{triplet}.list")
    )
    if len(boost_info) != 1:
        raise RuntimeError("cannot identify Boost.TypeTraits vcpkg info list")
    boost_entries = [
        f"{triplet}/lib/cmake/boost_type_traits-1.91.0/{path.name}"
        for path in (boost_config, boost_version)
    ]
    existing = boost_info[0].read_text(encoding="utf-8").splitlines()
    for boost_entry in boost_entries:
        if boost_entry not in existing:
            existing.append(boost_entry)
    existing.sort()
    boost_info[0].write_text("\n".join(existing) + "\n", encoding="utf-8", newline="\n")
    generated.update(
        {
            boost_entries[0]: sha256(boost_config),
            boost_entries[1]: sha256(boost_version),
        }
    )

    sqlite_dir = triplet_root / "share" / "unofficial-sqlite3"
    sqlite_dir.mkdir(parents=True, exist_ok=True)
    sqlite_config = sqlite_dir / "unofficial-sqlite3-config.cmake"
    if not sqlite_config.is_file():
        sqlite_config.write_text(
        "# Generated from the locked static sqlite3 package.\n"
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(Threads)\n"
        "if(NOT TARGET unofficial::sqlite3::sqlite3)\n"
        "  add_library(unofficial::sqlite3::sqlite3 STATIC IMPORTED)\n"
        "  get_filename_component(_ROCGALGAME_SQLITE_PREFIX \"${CMAKE_CURRENT_LIST_DIR}/../..\" ABSOLUTE)\n"
        "  set_target_properties(unofficial::sqlite3::sqlite3 PROPERTIES\n"
        "    IMPORTED_LOCATION \"${_ROCGALGAME_SQLITE_PREFIX}/lib/libsqlite3.a\"\n"
        "    INTERFACE_INCLUDE_DIRECTORIES \"${_ROCGALGAME_SQLITE_PREFIX}/include\"\n"
        "    INTERFACE_LINK_LIBRARIES \"Threads::Threads;${CMAKE_DL_LIBS};m\")\n"
        "  unset(_ROCGALGAME_SQLITE_PREFIX)\n"
        "endif()\n"
        "set(unofficial-sqlite3_FOUND TRUE)\n",
            encoding="utf-8",
            newline="\n",
        )
    sqlite_info = list((stage / "vcpkg" / "info").glob(f"sqlite3_*_{triplet}.list"))
    if len(sqlite_info) != 1:
        raise RuntimeError("cannot identify sqlite3 vcpkg info list")
    sqlite_entry = (
        f"{triplet}/share/unofficial-sqlite3/unofficial-sqlite3-config.cmake"
    )
    existing = sqlite_info[0].read_text(encoding="utf-8").splitlines()
    if sqlite_entry not in existing:
        existing.append(sqlite_entry)
        existing.sort()
        sqlite_info[0].write_text(
            "\n".join(existing) + "\n", encoding="utf-8", newline="\n"
        )
    generated[sqlite_entry] = sha256(sqlite_config)

    jxr_dir = triplet_root / "share" / "jxr"
    jxr_dir.mkdir(parents=True, exist_ok=True)
    jxr_config = jxr_dir / "JXRConfig.cmake"
    jxr_config.write_text(
        "# Generated from the locked static jxrlib package.\n"
        "get_filename_component(_ROCGALGAME_JXR_PREFIX \"${CMAKE_CURRENT_LIST_DIR}/../..\" ABSOLUTE)\n"
        "set(JXR_INCLUDE_DIRS \"${_ROCGALGAME_JXR_PREFIX}/include/jxrlib\")\n"
        "set(JXR_LIBRARIES\n"
        "  \"${_ROCGALGAME_JXR_PREFIX}/lib/libjxrglue.a\"\n"
        "  \"${_ROCGALGAME_JXR_PREFIX}/lib/libjpegxr.a\")\n"
        "set(JXR_FOUND TRUE)\n",
        encoding="utf-8",
        newline="\n",
    )
    jxr_info = list((stage / "vcpkg" / "info").glob(f"jxrlib_*_{triplet}.list"))
    if len(jxr_info) != 1:
        raise RuntimeError("cannot identify jxrlib vcpkg info list")
    jxr_entry = f"{triplet}/share/jxr/JXRConfig.cmake"
    existing = jxr_info[0].read_text(encoding="utf-8").splitlines()
    if jxr_entry not in existing:
        existing.append(jxr_entry)
        existing.sort()
        jxr_info[0].write_text(
            "\n".join(existing) + "\n", encoding="utf-8", newline="\n"
        )
    generated[jxr_entry] = sha256(jxr_config)

    boost_include = str(triplet_root / "include")
    for boost_targets in sorted(triplet_root.rglob("boost_*-targets.cmake")):
        boost_text = boost_targets.read_text(encoding="utf-8")
        normalized_boost = boost_text.replace(
            "${_IMPORT_PREFIX}/include", boost_include
        )
        normalized_boost = re.sub(
            r"set_property\(TARGET Boost::[^ )]+ PROPERTY IMPORTED_GLOBAL TRUE\)\n",
            "",
            normalized_boost,
        )
        if normalized_boost != boost_text:
            boost_targets.write_text(
                normalized_boost, encoding="utf-8", newline="\n"
            )
        boost_entry = boost_targets.relative_to(stage).as_posix()
        generated[boost_entry] = sha256(boost_targets)

    cocos_targets = triplet_root / "share" / "cocos2dx" / "cocos2dx-targets.cmake"
    if not cocos_targets.is_file():
        raise RuntimeError("locked cocos2dx package is missing its target metadata")
    cocos_text = cocos_targets.read_text(encoding="utf-8")
    vcpkg_pkgconfig_prefix = str(triplet_root / "lib" / "pkgconfig" / "../..")
    sysroot_prefix = "/workspace/build/gkd350h-glibc234/sysroot/usr"
    cocos_text = cocos_text.replace(
        f"{vcpkg_pkgconfig_prefix}/include", f"{sysroot_prefix}/include"
    )
    cocos_text = cocos_text.replace(
        f"{vcpkg_pkgconfig_prefix}/lib",
        f"{sysroot_prefix}/lib/aarch64-linux-gnu",
    )
    cocos_targets.write_text(cocos_text, encoding="utf-8", newline="\n")
    cocos_entry = cocos_targets.relative_to(stage).as_posix()
    generated[cocos_entry] = sha256(cocos_targets)

    libxml_targets = (
        triplet_root / "lib" / "cmake" / "libxml2" / "libxml2-export.cmake"
    )
    if not libxml_targets.is_file():
        raise RuntimeError("locked libxml2 package is missing its target metadata")
    libxml_text = libxml_targets.read_text(encoding="utf-8")
    if "INTERFACE_COMPILE_OPTIONS \"-pthread;-DXML_STATIC\"" not in libxml_text:
        libxml_text = libxml_text.replace(
            "set_target_properties(LibXml2::LibXml2 PROPERTIES\n",
            "set_target_properties(LibXml2::LibXml2 PROPERTIES\n"
            "  INTERFACE_COMPILE_OPTIONS \"-pthread;-DXML_STATIC\"\n",
        )
    libxml_targets.write_text(libxml_text, encoding="utf-8", newline="\n")
    libxml_entry = libxml_targets.relative_to(stage).as_posix()
    generated[libxml_entry] = sha256(libxml_targets)

    websockets_config = (
        triplet_root
        / "lib"
        / "cmake"
        / "libwebsockets"
        / "libwebsockets-config.cmake"
    )
    if not websockets_config.is_file():
        raise RuntimeError("locked libwebsockets package is missing its config metadata")
    websockets_text = websockets_config.read_text(encoding="utf-8")
    websockets_text = websockets_text.replace(
        '"${CMAKE_CURRENT_LIST_DIR}/../include"',
        '"${CMAKE_CURRENT_LIST_DIR}/../../../include"',
    )
    websockets_config.write_text(
        websockets_text, encoding="utf-8", newline="\n"
    )
    websockets_entry = websockets_config.relative_to(stage).as_posix()
    generated[websockets_entry] = sha256(websockets_config)

    tiff_config = triplet_root / "lib" / "cmake" / "tiff" / "TiffConfig.cmake"
    if not tiff_config.is_file():
        raise RuntimeError("locked TIFF package is missing TiffConfig.cmake")
    tiff_text = tiff_config.read_text(encoding="utf-8")
    old_dependency_loader = (
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(liblzma CONFIG)\n\n"
    )
    dependency_loader = (
        "include(CMakeFindDependencyMacro)\n"
        "find_dependency(ZLIB)\n"
        "find_dependency(JPEG)\n"
        "find_dependency(liblzma CONFIG)\n"
        "if(NOT TARGET CMath::CMath)\n"
        "  add_library(CMath::CMath INTERFACE IMPORTED)\n"
        "  set_target_properties(CMath::CMath PROPERTIES INTERFACE_LINK_LIBRARIES m)\n"
        "endif()\n\n"
    )
    while tiff_text.startswith(old_dependency_loader):
        tiff_text = tiff_text[len(old_dependency_loader) :]
    while tiff_text.startswith(dependency_loader):
        tiff_text = tiff_text[len(dependency_loader) :]
    tiff_config.write_text(
        dependency_loader + tiff_text, encoding="utf-8", newline="\n"
    )
    tiff_entry = f"{triplet}/lib/cmake/tiff/TiffConfig.cmake"
    generated[tiff_entry] = sha256(tiff_config)
    return generated


def refresh_metadata(destination: Path, lock_path: Path) -> None:
    if not destination.is_dir():
        raise RuntimeError(f"installed dependency tree is missing: {destination}")
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    triplet = lock.get("triplet")
    if not isinstance(triplet, str):
        raise RuntimeError("invalid dependency lock")
    marker = destination / ".rocgalgame-dependency-lock.sha256"
    expected = sha256(lock_path) + "  " + lock_path.name
    if not marker.is_file() or marker.read_text(encoding="ascii").strip() != expected:
        raise RuntimeError("installed dependency tree does not match lock")
    generated = generate_recovery_metadata(destination, triplet)
    triplet_root = destination / triplet
    normalized_count = 0
    for path in triplet_root.rglob("*"):
        if path.is_file() and normalize_text_artifact(path, triplet_root):
            normalized_count += 1
    (destination / ".rocgalgame-generated-metadata.json").write_text(
        json.dumps(generated, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    print(
        f"[vcpkg_materialize] refreshed metadata files={len(generated)} "
        f"normalized_text={normalized_count}"
    )


def materialize(cache_root: Path, lock_path: Path, destination: Path) -> None:
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    if lock.get("schema") != SCHEMA_VERSION:
        raise RuntimeError(f"unsupported dependency lock schema: {lock.get('schema')}")
    triplet = lock.get("triplet")
    packages = lock.get("packages")
    if not isinstance(triplet, str) or not isinstance(packages, list):
        raise RuntimeError("invalid dependency lock")
    if lock.get("package_count") != len(packages):
        raise RuntimeError("dependency lock package count mismatch")
    if destination.exists():
        raise RuntimeError(f"clean destination already exists: {destination}")

    stage = destination.with_name(destination.name + ".materializing")
    if stage.exists():
        raise RuntimeError(f"stale materialization directory exists: {stage}")
    triplet_root = stage / triplet
    info_root = stage / "vcpkg" / "info"
    triplet_root.mkdir(parents=True)
    info_root.mkdir(parents=True)
    (stage / "vcpkg" / "updates").mkdir(parents=True)

    installed_files: dict[Path, str] = {}
    status_paragraphs: list[dict[str, str]] = []
    try:
        for index, record in enumerate(packages, start=1):
            if not isinstance(record, dict):
                raise RuntimeError("invalid dependency lock package record")
            name = str(record["name"])
            abi = str(record["abi"])
            archive = cache_root / str(record["archive"])
            if archive.stem != abi or sha256(archive) != record["archive_sha256"]:
                raise RuntimeError(f"archive hash mismatch for {name}: {archive}")
            paragraphs, primary = primary_control(archive)
            if (
                primary.get("Package") != name
                or primary.get("Architecture") != triplet
                or primary.get("Abi") != abi
            ):
                raise RuntimeError(f"archive metadata mismatch for {name}")

            listed: set[Path] = {Path(triplet)}
            with zipfile.ZipFile(archive) as package:
                for member in package.infolist():
                    if member.is_dir() or member.filename in ("CONTROL", "BUILD_INFO"):
                        continue
                    relative = safe_member_path(member.filename)
                    data = package.read(member)
                    digest = hashlib.sha256(data).hexdigest()
                    target = triplet_root / relative
                    previous = installed_files.get(relative)
                    if previous is not None and previous != digest:
                        raise RuntimeError(
                            f"dependency file conflict at {relative}: {name} differs"
                        )
                    if previous is None:
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(data)
                        installed_files[relative] = digest
                    listed.add(Path(triplet) / relative)
                    for parent in relative.parents:
                        if parent == Path("."):
                            break
                        listed.add(Path(triplet) / parent)

            rendered_list = "\n".join(
                path.as_posix()
                + ("/" if (stage / path).is_dir() else "")
                for path in sorted(listed, key=lambda value: value.as_posix())
            )
            info_name = f"{name}_{version_string(primary)}_{triplet}.list"
            (info_root / info_name).write_text(
                rendered_list + "\n", encoding="utf-8", newline="\n"
            )
            for paragraph in paragraphs:
                installed = dict(paragraph)
                installed["Status"] = "install ok installed"
                status_paragraphs.append(installed)
            print(f"[vcpkg_materialize] {index}/{len(packages)} {name} {abi[:12]}")

        generated_metadata = generate_recovery_metadata(stage, triplet)
        status_paragraphs.sort(
            key=lambda item: (
                item.get("Architecture", ""),
                item.get("Package", ""),
                item.get("Feature", ""),
            )
        )
        (stage / "vcpkg" / "status").write_text(
            render_paragraphs(status_paragraphs), encoding="utf-8", newline="\n"
        )
        (stage / ".rocgalgame-dependency-lock.sha256").write_text(
            sha256(lock_path) + "  " + lock_path.name + "\n",
            encoding="ascii",
            newline="\n",
        )
        (stage / ".rocgalgame-generated-metadata.json").write_text(
            json.dumps(generated_metadata, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        normalized_count = 0
        final_triplet_root = destination / triplet
        for path in triplet_root.rglob("*"):
            if path.is_file() and normalize_text_artifact(
                path, triplet_root, final_triplet_root
            ):
                normalized_count += 1
        os.replace(stage, destination)
    except BaseException:
        if stage.exists():
            shutil.rmtree(stage)
        raise

    print(
        f"[vcpkg_materialize] installed={destination} packages={len(packages)} "
        f"files={len(installed_files)} normalized_text={normalized_count}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    lock_parser = subparsers.add_parser("lock")
    lock_parser.add_argument("--cache", type=Path, required=True)
    lock_parser.add_argument("--status", type=Path, required=True)
    lock_parser.add_argument("--output", type=Path, required=True)
    lock_parser.add_argument("--triplet", required=True)

    install_parser = subparsers.add_parser("install")
    install_parser.add_argument("--cache", type=Path, required=True)
    install_parser.add_argument("--lock", type=Path, required=True)
    install_parser.add_argument("--destination", type=Path, required=True)

    metadata_parser = subparsers.add_parser("metadata")
    metadata_parser.add_argument("--lock", type=Path, required=True)
    metadata_parser.add_argument("--destination", type=Path, required=True)

    args = parser.parse_args()
    if args.command == "lock":
        create_lock(args.cache, args.status, args.output, args.triplet)
    elif args.command == "install":
        materialize(args.cache, args.lock, args.destination)
    else:
        refresh_metadata(args.destination, args.lock)
    return 0


if __name__ == "__main__":
    sys.exit(main())
