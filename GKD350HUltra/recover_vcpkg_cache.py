#!/usr/bin/env python3
"""Recover vcpkg binary packages from preserved CMake build trees.

This tool never compiles sources. It replays CMake's install scripts into an
isolated staging directory, restores vcpkg metadata from the previous status
database, and writes ABI-addressed ZIP archives for normal vcpkg restoration.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
import re


TRIPLET = "arm64-linux-gkd-glibc234"


def parse_paragraphs(text_value: str) -> list[dict[str, str]]:
    paragraphs: list[dict[str, str]] = []
    current: dict[str, str] = {}
    key = ""
    for raw_line in text_value.splitlines() + [""]:
        if not raw_line:
            if current:
                paragraphs.append(current)
                current = {}
                key = ""
            continue
        if raw_line[0].isspace() and key:
            current[key] += "\n" + raw_line
            continue
        key, value = raw_line.split(":", 1)
        current[key] = value.lstrip()
    return paragraphs


def parse_status(path: Path) -> list[dict[str, str]]:
    return parse_paragraphs(path.read_text(encoding="utf-8"))


def serialize_control(paragraphs: list[dict[str, str]], abi: str) -> str:
    rendered: list[str] = []
    for paragraph in paragraphs:
        fields = dict(paragraph)
        fields.pop("Status", None)
        if "Feature" not in fields:
            fields["Abi"] = abi
        rendered.append("\n".join(f"{key}: {value}" for key, value in fields.items()))
    return "\n\n".join(rendered) + "\n"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def cmake_build_dir(port_root: Path, triplet: str) -> Path | None:
    candidates = [
        port_root / f"{triplet}-rel",
        port_root / f"{triplet}-release",
    ]
    candidates.extend(sorted(port_root.glob(f"{triplet}*rel*")))
    for candidate in candidates:
        if (candidate / "CMakeCache.txt").is_file() and (
            candidate / "cmake_install.cmake"
        ).is_file():
            return candidate
    return None


def copy_optional_port_files(version_root: Path | None, share_dir: Path) -> None:
    if version_root is None:
        return
    for name in ("usage", "copyright"):
        source = version_root / name
        if source.is_file() and not (share_dir / name).exists():
            shutil.copy2(source, share_dir / name)


def matching_version_root(port_root: Path, abi_entries: dict[str, str]) -> Path | None:
    version_parent = port_root.parent.parent / "versioning_" / "versions" / port_root.name
    expected_portfile = abi_entries.get("portfile.cmake", "")
    expected_manifest = abi_entries.get("vcpkg.json", "")
    if not version_parent.is_dir():
        return None
    for candidate in sorted(version_parent.iterdir()):
        portfile = candidate / "portfile.cmake"
        manifest = candidate / "vcpkg.json"
        if not portfile.is_file() or not manifest.is_file():
            continue
        if expected_portfile and sha256(portfile) != expected_portfile:
            continue
        if expected_manifest and sha256(manifest) != expected_manifest:
            continue
        return candidate
    return None


def read_abi_entries(path: Path) -> dict[str, str]:
    entries: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if " " in line:
            key, value = line.split(" ", 1)
            entries[key] = value
    return entries


def zip_tree(source: Path, target: Path) -> None:
    temporary = target.with_suffix(".zip.tmp")
    if temporary.exists():
        temporary.unlink()
    target.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(source.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(source).as_posix())
    os.replace(temporary, target)


def prepare_stage(stage: Path) -> None:
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)


def finish_stage(
    port: str,
    stage: Path,
    abi_info: Path,
    paragraphs: list[dict[str, str]],
    port_root: Path,
) -> None:
    share_dir = stage / "share" / port
    share_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(abi_info, share_dir / "vcpkg_abi_info.txt")
    abi_entries = read_abi_entries(abi_info)
    copy_optional_port_files(matching_version_root(port_root, abi_entries), share_dir)
    abi = sha256(abi_info)
    (stage / "CONTROL").write_text(
        serialize_control(paragraphs, abi), encoding="utf-8", newline="\n"
    )
    (stage / "BUILD_INFO").write_text(
        "CRTLinkage: dynamic\nLibraryLinkage: static\n",
        encoding="ascii",
        newline="\n",
    )


def normalize_destdir(raw: Path, stage: Path, package_name: str) -> None:
    candidates = [path for path in raw.rglob(package_name) if path.is_dir()]
    if not candidates:
        candidates = [path for path in raw.rglob(TRIPLET) if path.is_dir()]
    if len(candidates) != 1:
        raise RuntimeError(
            f"expected one DESTDIR package directory for {package_name}, found {len(candidates)}"
        )
    shutil.copytree(candidates[0], stage, dirs_exist_ok=True)


def assert_install_only(command: list[str], cwd: Path, env: dict[str, str]) -> None:
    preview = subprocess.run(
        command[:1] + ["-n"] + command[1:],
        cwd=cwd,
        env=env,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    ).stdout
    compiler = re.compile(
        r"(?:^|\s)(?:/[^\s]+/)?(?:aarch64-linux-gnu-)?"
        r"(?:cc|gcc|g\+\+|c\+\+|clang|clang\+\+)(?:\s|$)"
    )
    if any(compiler.search(line) for line in preview.splitlines()):
        raise RuntimeError("install target would invoke a compiler")


def recover_non_cmake(
    port: str,
    port_root: Path,
    package_root: Path,
    paragraphs: list[dict[str, str]],
    staging_root: Path,
    installed_triplet: Path,
) -> tuple[Path, str] | None:
    stage = staging_root / f"{port}_{TRIPLET}"
    existing = package_root / f"{port}_{TRIPLET}"
    if port == "boost-type-traits":
        sources = sorted((port_root / "src").glob("*.clean"))
        if len(sources) != 1:
            raise RuntimeError("expected one extracted Boost.TypeTraits source")
        include_source = sources[0] / "include"
        if not include_source.is_dir():
            include_source = sources[0] / "libs" / "type_traits" / "include"
        if not include_source.is_dir():
            raise RuntimeError("Boost.TypeTraits headers are missing")
        prepare_stage(stage)
        shutil.copytree(include_source, stage / "include", dirs_exist_ok=True)
        config_dir = stage / "lib" / "cmake" / "boost_type_traits-1.91.0"
        config_dir.mkdir(parents=True, exist_ok=True)
        (config_dir / "boost_type_traits-config.cmake").write_text(
            "include(CMakeFindDependencyMacro)\n"
            "find_dependency(boost_config 1.91.0 EXACT HINTS \"${CMAKE_CURRENT_LIST_DIR}/..\")\n"
            "if(NOT TARGET Boost::type_traits)\n"
            "  add_library(Boost::type_traits INTERFACE IMPORTED)\n"
            "  get_filename_component(_prefix \"${CMAKE_CURRENT_LIST_DIR}/../../../..\" ABSOLUTE)\n"
            "  set_target_properties(Boost::type_traits PROPERTIES INTERFACE_INCLUDE_DIRECTORIES \"${_prefix}/include\" INTERFACE_LINK_LIBRARIES \"Boost::config\")\n"
            "endif()\n"
            "set(boost_type_traits_VERSION \"1.91.0\")\n"
            "set(boost_type_traits_FOUND TRUE)\n",
            encoding="utf-8",
            newline="\n",
        )
        (config_dir / "boost_type_traits-config-version.cmake").write_text(
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
        return stage, "recovered-boost-source-artifacts"
    if existing.is_dir():
        prepare_stage(stage)
        shutil.copytree(existing, stage, dirs_exist_ok=True)
        return stage, "recovered-existing-package"

    stdout_log = port_root / f"stdout-{TRIPLET}.log"
    if stdout_log.is_file() and "VCPKG_POLICY_EMPTY_PACKAGE" in stdout_log.read_text(
        encoding="utf-8", errors="replace"
    ):
        prepare_stage(stage)
        return stage, "recovered-empty-package"

    build_dir = port_root / f"{TRIPLET}-rel"
    file_only_ports = {"egl-registry", "opengl-registry", "xcb-util-m4"}
    if not build_dir.is_dir() and port not in file_only_ports:
        return None
    raw = staging_root / f"{port}_{TRIPLET}.destdir"
    prepare_stage(raw)
    prepare_stage(stage)
    env = dict(os.environ)
    env["DESTDIR"] = str(raw)
    package_name = f"{port}_{TRIPLET}"
    if port == "ffmpeg":
        command = ["make", "install-headers"]
        assert_install_only(command, build_dir, env)
        subprocess.run(command, cwd=build_dir, env=env, check=True)
        normalize_destdir(raw, stage, package_name)
        library_dir = stage / "lib"
        pkgconfig_dir = library_dir / "pkgconfig"
        library_dir.mkdir(parents=True, exist_ok=True)
        for library in sorted(build_dir.glob("lib*/lib*.a")):
            shutil.copy2(library, library_dir / library.name)
        for pc_file in pkgconfig_dir.glob("*.pc"):
            lines = pc_file.read_text(encoding="utf-8").splitlines()
            lines = [
                "prefix=${pcfiledir}/../.." if line.startswith("prefix=") else line
                for line in lines
            ]
            pc_file.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        method = "recovered-ffmpeg-artifacts"
    elif port == "harfbuzz" and (
        build_dir / "meson-info" / "intro-installed.json"
    ).is_file():
        manifest = json.loads(
            (build_dir / "meson-info" / "intro-installed.json").read_text(
                encoding="utf-8"
            )
        )
        marker = f"/{package_name}/"
        for source_name, target_name in manifest.items():
            source = Path(source_name)
            if marker not in target_name or not source.is_file():
                raise RuntimeError(f"invalid or incomplete Meson install entry: {source_name}")
            relative = Path(target_name.split(marker, 1)[1])
            target = stage / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        method = "recovered-meson-manifest"
    elif port == "libffi":
        include_dir = stage / "include"
        library_dir = stage / "lib"
        pkgconfig_dir = library_dir / "pkgconfig"
        include_dir.mkdir(parents=True, exist_ok=True)
        pkgconfig_dir.mkdir(parents=True, exist_ok=True)
        for header in ("ffi.h", "ffitarget.h"):
            shutil.copy2(build_dir / "include" / header, include_dir / header)
        shutil.copy2(build_dir / ".libs" / "libffi.a", library_dir / "libffi.a")
        pc_target = pkgconfig_dir / "libffi.pc"
        shutil.copy2(build_dir / "libffi.pc", pc_target)
        lines = pc_target.read_text(encoding="utf-8").splitlines()
        lines = [
            "prefix=${pcfiledir}/../.." if line.startswith("prefix=") else line
            for line in lines
        ]
        pc_target.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        method = "recovered-libffi-artifacts"
    elif port == "openssl":
        for relative in (
            Path("include/openssl"),
            Path("lib/pkgconfig"),
            Path("share/openssl"),
        ):
            source = installed_triplet / relative
            if not source.is_dir():
                raise RuntimeError(f"missing installed OpenSSL directory: {source}")
            shutil.copytree(source, stage / relative, dirs_exist_ok=True)
        (stage / "lib").mkdir(parents=True, exist_ok=True)
        for name in ("libcrypto.a", "libssl.a"):
            shutil.copy2(installed_triplet / "lib" / name, stage / "lib" / name)
        method = "recovered-openssl-artifacts"
    elif port == "pthread-stubs":
        pkgconfig_dir = stage / "lib" / "pkgconfig"
        pkgconfig_dir.mkdir(parents=True, exist_ok=True)
        pc_target = pkgconfig_dir / "pthread-stubs.pc"
        shutil.copy2(build_dir / "pthread-stubs.pc", pc_target)
        lines = pc_target.read_text(encoding="utf-8").splitlines()
        lines = [
            "prefix=${pcfiledir}/../.." if line.startswith("prefix=") else line
            for line in lines
        ]
        pc_target.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        method = "recovered-pthread-stubs-artifacts"
    elif port in ("egl-registry", "opengl-registry"):
        sources = sorted((port_root / "src").glob("*.clean"))
        if len(sources) != 1:
            raise RuntimeError(f"expected one extracted registry source for {port}")
        source = sources[0]
        if port == "egl-registry":
            for name in ("KHR", "EGL"):
                shutil.copytree(
                    source / "api" / name,
                    stage / "include" / name,
                    dirs_exist_ok=True,
                )
            destination = stage / "share" / "opengl"
            destination.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source / "api" / "egl.xml", destination / "egl.xml")
        else:
            for name in ("GL", "GLES", "GLES2", "GLES3", "GLSC", "GLSC2"):
                shutil.copytree(
                    source / "api" / name,
                    stage / "include" / name,
                    dirs_exist_ok=True,
                )
            destination = stage / "share" / "opengl"
            destination.mkdir(parents=True, exist_ok=True)
            registry_files = list((source / "xml").glob("*.xml"))
            registry_files += list((source / "xml").glob("*.rnc"))
            registry_files += [source / "xml" / "readme.pdf", source / "xml" / "reg.py"]
            for registry_file in registry_files:
                if registry_file.is_file():
                    shutil.copy2(registry_file, destination / registry_file.name)
            pkgconfig = stage / "share" / "pkgconfig"
            pkgconfig.mkdir(parents=True, exist_ok=True)
            (pkgconfig / "khronos-opengl-registry.pc").write_text(
                "prefix=${pcfiledir}/../..\n"
                "datadir=${prefix}/share\n"
                "specdir=${datadir}/opengl\n"
                "Name: khronos-opengl-registry\n"
                "Description: Khronos OpenGL registry\n"
                "Version: git3530768138c5ba3dfbb2c43c830493f632f7ea33\n",
                encoding="utf-8",
                newline="\n",
            )
        method = "recovered-registry-files"
    elif port == "xcb-util-m4":
        sources = sorted((port_root / "src").glob("*.clean"))
        if len(sources) != 1:
            raise RuntimeError("expected one extracted xcb-util-m4 source")
        destination = stage / "share" / "xorg" / "aclocal"
        destination.mkdir(parents=True, exist_ok=True)
        for name in (
            "ax_compare_version.m4",
            "xcb_util_common.m4",
            "xcb_util_m4_with_include_path.m4",
        ):
            shutil.copy2(sources[0] / name, destination / name)
        method = "recovered-xcb-util-m4-files"
    elif (build_dir / "Makefile").is_file():
        target = "install_sw" if port == "openssl" else "install"
        command = ["make", target]
        assert_install_only(command, build_dir, env)
        subprocess.run(command, cwd=build_dir, env=env, check=True)
        method = "recovered-make-install"
    else:
        return None
    if port not in (
        "ffmpeg", "harfbuzz", "libffi", "openssl", "pthread-stubs",
        "egl-registry", "opengl-registry", "xcb-util-m4",
    ):
        normalize_destdir(raw, stage, package_name)
    shutil.rmtree(raw)
    return stage, method


def recover_port(
    port: str,
    buildtrees: Path,
    status_paragraphs: list[dict[str, str]],
    staging_root: Path,
    binary_cache: Path,
    cmake: Path,
    package_root: Path,
    installed: Path,
) -> str:
    port_root = buildtrees / port
    abi_info = port_root / f"{TRIPLET}.vcpkg_abi_info.txt"
    if not abi_info.is_file():
        return "no-abi"
    paragraphs = [
        item
        for item in status_paragraphs
        if item.get("Package") == port and item.get("Architecture") == TRIPLET
    ]
    if not paragraphs or not any("Feature" not in item for item in paragraphs):
        return "no-status-metadata"

    abi = sha256(abi_info)
    archive = binary_cache / abi[:2] / f"{abi}.zip"
    if archive.is_file():
        with zipfile.ZipFile(archive) as existing:
            if existing.testzip() is None:
                return "cached"
        return "invalid-existing-cache"

    existing = package_root / f"{port}_{TRIPLET}"
    if port in ("libvorbis", "sqlite3") and (
        build_dir := cmake_build_dir(port_root, TRIPLET)
    ) is not None:
        stage = staging_root / f"{port}_{TRIPLET}"
        prepare_stage(stage)
        subprocess.run(
            [str(cmake), "--install", str(build_dir), "--prefix", str(stage)],
            check=True,
        )
        method = "recovered-cmake"
    elif existing.is_dir():
        recovered = recover_non_cmake(
            port, port_root, package_root, paragraphs, staging_root, installed / TRIPLET
        )
        if recovered is None:
            return "no-install-recovery"
        stage, method = recovered
    elif (build_dir := cmake_build_dir(port_root, TRIPLET)) is not None:
        stage = staging_root / f"{port}_{TRIPLET}"
        prepare_stage(stage)
        subprocess.run(
            [str(cmake), "--install", str(build_dir), "--prefix", str(stage)],
            check=True,
        )
        method = "recovered-cmake"
    else:
        recovered = recover_non_cmake(
            port, port_root, package_root, paragraphs, staging_root, installed / TRIPLET
        )
        if recovered is None:
            return "no-install-recovery"
        stage, method = recovered

    finish_stage(port, stage, abi_info, paragraphs, port_root)
    zip_tree(stage, archive)
    with zipfile.ZipFile(archive) as recovered:
        bad_file = recovered.testzip()
        names = set(recovered.namelist())
    if bad_file is not None or "CONTROL" not in names or "BUILD_INFO" not in names:
        archive.unlink(missing_ok=True)
        return "archive-validation-failed"
    return method


def package_version(paragraphs: list[dict[str, str]], port: str) -> str:
    primary = next(
        item
        for item in paragraphs
        if item.get("Package") == port
        and item.get("Architecture") == TRIPLET
        and "Feature" not in item
    )
    for key in ("Version", "Version-Date", "Version-Semver", "Version-String"):
        if key in primary:
            return primary[key]
    raise RuntimeError(f"status metadata has no version for {port}")


def restore_archive(
    port: str,
    archive: Path,
    installed: Path,
    paragraphs: list[dict[str, str]],
) -> str:
    triplet_root = installed / TRIPLET
    info_root = installed / "vcpkg" / "info"
    triplet_root.mkdir(parents=True, exist_ok=True)
    info_root.mkdir(parents=True, exist_ok=True)
    package_paths: set[Path] = set()
    preserved_paths: list[Path] = []
    with zipfile.ZipFile(archive) as package:
        if package.testzip() is not None:
            raise RuntimeError(f"corrupt cache archive: {archive}")
        for member in package.infolist():
            relative = Path(member.filename)
            if member.is_dir() or member.filename in ("CONTROL", "BUILD_INFO"):
                continue
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError(f"unsafe cache member: {member.filename}")
            target = triplet_root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            data = package.read(member)
            if target.exists():
                if not target.is_file():
                    raise RuntimeError(f"installed-file conflict: {target}")
                if target.read_bytes() != data:
                    if relative == Path("share") / port / "vcpkg_abi_info.txt":
                        target.write_bytes(data)
                    else:
                        preserved_paths.append(target)
            else:
                target.write_bytes(data)
            package_paths.add(relative)

    listed: set[Path] = {Path(TRIPLET)}
    for path in package_paths:
        listed.add(Path(TRIPLET) / path)
        for parent in path.parents:
            if parent == Path("."):
                break
            listed.add(Path(TRIPLET) / parent)
    version = package_version(paragraphs, port)
    info_file = info_root / f"{port}_{version}_{TRIPLET}.list"
    rendered = "\n".join(
        item.as_posix() + ("/" if (triplet_root / item.relative_to(TRIPLET)).is_dir() else "")
        for item in sorted(listed, key=lambda value: value.as_posix())
    )
    info_file.write_text(rendered + "\n", encoding="utf-8", newline="\n")
    for path in preserved_paths:
        print(f"[vcpkg_recovery] preserve existing postprocessed file: {path}")
    return (
        f"restored-installed-preserved-{len(preserved_paths)}"
        if preserved_paths
        else "restored-installed"
    )


def rebuild_status(
    status_file: Path,
    existing_paragraphs: list[dict[str, str]],
    selected: list[str],
    buildtrees: Path,
    binary_cache: Path,
) -> int:
    selected_set = set(selected)
    retained = [
        item
        for item in existing_paragraphs
        if not (
            item.get("Architecture") == TRIPLET
            and item.get("Package") in selected_set
        )
    ]
    restored: list[dict[str, str]] = []
    for port in selected:
        abi_info = buildtrees / port / f"{TRIPLET}.vcpkg_abi_info.txt"
        abi = sha256(abi_info)
        archive = binary_cache / abi[:2] / f"{abi}.zip"
        if not archive.is_file():
            raise RuntimeError(f"cannot rebuild status without cache archive: {port}")
        with zipfile.ZipFile(archive) as package:
            control = parse_paragraphs(package.read("CONTROL").decode("utf-8"))
        if not control or any(item.get("Architecture") != TRIPLET for item in control):
            raise RuntimeError(f"invalid CONTROL metadata in {archive}")
        for item in control:
            item["Status"] = "install ok installed"
        restored.extend(control)

    all_paragraphs = retained + restored
    all_paragraphs.sort(
        key=lambda item: (
            item.get("Architecture", ""),
            item.get("Package", ""),
            item.get("Feature", ""),
        )
    )
    rendered = "\n\n".join(
        "\n".join(f"{key}: {value}" for key, value in item.items())
        for item in all_paragraphs
    ) + "\n"
    temporary = status_file.with_suffix(".tmp")
    temporary.write_text(rendered, encoding="utf-8", newline="\n")
    os.replace(temporary, status_file)
    return len(restored)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, default=Path("/workspace"))
    parser.add_argument("--recover", action="store_true")
    parser.add_argument("--restore-installed", action="store_true")
    parser.add_argument("--refresh-existing", action="store_true")
    parser.add_argument("--rebuild-status", action="store_true")
    parser.add_argument("--ports", nargs="*", default=[])
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    build_root = workspace / "build" / "gkd350h-glibc234"
    vcpkg_root = build_root / "vcpkg"
    buildtrees = vcpkg_root / "buildtrees"
    installed = build_root / "krkr2" / "vcpkg_installed"
    status_file = installed / "vcpkg" / "status"
    staging_root = workspace / ".local" / "vcpkg-recovery" / "staging"
    binary_cache = vcpkg_root / "binary-cache"
    package_root = vcpkg_root / "packages"
    cmake = workspace / "GKD350HUltra" / "tools" / "cmake" / "bin" / "cmake"

    for required in (buildtrees, status_file, cmake):
        if not required.exists():
            raise SystemExit(f"missing recovery input: {required}")

    paragraphs = parse_status(status_file)
    available = sorted(
        path.parent.name
        for path in buildtrees.glob(f"*/{TRIPLET}.vcpkg_abi_info.txt")
    )
    selected = sorted(set(args.ports or available))
    unknown = sorted(set(selected) - set(available))
    if unknown:
        raise SystemExit(f"ports have no preserved ABI metadata: {', '.join(unknown)}")

    counts: dict[str, int] = {}
    for port in selected:
        port_root = buildtrees / port
        abi_info = port_root / f"{TRIPLET}.vcpkg_abi_info.txt"
        abi = sha256(abi_info)
        archive = binary_cache / abi[:2] / f"{abi}.zip"
        existing_package = package_root / f"{port}_{TRIPLET}"
        if args.refresh_existing and existing_package.is_dir() and archive.is_file():
            archive.unlink()
        if archive.is_file():
            state = "cached"
        else:
            state = "recoverable"
        if args.recover and state == "recoverable":
            state = recover_port(
                port,
                buildtrees,
                paragraphs,
                staging_root,
                binary_cache,
                cmake,
                package_root,
                installed,
            )
        if args.restore_installed and archive.is_file():
            state = restore_archive(port, archive, installed, paragraphs)
        counts[state] = counts.get(state, 0) + 1
        print(f"[vcpkg_recovery] {port}: {state}")

    if args.rebuild_status:
        restored_paragraphs = rebuild_status(
            status_file, paragraphs, selected, buildtrees, binary_cache
        )
        print(
            f"[vcpkg_recovery] rebuilt status: ports={len(selected)} "
            f"paragraphs={restored_paragraphs}"
        )

    summary = " ".join(f"{key}={counts[key]}" for key in sorted(counts))
    print(f"[vcpkg_recovery] summary {summary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
