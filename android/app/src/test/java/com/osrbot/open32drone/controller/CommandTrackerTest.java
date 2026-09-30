package com.osrbot.open32drone.controller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class CommandTrackerTest {
    @Test
    public void matchingAckCompletesPendingCommand() {
        CommandTracker tracker = new CommandTracker(2500);
        assertTrue(tracker.begin(400, 1000));
        assertFalse(tracker.begin(22, 1001));
        assertFalse(tracker.acknowledge(21));
        assertEquals(400, tracker.pendingCommand());
        assertTrue(tracker.acknowledge(400));
        assertEquals(-1, tracker.pendingCommand());
    }

    @Test
    public void commandExpiresOnlyAfterTimeout() {
        CommandTracker tracker = new CommandTracker(2500);
        assertTrue(tracker.begin(22, 1000));
        assertEquals(-1, tracker.expire(3499));
        assertEquals(22, tracker.expire(3500));
        assertEquals(-1, tracker.pendingCommand());
    }

    @Test
    public void retryScheduleIsBoundedAndIncrementsConfirmation() {
        CommandTracker tracker = new CommandTracker(2500);
        assertTrue(tracker.begin(22, 1000));
        assertEquals(0, tracker.recordSend(22, 1000));
        assertFalse(tracker.retryDue(1349, 350, 3));
        assertTrue(tracker.retryDue(1350, 350, 3));
        assertEquals(1, tracker.recordSend(22, 1350));
        assertTrue(tracker.retryDue(1700, 350, 3));
        assertEquals(2, tracker.recordSend(22, 1700));
        assertFalse(tracker.retryDue(2050, 350, 3));
        assertEquals(-1, tracker.recordSend(21, 2050));
    }
}
