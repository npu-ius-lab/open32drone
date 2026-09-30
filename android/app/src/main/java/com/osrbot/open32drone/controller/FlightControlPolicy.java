package com.osrbot.open32drone.controller;

/** Keeps UI availability and the transmitted MANUAL_CONTROL policy identical. */
final class FlightControlPolicy {
    private static final int MODE_AUTOMATIC = 3;

    private FlightControlPolicy() {
    }

    static Decision evaluate(boolean connected, boolean armed, boolean physicalRcActive,
                             int mode, int pendingCommand, int automaticCommand) {
        boolean pendingVertical = isVerticalAutomationCommand(pendingCommand);
        boolean activeVertical = isVerticalAutomationCommand(automaticCommand);
        boolean verticalAutomation = pendingVertical || activeVertical;
        boolean operationPending = pendingCommand >= 0;
        boolean blockingOperation = pendingCommand >= 0 && !pendingVertical;
        boolean unrecognizedAutomatic = mode == MODE_AUTOMATIC && !verticalAutomation;

        // A successful TAKEOFF ACK means the FCU has already passed pre-arm and armed,
        // although the next heartbeat can still contain the previous armed bit. This
        // lets the pilot intervene during that telemetry gap without weakening pre-arm.
        boolean acceptedTakeoffTransition = automaticCommand == MavlinkCodec.CMD_NAV_TAKEOFF;
        boolean flightControlActive = armed || acceptedTakeoffTransition;
        boolean manualControlEnabled = connected && flightControlActive
                && !physicalRcActive && !blockingOperation && !unrecognizedAutomatic;
        boolean leftStickEnabled = manualControlEnabled;

        return new Decision(operationPending, verticalAutomation, flightControlActive,
                manualControlEnabled, leftStickEnabled, manualControlEnabled);
    }

    static boolean isVerticalAutomationCommand(int command) {
        return command == MavlinkCodec.CMD_NAV_TAKEOFF
                || command == MavlinkCodec.CMD_NAV_LAND;
    }

    static final class Decision {
        final boolean operationPending;
        final boolean verticalAutomation;
        final boolean flightControlActive;
        final boolean manualControlEnabled;
        final boolean leftStickEnabled;
        final boolean rightStickEnabled;

        Decision(boolean operationPending, boolean verticalAutomation,
                 boolean flightControlActive, boolean manualControlEnabled,
                 boolean leftStickEnabled, boolean rightStickEnabled) {
            this.operationPending = operationPending;
            this.verticalAutomation = verticalAutomation;
            this.flightControlActive = flightControlActive;
            this.manualControlEnabled = manualControlEnabled;
            this.leftStickEnabled = leftStickEnabled;
            this.rightStickEnabled = rightStickEnabled;
        }
    }
}
