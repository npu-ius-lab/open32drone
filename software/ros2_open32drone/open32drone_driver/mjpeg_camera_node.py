#!/usr/bin/env python3
# Copyright (c) 2026 Open32Drone project
# Open32Drone 驱动包：MJPEG 图传接收节点
# 接收飞控（open32drone_v3 固件）的 MJPEG 视频流，发布为 ROS2 图像话题。
# 用法：ros2 run open32drone_driver camera

import threading
import time
import urllib.request

import cv2
import numpy as np

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
from sensor_msgs.msg import CompressedImage, Image


class MjpegParser:
    """增量解析 multipart/x-mixed-replace MJPEG 流（固件 wifi.ino 格式）。"""

    def __init__(self, boundary: bytes):
        self.boundary = boundary
        self.delim = b"\r\n--" + boundary
        self.buf = b""
        self.content_length = 0
        self.state = 0  # 0: 等 boundary, 1: 等 headers, 2: 等 body
        self._max_skip = 1024 * 1024

    def feed(self, data: bytes):
        frames = []
        self.buf += data
        while True:
            if self.state == 0:
                idx = self.buf.find(self.delim)
                if idx < 0:
                    idx = self.buf.find(b"--" + self.boundary)
                    if idx >= 0:
                        self.buf = self.buf[idx + len(b"--" + self.boundary):]
                        self.state = 1
                        continue
                    if len(self.buf) > self._max_skip:
                        self.buf = self.buf[-len(self.delim) - 1:]
                    break
                self.buf = self.buf[idx + len(self.delim):]
                self.state = 1
            elif self.state == 1:
                idx = self.buf.find(b"\r\n\r\n")
                if idx < 0:
                    self.buf = self.buf[-4096:]
                    break
                header_block = self.buf[:idx].decode("ascii", errors="ignore")
                self.buf = self.buf[idx + 4:]
                self.content_length = self._parse_length(header_block)
                if self.content_length <= 0:
                    self.state = 0
                    continue
                self.state = 2
            else:
                if len(self.buf) >= self.content_length:
                    frame = self.buf[:self.content_length]
                    self.buf = self.buf[self.content_length:]
                    frames.append(frame)
                    self.state = 0
                else:
                    break
        return frames

    @staticmethod
    def _parse_length(header_block: str) -> int:
        for line in header_block.split("\r\n"):
            if line.lower().startswith("content-length:"):
                try:
                    return int(line.split(":", 1)[1].strip())
                except ValueError:
                    return 0
        return 0


class Open32DroneCameraNode(Node):
    def __init__(self):
        super().__init__("open32drone_camera")
        self.declare_parameter("url", "http://192.168.4.1/stream")
        self.declare_parameter("fps", 0.0)
        self.declare_parameter("frame_id", "camera")
        self.declare_parameter("publish_compressed", True)
        self.declare_parameter("reconnect_delay", 2.0)

        self.url = self.get_parameter("url").value
        self.fps = float(self.get_parameter("fps").value)
        self.frame_id = self.get_parameter("frame_id").value
        self.publish_compressed = bool(self.get_parameter("publish_compressed").value)
        self.reconnect_delay = float(self.get_parameter("reconnect_delay").value)

        qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=2,
        )
        self.pub_image = self.create_publisher(Image, "image_raw", qos)
        self.pub_compressed = self.create_publisher(
            CompressedImage, "image_raw/compressed", qos
        )

        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

        self.get_logger().info(
            "Open32Drone camera: %s -> image_raw (bgr8)" % self.url
        )

    def _run(self):
        min_interval = 1.0 / self.fps if self.fps > 0 else 0.0
        while not self._stop.is_set():
            try:
                self._stream_loop(min_interval)
            except Exception as exc:  # noqa: BLE001 - 保持节点存活
                if self._stop.is_set():
                    break
                self.get_logger().warn(
                    "Stream error: %s - reconnect in %.1fs" % (exc, self.reconnect_delay)
                )
                self._stop.wait(self.reconnect_delay)

    def _stream_loop(self, min_interval: float):
        req = urllib.request.Request(self.url)
        resp = urllib.request.urlopen(req, timeout=10)
        ct = resp.headers.get("Content-Type", "")
        boundary = self._extract_boundary(ct)
        parser = MjpegParser(boundary)
        self.get_logger().info("Connected, boundary=%s" % boundary.decode())

        last_publish = 0.0
        while not self._stop.is_set():
            chunk = resp.read(4096)
            if not chunk:
                self.get_logger().warn("Stream ended")
                break
            for jpeg in parser.feed(chunk):
                now = time.monotonic()
                if min_interval and (now - last_publish) < min_interval:
                    continue
                last_publish = now
                self._publish(jpeg)

    @staticmethod
    def _extract_boundary(content_type: str) -> bytes:
        import re

        m = re.search(r"boundary=(?:\"([^\"]+)\"|([^;]+))", content_type, re.I)
        if m:
            b = (m.group(1) or m.group(2)).strip()
            if b:
                return b.encode()
        # 固件 wifi.ino 的 PART_BOUNDARY
        return b"123456789000000000000987654321"

    def _publish(self, jpeg: bytes):
        arr = np.frombuffer(jpeg, dtype=np.uint8)
        img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
        if img is None:
            self.get_logger().warn("Failed to decode JPEG frame (%d bytes)" % len(jpeg))
            return

        stamp = self.get_clock().now().to_msg()
        h, w = img.shape[:2]

        msg = Image()
        msg.header.stamp = stamp
        msg.header.frame_id = self.frame_id
        msg.height = h
        msg.width = w
        msg.encoding = "bgr8"
        msg.is_bigendian = False
        msg.step = w * 3
        msg.data = img.tobytes()
        self.pub_image.publish(msg)

        if self.publish_compressed:
            comp = CompressedImage()
            comp.header.stamp = stamp
            comp.header.frame_id = self.frame_id
            comp.format = "jpeg"
            comp.data = jpeg
            self.pub_compressed.publish(comp)

    def shutdown(self):
        self._stop.set()
        self._thread.join(timeout=5.0)


def main(args=None):
    rclpy.init(args=args)
    node = Open32DroneCameraNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.shutdown()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
