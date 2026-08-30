package com.osrbot.open32drone.controller;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Minimal MAVLink 1/2 codec for the messages used by Open32Drone. */
final class MavlinkCodec {
    static final int MSG_HEARTBEAT = 0;
    static final int MSG_SYS_STATUS = 1;
    static final int MSG_ATTITUDE = 30;
    static final int MSG_ATTITUDE_QUATERNION = 31;
    static final int MSG_LOCAL_POSITION_NED = 32;
    static final int MSG_MANUAL_CONTROL = 69;
    static final int MSG_COMMAND_LONG = 76;
    static final int MSG_COMMAND_ACK = 77;
    static final int MSG_EXTENDED_SYS_STATE = 245;
    static final int MSG_STATUSTEXT = 253;
    static final int MSG_CURRENT_MODE = 436;

    static final int STANDARD_MODE_TAKEOFF = 8;
    static final int STANDARD_MODE_LAND = 9;

    static final int CMD_NAV_LAND = 21;
    static final int CMD_NAV_TAKEOFF = 22;
    static final int CMD_COMPONENT_ARM_DISARM = 400;

    private static final int MAVLINK2_MAGIC = 0xFD;
    private static final int MAVLINK1_MAGIC = 0xFE;
    private static final Map<Integer, Integer> CRC_EXTRAS = new HashMap<>();

    static {
        CRC_EXTRAS.put(MSG_HEARTBEAT, 50);
        CRC_EXTRAS.put(MSG_SYS_STATUS, 124);
        CRC_EXTRAS.put(MSG_ATTITUDE, 39);
        CRC_EXTRAS.put(MSG_ATTITUDE_QUATERNION, 246);
        CRC_EXTRAS.put(MSG_LOCAL_POSITION_NED, 185);
        CRC_EXTRAS.put(MSG_MANUAL_CONTROL, 243);
        CRC_EXTRAS.put(MSG_COMMAND_LONG, 152);
        CRC_EXTRAS.put(MSG_COMMAND_ACK, 143);
        CRC_EXTRAS.put(MSG_EXTENDED_SYS_STATE, 130);
        CRC_EXTRAS.put(MSG_STATUSTEXT, 83);
        CRC_EXTRAS.put(MSG_CURRENT_MODE, 193);
    }

    private MavlinkCodec() {
    }

    static byte[] heartbeat(int sequence, int systemId, int componentId) {
        ByteBuffer payload = littleEndian(9);
        payload.putInt(0);             // custom_mode
        payload.put((byte) 6);         // MAV_TYPE_GCS
        payload.put((byte) 8);         // MAV_AUTOPILOT_INVALID
        payload.put((byte) 0);         // base_mode
        payload.put((byte) 4);         // MAV_STATE_ACTIVE
        payload.put((byte) 3);         // mavlink_version
        return frameV2(MSG_HEARTBEAT, 50, sequence, systemId, componentId, payload.array());
    }

    static byte[] manualControl(int sequence, int systemId, int componentId, int targetSystem,
                                short pitch, short roll, short throttle, short yaw) {
        ByteBuffer payload = littleEndian(11);
        payload.putShort(pitch);
        payload.putShort(roll);
        payload.putShort(throttle);
        payload.putShort(yaw);
        payload.putShort((short) 0);    // buttons
        payload.put((byte) targetSystem);
        return frameV2(MSG_MANUAL_CONTROL, 243, sequence, systemId, componentId, payload.array());
    }

    static byte[] commandLong(int sequence, int systemId, int componentId, int targetSystem,
                              int targetComponent, int command, float... parameters) {
        return commandLongWithConfirmation(sequence, systemId, componentId, targetSystem,
                targetComponent, command, 0, parameters);
    }

    static byte[] commandLongWithConfirmation(int sequence, int systemId, int componentId,
                                               int targetSystem, int targetComponent, int command,
                                               int confirmation, float... parameters) {
        ByteBuffer payload = littleEndian(33);
        for (int i = 0; i < 7; i++) {
            payload.putFloat(i < parameters.length ? parameters[i] : Float.NaN);
        }
        payload.putShort((short) command);
        payload.put((byte) targetSystem);
        payload.put((byte) targetComponent);
        payload.put((byte) Math.max(0, Math.min(confirmation, 255)));
        return frameV2(MSG_COMMAND_LONG, 152, sequence, systemId, componentId, payload.array());
    }

    static List<Frame> parseDatagram(byte[] datagram, int length) {
        List<Frame> frames = new ArrayList<>();
        int cursor = 0;
        while (cursor < length) {
            int magic = unsigned(datagram[cursor]);
            if (magic != MAVLINK2_MAGIC && magic != MAVLINK1_MAGIC) {
                cursor++;
                continue;
            }

            int payloadLength = cursor + 1 < length ? unsigned(datagram[cursor + 1]) : -1;
            int headerLength = magic == MAVLINK2_MAGIC ? 10 : 6;
            int signatureLength = magic == MAVLINK2_MAGIC && cursor + 2 < length
                    && (unsigned(datagram[cursor + 2]) & 0x01) != 0 ? 13 : 0;
            int frameLength = headerLength + payloadLength + 2 + signatureLength;
            if (payloadLength < 0 || cursor + frameLength > length) {
                break;
            }

            int messageId;
            int systemId;
            int componentId;
            int payloadOffset;
            if (magic == MAVLINK2_MAGIC) {
                systemId = unsigned(datagram[cursor + 5]);
                componentId = unsigned(datagram[cursor + 6]);
                messageId = unsigned(datagram[cursor + 7])
                        | (unsigned(datagram[cursor + 8]) << 8)
                        | (unsigned(datagram[cursor + 9]) << 16);
                payloadOffset = cursor + 10;
            } else {
                systemId = unsigned(datagram[cursor + 3]);
                componentId = unsigned(datagram[cursor + 4]);
                messageId = unsigned(datagram[cursor + 5]);
                payloadOffset = cursor + 6;
            }

            Integer crcExtra = CRC_EXTRAS.get(messageId);
            if (crcExtra != null && validChecksum(datagram, cursor, headerLength, payloadLength, crcExtra)) {
                byte[] payload = new byte[payloadLength];
                System.arraycopy(datagram, payloadOffset, payload, 0, payloadLength);
                frames.add(new Frame(messageId, systemId, componentId, payload));
            }
            cursor += frameLength;
        }
        return frames;
    }

