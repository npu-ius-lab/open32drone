import hashlib
import re
import subprocess
import tarfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RepositoryContractTests(unittest.TestCase):
    def test_only_current_matching_downloads_are_present(self):
        release = ROOT / "releases" / "minimal"
        expected = {
            "Open32Drone-minimal-app.bin":
                "8840609195f10882c5656f0eeac41e5d120f4f7033b277d8cda58a301bb3b899",
            "Open32Drone-minimal-merged.bin":
                "8fdb989afd31c5381d6e941f0573d619997c0f7ad9eae10af5cef89d416627da",
            "Open32Drone-Controller-0.1.apk":
                "b1188238b774ec2ebec2e40a20b658eb81a24281a6f958f1bc21ca9358213d0f",
            "Open32Drone-ROS2-minimal.tar.gz":
                "6e6220fd66dd247964de04dfb8b9dcc654c8566b6b19bd44cd4d4f9574a09c47",
        }
        self.assertEqual(
            {path.name for path in release.iterdir()},
            {*expected, "README.md", "README.zh-CN.md", "SHA256SUMS"},
        )
        self.assertEqual(
            {path.name for path in (ROOT / "releases").iterdir()},
            {"minimal"},
        )
        for name, digest in expected.items():
            actual = hashlib.sha256((release / name).read_bytes()).hexdigest()
            self.assertEqual(actual, digest)
        checksum_lines = (release / "SHA256SUMS").read_text(
            encoding="utf-8"
        ).splitlines()
        self.assertEqual(
            set(checksum_lines),
            {f"{digest}  {name}" for name, digest in expected.items()},
        )
        tracked_tools = subprocess.check_output(
            ["git", "ls-files", "tools"], cwd=ROOT, text=True
        ).strip()
        self.assertEqual(tracked_tools, "")

    def test_ros_archive_matches_source_without_macos_metadata(self):
        archive_path = ROOT / "releases" / "minimal" / "Open32Drone-ROS2-minimal.tar.gz"
        expected = {
            f"ros2/{path.relative_to(ROOT / 'ros2').as_posix()}": path.read_bytes()
            for path in (ROOT / "ros2").rglob("*")
            if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc"
        }
        with tarfile.open(archive_path, "r:gz") as archive:
            members = {member.name: member for member in archive.getmembers() if member.isfile()}
            self.assertFalse(
                [name for name in members if any(part.startswith("._") for part in Path(name).parts)]
            )
            self.assertEqual(set(members), set(expected))
            for name, content in expected.items():
                extracted = archive.extractfile(members[name])
                self.assertIsNotNone(extracted)
                self.assertEqual(extracted.read(), content)

    def test_minimal_top_level_layout(self):
        expected = {"firmware", "android", "ros2", "tests", "docs"}
        for name in expected:
            self.assertTrue((ROOT / name).is_dir(), name)

    def test_bilingual_document_pairs(self):
        docs = ROOT / "docs"
        expected = {
            "GETTING_STARTED.md",
            "TROUBLESHOOTING.md",
            "ROS2.md",
            "FIRMWARE_REFERENCE.md",
            "CODE_WALKTHROUGH.md",
            "DEVELOPMENT.md",
            "CAPABILITIES.md",
        }
        english = sorted(
            path for path in docs.glob("*.md")
            if not path.name.endswith(".zh-CN.md")
        )
        self.assertEqual({path.name for path in english}, expected)
        translated_names = {
            path.with_name(path.stem + ".zh-CN.md").name for path in english
        }
        self.assertEqual(
            {path.name for path in docs.glob("*.md")},
            expected | translated_names,
        )
        for path in english:
            translated = path.with_name(path.stem + ".zh-CN.md")
            self.assertTrue(translated.exists(), translated.name)

    def test_markdown_bold_boundaries_render_in_gitea(self):
        unsafe = re.compile(r"\*\*[^*\n]+\*\*(?=[\w\[])")
        failures = []
        markdown = list(ROOT.glob("*.md"))
        markdown.extend((ROOT / "docs").glob("*.md"))
        markdown.extend((ROOT / "releases" / "minimal").glob("*.md"))
        for path in sorted(markdown):
            for line_number, line in enumerate(
                path.read_text(encoding="utf-8").splitlines(), start=1
            ):
                if unsafe.search(line):
                    failures.append(f"{path.relative_to(ROOT)}:{line_number}")
        self.assertEqual(failures, [])

    def test_output_is_not_tracked(self):
        tracked = subprocess.check_output(
            ["git", "ls-files", "output"], cwd=ROOT, text=True
        ).strip()
        self.assertEqual(tracked, "")
        ignore_rules = (ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
        self.assertIn("output/", ignore_rules)

    def test_firmware_downloads_embed_current_source_identity(self):
        firmware = ROOT / "firmware"
        digest = hashlib.sha256()
        for path in sorted(firmware.iterdir(), key=lambda item: item.name):
            if not path.is_file() or path.name == "source_identity.h":
                continue
            digest.update(path.name.encode("utf-8"))
            digest.update(b"\0")
            digest.update(path.read_bytes())
            digest.update(b"\0")
        source_id = digest.hexdigest()
        identity = (firmware / "source_identity.h").read_text(encoding="utf-8")
        declared = re.search(
            r'^#define OPEN32DRONE_FIRMWARE_SOURCE_SHA256 "([0-9a-f]{64})"$',
            identity,
            flags=re.MULTILINE,
        )
        self.assertIsNotNone(declared)
        self.assertEqual(declared.group(1), source_id)
        encoded = source_id.encode("ascii")
        for name in (
            "Open32Drone-minimal-app.bin",
            "Open32Drone-minimal-merged.bin",
        ):
            self.assertIn(encoded, (ROOT / "releases" / "minimal" / name).read_bytes())

    def test_ci_rebuilds_firmware_and_checks_source_identity(self):
        workflow = (ROOT / ".github" / "workflows" / "quality.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("arduino-cli compile", workflow)
        self.assertIn("OPEN32DRONE_FIRMWARE_SOURCE_SHA256", workflow)
        self.assertIn('grep -aFq "$source_id"', workflow)


if __name__ == "__main__":
    unittest.main()
