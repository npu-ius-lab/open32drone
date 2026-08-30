package com.osrbot.open32drone.controller;

final class FlightControlMapper {
    private static final float STICK_DEADBAND = 0.06f;

    private FlightControlMapper() {
    }

    static ManualControl map(float leftX, float leftY, float rightX, float rightY) {
        return map(leftX, leftY, rightX, rightY, 0.0f);
    }

    static ManualControl map(float leftX, float leftY, float rightX, float rightY, float expo) {
        short yaw = signedAxis(leftX, expo);
        short throttle = throttleAxis(leftY);
        short roll = signedAxis(rightX, expo);
        short pitch = signedAxis(rightY, expo);
        return new ManualControl(pitch, roll, throttle, yaw);
    }

    /** Ground-safe stream required by the firmware before MAVLink arming. */
    static ManualControl groundSafe() {
        return new ManualControl((short) 0, (short) 0, (short) 0, (short) 0);
    }

    static short signedAxis(float value) {
        return signedAxis(value, 0.0f);
    }

    static short signedAxis(float value, float expo) {
        float shaped = applyExpo(deadband(clamp(value)), expo);
        return (short) Math.round(shaped * 1000.0f);
    }

    static short throttleAxis(float value) {
        float shaped = deadband(clamp(value));
        return (short) Math.round((0.5f + shaped * 0.5f) * 1000.0f);
    }

    private static float deadband(float value) {
        float magnitude = Math.abs(value);
        if (magnitude <= STICK_DEADBAND) return 0.0f;
        float scaled = (magnitude - STICK_DEADBAND) / (1.0f - STICK_DEADBAND);
        return Math.copySign(scaled, value);
    }

    private static float applyExpo(float value, float expo) {
        float boundedExpo = Math.max(0.0f, Math.min(0.8f, expo));
        return (1.0f - boundedExpo) * value + boundedExpo * value * value * value;
    }

    private static float clamp(float value) {
        return Math.max(-1.0f, Math.min(1.0f, value));
    }

    static final class ManualControl {
        final short pitch;
        final short roll;
        final short throttle;
        final short yaw;

        ManualControl(short pitch, short roll, short throttle, short yaw) {
            this.pitch = pitch;
            this.roll = roll;
            this.throttle = throttle;
            this.yaw = yaw;
        }

        static ManualControl neutral() {
            return new ManualControl((short) 0, (short) 0, (short) 500, (short) 0);
        }
    }
}
