package com.osrbot.open32drone.controller;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

final class MjpegFrameReader {
    private static final int JPEG_MARKER_PREFIX = 0xff;
    private static final int JPEG_START = 0xd8;
    private static final int JPEG_END = 0xd9;

    private MjpegFrameReader() {}

    static byte[] readFrame(InputStream input, int maxFrameBytes) throws IOException {
        ByteArrayOutputStream frame = null;
        int previous = -1;
        int current;
        while ((current = input.read()) >= 0) {
            if (frame == null) {
                if (previous == JPEG_MARKER_PREFIX && current == JPEG_START) {
                    frame = new ByteArrayOutputStream(Math.min(maxFrameBytes, 16 * 1024));
                    frame.write(JPEG_MARKER_PREFIX);
                    frame.write(JPEG_START);
                }
            } else if (frame.size() >= maxFrameBytes) {
                frame = null;
            } else {
                frame.write(current);
                if (previous == JPEG_MARKER_PREFIX && current == JPEG_END) {
                    return frame.toByteArray();
                }
            }
            previous = current;
        }
        return null;
    }
}
