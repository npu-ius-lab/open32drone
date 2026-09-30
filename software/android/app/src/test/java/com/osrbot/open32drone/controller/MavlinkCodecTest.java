package com.osrbot.open32drone.controller;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import java.util.List;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import org.junit.Test;

public class MavlinkCodecTest {
    @Test
    public void heartbeatRoundTripsWithChecksumValidation() {
        byte[] packet = MavlinkCodec.heartbeat(0, 255, 190);
        assertArrayEquals(hex("fd 09 00 00 00 ff be 00 00 00 00 00 00 00 06 08 00 04 03 3d 48"), packet);
        List<MavlinkCodec.Frame> frames = MavlinkCodec.parseDatagram(packet, packet.length);
        assertEquals(1, frames.size());
        assertEquals(MavlinkCodec.MSG_HEARTBEAT, frames.get(0).messageId);
        assertEquals(255, frames.get(0).systemId);
        assertEquals(190, frames.get(0).componentId);
    }

    @Test
    public void corruptedPacketIsRejected() {
        byte[] packet = MavlinkCodec.heartbeat(7, 255, 190);
        packet[10] ^= 0x01;
        assertEquals(0, MavlinkCodec.parseDatagram(packet, packet.length).size());
    }

    @Test
    public void manualControlPayloadMatchesFirmwareAxisContract() {
        byte[] packet = MavlinkCodec.manualControl(1, 255, 190, 1,
                (short) 111, (short) -222, (short) 500, (short) 333);
        assertArrayEquals(hex("fd 0b 00 00 01 ff be 45 00 00 6f 00 22 ff f4 01 4d 01 00 00 01 66 cb"), packet);
        MavlinkCodec.Frame frame = MavlinkCodec.parseDatagram(packet, packet.length).get(0);
        assertEquals(MavlinkCodec.MSG_MANUAL_CONTROL, frame.messageId);
        assertArrayEquals(new byte[] {111, 0, 34, -1, -12, 1, 77, 1, 0, 0, 1}, frame.payload);
    }

    @Test
    public void takeoffCommandPlacesRelativeHeightInParam7() {
        byte[] packet = MavlinkCodec.commandLong(3, 255, 190, 1, 1,
                MavlinkCodec.CMD_NAV_TAKEOFF,
                Float.NaN, Float.NaN, Float.NaN, Float.NaN, Float.NaN, Float.NaN, 0.60f);
        MavlinkCodec.Frame frame = MavlinkCodec.parseDatagram(packet, packet.length).get(0);
        assertEquals(MavlinkCodec.MSG_COMMAND_LONG, frame.messageId);
        assertEquals(0.60f, MavlinkCodec.littleFloat(frame.payload, 24), 0.0001f);
        assertEquals(MavlinkCodec.CMD_NAV_TAKEOFF,
                MavlinkCodec.unsignedShort(frame.payload, 28));
        assertEquals(1, MavlinkCodec.unsigned(frame.payload[30]));
        assertEquals(1, MavlinkCodec.unsigned(frame.payload[31]));
        assertEquals(0, MavlinkCodec.unsigned(frame.payload[32]));
    }

    @Test
    public void retriedCommandCarriesMavlinkConfirmation() {
        byte[] packet = MavlinkCodec.commandLongWithConfirmation(4, 255, 190, 1, 1,
                MavlinkCodec.CMD_NAV_TAKEOFF, 2,
                Float.NaN, Float.NaN, Float.NaN, Float.NaN, Float.NaN, Float.NaN, 0.60f);
        MavlinkCodec.Frame frame = MavlinkCodec.parseDatagram(packet, packet.length).get(0);
        assertEquals(2, MavlinkCodec.unsigned(frame.payload[32]));
    }

    @Test
    public void firmwareQuaternionTelemetryConvertsToEulerDegrees() {
        byte[] payload = new byte[32];
        ByteBuffer buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
        buffer.putInt(1234);
        float halfSqrt = (float) Math.sqrt(0.5);
        buffer.putFloat(halfSqrt);
        buffer.putFloat(0.0f);
        buffer.putFloat(0.0f);
        buffer.putFloat(halfSqrt);
        float[] euler = MavlinkCodec.quaternionEulerDegrees(payload);
        assertEquals(0.0f, euler[0], 0.01f);
        assertEquals(0.0f, euler[1], 0.01f);
        assertEquals(90.0f, euler[2], 0.01f);
    }

    @Test
    public void currentModePayloadRestoresAutomaticTakeoffAfterReconnect() {
        byte[] payload = new byte[9];
        ByteBuffer buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN);
        buffer.putInt(3);
        buffer.putInt(3);
        buffer.put((byte) MavlinkCodec.STANDARD_MODE_TAKEOFF);
        assertEquals(3, MavlinkCodec.currentCustomMode(payload));
        assertEquals(MavlinkCodec.STANDARD_MODE_TAKEOFF,
                MavlinkCodec.currentStandardMode(payload));
    }

    private static byte[] hex(String value) {
        String[] parts = value.split(" ");
        byte[] bytes = new byte[parts.length];
        for (int i = 0; i < parts.length; i++) bytes[i] = (byte) Integer.parseInt(parts[i], 16);
        return bytes;
    }
}
