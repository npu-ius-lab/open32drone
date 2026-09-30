package com.osrbot.open32drone.controller;

/** Recognises the emergency-disarm corner gesture on the circular virtual stick. */
final class StickGesture {
    static final int NONE = 0;
    static final int DISARM = 1;

    private static final float CORNER_AXIS_MIN = 0.52f;
    private static final float OUTER_RADIUS_MIN = 0.78f;

    private StickGesture() {
    }

    static int classify(float x, float y) {
        if (y > -CORNER_AXIS_MIN || Math.hypot(x, y) < OUTER_RADIUS_MIN) return NONE;
        if (x <= -CORNER_AXIS_MIN) return DISARM;
        return NONE;
    }
}
