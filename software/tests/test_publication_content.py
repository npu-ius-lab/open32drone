"""Public-tree hygiene; no scan of ignored personal directories or Git history."""
import io
import re
import subprocess
import tarfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def source_files():
    git_root = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"], cwd=ROOT,
        capture_output=True, text=True,
    )
    if git_root.returncode or Path(git_root.stdout.strip()).resolve() != ROOT.resolve():
        # Source archives have no Git database. Never inspect a parent repository.
        ignored = {".git", "node_modules", "__pycache__", ".venv", "output",
                   ".gradle", "build", "install", ".temp", ".vitepress/cache", ".vitepress/dist"}
        return sorted(path for path in ROOT.rglob("*")
                      if path.is_file()
                      and not ignored.intersection(path.relative_to(ROOT).parts)
                      and ".vitepress/cache" not in path.as_posix()
                      and ".vitepress/dist" not in path.as_posix())
    listed = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=ROOT,
    )
    return sorted({ROOT / name.decode() for name in listed.split(b"\0")
                   if name and (ROOT / name.decode()).is_file()})


class PublicationContentTests(unittest.TestCase):
    def test_no_local_materials_in_candidate(self):
        blocked_dirs = {"output", "work", "artifacts", "backups", "secrets",
                        ".codex", ".claude", ".cursor", ".venv"}
        blocked_names = {"AGENTS.md", "AGENT.md", "CLAUDE.md", "GEMINI.md",
                         "CODEX.md", ".env", ".cursorrules", "copilot-instructions.md"}
        failures = []
        for path in source_files():
            relative = path.relative_to(ROOT)
            if (blocked_dirs.intersection(relative.parts)
                    or relative.parts[0] == "reference"
                    or path.name in blocked_names
                    or path.suffix in {".bag", ".db3", ".mcap", ".jks", ".keystore", ".pem"}
                    or path.is_symlink()):
                failures.append(str(relative))
        self.assertEqual(failures, [])

    def test_source_and_packages_have_no_personal_paths_or_key_material(self):
        patterns = {
            "personal path": rb"(?:/(?:Users|home)/[A-Za-z0-9_.-]+/|[A-Za-z]:[/\\]Users[/\\])",
            "private key": rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----",
            "github token": rb"(?:ghp_|github_pat_)[A-Za-z0-9_]{25,}",
        }
        failures = []

        def check(name, data):
            for label, pattern in patterns.items():
                if re.search(pattern, data):
                    failures.append(f"{name}: {label}")

        for path in source_files():
            name = path.relative_to(ROOT).as_posix()
            data = path.read_bytes()
            check(name, data)
            if path.suffix in {".apk", ".3mf"}:
                with zipfile.ZipFile(io.BytesIO(data)) as archive:
                    for member in archive.namelist():
                        if not member.endswith("/"):
                            check(f"{name}:{member}", archive.read(member))
            elif name.endswith(".tar.gz"):
                with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
                    for member in archive.getmembers():
                        self.assertFalse(member.issym() or member.islnk())
                        self.assertFalse(Path(member.name).is_absolute())
                        self.assertNotIn("..", Path(member.name).parts)
                        self.assertEqual(member.uname, "")
                        self.assertEqual(member.gname, "")
                        if member.isfile():
                            check(f"{name}:{member.name}", archive.extractfile(member).read())
        self.assertEqual(failures, [])

    def test_cad_account_metadata_is_empty(self):
        with zipfile.ZipFile(ROOT / "software/hardware/3d-model/open32drone-frame.3mf") as archive:
            model = archive.read("3D/3dmodel.model")
        self.assertRegex(model, rb'<metadata name="DesignerUserId">\s*</metadata>')

    def test_operator_docs_do_not_link_to_private_experiment_outputs(self):
        failures = []
        for path in source_files():
            if path.suffix != ".md":
                continue
            # Output directories in runnable examples are fine; hyperlinks to
            # absent personal experiment results are not.
            if re.search(r"\]\([^\n)]*(?:\.\./)+output/", path.read_text(encoding="utf-8")):
                failures.append(str(path.relative_to(ROOT)))
        self.assertEqual(failures, [])


if __name__ == "__main__":
    unittest.main()
