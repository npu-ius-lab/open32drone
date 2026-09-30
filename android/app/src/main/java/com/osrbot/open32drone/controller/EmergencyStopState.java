package com.osrbot.open32drone.controller;

/** Sending, command acceptance, and fresh disarmed telemetry are separate facts. */
final class EmergencyStopState {
    private long requestedAt;
    private boolean pending;
    private boolean controlBlocked;
    private String message = "";

    synchronized void request(long now) {
        requestedAt = now;
        pending = true;
        controlBlocked = true;
        message = "Emergency stop: attempting transmission";
    }

    synchronized void acknowledge(int result) {
        if (!pending) return;
        if (result == 0) message = "Emergency stop accepted; awaiting disarmed state";
        else if (result != 5) {
            pending = false;
            message = "Emergency stop rejected; result unconfirmed";
        }
    }

    synchronized boolean heartbeat(boolean armed, long receivedAt) {
        if (!controlBlocked || armed || receivedAt <= requestedAt) return false;
        pending = false;
        controlBlocked = false;
        message = "Emergency stop: FCU confirms disarmed";
        return true;
    }

    synchronized boolean expire(long now, long timeoutMs) {
        if (!pending || now - requestedAt < timeoutMs) return false;
        pending = false;
        message = "Emergency stop: result unconfirmed";
        return true;
    }

    synchronized boolean pending() { return pending; }
    synchronized boolean blocksControl() { return controlBlocked; }
    synchronized String message() { return message; }
}
