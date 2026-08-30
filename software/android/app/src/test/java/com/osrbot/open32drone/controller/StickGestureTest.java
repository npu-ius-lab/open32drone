package com.osrbot.open32drone.controller;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class StickGestureTest {
    @Test
    public void circularBottomRightCornerDoesNotArm() {
        assertEquals(StickGesture.NONE, StickGesture.classify(0.707f, -0.707f));
    }

    @Test
    public void circularBottomLeftCornerDisarms() {
        assertEquals(StickGesture.DISARM, StickGesture.classify(-0.707f, -0.707f));
    }

    @Test
    public void centerAndCardinalDirectionsDoNotTrigger() {
        assertEquals(StickGesture.NONE, StickGesture.classify(0.0f, 0.0f));
        assertEquals(StickGesture.NONE, StickGesture.classify(0.0f, -1.0f));
        assertEquals(StickGesture.NONE, StickGesture.classify(1.0f, 0.0f));
    }

    @Test
    public void partialDiagonalDoesNotTrigger() {
        assertEquals(StickGesture.NONE, StickGesture.classify(0.45f, -0.45f));
    }
}
