#!/usr/bin/env python3
"""Execute a reviewed optical ELF from a private temporary staging tree."""
import fcntl
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import tempfile

MAX_EXECUTABLE = 512 * 1024 * 1024
MAX_STAGED_BYTES = 512 * 1024 * 1024
MAX_DIRECTORY_ENTRIES = 4096
MAX_PATH_DEPTH = 32


def stage_executable(path, require_execute=True):
    source = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    memory = None
    try:
        before = os.fstat(source)
        if (not stat.S_ISREG(before.st_mode) or not 20 <= before.st_size <= MAX_EXECUTABLE
                or (require_execute and not before.st_mode & 0o111)):
            raise ValueError("The optical entry point is not a bounded executable file.")
        if not hasattr(os, "memfd_create"):
            raise ValueError("This kernel does not provide sealed memory-file execution.")
        memory = os.memfd_create("isolde-optical-game", os.MFD_CLOEXEC | os.MFD_ALLOW_SEALING)
        first = True
        remaining = before.st_size
        while remaining:
            data = os.read(source, min(1024 * 1024, remaining))
            if not data:
                raise ValueError("The optical executable became unreadable.")
            if first:
                if not data.startswith(b"\x7fELF"):
                    raise ValueError("The optical entry point is not a Linux ELF executable.")
                first = False
            view = memoryview(data)
            while view:
                written = os.write(memory, view)
                if written <= 0:
                    raise OSError("Could not stage the optical executable.")
                view = view[written:]
            remaining -= len(data)
        after = os.fstat(source)
        if (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns) != (
                before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns):
            raise ValueError("The optical executable changed while being read.")
        os.fchmod(memory, 0o500)
        fcntl.fcntl(memory, fcntl.F_ADD_SEALS,
                    fcntl.F_SEAL_WRITE | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SEAL)
        os.set_inheritable(memory, True)
        result = memory
        memory = None
        return result
    finally:
        os.close(source)
        if memory is not None:
            os.close(memory)


def stage_tree(root, staging, target):
    remaining = MAX_DIRECTORY_ENTRIES
    staged_bytes = 0
    stack = [(root, staging, 0)]
    while stack:
        source, destination, depth = stack.pop()
        with os.scandir(source) as entries:
            for entry in entries:
                remaining -= 1
                if remaining < 0:
                    raise ValueError("The optical game directory contains too many entries to stage safely.")
                original = Path(entry.path)
                staged = destination / entry.name
                if entry.is_symlink():
                    resolved = Path(os.path.realpath(original))
                    try:
                        relative = resolved.relative_to(root)
                    except ValueError:
                        raise ValueError("The optical game contains a link outside the reviewed disc.") from None
                    os.symlink(os.path.relpath(staging / relative, destination), staged)
                elif entry.is_dir(follow_symlinks=False):
                    if depth + 1 >= MAX_PATH_DEPTH:
                        raise ValueError("The optical game exceeds the staging path-depth limit.")
                    os.mkdir(staged, 0o700)
                    stack.append((original, staged, depth + 1))
                elif entry.is_file(follow_symlinks=False):
                    fd = os.open(original, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
                    try:
                        header = os.read(fd, 4)
                    finally:
                        os.close(fd)
                    if header != b"\x7fELF":
                        if original == target:
                            raise ValueError("The optical entry point is not a Linux ELF executable.")
                        os.symlink(str(original), staged)
                        continue
                    descriptor = stage_executable(original, require_execute=original == target)
                    try:
                        staged_bytes += os.fstat(descriptor).st_size
                        if staged_bytes > MAX_STAGED_BYTES:
                            raise ValueError("The optical game exceeds the executable staging size limit.")
                        os.lseek(descriptor, 0, os.SEEK_SET)
                        with os.fdopen(os.open(staged, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o500), "wb") as output:
                            with os.fdopen(os.dup(descriptor), "rb") as sealed:
                                shutil.copyfileobj(sealed, output, 1024 * 1024)
                    finally:
                        os.close(descriptor)
                else:
                    raise ValueError("The optical game contains an unsupported special file.")


def execute(path, disc_root="/media/games"):
    root = Path(disc_root)
    target = Path(path)
    if not root.is_absolute() or not target.is_absolute():
        raise ValueError("The optical executable and disc root must be absolute.")
    try:
        parts = target.relative_to(root).parts
    except ValueError:
        raise ValueError("The optical executable is outside the reviewed disc.") from None
    if (not parts or len(parts) > MAX_PATH_DEPTH or ".." in parts
            or os.path.realpath(root) != str(root) or os.path.realpath(target) != str(target)):
        raise ValueError("The optical executable path is not a canonical disc path.")

    staging = None
    try:
        staging = Path(tempfile.mkdtemp(prefix="isolde-optical-"))
        if staging.is_relative_to(root):
            raise ValueError("Optical staging must not be inside the reviewed disc.")
        stage_tree(root, staging, target)
        os.execve(str(staging.joinpath(*parts)), [path], os.environ)
    finally:
        if staging is not None:
            shutil.rmtree(staging)


def main():
    if len(sys.argv) != 2:
        raise ValueError("Expected one reviewed optical executable.")
    target = sys.argv[1]
    path = PurePosixPath(target)
    if (not target.startswith("/media/games/") or str(path) != target
            or any(part in (".", "..") for part in path.parts)):
        raise ValueError("The optical executable is outside the reviewed disc.")
    execute(target)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print("Optical game: " + str(error), file=sys.stderr)
        sys.exit(2)
