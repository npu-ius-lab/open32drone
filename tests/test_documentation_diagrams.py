import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIAGRAMS = [
    name + suffix
    for name in ("firmware-map", "control-cycle", "attitude-control",
                 "position-control", "auto-flight-states", "command-path",
                 "boot-sequence", "model-hierarchy")
    for suffix in ("", ".en")
] + [name + suffix for name in ("tuning-order", "ros-dataflow", "ros-position-frames")
     for suffix in ("", ".en")]


class DocumentationDiagramTests(unittest.TestCase):
    def test_explanatory_diagrams_are_images_not_code_fences(self):
        for directory in ("guide", "reference", "project"):
            for path in (ROOT / "docs" / directory).glob("*.md"):
                for language, content in re.findall(r"^```(\w*)\n([\s\S]*?)^```", path.read_text(), flags=re.M):
                    self.assertNotEqual(language, "mermaid", str(path))
                    if language == "text":
                        self.assertNotRegex(content, r"(?:->|→|├|└|│)", str(path))

    def test_diagram_assets_are_valid_accessible_and_referenced(self):
        pages = "\n".join(path.read_text() for folder in ("guide", "reference", "project")
                          for path in (ROOT / "docs" / folder).glob("*.md"))
        ns = {"svg": "http://www.w3.org/2000/svg"}
        for name in DIAGRAMS:
            asset = ROOT / f"docs/public/media/figures/{name}.svg"
            root = ET.parse(asset).getroot()
            self.assertEqual(root.get("role"), "img")
            labels = root.get("aria-labelledby", "").split()
            self.assertEqual(len(labels), 2)
            for label in labels:
                self.assertIsNotNone(root.find(f".//*[@id='{label}']"))
            self.assertTrue(root.find("svg:title", ns).text)
            self.assertTrue(root.find("svg:desc", ns).text)
            self.assertIsNotNone(root.get("viewBox"))
            self.assertIsNone(root.find(".//svg:script", ns))
            self.assertIn(f"](/media/figures/{name}.svg)", pages)

    def test_executable_examples_remain_copyable(self):
        source = (ROOT / "docs/reference/source-build.zh-CN.md").read_text()
        self.assertIn("```bash\narduino-cli", source)
        self.assertIn("write-flash 0x0", source)
        flight = (ROOT / "docs/guide/04-firmware-flight.md").read_text()
        self.assertIn("p PWR_VOLT_SCALE", flight)

    def test_source_guide_explains_loop_timing_and_watchdog(self):
        for suffix in ("", ".zh-CN"):
            source = (ROOT / f"docs/reference/source-build{suffix}.md").read_text()
            for reference in ("time.ino", "loop_watchdog.ino", "syncParameters()"):
                self.assertIn(reference, source)
            self.assertNotIn("减少时序抖动，不改变控制方程", source)
            self.assertNotIn("remove jitter without changing", source)
        chinese = (ROOT / "docs/reference/source-build.zh-CN.md").read_text()
        self.assertIn("Initializing complete\nGyro calibration complete", chinese)
        for suffix in ("", ".en"):
            root = ET.parse(ROOT / f"docs/public/media/figures/control-cycle{suffix}.svg").getroot()
            labels = " ".join(root.itertext())
            self.assertIn("09", labels)
            self.assertIn("completeControlLoopWatchdog()", labels)


if __name__ == "__main__":
    unittest.main()
