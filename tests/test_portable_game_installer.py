import importlib.util
import io
import json
from pathlib import Path
import shutil
import stat
import unittest
from unittest import mock
import uuid
import tarfile
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

    def package_ustar(self, compressed=False):
        self.archive=self.root/("fixture.tar.gz" if compressed else "fixture.tar")
        files=((installer.MANIFEST,json.dumps(self.manifest).encode(),0o644),
               ("bin/game",b"\x7fELF"+b"\x00"*32,0o755))
        with tarfile.open(self.archive,"w:gz" if compressed else "w",format=tarfile.USTAR_FORMAT) as archive:
            directory=tarfile.TarInfo("bin/");directory.type=tarfile.DIRTYPE;directory.mode=0o755
            archive.addfile(directory)
            for name,data,mode in files:
                info=tarfile.TarInfo(name);info.size=len(data);info.mode=mode
                archive.addfile(info,io.BytesIO(data))

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

    def test_explicit_ustar_bundle_installs_and_uninstalls_safely(self):
        self.package_ustar()
        preview=installer.inspect(self.archive)
        self.assertEqual(preview["archiveFormat"],"ustar")
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination,preview["archiveSha256"])
        entry=destination/"bin/game"
        self.assertEqual(entry.read_bytes(),b"\x7fELF"+b"\x00"*32)
        self.assertTrue(entry.stat().st_mode & stat.S_IXUSR)
        installer.uninstall(destination)
        self.assertFalse(destination.exists())

    def test_ustar_links_and_traversal_are_rejected(self):
        self.archive=self.root/"unsafe.tar"
        with tarfile.open(self.archive,"w",format=tarfile.USTAR_FORMAT) as archive:
            link=tarfile.TarInfo("../outside");link.type=tarfile.SYMTYPE;link.linkname="/etc/passwd"
            archive.addfile(link)
        with self.assertRaisesRegex(ValueError,"unsafe"):
            installer.inspect(self.archive)

    def test_tar_member_limits_are_enforced_during_enumeration(self):
        self.package_ustar()
        with mock.patch.object(installer, "MAX_ENTRIES", 2), \
             mock.patch.object(tarfile.TarFile, "getmembers", side_effect=AssertionError("unbounded enumeration")):
            with self.assertRaisesRegex(ValueError, "too many"):
                installer.inspect(self.archive)

    def test_tar_expanded_limit_is_enforced_during_enumeration(self):
        self.package_ustar()
        with mock.patch.object(installer, "MAX_EXPANDED", 20):
            with self.assertRaisesRegex(ValueError, "unpacked size"):
                installer.inspect(self.archive)

    def test_gzip_ustar_preflight_install_and_uninstall(self):
        self.package_ustar(compressed=True)
        preview=installer.inspect(self.archive)
        self.assertEqual(preview["archiveFormat"],"ustar-gzip")
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination,preview["archiveSha256"])
        self.assertEqual((destination/"bin/game").read_bytes(),b"\x7fELF"+b"\x00"*32)
        installer.uninstall(destination)
        self.assertFalse(destination.exists())

    def test_gzip_extended_headers_and_links_are_rejected(self):
        self.archive=self.root/"extended.tar.gz"
        with tarfile.open(self.archive,"w:gz",format=tarfile.PAX_FORMAT) as archive:
            info=tarfile.TarInfo("a"*150);info.size=1
            archive.addfile(info,io.BytesIO(b"x"))
        with self.assertRaisesRegex(ValueError,"Extended tar headers"):
            installer.inspect(self.archive)
        with tarfile.open(self.archive,"w:gz",format=tarfile.USTAR_FORMAT) as archive:
            info=tarfile.TarInfo("link");info.type=tarfile.SYMTYPE;info.linkname="/etc"
            archive.addfile(info)
        with self.assertRaisesRegex(ValueError,"unsafe"):
            installer.inspect(self.archive)

    def test_gzip_truncated_and_expansion_limits_are_rejected(self):
        self.package_ustar(compressed=True)
        with mock.patch.object(installer,"MAX_EXPANDED",20):
            with self.assertRaisesRegex(ValueError,"unpacked size"):
                installer.inspect(self.archive)
        self.archive.write_bytes(self.archive.read_bytes()[:20])
        with self.assertRaises((ValueError, EOFError, tarfile.TarError, OSError)):
            installer.inspect(self.archive)

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
        with mock.patch.object(installer,"remove_owned_files",side_effect=OSError("simulated cleanup failure")):
            with self.assertRaisesRegex(OSError,"cleanup failure"):
                installer.uninstall(destination)
        self.assertTrue((destination/"bin/game").is_file())
        self.assertFalse(any(path.name.startswith(".isolde-uninstalling-") for path in destination.parent.iterdir()))

    def test_prepared_uninstall_can_be_recovered_before_catalog_commit(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        installer.prepare_uninstall(destination)
        pending=destination.with_name("install.uninstall-pending")
        self.assertFalse(destination.exists())
        self.assertTrue((pending/"bin/game").is_file())
        installer.recover_uninstall(destination)
        self.assertTrue((destination/"bin/game").is_file())
        self.assertFalse(pending.exists())

    def test_staged_uninstall_refuses_conflicts_and_modified_contents(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        installer.prepare_uninstall(destination)
        pending=destination.with_name("install.uninstall-pending")
        destination.mkdir()
        (destination/"user-data").write_bytes(b"keep")
        with self.assertRaisesRegex(ValueError,"still present"):
            installer.finalize_uninstall(destination)
        with self.assertRaisesRegex(ValueError,"existing installation"):
            installer.recover_uninstall(destination)
        self.assertEqual((destination/"user-data").read_bytes(),b"keep")
        shutil.rmtree(destination)
        (pending/"unowned").write_bytes(b"keep")
        with self.assertRaisesRegex(ValueError,"unowned"):
            installer.finalize_uninstall(destination)
        self.assertEqual((pending/"unowned").read_bytes(),b"keep")

    def test_cleanup_never_recursively_deletes_new_unowned_files(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        installer.prepare_uninstall(destination)
        pending=destination.with_name("install.uninstall-pending")
        original=installer.remove_owned_files
        def inject_unowned(root, files, directories, marker):
            (root/"bin"/"new-save").write_bytes(b"keep")
            return original(root, files, directories, marker)
        with mock.patch.object(installer,"remove_owned_files",side_effect=inject_unowned):
            with self.assertRaises(OSError):
                installer.finalize_uninstall(destination)
        self.assertEqual((pending/"bin"/"new-save").read_bytes(),b"keep")
        self.assertTrue((pending/".isolde-install.json").exists())

    def test_cleanup_preserves_file_changed_after_verification(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        installer.prepare_uninstall(destination)
        pending=destination.with_name("install.uninstall-pending")
        original=installer.remove_owned_files
        def modify_owned(root, files, directories, marker):
            (root/"bin"/"game").write_bytes(b"new user data")
            return original(root, files, directories, marker)
        with mock.patch.object(installer,"remove_owned_files",side_effect=modify_owned):
            with self.assertRaisesRegex(ValueError,"changed during cleanup"):
                installer.finalize_uninstall(destination)
        self.assertEqual((pending/"bin"/"game").read_bytes(),b"new user data")

    def test_cleanup_refuses_redirected_directory_after_verification(self):
        self.package()
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        installer.prepare_uninstall(destination)
        pending=destination.with_name("install.uninstall-pending")
        outside=self.root/"outside";outside.mkdir()
        (outside/"game").write_bytes(b"keep")
        original=installer.remove_owned_files
        def redirect(root, files, directories, marker):
            (root/"bin").rename(root/"bin-moved")
            (root/"bin").symlink_to(outside, target_is_directory=True)
            return original(root, files, directories, marker)
        with mock.patch.object(installer,"remove_owned_files",side_effect=redirect):
            with self.assertRaises(OSError):
                installer.finalize_uninstall(destination)
        self.assertEqual((outside/"game").read_bytes(),b"keep")
        self.assertTrue((pending/"bin-moved"/"game").is_file())

    def test_atomic_no_replace_preserves_existing_destination(self):
        source=self.root/"source";source.mkdir();(source/"data").write_bytes(b"source")
        destination=self.root/"destination";destination.mkdir()
        with self.assertRaises(OSError):
            installer.rename_without_replace(source,destination)
        self.assertEqual((source/"data").read_bytes(),b"source")
        self.assertTrue(destination.is_dir())

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
