#!/usr/bin/env python3
"""Install bounded, inert Isolde portable-game bundles; never execute archive content."""
import argparse
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import signal
import zipfile

MAX_ENTRIES = 10000
MAX_EXPANDED = 2 * 1024 * 1024 * 1024
MAX_ARCHIVE = 512 * 1024 * 1024
MANIFEST = "isolde-game.json"


def inspect(source):
    source = Path(source)
    if source.is_symlink() or not source.is_file() or source.stat().st_size > MAX_ARCHIVE:
        raise ValueError("Choose a regular portable game ZIP no larger than 512 MiB.")
    with zipfile.ZipFile(source) as archive:
        infos = archive.infolist()
        if not infos or len(infos) > MAX_ENTRIES:
            raise ValueError("Portable game bundle has too many or too few files.")
        total = 0
        names = set()
        for info in infos:
            path = PurePosixPath(info.filename)
            mode = (info.external_attr >> 16) & 0xFFFF
            file_type = stat.S_IFMT(mode)
            canonical_name = "/".join(path.parts) + ("/" if info.is_dir() else "")
            if (not info.filename or path.is_absolute() or "\\" in info.filename
                    or "\x00" in info.filename or info.filename != canonical_name
                    or any(part in ("", ".", "..") for part in path.parts)
                    or info.filename in names or info.flag_bits & 1
                    or file_type not in (0, stat.S_IFREG, stat.S_IFDIR)):
                raise ValueError("Portable game bundle contains an unsafe or duplicate path.")
            names.add(info.filename)
            if info.file_size < 0 or info.file_size > MAX_EXPANDED:
                raise ValueError("Portable game file exceeds the unpacked size limit.")
            if not info.is_dir() and info.compress_size > MAX_ARCHIVE:
                raise ValueError("Portable game member exceeds the compressed size limit.")
            total += info.file_size
            if total > MAX_EXPANDED:
                raise ValueError("Portable game bundle exceeds the 2 GiB unpacked size limit.")
        try:
            manifest_info=archive.getinfo(MANIFEST)
            if manifest_info.file_size>65536:raise ValueError("Portable game manifest exceeds its 64 KiB limit.")
            manifest = json.loads(archive.read(manifest_info))
        except (KeyError, UnicodeDecodeError, json.JSONDecodeError):
            raise ValueError("Bundle must contain a valid isolde-game.json manifest.") from None
        if (not isinstance(manifest, dict) or set(manifest) != {"schemaVersion", "title", "platform", "entrypoint"}
                or manifest["schemaVersion"] != 1
                or not isinstance(manifest["title"], str) or not manifest["title"].strip()
                or len(manifest["title"]) > 256 or any(ord(c) < 32 for c in manifest["title"])
                or not isinstance(manifest["platform"], str) or not manifest["platform"].strip()
                or len(manifest["platform"]) > 80
                or not isinstance(manifest["entrypoint"], str)):
            raise ValueError("Portable game manifest has invalid fields.")
        entry = PurePosixPath(manifest["entrypoint"])
        if (entry.is_absolute() or "\\" in manifest["entrypoint"]
                or any(part in ("", ".", "..") for part in entry.parts)
                or manifest["entrypoint"] not in names
                or archive.getinfo(manifest["entrypoint"]).is_dir()):
            raise ValueError("Portable game entry point must name a file inside the bundle.")
        entry_info=archive.getinfo(manifest["entrypoint"])
        if entry_info.file_size > MAX_ARCHIVE:raise ValueError("Game entry point exceeds the 512 MiB executable limit.")
        with archive.open(entry_info) as stream:executable=stream.read(20)
        if not executable.startswith(b"\x7fELF") or len(executable) < 20:
            raise ValueError("Only native Linux ELF game entry points are supported by this installer.")
        return {"schemaVersion": 1, "title": manifest["title"].strip(),
                "platform": manifest["platform"].strip(), "entrypoint": manifest["entrypoint"],
                "entrySize": len(executable), "archiveEntries": len(infos), "unpackedBytes": total}


def install(source, destination):
    metadata = inspect(source)
    source = Path(source)
    destination = Path(destination)
    stage = destination.with_name(destination.name + ".staging")
    if destination.exists() or destination.is_symlink() or stage.exists() or stage.is_symlink():
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
        with zipfile.ZipFile(source) as archive:
            written=0
            for info in archive.infolist():
                relative = PurePosixPath(info.filename)
                target = stage.joinpath(*relative.parts)
                if info.is_dir():
                    target.mkdir(mode=0o700, parents=True, exist_ok=True)
                    continue
                target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
                if hasattr(os, "O_NOFOLLOW"):
                    flags |= os.O_NOFOLLOW
                descriptor = os.open(target, flags, 0o600)
                copied=0
                mode=(info.external_attr>>16)&0o111
                with os.fdopen(descriptor, "wb") as output, archive.open(info) as input_file:
                    while True:
                        chunk=input_file.read(1024*1024)
                        if not chunk:break
                        copied+=len(chunk);written+=len(chunk)
                        if copied>info.file_size or written>MAX_EXPANDED:
                            raise ValueError("Portable game archive expanded beyond its declared size.")
                        output.write(chunk)
                    if copied!=info.file_size:raise ValueError("Portable game archive member size changed.")
                    output.flush()
                    os.fsync(output.fileno())
                if mode:target.chmod(0o700)
        entry = stage.joinpath(*PurePosixPath(metadata["entrypoint"]).parts)
        entry.chmod(0o700)
        marker = stage / ".isolde-install.json"
        marker.write_text(json.dumps(metadata, ensure_ascii=False), encoding="utf-8")
        os.chmod(marker, 0o600)
        os.replace(stage, destination)
    except Exception:
        shutil.rmtree(stage, ignore_errors=True)
        raise
    print(json.dumps(metadata, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--inspect", action="store_true")
    group.add_argument("--install", action="store_true")
    parser.add_argument("--source", required=True)
    parser.add_argument("--destination")
    args = parser.parse_args()
    if args.inspect:
        result = inspect(args.source)
        print(json.dumps(result, ensure_ascii=False))
    else:
        if not args.destination:
            parser.error("--install requires --destination")
        install(args.source, args.destination)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, zipfile.BadZipFile, RuntimeError) as error:
        print("Portable game install: " + str(error), file=sys.stderr)
        sys.exit(2)
