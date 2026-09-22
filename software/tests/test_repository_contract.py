import hashlib
import re
import subprocess
import tarfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOFTWARE = ROOT / "software"


def tracked_paths(directory):
    # A downloaded source archive has no index; do not query a parent checkout.
    if not (ROOT / ".git").exists():
        return ""
    return subprocess.check_output(
        ["git", "ls-files", directory], cwd=ROOT, text=True
    ).strip()


class RepositoryContractTests(unittest.TestCase):
    def test_only_current_matching_downloads_are_present(self):
        release = SOFTWARE / "releases" / "minimal"
        expected = {
            "Open32Drone-minimal-app.bin":
                "d400a698924a3400973361ce543058c3fc475b5ff662c43cb89585de8fcff1e2",
            "Open32Drone-minimal-merged.bin":
                "ef0c8286691318236b18cf4b1095b3cf6a127a734c740fd117efe8ce55dba0b0",
            "Open32Drone-Controller-0.1.apk":
                "b1188238b774ec2ebec2e40a20b658eb81a24281a6f958f1bc21ca9358213d0f",
            "Open32Drone-ROS2-minimal.tar.gz":
                "98fdee82fae7d1da6d0d339003c7ce1afdfe064a3596f86b8e2013b0ed9a0a4b",
        }
        self.assertEqual(
            {path.name for path in release.iterdir()},
            {*expected, "README.md", "README.zh-CN.md", "SHA256SUMS"},
        )
        self.assertEqual(
            {path.name for path in (SOFTWARE / "releases").iterdir()},
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
        tracked_tools = tracked_paths("tools")
        self.assertEqual(tracked_tools, "")

    def test_ros_archive_matches_source_without_macos_metadata(self):
        archive_path = SOFTWARE / "releases" / "minimal" / "Open32Drone-ROS2-minimal.tar.gz"
        expected = {
            f"ros2/{path.relative_to(SOFTWARE / 'ros2').as_posix()}": path.read_bytes()
            for path in (SOFTWARE / "ros2").rglob("*")
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

    def test_preserved_directory_layout(self):
        expected = {"software/firmware", "software/android", "software/ros2",
                    "software/tests", "software/hardware", "software/releases",
                    "software/simulation", "img", "docs"}
        for name in expected:
            self.assertTrue((ROOT / name).is_dir(), name)

    def test_bilingual_document_pairs(self):
        docs = ROOT / "docs"
        self.assertEqual({p.name for p in docs.glob("*.md")}, {"index.md"})
        chapters = {"01-project", "02-goals", "03-hardware", "04-firmware-flight",
                    "05-tuning", "06-ros", "07-rl"}
        self.assertEqual({p.name for p in (docs / "guide").glob("*.md")},
                         {name + suffix for name in chapters for suffix in (".md", ".en.md")})
        for name in chapters:
            for suffix in (".md", ".en.md"):
                text = (docs / "guide" / (name + suffix)).read_text(encoding="utf-8")
                self.assertNotIn(f"[English]({name}.en.md)", text)
                self.assertNotIn(f"[简体中文]({name}.md)", text)
        switch = (docs / ".vitepress/theme/components/LanguageSwitch.vue").read_text(encoding="utf-8")
        self.assertIn('aria-controls="site-language-options"', switch)
        self.assertIn('<summary', switch)
        self.assertIn("nav-bar-content-after", (docs / ".vitepress/theme/index.ts").read_text(encoding="utf-8"))
        for directory, names in {
            "reference": {"source-build", "firmware"},
            "project": {"changelog", "contributing", "third-party"},
        }.items():
            self.assertEqual({p.name for p in (docs / directory).glob("*.md")},
                             {name + suffix for name in names for suffix in (".md", ".zh-CN.md")})

    def test_root_preserves_readme_and_tutorial_entries(self):
        # Local ignored agent rules are not publication files.
        self.assertEqual({p.name for p in ROOT.glob("*.md") if not p.is_symlink()},
                         {"README.md", "README_zh_CN.md", "tutorial.md", "tutorial_zh_CN.md"})

    def test_document_relative_links_resolve_after_consolidation(self):
        paths = list((ROOT / "docs").rglob("*.md"))
        paths += list(ROOT.glob("*.md"))
        for folder in ("hardware", "simulation", "releases"):
            paths += list((SOFTWARE / folder).rglob("*.md"))
        failures = []
        for path in paths:
            if any(part in {"dist", "cache"} for part in path.parts):
                continue
            text = re.sub(r"```[^\n]*\n.*?```", "", path.read_text(encoding="utf-8"), flags=re.S)
            for href in re.findall(r"!?\[[^\]\n]*\]\(([^\s)]+)\)", text):
                if re.match(r"[a-z]+:", href) or href.startswith("#"):
                    continue
                target = href.split("#", 1)[0]
                resolved = ((ROOT / "docs/public" / target.lstrip("/"))
                            if target.startswith("/media/") else path.parent / target)
                if not resolved.exists():
                    failures.append(f"{path.relative_to(ROOT)}: {href}")
        self.assertEqual(failures, [])

    def test_markdown_bold_boundaries_render_in_gitea(self):
        unsafe = re.compile(r"\*\*[^*\n]+\*\*(?=[\w\[])")
        failures = []
        markdown = list(ROOT.glob("*.md"))
        markdown.extend((ROOT / "docs").glob("guide/*.md"))
        markdown.extend((ROOT / "docs").glob("reference/*.md"))
        markdown.extend((ROOT / "docs").glob("project/*.md"))
        markdown.extend((SOFTWARE / "releases" / "minimal").glob("*.md"))
        for path in sorted(markdown):
            for line_number, line in enumerate(
                path.read_text(encoding="utf-8").splitlines(), start=1
            ):
                if unsafe.search(line):
                    failures.append(f"{path.relative_to(ROOT)}:{line_number}")
        self.assertEqual(failures, [])

    def test_output_is_not_tracked(self):
        tracked = tracked_paths("output")
        self.assertEqual(tracked, "")
        ignore_rules = (ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
        self.assertIn("output/", ignore_rules)

    def test_firmware_downloads_embed_current_source_identity(self):
        firmware = SOFTWARE / "firmware"
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
            self.assertIn(encoded, (SOFTWARE / "releases" / "minimal" / name).read_bytes())

    def test_ci_rebuilds_firmware_and_checks_source_identity(self):
        workflow = (ROOT / ".github" / "workflows" / "quality.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("arduino-cli compile", workflow)
        self.assertIn("OPEN32DRONE_FIRMWARE_SOURCE_SHA256", workflow)
        self.assertIn('grep -aFq "$source_id"', workflow)


if __name__ == "__main__":
    unittest.main()
