#!/usr/bin/env python3
"""Install bounded, inert Isolde portable-game bundles; never execute archive content."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import subprocess
import sys
import signal
import tarfile
import zipfile

MAX_ENTRIES = 10000
MAX_EXPANDED = 2 * 1024 * 1024 * 1024
MAX_ARCHIVE = 512 * 1024 * 1024
MAX_TREE_DEPTH = 32
MAX_TREE_PATH_BYTES = 4 * 1024 * 1024
MIN_FREE_RESERVE = 64 * 1024 * 1024
MANIFEST = "isolde-game.json"


class StrictTarInfo(tarfile.TarInfo):
    def _proc_pax(self, archive):
        raise ValueError("Extended tar headers are not supported.")

    def _proc_gnulong(self, archive):
        raise ValueError("Extended tar headers are not supported.")

    def _proc_sparse(self, archive):
        raise ValueError("Sparse tar entries are not supported.")


def open_bundle(stream, archive_format):
    stream.seek(0)
    if archive_format == "zip":
        return zipfile.ZipFile(stream)
    modes = {"ustar": "r:", "ustar-gzip": "r:gz", "ustar-bzip2": "r:bz2"}
    return tarfile.open(fileobj=stream, mode=modes[archive_format],
                        tarinfo=StrictTarInfo)


def bundle_format(stream):
    if zipfile.is_zipfile(stream):
        stream.seek(0)
        return "zip"
    stream.seek(0)
    if stream.read(2) == b"\x1f\x8b":
        stream.seek(0)
        return "ustar-gzip"
    stream.seek(0)
    if stream.read(3) == b"BZh":
        stream.seek(0)
        return "ustar-bzip2"
    stream.seek(0)
    if stream.read(6) == b"\xfd7zXZ\x00":
        raise ValueError("XZ-compressed USTAR bundles are unsupported because their decoder memory cannot be safely bounded.")
    stream.seek(257)
    if stream.read(5) == b"ustar":
        stream.seek(0)
        return "ustar"
    raise ValueError("Choose a native x86-64 Linux ELF or ZIP, USTAR, gzip- or bzip2-compressed USTAR game bundle.")


def bundle_members(archive, archive_format):
    members = []
    total = 0
    names = set()
    for info in archive.infolist() if archive_format == "zip" else archive:
        if len(members) >= MAX_ENTRIES:
            raise ValueError("Portable game bundle has too many or too few entries.")
        if archive_format == "zip":
            raw_name = info.filename
            directory = info.is_dir()
            size = info.file_size
            mode = (info.external_attr >> 16) & 0xFFFF
            file_type = stat.S_IFMT(mode)
            invalid_type = file_type not in (0, stat.S_IFREG, stat.S_IFDIR)
            invalid_encryption = bool(info.flag_bits & 1)
            compressed_size = info.compress_size
        else:
            raw_name = info.name
            directory = info.isdir()
            size = info.size
            mode = info.mode
            invalid_type = not (directory or info.isfile()) or bool(info.pax_headers)
            invalid_encryption = False
            compressed_size = size
        name = raw_name.rstrip("/") if directory and archive_format!="zip" else (
            raw_name[:-1] if directory and raw_name.endswith("/") else raw_name)
        path = PurePosixPath(name)
        canonical_name = "/".join(path.parts) + ("/" if directory else "")
        canonical_match=raw_name==canonical_name or (
            archive_format!="zip" and directory and raw_name==name)
        if (not name or not path.parts or path.is_absolute() or "\\" in name or "\x00" in name
                or len(name.encode("utf-8", "surrogatepass")) > 4096
                or not canonical_match or any(part in ("", ".", "..") for part in path.parts)
                or path.parts[0] == ".isolde-install.json" or invalid_type
                or invalid_encryption or size < 0 or compressed_size > MAX_ARCHIVE):
            raise ValueError("Portable game bundle contains an unsafe or non-canonical entry.")
        if name in names:
            raise ValueError("Portable game bundle contains an unsafe or duplicate path.")
        names.add(name)
        total += size
        if size > MAX_EXPANDED or total > MAX_EXPANDED:
            raise ValueError("Portable game bundle exceeds the 2 GiB unpacked size limit.")
        members.append({"name": name, "directory": directory, "size": size,
                        "mode": mode, "info": info})
    return members


def open_member(archive, member, archive_format):
    if archive_format == "zip":
        return archive.open(member["info"])
    result = archive.extractfile(member["info"])
    if result is None:
        raise ValueError("Portable game archive contains an unreadable file.")
    return result


def inspect_windows(source):
    source = Path(source)
    if source.is_symlink():
        raise ValueError("Choose a regular Windows executable no larger than 512 MiB.")
    descriptor = os.open(source, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, "rb") as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode) or not 64 <= before.st_size <= MAX_ARCHIVE:
            raise ValueError("Choose a regular Windows executable no larger than 512 MiB.")
        header = stream.read(64)
        if header[:2] != b"MZ":
            raise ValueError("The selected file is not a Windows PE executable.")
        pe_offset = struct.unpack_from("<I", header, 60)[0]
        if pe_offset < 64 or pe_offset > min(before.st_size - 24, 1024 * 1024):
            raise ValueError("The Windows PE header offset is invalid.")
        stream.seek(pe_offset)
        pe = stream.read(24)
        if len(pe) != 24 or pe[:4] != b"PE\0\0":
            raise ValueError("The Windows PE header is invalid.")
        machine, sections, _, _, _, optional_size, flags = struct.unpack_from("<HHIIIHH", pe, 4)
        if not 1 <= sections <= 96 or not flags & 2 or flags & 0x2000:
            raise ValueError("The Windows PE executable header is unsupported or invalid.")
        if optional_size < 2 or pe_offset + 24 + optional_size + sections * 40 > before.st_size:
            raise ValueError("The Windows PE headers are truncated.")
        magic_data = stream.read(2)
        if len(magic_data) != 2:
            raise ValueError("The Windows PE optional header is truncated.")
        magic = struct.unpack("<H", magic_data)[0]
        minimum = {0x10b: 96, 0x20b: 112}.get(magic)
        if not minimum or optional_size < minimum:
            raise ValueError("The Windows PE optional header is unsupported or invalid.")
        stream.seek(0)
        digest = hashlib.sha256()
        remaining = before.st_size
        while remaining:
            chunk = stream.read(min(1024 * 1024, remaining))
            if not chunk:
                raise ValueError("The Windows executable changed or became unreadable.")
            digest.update(chunk)
            remaining -= len(chunk)
        after = os.fstat(stream.fileno())
        identity = lambda value: (value.st_dev, value.st_ino, value.st_size,
                                   value.st_mtime_ns, value.st_ctime_ns)
        if identity(before) != identity(after):
            raise ValueError("The Windows executable changed during inspection.")
        architecture = {0x14c: "x86", 0x8664: "x86-64", 0xaa64: "arm64"}.get(machine, "other")
        return {"schemaVersion": 1, "format": "PE32+" if magic == 0x20b else "PE32",
                "architecture": architecture, "fileSize": before.st_size,
                "sha256": digest.hexdigest(), "supported": False}


def install_windows(source):
    source = Path(source)
    if source.is_symlink():
        raise ValueError("Choose a regular Windows executable no larger than 512 MiB.")
    descriptor = os.open(source, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, "rb") as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode) or not 64 <= before.st_size <= MAX_ARCHIVE:
            raise ValueError("Choose a regular Windows executable no larger than 512 MiB.")
        header = stream.read(64)
        if header[:2] != b"MZ":
            raise ValueError("The selected file is not a Windows PE executable.")
        pe_offset = struct.unpack_from("<I", header, 60)[0]
        if pe_offset < 64 or pe_offset > min(before.st_size - 24, 1024 * 1024):
            raise ValueError("The Windows PE header offset is invalid.")
        stream.seek(pe_offset)
        pe = stream.read(24)
        if len(pe) != 24 or pe[:4] != b"PE\0\0":
            raise ValueError("The Windows PE header is invalid.")
    wine_check = subprocess.run(["which", "wine"], capture_output=True, text=True, timeout=5)
    if wine_check.returncode != 0:
        raise ValueError("Wine is not available. Windows installers cannot be executed.")
    try:
        result = subprocess.run(
            ["wine", str(source)],
            capture_output=True, text=True, timeout=300,
            env={"WINEDEBUG": "-all", "WINEDLLOVERRIDES": ""}
        )
    except subprocess.TimeoutExpired:
        raise ValueError("The Windows installer exceeded its time limit. No files were installed.")
    if result.returncode != 0:
        raise ValueError("The Windows installer exited with an error. No files were installed.")
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    return {"schemaVersion": 1, "title": source.stem, "platform": "Windows",
            "entrypoint": source.name, "root": "", "archiveSha256": digest,
            "installerType": "windows-pe", "wineAvailable": True}


def validate_native_executable(stream, size):
    stream.seek(0)
    header=stream.read(64)
    if (len(header)!=64 or header[:6]!=b"\x7fELF\x02\x01" or header[6]!=1
            or struct.unpack_from("<H",header,16)[0] not in (2,3)
            or struct.unpack_from("<H",header,18)[0]!=62
            or struct.unpack_from("<I",header,20)[0]!=1):
        raise ValueError("Choose a native x86-64 Linux ELF executable.")
    if header[8:11] in (b"AI\x01",b"AI\x02"):
        raise ValueError("AppImage files need a separate verified runtime and are not supported.")
    entry,phoff=struct.unpack_from("<QQ",header,24)
    ehsize,phentsize,phnum=struct.unpack_from("<HHH",header,52)
    if (ehsize!=64 or phentsize!=56 or not 1<=phnum<=1024
            or phoff<64 or phoff+phentsize*phnum>size or not entry):
        raise ValueError("The ELF executable has an invalid program header.")
    stream.seek(phoff)
    executable_entry=False
    for _ in range(phnum):
        segment=stream.read(phentsize)
        ptype,flags,offset,vaddr,_,filesz,memsz,_=struct.unpack("<IIQQQQQQ",segment)
        if ptype==1:
            if filesz>memsz or offset+filesz>size:
                raise ValueError("The ELF executable has an invalid load segment.")
            if flags&1 and vaddr<=entry<vaddr+filesz:
                executable_entry=True
    if not executable_entry:
        raise ValueError("The ELF entry point is not in an executable load segment.")


def catalog_text(value, maximum):
    if (not isinstance(value,str) or not value.strip()
            or any(ord(char)<32 or 0xd800<=ord(char)<=0xdfff for char in value)):
        return False
    return len(value.encode("utf-16-le"))//2<=maximum


def inspect(source, title_hint=None):
    source = Path(source)
    if source.is_symlink():
        raise ValueError("Choose a regular portable game bundle no larger than 512 MiB.")
    descriptor=os.open(source,os.O_RDONLY|getattr(os,"O_NOFOLLOW",0)|getattr(os,"O_NONBLOCK",0))
    if not stat.S_ISREG(os.fstat(descriptor).st_mode) or os.fstat(descriptor).st_size>MAX_ARCHIVE:
        os.close(descriptor)
        raise ValueError("Choose a regular portable game bundle no larger than 512 MiB.")
    with os.fdopen(descriptor,"rb") as stream:
        before=os.fstat(stream.fileno())
        digest=hashlib.sha256()
        remaining=before.st_size
        while remaining:
            chunk=stream.read(min(1024*1024,remaining))
            if not chunk:
                raise ValueError("The package changed during preflight. Review it again.")
            digest.update(chunk)
            remaining-=len(chunk)
        stream.seek(0)
        header=stream.read(4)
        if header.startswith(b"\x7fELF"):
            validate_native_executable(stream,before.st_size)
            title=title_hint if title_hint is not None else source.name
            if (not catalog_text(title,256) or "/" in title or "\\" in title):
                raise ValueError("The executable filename cannot be used as a game title.")
            after=os.fstat(stream.fileno())
            identity=lambda value: (value.st_dev,value.st_ino,value.st_size,value.st_mtime_ns,value.st_ctime_ns)
            if identity(before)!=identity(after):
                raise ValueError("The executable changed during preflight. Review it again.")
            return {"schemaVersion":1,"title":title.strip(),"platform":"PC","entrypoint":"bin/game",
                    "entrySize":before.st_size,"archiveEntries":1,"unpackedBytes":before.st_size,
                    "archiveSha256":digest.hexdigest(),"archiveFormat":"native-elf"}
        archive_format=bundle_format(stream)
        with open_bundle(stream,archive_format) as archive:
            members=bundle_members(archive,archive_format)
            if not members or len(members) > MAX_ENTRIES:
                raise ValueError("Portable game bundle has too many or too few entries.")
            total = sum(member["size"] for member in members)
            names={member["name"] for member in members}
            manifest_members=[member for member in members if member["name"]==MANIFEST]
            if len(manifest_members)!=1 or manifest_members[0]["directory"]:
                raise ValueError("Bundle must contain a valid isolde-game.json manifest.")
            manifest_member=manifest_members[0]
            if manifest_member["size"]>65536:
                raise ValueError("Portable game manifest exceeds its 64 KiB limit.")
            try:
                with open_member(archive,manifest_member,archive_format) as manifest_stream:
                    manifest=json.loads(manifest_stream.read(65537))
            except (UnicodeDecodeError, json.JSONDecodeError):
                raise ValueError("Bundle must contain a valid isolde-game.json manifest.") from None
            if (not isinstance(manifest, dict) or set(manifest) != {"schemaVersion", "title", "platform", "entrypoint"}
                    or type(manifest["schemaVersion"]) is not int or manifest["schemaVersion"] != 1
                    or not catalog_text(manifest["title"],256)
                    or not catalog_text(manifest["platform"],80)
                    or not isinstance(manifest["entrypoint"], str)):
                raise ValueError("Portable game manifest has invalid fields.")
            entry = PurePosixPath(manifest["entrypoint"])
            if (entry.is_absolute() or "\\" in manifest["entrypoint"]
                    or any(part in ("", ".", "..") for part in entry.parts)
                    or manifest["entrypoint"] not in names
                    or next(member for member in members if member["name"]==manifest["entrypoint"])["directory"]):
                raise ValueError("Portable game entry point must name a file inside the bundle.")
            entry_info=next(member for member in members if member["name"]==manifest["entrypoint"])
            if entry_info["size"] > MAX_ARCHIVE:raise ValueError("Game entry point exceeds the 512 MiB executable limit.")
            with open_member(archive,entry_info,archive_format) as entry_stream:executable=entry_stream.read(20)
            if not executable.startswith(b"\x7fELF") or len(executable) < 20:
                raise ValueError("Only native Linux ELF game entry points are supported by this installer.")
            after=os.fstat(stream.fileno())
            identity=lambda value: (value.st_dev,value.st_ino,value.st_size,value.st_mtime_ns,value.st_ctime_ns)
            if identity(before)!=identity(after):
                raise ValueError("The package changed during preflight. Review it again.")
            return {"schemaVersion": 1, "title": manifest["title"].strip(),
                    "platform": manifest["platform"].strip(), "entrypoint": manifest["entrypoint"],
                    "entrySize": entry_info["size"], "archiveEntries": len(members), "unpackedBytes": total,
                    "archiveSha256": digest.hexdigest(),"archiveFormat":archive_format}


def _directory_identity(value):
    return (value.st_dev, value.st_ino, value.st_size, value.st_mode,
            value.st_mtime_ns, value.st_ctime_ns, value.st_nlink)


def _open_directory_root(source):
    root = Path(source)
    if root.is_symlink() or not root.is_dir() or root.resolve(strict=True) != root.absolute():
        raise ValueError("Choose a real game directory inside an approved media folder.")
    before = os.stat(root, follow_symlinks=False)
    flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) | getattr(os, "O_NOFOLLOW", 0)
    descriptor = os.open(root, flags)
    try:
        opened = os.fstat(descriptor)
    except Exception:
        os.close(descriptor)
        raise
    if (not stat.S_ISDIR(before.st_mode) or not stat.S_ISDIR(opened.st_mode)
            or _directory_identity(before) != _directory_identity(opened)):
        os.close(descriptor)
        raise ValueError("The game directory changed while it was being opened.")
    return root, descriptor, _directory_identity(opened)


def _verify_directory_root(root, descriptor, expected_identity):
    opened = os.fstat(descriptor)
    try:
        current = os.stat(root, follow_symlinks=False)
    except OSError:
        raise ValueError("The game directory root changed during inspection or copy.") from None
    if (_directory_identity(opened) != expected_identity
            or _directory_identity(current) != expected_identity
            or not stat.S_ISDIR(current.st_mode)):
        raise ValueError("The game directory root changed during inspection or copy.")


def _safe_tree_path(relative):
    path = PurePosixPath(relative)
    if (not relative or path.is_absolute() or "\\" in relative or "\x00" in relative
            or len(relative.encode("utf-8", "strict")) > 4096
            or any(part in ("", ".", "..") for part in path.parts)
            or path.as_posix() != relative or path.parts[0] == ".isolde-install.json"):
        raise ValueError("The game directory contains an unsafe path.")
    return path


def _read_directory_file(parent_fd, name, relative, total, expected_stat):
    flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0)
    descriptor = os.open(name, flags, dir_fd=parent_fd)
    with os.fdopen(descriptor, "rb") as source:
        before = os.fstat(source.fileno())
        if (not stat.S_ISREG(before.st_mode) or before.st_nlink != 1
                or _directory_identity(expected_stat) != _directory_identity(before)):
            raise ValueError("Game directory files must be regular files without hard links and must not change during inspection.")
        if before.st_size < 0 or before.st_size > MAX_ARCHIVE or total + before.st_size > MAX_EXPANDED:
            raise ValueError("Game directory exceeds the 2 GiB total or 512 MiB per-file limit.")
        digest = hashlib.sha256()
        remaining = before.st_size
        while remaining:
            chunk = source.read(min(1024 * 1024, remaining))
            if not chunk:
                raise ValueError("A game directory file changed or became unreadable.")
            digest.update(chunk)
            remaining -= len(chunk)
        if source.read(1):
            raise ValueError("A game directory file grew during inspection.")
        after = os.fstat(source.fileno())
        if _directory_identity(before) != _directory_identity(after):
            raise ValueError("A game directory file changed during inspection.")
        return {"path": relative, "size": before.st_size, "mode": before.st_mode & 0o111,
                "sha256": digest.hexdigest()}, total + before.st_size


def _inspect_directory_tree(source):
    root, root_fd, root_identity = _open_directory_root(source)
    records = []
    files = []
    directories = []
    total = 0
    path_bytes = 0

    def walk(directory_fd, prefix, depth):
        nonlocal total, path_bytes
        if depth > MAX_TREE_DEPTH:
            raise ValueError("Game directory exceeds the 32-level path limit.")
        names = []
        try:
            with os.scandir(directory_fd) as entries:
                for entry in entries:
                    if len(records) + len(names) >= MAX_ENTRIES:
                        raise ValueError("Game directory exceeds the 10,000-entry limit.")
                    names.append(entry.name)
            names.sort(key=lambda item: item.encode("utf-8", "strict"))
        except UnicodeEncodeError:
            raise ValueError("Game directory contains a filename that is not valid UTF-8.") from None
        for name in names:
            if (not name or name in (".", "..") or any(ord(char) < 32 for char in name)
                    or any(0xD800 <= ord(char) <= 0xDFFF for char in name)):
                raise ValueError("Game directory contains an unsupported filename.")
            relative = f"{prefix}/{name}" if prefix else name
            parts = _safe_tree_path(relative)
            path_bytes += len(relative.encode("utf-8", "strict"))
            if path_bytes > MAX_TREE_PATH_BYTES:
                raise ValueError("Game directory path inventory exceeds the 4 MiB limit.")
            if len(records) >= MAX_ENTRIES:
                raise ValueError("Game directory exceeds the 10,000-entry limit.")
            entry_stat = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
            if stat.S_ISLNK(entry_stat.st_mode):
                raise ValueError("Game directories cannot contain symbolic links.")
            if stat.S_ISDIR(entry_stat.st_mode):
                child_flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) | getattr(os, "O_NOFOLLOW", 0)
                child_fd = os.open(name, child_flags, dir_fd=directory_fd)
                try:
                    opened = os.fstat(child_fd)
                    if _directory_identity(entry_stat) != _directory_identity(opened):
                        raise ValueError("A game directory changed during inspection.")
                    record = {"type": "directory", "path": relative, "mode": opened.st_mode & 0o111}
                    records.append(record)
                    directories.append(relative)
                    walk(child_fd, relative, depth + 1)
                    after = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
                    if _directory_identity(opened) != _directory_identity(after):
                        raise ValueError("A game directory changed during inspection.")
                finally:
                    os.close(child_fd)
            elif stat.S_ISREG(entry_stat.st_mode):
                record, total = _read_directory_file(directory_fd, name, relative, total, entry_stat)
                record["type"] = "file"
                records.append(record)
                files.append(record)
            else:
                raise ValueError("Game directories cannot contain devices, sockets or other special files.")
            if len(records) > MAX_ENTRIES:
                raise ValueError("Game directory exceeds the 10,000-entry limit.")

    try:
        walk(root_fd, "", 1)
        _verify_directory_root(root, root_fd, root_identity)
    finally:
        os.close(root_fd)
    digest = hashlib.sha256()
    for record in records:
        encoded = json.dumps(record, ensure_ascii=False, sort_keys=True,
                             separators=(",", ":")).encode("utf-8", "strict")
        digest.update(struct.pack(">I", len(encoded)))
        digest.update(encoded)
    return {"files": files, "directories": directories, "total": total,
            "archiveSha256": digest.hexdigest(), "archiveEntries": len(records),
            "rootIdentity": root_identity}


def inspect_directory(source, entrypoint):
    root = Path(source)
    relative = _safe_tree_path(entrypoint)
    inventory = _inspect_directory_tree(root)
    selected = next((item for item in inventory["files"] if item["path"] == relative.as_posix()), None)
    if not selected or not selected["mode"]:
        raise ValueError("Choose an executable file inside the selected game directory.")
    _, root_fd, root_identity = _open_directory_root(root)
    if root_identity != inventory["rootIdentity"]:
        os.close(root_fd)
        raise ValueError("The game directory root changed after inspection.")
    current_fd = root_fd
    try:
        for part in relative.parts[:-1]:
            next_fd = os.open(part, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0) | getattr(os, "O_NOFOLLOW", 0),
                              dir_fd=current_fd)
            if current_fd != root_fd:
                os.close(current_fd)
            current_fd = next_fd
        descriptor = os.open(relative.parts[-1], os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
                             | getattr(os, "O_NONBLOCK", 0),
                             dir_fd=current_fd)
        with os.fdopen(descriptor, "rb") as executable:
            before = os.fstat(executable.fileno())
            if (not stat.S_ISREG(before.st_mode) or before.st_nlink != 1
                    or before.st_size != selected["size"]):
                raise ValueError("The selected game executable changed during inspection.")
            validate_native_executable(executable, before.st_size)
            executable.seek(0)
            digest = hashlib.sha256()
            for chunk in iter(lambda: executable.read(1024 * 1024), b""):
                digest.update(chunk)
            if digest.hexdigest() != selected["sha256"]:
                raise ValueError("The selected game executable changed after preflight.")
            after = os.fstat(executable.fileno())
            if _directory_identity(before) != _directory_identity(after):
                raise ValueError("The selected game executable changed during inspection.")
        _verify_directory_root(root, root_fd, root_identity)
    finally:
        if current_fd != root_fd:
            os.close(current_fd)
        os.close(root_fd)
    title = root.name
    if not catalog_text(title,256):
        raise ValueError("The selected game directory name cannot be used as a title.")
    return {"schemaVersion": 1, "title": title.strip(), "platform": "PC",
            "entrypoint": relative.as_posix(), "entrySize": selected["size"],
            "archiveEntries": inventory["archiveEntries"], "unpackedBytes": inventory["total"],
            "archiveSha256": inventory["archiveSha256"], "archiveFormat": "directory-copy"}


def install_directory(source, destination, entrypoint, expected_sha256):
    source = Path(source)
    destination = Path(destination)
    stage = destination.with_name(destination.name + ".staging")
    if (destination.exists() or destination.is_symlink() or stage.exists() or stage.is_symlink()
            or destination.with_name(destination.name + ".staging-source").exists()):
        raise ValueError("Install destination already exists. Existing files will not be overwritten.")
    destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    if destination.parent.is_symlink() or destination.parent.resolve() != destination.parent.absolute():
        raise ValueError("Install destination parent must not be a symlink.")
    metadata = inspect_directory(source, entrypoint)
    if metadata["archiveSha256"] != expected_sha256:
        raise ValueError("The game directory changed after preflight. Review it again before installing.")
    free = shutil.disk_usage(destination.parent).free
    if free < metadata["unpackedBytes"] + MIN_FREE_RESERVE:
        raise ValueError("Insufficient free space for this game directory and the 64 MiB safety reserve.")
    stage.mkdir(mode=0o700)
    def interrupted(signum, frame):
        raise InterruptedError("Installation canceled")
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    try:
        _, root_fd, root_identity = _open_directory_root(source)
        try:
            inventory = _inspect_directory_tree(source)
            if (inventory["archiveSha256"] != expected_sha256
                    or inventory["rootIdentity"] != root_identity):
                raise ValueError("The game directory changed after preflight. Review it again before installing.")
            for name in sorted(inventory["directories"], key=lambda value: (value.count("/"), value)):
                target = stage.joinpath(*PurePosixPath(name).parts)
                target.mkdir(mode=0o700, parents=True, exist_ok=True)
            written = 0
            for item in inventory["files"]:
                remaining = item["size"]
                if shutil.disk_usage(destination.parent).free < remaining + MIN_FREE_RESERVE:
                    raise ValueError("Free space fell below the game-copy safety reserve; staged files were discarded.")
                parts = PurePosixPath(item["path"]).parts
                source_fd = os.dup(root_fd)
                try:
                    for part in parts[:-1]:
                        child_fd = os.open(part, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
                                           | getattr(os, "O_NOFOLLOW", 0), dir_fd=source_fd)
                        os.close(source_fd)
                        source_fd = child_fd
                    input_fd = os.open(parts[-1], os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
                                       | getattr(os, "O_NONBLOCK", 0), dir_fd=source_fd)
                finally:
                    os.close(source_fd)
                target = stage.joinpath(*parts)
                try:
                    output_fd = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL
                                        | getattr(os, "O_NOFOLLOW", 0), 0o700 if item["mode"] else 0o600)
                except Exception:
                    os.close(input_fd)
                    raise
                digest = hashlib.sha256()
                with os.fdopen(input_fd, "rb") as input_file, os.fdopen(output_fd, "wb") as output_file:
                    before = os.fstat(input_file.fileno())
                    if not stat.S_ISREG(before.st_mode) or before.st_nlink != 1 or before.st_size != item["size"]:
                        raise ValueError("A game directory file changed during copy.")
                    while remaining:
                        chunk = input_file.read(min(1024 * 1024, remaining))
                        if not chunk:
                            raise ValueError("A game directory file changed or became unreadable during copy.")
                        digest.update(chunk)
                        output_file.write(chunk)
                        remaining -= len(chunk)
                        written += len(chunk)
                        if written > MAX_EXPANDED:
                            raise ValueError("Game directory expanded beyond the 2 GiB copy limit.")
                    if input_file.read(1) or digest.hexdigest() != item["sha256"]:
                        raise ValueError("A game directory file changed after preflight; staged files were discarded.")
                    after = os.fstat(input_file.fileno())
                    if _directory_identity(before) != _directory_identity(after):
                        raise ValueError("A game directory file changed during copy.")
                    output_file.flush()
                    os.fsync(output_file.fileno())
            after_copy = inspect_directory(source, entrypoint)
            if after_copy["archiveSha256"] != expected_sha256:
                raise ValueError("The game directory changed during copy; staged files were discarded.")
            _verify_directory_root(source, root_fd, root_identity)
            entry = stage.joinpath(*PurePosixPath(entrypoint).parts)
            if (entry.is_symlink() or not entry.is_file() or entry.stat().st_size != metadata["entrySize"]):
                raise ValueError("Installed game entry point failed destination verification.")
            entry.chmod(0o700)
            with entry.open("rb") as installed_entry:
                validate_native_executable(installed_entry, metadata["entrySize"])
            owned_files = []
            owned_directories = []
            for path in stage.rglob("*"):
                relative = path.relative_to(stage).as_posix()
                if path.is_symlink():
                    raise ValueError("Install staging unexpectedly contains a symbolic link.")
                if path.is_dir():
                    owned_directories.append(relative)
                elif path.is_file():
                    file_digest = hashlib.sha256()
                    with path.open("rb") as installed_file:
                        for chunk in iter(lambda: installed_file.read(1024 * 1024), b""):
                            file_digest.update(chunk)
                    owned_files.append({"path": relative, "size": path.stat().st_size,
                                        "sha256": file_digest.hexdigest()})
                else:
                    raise ValueError("Install staging contains an unsupported file type.")
            marker = stage / ".isolde-install.json"
            ownership = {**metadata, "ownershipSchemaVersion": 1,
                         "ownedFiles": owned_files, "ownedDirectories": owned_directories}
            marker_bytes = json.dumps(ownership, ensure_ascii=False).encode("utf-8")
            if len(marker_bytes) > 16 * 1024 * 1024:
                raise ValueError("The managed install ownership record exceeds its 16 MiB limit.")
            with marker.open("wb") as marker_file:
                marker_file.write(marker_bytes)
                marker_file.flush()
                os.fsync(marker_file.fileno())
            marker.chmod(0o600)
            for directory in sorted((path for path in stage.rglob("*") if path.is_dir()),
                                    key=lambda path: len(path.parts), reverse=True) + [stage]:
                descriptor = os.open(directory, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
                try:
                    os.fsync(descriptor)
                finally:
                    os.close(descriptor)
            rename_without_replace(stage, destination)
            descriptor = os.open(destination.parent, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
            try:
                os.fsync(descriptor)
            finally:
                os.close(descriptor)
        finally:
            os.close(root_fd)
    except Exception:
        shutil.rmtree(stage, ignore_errors=True)
        raise
    print(json.dumps(metadata, ensure_ascii=False))


def install(source, destination, expected_sha256=None):
    source = Path(source)
    destination = Path(destination)
    stage = destination.with_name(destination.name + ".staging")
    snapshot = destination.with_name(destination.name + ".staging-source")
    if (destination.exists() or destination.is_symlink() or stage.exists() or stage.is_symlink()
            or snapshot.exists() or snapshot.is_symlink()):
        raise ValueError("Install destination already exists. Existing files will not be overwritten.")
    destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    if destination.parent.is_symlink() or destination.parent.resolve() != destination.parent.absolute():
        raise ValueError("Install destination parent must not be a symlink.")
    stage.mkdir(mode=0o700)
    def interrupted(signum, frame):
        raise InterruptedError("Installation canceled")
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    try:
        if source.is_symlink() or not source.is_file():
            raise ValueError("Choose a regular portable game bundle.")
        source_fd = os.open(source, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
        if not stat.S_ISREG(os.fstat(source_fd).st_mode):
            os.close(source_fd)
            raise ValueError("Choose a regular portable game bundle.")
        try:
            snapshot_fd = os.open(snapshot, os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0), 0o600)
        except Exception:
            os.close(source_fd)
            raise
        with os.fdopen(source_fd, "rb") as input_file, os.fdopen(snapshot_fd, "wb") as saved:
            copied_archive=0
            while True:
                chunk=input_file.read(1024*1024)
                if not chunk:break
                copied_archive+=len(chunk)
                if copied_archive>MAX_ARCHIVE:raise ValueError("Choose a regular portable game bundle no larger than 512 MiB.")
                saved.write(chunk)
            saved.flush();os.fsync(saved.fileno())
        metadata=inspect(snapshot, title_hint=source.name)
        if expected_sha256 and metadata["archiveSha256"]!=expected_sha256:
            raise ValueError("The package changed after preflight. Review it again before installing.")
        if metadata["archiveFormat"]=="native-elf":
            target=stage/"bin"/"game"
            target.parent.mkdir(mode=0o700)
            descriptor=os.open(target,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o700)
            with os.fdopen(descriptor,"wb") as output, snapshot.open("rb") as input_file:
                shutil.copyfileobj(input_file,output,1024*1024)
                output.flush();os.fsync(output.fileno())
        else:
            with snapshot.open("rb") as source_stream, open_bundle(source_stream,metadata["archiveFormat"]) as archive:
                members=bundle_members(archive,metadata["archiveFormat"])
                written=0
                for member in members:
                    relative = PurePosixPath(member["name"])
                    target = stage.joinpath(*relative.parts)
                    if member["directory"]:
                        target.mkdir(mode=0o700, parents=True, exist_ok=True)
                        continue
                    target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
                    if hasattr(os, "O_NOFOLLOW"):
                        flags |= os.O_NOFOLLOW
                    descriptor = os.open(target, flags, 0o600)
                    copied=0
                    mode=member["mode"]&0o111
                    with os.fdopen(descriptor, "wb") as output, open_member(archive,member,metadata["archiveFormat"]) as input_file:
                        while True:
                            chunk=input_file.read(1024*1024)
                            if not chunk:break
                            copied+=len(chunk);written+=len(chunk)
                            if copied>member["size"] or written>MAX_EXPANDED:
                                raise ValueError("Portable game archive expanded beyond its declared size.")
                            output.write(chunk)
                        if copied!=member["size"]:raise ValueError("Portable game archive member size changed.")
                        output.flush()
                        os.fsync(output.fileno())
                    if mode:target.chmod(0o700)
        entry = stage.joinpath(*PurePosixPath(metadata["entrypoint"]).parts)
        if (entry.is_symlink() or not entry.is_file()
                or entry.resolve(strict=True) != entry.absolute()
                or entry.stat().st_size != metadata["entrySize"]):
            raise ValueError("Installed entry point failed destination verification.")
        entry.chmod(0o700)
        with entry.open("rb") as installed_entry:
            if installed_entry.read(20)[:4] != b"\x7fELF":
                raise ValueError("Installed entry point failed ELF verification.")
        owned_files = []
        owned_directories = []
        for path in stage.rglob("*"):
            relative = path.relative_to(stage).as_posix()
            if path.is_symlink():
                raise ValueError("Install staging unexpectedly contains a symbolic link.")
            if path.is_dir():
                owned_directories.append(relative)
            elif path.is_file():
                file_digest = hashlib.sha256()
                with path.open("rb") as installed_file:
                    for chunk in iter(lambda: installed_file.read(1024 * 1024), b""):
                        file_digest.update(chunk)
                owned_files.append({"path": relative, "size": path.stat().st_size,
                                    "sha256": file_digest.hexdigest()})
            else:
                raise ValueError("Install staging contains an unsupported file type.")
        marker = stage / ".isolde-install.json"
        with marker.open("w",encoding="utf-8") as marker_file:
            marker_file.write(json.dumps({**metadata, "ownershipSchemaVersion": 1,
                                          "ownedFiles": owned_files,
                                          "ownedDirectories": owned_directories},
                                         ensure_ascii=False))
            marker_file.flush();os.fsync(marker_file.fileno())
        os.chmod(marker, 0o600)
        for directory in sorted((path for path in stage.rglob("*") if path.is_dir()),key=lambda path:len(path.parts),reverse=True)+[stage]:
            descriptor=os.open(directory,os.O_RDONLY|getattr(os,"O_DIRECTORY",0))
            try:os.fsync(descriptor)
            finally:os.close(descriptor)
        rename_without_replace(stage, destination)
        descriptor=os.open(destination.parent,os.O_RDONLY|getattr(os,"O_DIRECTORY",0))
        try:os.fsync(descriptor)
        finally:os.close(descriptor)
    except Exception:
        shutil.rmtree(stage, ignore_errors=True)
        raise
    finally:
        try:snapshot.unlink()
        except FileNotFoundError:pass
    print(json.dumps(metadata, ensure_ascii=False))


def verify_install(root, expected_name, allow_missing=False):
    root = Path(root)
    if (root.name != expected_name or root.is_symlink() or not root.is_dir()
            or root.resolve(strict=True) != root.absolute()):
        raise ValueError("Choose a verified Isolde-managed install folder.")
    marker = root / ".isolde-install.json"
    if (marker.is_symlink() or not marker.is_file() or marker.resolve(strict=True) != marker.absolute()
            or marker.stat().st_size > 16 * 1024 * 1024):
        raise ValueError("The install ownership marker is missing or redirected; files were preserved.")
    try:
        with marker.open("rb") as marker_file:
            marker_bytes = marker_file.read(16 * 1024 * 1024 + 1)
        if len(marker_bytes) > 16 * 1024 * 1024:
            raise ValueError("The install ownership marker is too large; files were preserved.")
        ownership = json.loads(marker_bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        raise ValueError("The install ownership marker is invalid; files were preserved.") from None
    files = ownership.get("ownedFiles") if isinstance(ownership, dict) else None
    directories = ownership.get("ownedDirectories") if isinstance(ownership, dict) else None
    if (not isinstance(ownership, dict) or ownership.get("ownershipSchemaVersion") != 1
            or not isinstance(files, list) or not isinstance(directories, list)
            or len(files) > MAX_ENTRIES or len(directories) > MAX_ENTRIES
            or len(files) + len(directories) > MAX_ENTRIES):
        raise ValueError("The install ownership manifest is incomplete; files were preserved.")
    expected_files = {}
    for item in files:
        if not isinstance(item, dict) or set(item) != {"path", "size", "sha256"}:
            raise ValueError("The install ownership manifest is invalid; files were preserved.")
        name, size, digest = item["path"], item["size"], item["sha256"]
        path = PurePosixPath(name) if isinstance(name, str) else PurePosixPath(".")
        if (not isinstance(name, str) or not name or path.is_absolute() or "\\" in name
                or any(part in ("", ".", "..") for part in path.parts)
                or name == ".isolde-install.json" or name in expected_files
                or type(size) is not int or size < 0
                or not isinstance(digest, str) or len(digest) != 64
                or any(ch not in "0123456789abcdef" for ch in digest)):
            raise ValueError("The install ownership manifest is invalid; files were preserved.")
        expected_files[name] = (size, digest)
    expected_directories = set()
    for name in directories:
        path = PurePosixPath(name) if isinstance(name, str) else PurePosixPath(".")
        if (not isinstance(name, str) or not name or path.is_absolute() or "\\" in name
                or any(part in ("", ".", "..") for part in path.parts)
                or name in expected_directories):
            raise ValueError("The install ownership manifest is invalid; files were preserved.")
        expected_directories.add(name)
    actual_files = set()
    actual_directories = set()
    for path in root.rglob("*"):
        if len(actual_files) + len(actual_directories) >= MAX_ENTRIES + 1:
            raise ValueError("The install contains too many files or directories; files were preserved.")
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            raise ValueError("The install contains a symbolic link; files were preserved.")
        if path == marker:
            continue
        if path.is_dir():
            actual_directories.add(relative)
        elif path.is_file():
            actual_files.add(relative)
        else:
            raise ValueError("The install contains an unsupported file type; files were preserved.")
    if (not actual_files.issubset(expected_files) or not actual_directories.issubset(expected_directories)
            or (not allow_missing and (actual_files != set(expected_files)
                                       or actual_directories != expected_directories))):
        raise ValueError("The install contains missing or unowned files; nothing was deleted.")
    for name in actual_files:
        size, expected_digest = expected_files[name]
        path = root.joinpath(*PurePosixPath(name).parts)
        digest = hashlib.sha256()
        with path.open("rb") as installed_file:
            for chunk in iter(lambda: installed_file.read(1024 * 1024), b""):
                digest.update(chunk)
        if path.stat().st_size != size or digest.hexdigest() != expected_digest:
            raise ValueError("An installed file changed after installation; nothing was deleted.")
    if ".isolde-install.json" not in {path.name for path in root.iterdir()}:
        raise ValueError("The install ownership marker changed; files were preserved.")
    return ({name: expected_files[name] for name in actual_files}, actual_directories,
            (len(marker_bytes), hashlib.sha256(marker_bytes).hexdigest()))


def inspect_installed(root):
    root = Path(root)
    verify_install(root, "install")
    marker = root / ".isolde-install.json"
    with marker.open("rb") as marker_file:
        ownership = json.loads(marker_file.read(16 * 1024 * 1024 + 1).decode("utf-8"))
    title, platform, entrypoint = ownership.get("title"), ownership.get("platform"), ownership.get("entrypoint")
    archive_digest = ownership.get("archiveSha256")
    entry_size = ownership.get("entrySize")
    entry = PurePosixPath(entrypoint) if isinstance(entrypoint, str) else PurePosixPath(".")
    if (type(ownership.get("schemaVersion")) is not int or ownership["schemaVersion"] != 1
            or not isinstance(title, str) or not title.strip() or len(title) > 256
            or any(ord(char) < 32 for char in title)
            or not isinstance(platform, str) or not platform.strip() or len(platform) > 80
            or any(ord(char) < 32 for char in platform)
            or not isinstance(entrypoint, str) or entry.is_absolute() or "\\" in entrypoint
            or len(entrypoint) > 4096 or entrypoint != entry.as_posix()
            or any(part in ("", ".", "..") for part in entry.parts)
            or type(entry_size) is not int or not 1 <= entry_size <= MAX_ARCHIVE
            or not isinstance(archive_digest, str) or len(archive_digest) != 64
            or any(char not in "0123456789abcdef" for char in archive_digest)):
        raise ValueError("The installed game metadata is invalid; files were preserved.")
    executable = root.joinpath(*entry.parts)
    if (executable.is_symlink() or not executable.is_file()
            or executable.resolve(strict=True) != executable.absolute()
            or executable.stat().st_size != entry_size
            or not executable.stat().st_mode & 0o111):
        raise ValueError("The installed game entry point is not a verified executable; files were preserved.")
    with executable.open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            raise ValueError("The installed game entry point is not a Linux ELF file; files were preserved.")
    return {"schemaVersion": 1, "title": title.strip(), "platform": platform.strip(),
            "entrypoint": entrypoint, "archiveSha256": archive_digest}


def open_owned_directory(root_fd, parts):
    current = os.dup(root_fd)
    try:
        for part in parts:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=current)
            os.close(current)
            current = child
        return current
    except Exception:
        os.close(current)
        raise


def remove_owned_files(pending, files, directories, marker):
    root_fd = os.open(pending, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        def remove_file(name, size, expected_digest):
            parts = PurePosixPath(name).parts
            parent = open_owned_directory(root_fd, parts[:-1])
            try:
                fd = os.open(parts[-1], os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=parent)
                try:
                    before = os.fstat(fd)
                    if not stat.S_ISREG(before.st_mode) or before.st_size != size:
                        raise ValueError("An installed file changed during cleanup; remaining files were preserved.")
                    digest = hashlib.sha256()
                    remaining = size + 1
                    while remaining:
                        chunk = os.read(fd, min(1024 * 1024, remaining))
                        if not chunk:
                            break
                        digest.update(chunk)
                        remaining -= len(chunk)
                    after = os.fstat(fd)
                    current = os.stat(parts[-1], dir_fd=parent, follow_symlinks=False)
                    identity = lambda value: (value.st_dev, value.st_ino, value.st_size,
                                               value.st_mtime_ns, value.st_ctime_ns)
                    if identity(before) != identity(after) or identity(after) != identity(current) or digest.hexdigest() != expected_digest:
                        raise ValueError("An installed file changed during cleanup; remaining files were preserved.")
                    os.unlink(parts[-1], dir_fd=parent)
                finally:
                    os.close(fd)
            finally:
                os.close(parent)
        for name, (size, expected_digest) in sorted(files.items()):
            remove_file(name, size, expected_digest)
        for name in sorted(directories, key=lambda value: len(PurePosixPath(value).parts), reverse=True):
            parts = PurePosixPath(name).parts
            parent = open_owned_directory(root_fd, parts[:-1])
            try:
                os.rmdir(parts[-1], dir_fd=parent)
            finally:
                os.close(parent)
        if set(os.listdir(root_fd)) != {".isolde-install.json"}:
            raise ValueError("The install acquired unowned files during cleanup; remaining files were preserved.")
        remove_file(".isolde-install.json", *marker)
        root_stat = os.fstat(root_fd)
        current = os.stat(pending, follow_symlinks=False)
        if (root_stat.st_dev, root_stat.st_ino) != (current.st_dev, current.st_ino):
            raise ValueError("The pending folder changed during cleanup; remaining files were preserved.")
        os.rmdir(pending)
    finally:
        os.close(root_fd)


def sync_directory(path):
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def rename_without_replace(source, destination):
    libc = ctypes.CDLL(None, use_errno=True)
    rename = getattr(libc, "renameat2", None)
    if rename is None:
        raise OSError("Atomic no-replace rename is unavailable; files were preserved.")
    rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
    rename.restype = ctypes.c_int
    if rename(-100, os.fsencode(source), -100, os.fsencode(destination), 1) != 0:
        error = ctypes.get_errno()
        raise OSError(error, os.strerror(error), str(destination))


def prepare_uninstall(root, report=True):
    root = Path(root)
    pending = root.with_name("install.uninstall-pending")
    if pending.exists() or pending.is_symlink():
        raise ValueError("A previous uninstall needs recovery; existing files were preserved.")
    verify_install(root, "install")
    rename_without_replace(root, pending)
    sync_directory(root.parent)
    if report:
        print(json.dumps({"schemaVersion": 1, "staged": True}))


def recover_uninstall(root):
    root = Path(root)
    pending = root.with_name("install.uninstall-pending")
    if root.name != "install" or root.exists() or root.is_symlink():
        raise ValueError("Cannot restore over an existing installation.")
    verify_install(pending, "install.uninstall-pending")
    rename_without_replace(pending, root)
    sync_directory(root.parent)
    print(json.dumps({"schemaVersion": 1, "restored": True}))


def finalize_uninstall(root, allow_missing=False):
    root = Path(root)
    if root.name != "install":
        raise ValueError("Choose a verified Isolde-managed install folder.")
    pending = root.with_name("install.uninstall-pending")
    if root.exists() or root.is_symlink():
        raise ValueError("The installation is still present; nothing was deleted.")
    files, directories, marker = verify_install(pending, "install.uninstall-pending", allow_missing=allow_missing)
    remove_owned_files(pending, files, directories, marker)
    sync_directory(root.parent)
    print(json.dumps({"schemaVersion": 1, "removed": True}))


def uninstall(root):
    prepare_uninstall(root, report=False)
    try:
        finalize_uninstall(root)
    except Exception:
        pending = Path(root).with_name("install.uninstall-pending")
        if not Path(root).exists() and pending.exists():
            rename_without_replace(pending, root)
        raise


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--inspect", action="store_true")
    group.add_argument("--inspect-directory", action="store_true")
    group.add_argument("--inspect-windows", action="store_true")
    group.add_argument("--install-windows", action="store_true")
    group.add_argument("--install", action="store_true")
    group.add_argument("--install-directory", action="store_true")
    group.add_argument("--uninstall", action="store_true")
    group.add_argument("--prepare-uninstall", action="store_true")
    group.add_argument("--finalize-uninstall", action="store_true")
    group.add_argument("--recover-uninstall", action="store_true")
    group.add_argument("--resume-uninstall", action="store_true")
    group.add_argument("--inspect-installed", action="store_true")
    parser.add_argument("--source")
    parser.add_argument("--destination")
    parser.add_argument("--root")
    parser.add_argument("--expected-sha256")
    parser.add_argument("--entrypoint")
    args = parser.parse_args()
    if args.inspect or args.inspect_windows or args.inspect_directory:
        if not args.source:
            parser.error("--inspect requires --source")
        if args.inspect_windows:
            result = inspect_windows(args.source)
        elif args.inspect_directory:
            if not args.entrypoint:
                parser.error("--inspect-directory requires --entrypoint")
            result = inspect_directory(args.source, args.entrypoint)
        else:
            result = inspect(args.source)
        print(json.dumps(result, ensure_ascii=False))
    elif args.install_windows:
        if not args.source:
            parser.error("--install-windows requires --source")
        result = install_windows(args.source)
        print(json.dumps(result, ensure_ascii=False))
    elif args.inspect_installed:
        if not args.root:
            parser.error("--inspect-installed requires --root")
        print(json.dumps(inspect_installed(args.root), ensure_ascii=False))
    elif args.uninstall or args.prepare_uninstall or args.finalize_uninstall or args.recover_uninstall or args.resume_uninstall:
        if not args.root:
            parser.error("--uninstall requires --root")
        if args.prepare_uninstall:
            prepare_uninstall(args.root)
        elif args.finalize_uninstall or args.resume_uninstall:
            finalize_uninstall(args.root, allow_missing=args.resume_uninstall)
        elif args.recover_uninstall:
            recover_uninstall(args.root)
        else:
            uninstall(args.root)
    else:
        if not args.destination or not args.source:
            parser.error("--install requires --source and --destination")
        if args.install_directory:
            if not args.entrypoint or not args.expected_sha256:
                parser.error("--install-directory requires --entrypoint and --expected-sha256")
            install_directory(args.source, args.destination, args.entrypoint, args.expected_sha256)
        else:
            install(args.source, args.destination,args.expected_sha256)


if __name__ == "__main__":
    try:
        main()
    except (OSError, EOFError, ValueError, zipfile.BadZipFile, tarfile.TarError, RuntimeError) as error:
        print("Portable game install: " + str(error), file=sys.stderr)
        sys.exit(2)
