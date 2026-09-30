package com.osrbot.open32drone.controller;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class CameraFramePolicyTest {
    @Test
    public void requiresAReceivedFrame() {
        assertFalse(CameraFramePolicy.shouldShowLiveVideo(0, 1100, 2000));
    }

    @Test
    public void acceptsFrameAtFreshnessBoundary() {
        assertTrue(CameraFramePolicy.shouldShowLiveVideo(1000, 3000, 2000));
    }

    @Test
    public void rejectsStaleOrInvalidClockOrder() {
        assertFalse(CameraFramePolicy.shouldShowLiveVideo(1000, 3001, 2000));
        assertFalse(CameraFramePolicy.shouldShowLiveVideo(2000, 1999, 2000));
    }
}
