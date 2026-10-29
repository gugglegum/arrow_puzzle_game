"""Release safety checks; signatures themselves are verified by gh in Actions."""

import importlib.util
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("release_assets", Path(__file__).resolve().parents[1] / "tools/release-assets.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)

COMMIT = "a" * 40
VERSION = "0.5.0"


class ReleaseAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="arrow-release-tests-")
        self.root = Path(self.temporary.name)
        self.windows = self.root / "windows"
        self.linux = self.root / "linux"
        self.output = self.root / "combined"
        self.windows.mkdir()
        self.linux.mkdir()
        self.win_name = f"ArrowPuzzle-{VERSION}-windows-x64.zip"
        self.tar_name = f"ArrowPuzzle-{VERSION}-linux-x64.tar.gz"
        self.image_name = f"ArrowPuzzle-{VERSION}-linux-x64.AppImage"
        self.win_bytes = b"MZ Windows executable fixture"
        self.linux_bytes = b"\x7fELF Linux executable fixture"
        base = {"source_commit": COMMIT, "package_version": VERSION, "source_dirty": False}
        self.win_meta = base | {"executable_sha256": release.sha256(self.win_bytes)}
        self.linux_meta = base | {"executable_sha256": release.sha256(self.linux_bytes), "architecture": "x86_64"}
        self.write_json(self.windows / "build-info-windows-x64.json", self.win_meta)
        self.write_json(self.linux / "build-info-linux-x64.json", self.linux_meta)
        self.make_zip()
        self.make_tar()
        (self.linux / self.image_name).write_bytes(b"AppImage fixture")
        self.image_meta = self.linux_meta | {
            "source_archive_sha256": release.sha256((self.linux / self.tar_name).read_bytes()),
            "appimage_sha256": release.sha256((self.linux / self.image_name).read_bytes()),
        }
        self.write_json(self.linux / "build-info-linux-appimage-x64.json", self.image_meta)
        # Fake bundles exercise file handling only, never cryptographic verification.
        (self.windows / "attestation-windows-x64.sigstore.json").write_text("{}")
        (self.linux / "attestation-linux-x64.sigstore.json").write_text("{}")
        self.write_sums()

    def tearDown(self):
        self.temporary.cleanup()

    @staticmethod
    def write_json(path, value):
        path.write_text(json.dumps(value))

    def make_zip(self, binary=None, metadata=None):
        with zipfile.ZipFile(self.windows / self.win_name, "w") as archive:
            archive.writestr("ArrowPuzzle.exe", self.win_bytes if binary is None else binary)
            archive.writestr("build-info.json", json.dumps(self.win_meta if metadata is None else metadata))

    def make_tar(self, mode=0o755):
        with tarfile.open(self.linux / self.tar_name, "w:gz") as archive:
            for name, data, permissions in (("ArrowPuzzle", self.linux_bytes, mode),
                                             ("build-info.json", json.dumps(self.linux_meta).encode(), 0o644)):
                info = tarfile.TarInfo(f"ArrowPuzzle-{VERSION}-linux-x64/{name}")
                info.size, info.mode = len(data), permissions
                archive.addfile(info, io.BytesIO(data))

    def write_sums(self):
        windows = {"ArrowPuzzle.exe": release.sha256(self.win_bytes),
                   self.win_name: release.sha256((self.windows / self.win_name).read_bytes())}
        linux = {"ArrowPuzzle": release.sha256(self.linux_bytes),
                 self.tar_name: release.sha256((self.linux / self.tar_name).read_bytes()),
                 self.image_name: release.sha256((self.linux / self.image_name).read_bytes())}
        for directory, platform, values in ((self.windows, "windows", windows), (self.linux, "linux", linux)):
            (directory / f"SHA256SUMS-{platform}-x64.txt").write_text(
                "".join(f"{digest}  {name}\n" for name, digest in values.items()))

    def assemble(self):
        release.assemble(self.windows, self.linux, self.output, COMMIT, VERSION)

    def test_common_assets_have_distinct_names_and_five_checksum_subjects(self):
        self.assemble()
        self.assertEqual(len(list(self.output.iterdir())), 9)
        self.assertEqual(set(release.checksums(self.output / "SHA256SUMS.txt")),
                         {"ArrowPuzzle.exe", "ArrowPuzzle", self.win_name, self.tar_name, self.image_name})
        self.assertEqual((self.output / self.image_name).read_bytes(), b"AppImage fixture")

    def test_output_is_never_overwritten(self):
        self.output.mkdir()
        marker = self.output / "user-file"
        marker.write_text("keep")
        with self.assertRaisesRegex(ValueError, "overwrite"):
            self.assemble()
        self.assertEqual(marker.read_text(), "keep")

    def test_tampered_packages_are_rejected(self):
        for directory, name in ((self.windows, self.win_name), (self.linux, self.tar_name), (self.linux, self.image_name)):
            with self.subTest(name=name):
                path, original = directory / name, (directory / name).read_bytes()
                path.write_bytes(original + b"changed")
                with self.assertRaisesRegex(ValueError, "Checksum mismatch"):
                    self.assemble()
                self.assertFalse(self.output.exists())
                path.write_bytes(original)

    def test_source_version_and_dirty_metadata_are_checked_for_each_platform(self):
        for path in (self.windows / "build-info-windows-x64.json", self.linux / "build-info-linux-x64.json",
                     self.linux / "build-info-linux-appimage-x64.json"):
            original = json.loads(path.read_text())
            for field, value in (("source_commit", "b" * 40), ("package_version", "0.4.0"), ("source_dirty", True)):
                with self.subTest(path=path.name, field=field):
                    self.write_json(path, original | {field: value})
                    with self.assertRaises(ValueError):
                        self.assemble()
                    self.assertFalse(self.output.exists())
            self.write_json(path, original)

    def test_inner_archive_identity_is_checked(self):
        self.make_zip(metadata=self.win_meta | {"source_commit": "b" * 40})
        self.write_sums()
        with self.assertRaisesRegex(ValueError, "inner/outer"):
            self.assemble()

    def test_rehashed_archive_cannot_hide_changed_executable(self):
        self.make_zip(binary=b"substituted binary")
        self.write_sums()
        with self.assertRaisesRegex(ValueError, "executable checksum"):
            self.assemble()

    def test_appimage_must_use_the_same_tar_and_binary(self):
        path = self.linux / "build-info-linux-appimage-x64.json"
        for field in ("source_archive_sha256", "executable_sha256", "appimage_sha256"):
            with self.subTest(field=field):
                self.write_json(path, self.image_meta | {field: "0" * 64})
                with self.assertRaises(ValueError):
                    self.assemble()

    def test_missing_provenance_bundle_is_rejected_before_output(self):
        (self.linux / "attestation-linux-x64.sigstore.json").unlink()
        with self.assertRaisesRegex(ValueError, "Missing release asset"):
            self.assemble()
        self.assertFalse(self.output.exists())

    def test_missing_linux_execute_permission_is_rejected(self):
        self.make_tar(mode=0o644)
        self.write_sums()
        self.write_json(self.linux / "build-info-linux-appimage-x64.json", self.image_meta | {
            "source_archive_sha256": release.sha256((self.linux / self.tar_name).read_bytes())})
        with self.assertRaisesRegex(ValueError, "executable regular file"):
            self.assemble()

    def test_duplicate_and_unsafe_checksum_paths_are_rejected(self):
        path = self.windows / "SHA256SUMS-windows-x64.txt"
        original = path.read_text()
        for contents in (original + original, "0" * 64 + "  ../escape\n", "0" * 64 + "  ..\n", ""):
            with self.subTest(contents=contents):
                path.write_text(contents)
                with self.assertRaises(ValueError):
                    self.assemble()
                self.assertFalse(self.output.exists())

    def test_manual_branch_defaults_to_no_draft(self):
        value = release.release_identity(COMMIT, "refs/heads/master", "workflow_dispatch", False, VERSION)
        self.assertEqual(value["create_release"], "false")
        self.assertEqual(value["tag"], "")
        self.assertTrue(value["version"].startswith("dev-"))

    def test_tag_and_explicit_manual_draft_require_matching_project_version(self):
        for event, manual in (("push", False), ("workflow_dispatch", True)):
            value = release.release_identity(COMMIT, "refs/tags/v0.5.0", event, manual, VERSION)
            self.assertEqual(value["create_release"], "true")
            self.assertEqual(value["version"], VERSION)
            with self.assertRaisesRegex(ValueError, "CMake project version"):
                release.release_identity(COMMIT, "refs/tags/v0.4.0", event, manual, VERSION)

    def test_branch_and_invalid_tags_cannot_create_drafts(self):
        for ref in ("refs/heads/master", "refs/tags/v0.5.0-rc1", "refs/tags/v0.5.0junk"):
            with self.subTest(ref=ref), self.assertRaisesRegex(ValueError, "existing vX.Y.Z tag"):
                release.release_identity(COMMIT, ref, "workflow_dispatch", True, VERSION)


if __name__ == "__main__":
    unittest.main()
