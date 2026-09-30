"""Guarded A/B firmware uploader for Open32Drone development builds."""

from __future__ import annotations

import argparse
import hashlib
import http.client
import json
from pathlib import Path
import sys
import time


DEFAULT_HOST = "192.168.4.1"
DEFAULT_PORT = 8080


def _request_status(host: str, port: int, timeout: float) -> dict:
    connection = http.client.HTTPConnection(host, port, timeout=timeout)
    try:
        connection.request("GET", "/api/ota/status", headers={"Accept": "application/json"})
        response = connection.getresponse()
        payload = response.read()
        if response.status != 200:
            raise RuntimeError(f"OTA status HTTP {response.status}: {payload.decode(errors='replace')}")
        return json.loads(payload)
    finally:
        connection.close()


def _inspect_image(path: Path) -> tuple[int, str]:
    if not path.is_file():
        raise ValueError(f"firmware image not found: {path}")
    size = path.stat().st_size
    if size < 24:
        raise ValueError("firmware image is too small")
    digest = hashlib.sha256()
    with path.open("rb") as firmware:
        if firmware.read(1) != b"\xe9":
            raise ValueError("not an ESP application image (missing 0xE9 header)")
        firmware.seek(0)
        for chunk in iter(lambda: firmware.read(1024 * 1024), b""):
            digest.update(chunk)
    return size, digest.hexdigest()


def _upload(
    host: str,
    port: int,
    image: Path,
    size: int,
    digest: str,
    timeout: float,
) -> dict:
    connection = http.client.HTTPConnection(host, port, timeout=timeout)
    try:
        connection.putrequest("POST", "/api/ota/update", skip_accept_encoding=True)
        connection.putheader("Content-Type", "application/octet-stream")
        connection.putheader("Content-Length", str(size))
        connection.putheader("X-Firmware-SHA256", digest)
        connection.endheaders()

        sent = 0
        last_percent = -1
        with image.open("rb") as firmware:
            while True:
                chunk = firmware.read(16 * 1024)
                if not chunk:
                    break
                connection.send(chunk)
                sent += len(chunk)
                percent = sent * 100 // size
                if percent != last_percent:
                    print(f"\ruploading: {percent:3d}% ({sent}/{size} bytes)", end="", flush=True)
                    last_percent = percent
        print()

        response = connection.getresponse()
        payload = response.read()
        try:
            result = json.loads(payload)
        except json.JSONDecodeError as exc:
            raise RuntimeError(
                f"OTA response HTTP {response.status} is not JSON: {payload.decode(errors='replace')}"
            ) from exc
        if response.status != 200 or not result.get("ok"):
            raise RuntimeError(f"OTA rejected (HTTP {response.status}): {result.get('error', result)}")
        return result
    finally:
        connection.close()


def _wait_for_validated_boot(
    host: str,
    port: int,
    expected_slot: str,
    timeout: float = 50.0,
) -> dict:
    print(f"waiting for {expected_slot} to reboot and pass flight health checks ...")
    deadline = time.monotonic() + timeout
    last_status = None
    while time.monotonic() < deadline:
        time.sleep(1.0)
        try:
            status = _request_status(host, port, timeout=2.0)
        except (OSError, RuntimeError, ValueError, json.JSONDecodeError):
            continue
        last_status = status
        if status.get("active_slot") == expected_slot and not status.get("pending_verify"):
            return status
    if last_status and last_status.get("active_slot") != expected_slot:
        raise RuntimeError(
            "new image did not remain active; boot validation likely rolled back "
            f"to {last_status.get('active_slot')}"
        )
    raise RuntimeError("timed out waiting for the new image to pass boot validation")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Upload an Open32Drone app image to the inactive A/B slot."
    )
    parser.add_argument("firmware", type=Path, help="Arduino application .bin (not merged.bin)")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"aircraft IP (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"OTA HTTP port (default: {DEFAULT_PORT})")
    parser.add_argument("--timeout", type=float, default=20.0, help="HTTP upload timeout in seconds")
    parser.add_argument("--no-wait", action="store_true", help="do not wait for post-boot validation")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        size, digest = _inspect_image(args.firmware)
        status = _request_status(args.host, args.port, timeout=3.0)
        if not status.get("ready"):
            raise RuntimeError(f"aircraft is not OTA-ready: {status.get('reason', 'unknown')}")
        if status.get("partition_count", 0) < 2:
            raise RuntimeError("A/B partition table is missing; perform the one-time USB migration")
        maximum = int(status.get("max_image_bytes", 0))
        if maximum and size > maximum:
            raise RuntimeError(f"image is {size} bytes, inactive slot accepts at most {maximum}")

        if status.get("auth_required", True):
            raise RuntimeError(
                "this aircraft still runs authenticated firmware; install the private "
                "development firmware once over USB"
            )
        print("authentication: disabled by the private development firmware")

        next_slot = str(status.get("next_slot", ""))
        print(f"image: {args.firmware}")
        print(f"sha256: {digest}")
        print(f"target: {args.host}:{args.port} {status.get('active_slot')} -> {next_slot}")
        response_received = False
        try:
            result = _upload(
                args.host,
                args.port,
                args.firmware,
                size,
                digest,
                args.timeout,
            )
            response_received = True
            print(f"firmware accepted in {result.get('next_slot')}; aircraft is rebooting")
        except (OSError, http.client.HTTPException) as response_error:
            # The aircraft may reboot after committing the slot but before the
            # final HTTP response reaches the host. Never retry the write
            # automatically; prove the expected slot instead.
            print(f"upload response lost ({response_error}); checking boot slot")
        if not args.no_wait:
            validated = _wait_for_validated_boot(args.host, args.port, next_slot)
            print(
                "OTA complete: "
                f"active={validated.get('active_slot')} pending_verify={validated.get('pending_verify')}"
            )
        elif not response_received:
            raise RuntimeError("upload response was lost; omit --no-wait so the boot slot can be verified")
        return 0
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as exc:
        print(f"OTA failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
