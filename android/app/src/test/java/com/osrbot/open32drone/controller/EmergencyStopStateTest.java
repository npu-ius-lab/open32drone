package com.osrbot.open32drone.controller;

import static org.junit.Assert.*;
import org.junit.Test;

public class EmergencyStopStateTest {
    @Test public void ackIsNotProofOfMotorStop() {
        EmergencyStopState state = new EmergencyStopState();
        state.request(1000);
        state.acknowledge(0);
        assertTrue(state.pending());
        assertTrue(state.blocksControl());
        assertFalse(state.heartbeat(false, 999));
        assertFalse(state.heartbeat(true, 1001));
        assertTrue(state.heartbeat(false, 1002));
        assertFalse(state.pending());
        assertFalse(state.blocksControl());
    }

    @Test public void timeoutDoesNotResumeJoystickOrClaimSuccess() {
        EmergencyStopState state = new EmergencyStopState();
        state.request(1000);
        assertFalse(state.expire(3499, 2500));
        assertTrue(state.expire(3500, 2500));
        assertTrue(state.blocksControl());
        assertTrue(state.message().contains("unconfirmed"));
        assertTrue(state.heartbeat(false, 4000));
        assertFalse(state.blocksControl());
    }

    @Test public void rejectionCanBeRetriedAndLateAckCannotUndoConfirmation() {
        EmergencyStopState state = new EmergencyStopState();
        state.request(1000);
        state.acknowledge(2);
        assertFalse(state.pending());
        assertTrue(state.blocksControl());
        state.request(2000);
        assertTrue(state.heartbeat(false, 2001));
        state.acknowledge(0);
        assertTrue(state.message().contains("confirms disarmed"));
    }
}
