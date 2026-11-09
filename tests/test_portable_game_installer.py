import importlib.util
import json
from pathlib import Path
import shutil
import stat
import unittest
from unittest import mock
import uuid
import zipfile

spec=importlib.util.spec_from_file_location("portable_game_installer",Path(__file__).parents[1]/"tools/portable-game-installer.py")
installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)


class PortableGameInstallerTests(unittest.TestCase):
    def setUp(self):
        self.work=Path(__file__).parent/".portable-installer-testdata"
        self.work.mkdir(exist_ok=True)
        self.root=self.work/str(uuid.uuid4());self.root.mkdir()
        self.addCleanup(self.cleanup_work)
        self.archive=self.root/"fixture.zip"
        self.manifest={"schemaVersion":1,"title":"Inert Linux fixture","platform":"PC","entrypoint":"bin/game"}

    def cleanup_work(self):
        shutil.rmtree(self.root,ignore_errors=True)
        try:self.work.rmdir()
        except OSError:pass

    def package(self,extra=(),entry=b"\x7fELF"+b"\x00"*32,manifest=None):
        with zipfile.ZipFile(self.archive,"w") as archive:
            archive.writestr(installer.MANIFEST,json.dumps(manifest or self.manifest))
            archive.writestr("bin/game",entry)
            for item in extra: archive.writestr(*item)

    def test_inspects_and_installs_without_executing_payload(self):
        self.package()
        result=installer.inspect(self.archive)
        self.assertEqual(result["title"],"Inert Linux fixture")
        self.assertEqual(len(result["archiveSha256"]),64)
        self.assertEqual(result["entrySize"],36)
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        self.assertEqual((destination/"bin/game").read_bytes(),b"\x7fELF"+b"\x00"*32)
        self.assertTrue((destination/"bin/game").stat().st_mode & stat.S_IXUSR)
        self.assertTrue((destination/".isolde-install.json").is_file())
        with self.assertRaisesRegex(ValueError,"already exists"):installer.install(self.archive,destination)

    def test_installer_rejects_archive_replaced_after_preflight(self):
        self.package()
        preview=installer.inspect(self.archive)
        installed=self.root/"private"/"games"/"id"/"install"
        with zipfile.ZipFile(self.archive,"w") as archive:
            archive.writestr(installer.MANIFEST,json.dumps({**self.manifest,"title":"Changed after review"}))
            archive.writestr("bin/game",b"\x7fELF"+b"\x00"*32)
        with self.assertRaisesRegex(ValueError,"changed after preflight"):
            installer.install(self.archive,installed,preview["archiveSha256"])
        self.assertFalse(installed.exists())
        self.assertFalse(installed.with_name(installed.name+".staging").exists())
        self.assertFalse(installed.with_name(installed.name+".staging-source").exists())

    def test_write_failure_removes_unpublished_staging_data(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        with mock.patch.object(installer.os,"replace",side_effect=OSError("simulated publication failure")):
            with self.assertRaisesRegex(OSError,"publication failure"):
                installer.install(self.archive,destination)
        self.assertFalse(destination.exists())
        self.assertFalse(destination.with_name(destination.name+".staging").exists())
        self.assertFalse(destination.with_name(destination.name+".staging-source").exists())

    def test_safe_uninstall_removes_only_verified_install_and_preserves_saves(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        save=self.root/"private"/"native-profiles"/"id"/"runtime"/"saves"/"save.dat"
        save.parent.mkdir(parents=True);save.write_bytes(b"keep")
        installer.uninstall(destination)
        self.assertFalse(destination.exists())
        self.assertEqual(save.read_bytes(),b"keep")

    def test_uninstall_preserves_modified_or_unowned_payloads(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        entry=destination/"bin/game"
        entry.write_bytes(entry.read_bytes()+b"changed")
        with self.assertRaisesRegex(ValueError,"changed after installation"):
            installer.uninstall(destination)
        self.assertTrue(entry.exists())
        entry.write_bytes(b"\x7fELF"+b"\x00"*32)
        (destination/"user-save.dat").write_bytes(b"not owned")
        with self.assertRaisesRegex(ValueError,"unowned files"):
            installer.uninstall(destination)
        self.assertTrue((destination/"user-save.dat").exists())

    def test_uninstall_restores_folder_if_atomic_removal_fails(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        with mock.patch.object(installer.shutil,"rmtree",side_effect=OSError("simulated cleanup failure")):
            with self.assertRaisesRegex(OSError,"cleanup failure"):
                installer.uninstall(destination)
        self.assertTrue((destination/"bin/game").is_file())
        self.assertFalse(any(path.name.startswith(".isolde-uninstalling-") for path in destination.parent.iterdir()))

    def test_rejects_traversal_symlinks_duplicates_non_elf_and_bad_manifest(self):
        self.package(extra=[("../escape",b"unsafe")])
        with self.assertRaises(ValueError):installer.inspect(self.archive)
        self.package(entry=b"#!/bin/sh\n")
        with self.assertRaisesRegex(ValueError,"ELF"):installer.inspect(self.archive)
        self.package(manifest={"schemaVersion":1,"title":"x","platform":"PC","entrypoint":"../outside"})
        with self.assertRaises(ValueError):installer.inspect(self.archive)
        self.package(manifest={"schemaVersion":True,"title":"x","platform":"PC","entrypoint":"bin/game"})
        with self.assertRaisesRegex(ValueError,"invalid fields"):installer.inspect(self.archive)
        self.package(manifest={"schemaVersion":1,"title":"x","platform":"PC\nbad","entrypoint":"bin/game"})
        with self.assertRaisesRegex(ValueError,"invalid fields"):installer.inspect(self.archive)
        with zipfile.ZipFile(self.archive,"w") as archive:
            info=zipfile.ZipInfo("link");info.external_attr=(stat.S_IFLNK|0o777)<<16;archive.writestr(info,"/etc")
        with self.assertRaisesRegex(ValueError,"unsafe"):installer.inspect(self.archive)

    def test_archive_byte_and_entry_limits(self):
        self.archive.write_bytes(b"x"*(installer.MAX_ARCHIVE+1))
        with self.assertRaisesRegex(ValueError,"512 MiB"):installer.inspect(self.archive)
        with zipfile.ZipFile(self.archive,"w") as archive:
            for index in range(installer.MAX_ENTRIES+1):archive.writestr(f"{index}",b"")
        with self.assertRaisesRegex(ValueError,"too many"):installer.inspect(self.archive)


if __name__=="__main__":unittest.main()