    static int unsigned(byte value) {
        return value & 0xFF;
    }

    static int unsignedShort(byte[] payload, int offset) {
        if (offset + 2 > payload.length) return 0;
        return ByteBuffer.wrap(payload, offset, 2).order(ByteOrder.LITTLE_ENDIAN).getShort() & 0xFFFF;
    }

    static int signedByte(byte[] payload, int offset) {
        return offset < payload.length ? payload[offset] : -1;
    }

    static int littleInt(byte[] payload, int offset) {
        if (offset + 4 > payload.length) return 0;
        return ByteBuffer.wrap(payload, offset, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }

    static float littleFloat(byte[] payload, int offset) {
        if (offset + 4 > payload.length) return Float.NaN;
        return ByteBuffer.wrap(payload, offset, 4).order(ByteOrder.LITTLE_ENDIAN).getFloat();
    }

    static int currentStandardMode(byte[] payload) {
        return payload.length >= 9 ? unsigned(payload[8]) : -1;
    }

    static int currentCustomMode(byte[] payload) {
        return payload.length >= 4 ? littleInt(payload, 0) : -1;
    }

    static String statusText(byte[] payload) {
        if (payload.length <= 1) return "";
        int end = 1;
        int limit = Math.min(payload.length, 51);
        while (end < limit && payload[end] != 0) end++;
        return new String(payload, 1, end - 1, StandardCharsets.UTF_8).trim();
    }

    static float[] quaternionEulerDegrees(byte[] payload) {
        if (payload.length < 20) return new float[] {Float.NaN, Float.NaN, Float.NaN};
        float w = littleFloat(payload, 4);
        float x = littleFloat(payload, 8);
        float y = littleFloat(payload, 12);
        float z = littleFloat(payload, 16);
        if (!Float.isFinite(w) || !Float.isFinite(x) || !Float.isFinite(y) || !Float.isFinite(z)) {
            return new float[] {Float.NaN, Float.NaN, Float.NaN};
        }
        double roll = Math.atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y));
        double sinPitch = 2.0 * (w * y - z * x);
        double pitch = Math.asin(Math.max(-1.0, Math.min(1.0, sinPitch)));
        double yaw = Math.atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));
        return new float[] {(float) Math.toDegrees(roll), (float) Math.toDegrees(pitch),
                (float) Math.toDegrees(yaw)};
    }

    private static ByteBuffer littleEndian(int size) {
        return ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN);
    }

    private static byte[] frameV2(int messageId, int crcExtra, int sequence, int systemId,
                                  int componentId, byte[] payload) {
        byte[] frame = new byte[12 + payload.length];
        frame[0] = (byte) MAVLINK2_MAGIC;
        frame[1] = (byte) payload.length;
        frame[2] = 0;
        frame[3] = 0;
        frame[4] = (byte) sequence;
        frame[5] = (byte) systemId;
        frame[6] = (byte) componentId;
        frame[7] = (byte) messageId;
        frame[8] = (byte) (messageId >> 8);
        frame[9] = (byte) (messageId >> 16);
        System.arraycopy(payload, 0, frame, 10, payload.length);

        int crc = 0xFFFF;
        for (int i = 1; i < 10 + payload.length; i++) crc = crcAccumulate(frame[i], crc);
        crc = crcAccumulate((byte) crcExtra, crc);
        frame[10 + payload.length] = (byte) crc;
        frame[11 + payload.length] = (byte) (crc >> 8);
        return frame;
    }

    private static boolean validChecksum(byte[] frame, int start, int headerLength,
                                         int payloadLength, int crcExtra) {
        int crc = 0xFFFF;
        for (int i = start + 1; i < start + headerLength + payloadLength; i++) {
            crc = crcAccumulate(frame[i], crc);
        }
        crc = crcAccumulate((byte) crcExtra, crc);
        int checksumOffset = start + headerLength + payloadLength;
        int expected = unsigned(frame[checksumOffset]) | (unsigned(frame[checksumOffset + 1]) << 8);
        return crc == expected;
    }

    private static int crcAccumulate(byte value, int crc) {
        int tmp = unsigned(value) ^ (crc & 0xFF);
        tmp ^= (tmp << 4) & 0xFF;
        return ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF;
    }

    static final class Frame {
        final int messageId;
        final int systemId;
        final int componentId;
        final byte[] payload;

        Frame(int messageId, int systemId, int componentId, byte[] payload) {
            this.messageId = messageId;
            this.systemId = systemId;
            this.componentId = componentId;
            this.payload = payload;
        }
    }
}
