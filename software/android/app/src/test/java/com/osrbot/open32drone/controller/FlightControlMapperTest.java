package com.osrbot.open32drone.controller;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class FlightControlMapperTest {
    @Test
    public void centeredSticksProduceAltitudeHoldNeutral() {
        FlightControlMapper.ManualControl control = FlightControlMapper.map(0, 0, 0, 0);
        assertEquals(0, control.pitch);
        assertEquals(0, control.roll);
        assertEquals(500, control.throttle);
        assertEquals(0, control.yaw);
    }

    @Test
    public void axesUseFirmwareManualControlOrdering() {
        FlightControlMapper.ManualControl control = FlightControlMapper.map(1, -1, -1, 1);
        assertEquals(1000, control.pitch);
        assertEquals(-1000, control.roll);
        assertEquals(0, control.throttle);
        assertEquals(1000, control.yaw);
    }

    @Test
    public void deadbandKeepsSmallTouchNoiseNeutral() {
        FlightControlMapper.ManualControl control = FlightControlMapper.map(0.03f, -0.02f, 0.05f, -0.01f);
        assertEquals(0, control.pitch);
        assertEquals(0, control.roll);
        assertEquals(500, control.throttle);
        assertEquals(0, control.yaw);
    }

    @Test
    public void groundSafeStreamUsesZeroThrottleForPrearm() {
        FlightControlMapper.ManualControl control = FlightControlMapper.groundSafe();
        assertEquals(0, control.pitch);
        assertEquals(0, control.roll);
        assertEquals(0, control.throttle);
        assertEquals(0, control.yaw);
    }

    @Test
    public void expoSoftensMidStickWithoutReducingEndpoint() {
        assertEquals(1000, FlightControlMapper.signedAxis(1.0f, 0.55f));
        short linear = FlightControlMapper.signedAxis(0.5f, 0.0f);
        short soft = FlightControlMapper.signedAxis(0.5f, 0.55f);
        org.junit.Assert.assertTrue(soft > 0);
        org.junit.Assert.assertTrue(soft < linear);
    }
}
