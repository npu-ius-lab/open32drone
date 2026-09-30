import hashlib
import re
import subprocess
import tarfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def tracked_paths(directory):
    # A downloaded source archive has no index; do not query a parent checkout.
    if not (ROOT / ".git").exists():
        return ""
    return subprocess.check_output(
        ["git", "ls-files", directory], cwd=ROOT, text=True
    ).strip()


class RepositoryContractTests(unittest.TestCase):
    def test_download_batch_and_flashing_instructions(self):
        release = ROOT / "releases/open32drone"
        names = [line.split()[1] for line in (release / "SHA256SUMS").read_text().splitlines()]
        timestamps = set()
        for name in names:
            match = re.fullmatch(r"Open32Drone-(\d{8}-\d{6})-(full\.bin|app\.bin|android\.apk|ros2\.tar\.gz)", name)
            self.assertIsNotNone(match, name)
            timestamps.add(match.group(1))
        self.assertEqual(len(timestamps), 1)
        self.assertFalse((release / "BUILD_INFO.md").exists())
        for suffix in ("", ".en"):
            text = (ROOT / f"docs/guide/04-firmware-flight{suffix}.md").read_text()
            for required in ("https://espressif.github.io/esptool-js/", "CoolTerm", "Flash Address", "0x0", "0x1000", "full.bin", "{#preflight}", "{#android-first-flight}", "{#sbus}"):
                self.assertIn(required, text)
            self.assertLess(text.index("{#android-first-flight}"), text.index("{#sbus}"))
            self.assertNotIn("pip install", text)
            reference_suffix = "" if suffix else ".zh-CN"
            reference = (ROOT / f"docs/reference/source-build{reference_suffix}.md").read_text()
            for required in ("Windows", "macOS", "Ubuntu", "esptool==5.1.0", "pyserial==3.5", "serial.tools.miniterm", "erase-flash", "write-flash 0x0", "full.bin"):
                self.assertIn(required, reference)

    def test_public_branding_uses_open32drone(self):
        paths = list(ROOT.glob("README*.md"))
        for folder in ("guide", "reference", "project"):
            paths.extend((ROOT / "docs" / folder).glob("*.md"))
        paths.extend((ROOT / "releases" / "open32drone").glob("*.md"))
        for path in paths:
            self.assertNotIn("minimal", path.read_text().lower(), str(path))
        self.assertFalse((ROOT / "releases" / "minimal").exists())
        for locale in ("values", "values-en"):
            text = (ROOT / "android/app/src/main/res" / locale / "strings.xml").read_text()
            self.assertRegex(text, r'<string name="guide_title">Open32Drone[^<]*</string>')

    def test_only_current_matching_downloads_are_present(self):
        release = ROOT / "releases" / "open32drone"
        expected = {
            "Open32Drone-20260928-190250-app.bin":
                "a615cb0f3c14c7ff5c2114aa7a7736b3a3dc86366d66dbff86d61857c7cdb71d",
            "Open32Drone-20260928-190250-full.bin":
                "a757059ad97dd88e9d7a4c329574308d39eea3996f39d4f55c3b6e9d9617085e",
            "Open32Drone-20260928-190250-android.apk":
                "99d47efdac885ef5ed9cf251c7a86d7de68c664d9c6d472c6b47d6d0c7d1d44d",
            "Open32Drone-20260928-190250-ros2.tar.gz":
                "cd89d1a201b070f881253ca9bec51ada0ff3b50d27e5813b038170350e5e5c43",
        }
        self.assertEqual(
            {path.name for path in release.iterdir()},
            {*expected, "README.md", "README.zh-CN.md", "SHA256SUMS"},
        )
        self.assertEqual(
            {path.name for path in (ROOT / "releases").iterdir()},
            {"open32drone"},
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
        archive_path = ROOT / "releases" / "open32drone" / "Open32Drone-20260928-190250-ros2.tar.gz"
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

    def test_open32drone_top_level_layout(self):
        expected = {"firmware", "android", "ros2", "tests", "docs"}
        for name in expected:
            self.assertTrue((ROOT / name).is_dir(), name)

    def test_bilingual_document_pairs(self):
        docs = ROOT / "docs"
        self.assertEqual({p.name for p in docs.glob("*.md")}, {"index.md"})
        chapters = {"01-project", "03-hardware", "04-firmware-flight",
                    "05-tuning", "06-ros", "07-rl"}
        self.assertEqual({p.name for p in (docs / "guide").glob("*.md")},
                         {name + suffix for name in chapters for suffix in (".md", ".en.md")})
        for name in chapters:
            for suffix in (".md", ".en.md"):
                text = (docs / "guide" / (name + suffix)).read_text()
                self.assertNotRegex(text, r"(?m)^\[English\].*\[简体中文\]")
                self.assertTrue(text.strip())
        for directory, names in {
            "reference": {"source-build", "firmware"},
            "project": {"changelog", "contributing", "third-party"},
        }.items():
            self.assertEqual({p.name for p in (docs / directory).glob("*.md")},
                             {name + suffix for name in names for suffix in (".md", ".zh-CN.md")})

    def test_project_overview_stays_an_entry_page(self):
        headings = {
            "01-project.md": ["Open32Drone 是什么", "可以做什么", "系统组成", "开始制作", "参与项目"],
            "01-project.en.md": ["What is Open32Drone?", "What can you do?", "System overview", "Start building", "Join the project"],
        }
        for name, expected in headings.items():
            text = (ROOT / "docs/guide" / name).read_text()
            self.assertEqual(re.findall(r"^## (.+)$", text, flags=re.M), expected)
            self.assertIn("/media/photos/drone-complete.jpg", text)
            self.assertNotIn("PARAM_SET", text)
            self.assertNotIn("NVS", text)
            self.assertIn("https://github.com/npu-ius-lab/open32drone/issues", text)

    def test_root_has_only_shared_project_markdown(self):
        # Shared rules may be omitted from public exports; never ship a local symlink.
        personal_rules = {"AGENTS.override.md", "AGENT.md", "CLAUDE.md", "GEMINI.md", "CODEX.md"}
        expected = {"README.md", "README_zh_CN.md"}
        rules = ROOT / "AGENTS.md"
        self.assertFalse(rules.is_symlink())
        if rules.exists():
            self.assertTrue(rules.is_file())
            expected.add("AGENTS.md")
        self.assertEqual({p.name for p in ROOT.glob("*.md") if p.name not in personal_rules and not p.is_symlink()},
                         expected)

    def test_only_root_agent_rules_can_be_shared(self):
        if not (ROOT / ".git").exists():
            self.skipTest("Ignore rules require a Git checkout")
        personal_paths = {
            "AGENTS.override.md", "AGENT.md", "CLAUDE.md", "GEMINI.md", "CODEX.md",
            "docs/AGENTS.md", ".claude/settings.json", ".codex/config.toml",
            ".cursor/rules/local.mdc", ".cursorrules", ".github/copilot-instructions.md",
        }
        result = subprocess.run(
            ["git", "check-ignore", "--no-index", "--stdin"], cwd=ROOT,
            input="\n".join(sorted(personal_paths | {"AGENTS.md"})) + "\n",
            text=True, capture_output=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(set(result.stdout.splitlines()), personal_paths)

    def test_root_readme_language_links_and_images(self):
        for name in ("README.md", "README_zh_CN.md"):
            text = (ROOT / name).read_text(encoding="utf-8")
            language_links = set(re.findall(r'<a href="(\./README[^"/]*\.md)">', text))
            self.assertEqual(language_links, {"./README.md", "./README_zh_CN.md"})
            images = re.findall(r'<img\s+src="([^"]+)"', text)
            self.assertEqual(set(images), {"img/drone.PNG", "img/institute.png", "img/osrbot.png"})
            for target in language_links | set(images):
                self.assertTrue((ROOT / target).is_file(), f"{name}: {target}")

    def test_document_relative_links_resolve_after_consolidation(self):
        paths = list((ROOT / "docs").rglob("*.md"))
        paths += list(ROOT.glob("README*.md"))
        for folder in ("hardware", "simulation", "releases"):
            paths += list((ROOT / folder).rglob("*.md"))
        failures = []
        for path in paths:
            if any(part in {"dist", "cache"} for part in path.parts):
                continue
            text = re.sub(r"```[^\n]*\n.*?```", "", path.read_text(), flags=re.S)
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
        unsafe = re.compile(r"(?<!\*)\*\*(?!\s)[^*\n]*\S\*\*(?=[\w\[])")
        failures = []
        markdown = list(ROOT.glob("*.md"))
        markdown.extend((ROOT / "docs").glob("guide/*.md"))
        markdown.extend((ROOT / "docs").glob("reference/*.md"))
        markdown.extend((ROOT / "docs").glob("project/*.md"))
        markdown.extend((ROOT / "releases" / "open32drone").glob("*.md"))
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
            "Open32Drone-20260928-190250-app.bin",
            "Open32Drone-20260928-190250-full.bin",
        ):
            self.assertIn(encoded, (ROOT / "releases" / "open32drone" / name).read_bytes())

    def test_ci_rebuilds_firmware_and_checks_source_identity(self):
        workflow = (ROOT / ".github" / "workflows" / "quality.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("arduino-cli compile", workflow)
        self.assertIn("OPEN32DRONE_FIRMWARE_SOURCE_SHA256", workflow)
        self.assertIn('grep -aFq "$source_id"', workflow)


if __name__ == "__main__":
    unittest.main()
