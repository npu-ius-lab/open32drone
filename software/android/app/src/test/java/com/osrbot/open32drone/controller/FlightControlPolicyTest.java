package com.osrbot.open32drone.controller;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class FlightControlPolicyTest {
    private static FlightControlPolicy.Decision policy(boolean connected, boolean armed,
                                                       boolean physicalRc, int mode,
                                                       int pendingCommand, int automaticCommand) {
        return FlightControlPolicy.evaluate(connected, armed, physicalRc, mode,
                pendingCommand, automaticCommand);
    }

    @Test
    public void normalArmedPositionFlightKeepsBothSticksAndManualOutput() {
        FlightControlPolicy.Decision result = policy(true, true, false, 5, -1, -1);
        assertTrue(result.manualControlEnabled);
        assertTrue(result.flightControlActive);
        assertTrue(result.leftStickEnabled);
        assertTrue(result.rightStickEnabled);
    }

    @Test
    public void disarmedTakeoffRequestWaitsForAcceptedAckWithGroundSafeControl() {
        FlightControlPolicy.Decision result = policy(true, false, false, 5,
                MavlinkCodec.CMD_NAV_TAKEOFF, -1);
        assertTrue(result.verticalAutomation);
        assertFalse(result.manualControlEnabled);
        assertFalse(result.flightControlActive);
        assertFalse(result.leftStickEnabled);
        assertFalse(result.rightStickEnabled);
    }

    @Test
    public void acceptedTakeoffUnlocksBothSticksBeforeHeartbeatCatchesUp() {
        FlightControlPolicy.Decision result = policy(true, false, false, 5, -1,
                MavlinkCodec.CMD_NAV_TAKEOFF);
        assertTrue(result.manualControlEnabled);
        assertTrue(result.flightControlActive);
        assertTrue(result.leftStickEnabled);
        assertTrue(result.rightStickEnabled);
    }

    @Test
    public void automaticLandingKeepsManualAttitudeAvailable() {
        FlightControlPolicy.Decision result = policy(true, true, false, 3, -1,
                MavlinkCodec.CMD_NAV_LAND);
        assertTrue(result.manualControlEnabled);
        assertTrue(result.leftStickEnabled);
        assertTrue(result.rightStickEnabled);
    }

    @Test
    public void unrelatedAutomaticModeOrCommandLocksManualOutput() {
        FlightControlPolicy.Decision offboard = policy(true, true, false, 3, -1, -1);
        assertFalse(offboard.manualControlEnabled);
        assertFalse(offboard.leftStickEnabled);
        assertFalse(offboard.rightStickEnabled);

        FlightControlPolicy.Decision armPending = policy(true, true, false, 5, 400, -1);
        assertFalse(armPending.manualControlEnabled);
    }

    @Test
    public void physicalRcAlwaysRemovesPhoneAuthority() {
        FlightControlPolicy.Decision result = policy(true, true, true, 3, -1,
                MavlinkCodec.CMD_NAV_TAKEOFF);
        assertFalse(result.manualControlEnabled);
        assertFalse(result.leftStickEnabled);
        assertFalse(result.rightStickEnabled);
    }
}
