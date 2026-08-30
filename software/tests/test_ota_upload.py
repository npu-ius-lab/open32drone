import hashlib
import importlib.util
import pathlib
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "ros2/open32drone_driver/ota_upload.py"
SPEC = importlib.util.spec_from_file_location("open32drone_ota_upload", MODULE_PATH)
OTA_UPLOAD = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(OTA_UPLOAD)


class OtaUploadImageTests(unittest.TestCase):
    def write_image(self, payload: bytes) -> pathlib.Path:
        handle = tempfile.NamedTemporaryFile(delete=False)
        self.addCleanup(pathlib.Path(handle.name).unlink, missing_ok=True)
        with handle:
            handle.write(payload)
        return pathlib.Path(handle.name)

    def test_valid_esp_image_reports_exact_size_and_digest(self):
        payload = b"\xe9" + bytes(range(1, 48))
        size, digest = OTA_UPLOAD._inspect_image(self.write_image(payload))
        self.assertEqual(size, len(payload))
        self.assertEqual(digest, hashlib.sha256(payload).hexdigest())

    def test_non_esp_image_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "missing 0xE9"):
            OTA_UPLOAD._inspect_image(self.write_image(b"x" * 48))

    def test_tiny_image_is_rejected_before_header_processing(self):
        with self.assertRaisesRegex(ValueError, "too small"):
            OTA_UPLOAD._inspect_image(self.write_image(b"\xe9" * 8))


if __name__ == "__main__":
    unittest.main()
