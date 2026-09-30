package com.osrbot.open32drone.controller;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertNull;

import java.io.ByteArrayInputStream;

import org.junit.Test;

public class MjpegFrameReaderTest {
    @Test
    public void extractsConsecutiveJpegFramesFromMultipartBytes() throws Exception {
        byte[] stream = new byte[] {
                'h', 'd', 'r', (byte) 0xff, (byte) 0xd8, 1, 2, (byte) 0xff, (byte) 0xd9,
                '\r', '\n', '-', '-', (byte) 0xff, (byte) 0xd8, 3, 4, (byte) 0xff, (byte) 0xd9
        };
        ByteArrayInputStream input = new ByteArrayInputStream(stream);
        assertArrayEquals(new byte[] {(byte) 0xff, (byte) 0xd8, 1, 2, (byte) 0xff, (byte) 0xd9},
                MjpegFrameReader.readFrame(input, 64));
        assertArrayEquals(new byte[] {(byte) 0xff, (byte) 0xd8, 3, 4, (byte) 0xff, (byte) 0xd9},
                MjpegFrameReader.readFrame(input, 64));
        assertNull(MjpegFrameReader.readFrame(input, 64));
    }

    @Test
    public void discardsOversizedFrameAndFindsTheNextOne() throws Exception {
        byte[] stream = new byte[] {
                (byte) 0xff, (byte) 0xd8, 1, 2, 3, 4, 5, 6, 7, 8, (byte) 0xff, (byte) 0xd9,
                (byte) 0xff, (byte) 0xd8, 9, (byte) 0xff, (byte) 0xd9
        };
        assertArrayEquals(new byte[] {(byte) 0xff, (byte) 0xd8, 9, (byte) 0xff, (byte) 0xd9},
                MjpegFrameReader.readFrame(new ByteArrayInputStream(stream), 8));
    }
}
