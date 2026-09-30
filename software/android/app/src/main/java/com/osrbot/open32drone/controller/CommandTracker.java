package com.osrbot.open32drone.controller;

/** Tracks the single safety-critical MAVLink command currently awaiting an ACK. */
final class CommandTracker {
    private final long timeoutMs;
    private int pendingCommand = -1;
    private long pendingSinceMs;
    private long lastSentMs;
    private int sendCount;

    CommandTracker(long timeoutMs) {
        this.timeoutMs = timeoutMs;
    }

    synchronized boolean begin(int command, long nowMs) {
        if (pendingCommand >= 0) return false;
        pendingCommand = command;
        pendingSinceMs = nowMs;
        lastSentMs = 0;
        sendCount = 0;
        return true;
    }

    synchronized int recordSend(int command, long nowMs) {
        if (command != pendingCommand) return -1;
        int confirmation = Math.min(sendCount, 255);
        sendCount++;
        lastSentMs = nowMs;
        return confirmation;
    }

    synchronized boolean retryDue(long nowMs, long retryIntervalMs, int maxAttempts) {
        return pendingCommand >= 0 && sendCount > 0 && sendCount < maxAttempts
                && nowMs - lastSentMs >= retryIntervalMs;
    }

    synchronized boolean acknowledge(int command) {
        if (command != pendingCommand) return false;
        pendingCommand = -1;
        pendingSinceMs = 0;
        lastSentMs = 0;
        sendCount = 0;
        return true;
    }

    synchronized int expire(long nowMs) {
        if (pendingCommand < 0 || nowMs - pendingSinceMs < timeoutMs) return -1;
        int expired = pendingCommand;
        pendingCommand = -1;
        pendingSinceMs = 0;
        lastSentMs = 0;
        sendCount = 0;
        return expired;
    }

    synchronized int pendingCommand() {
        return pendingCommand;
    }

    synchronized void clear() {
        pendingCommand = -1;
        pendingSinceMs = 0;
        lastSentMs = 0;
        sendCount = 0;
    }
}
