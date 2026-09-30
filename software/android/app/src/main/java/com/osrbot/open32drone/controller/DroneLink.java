package com.osrbot.open32drone.controller;

import android.net.Network;
import android.os.Handler;
import android.os.Looper;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

final class DroneLink {
    static final int MODE_STABILIZE = 2;
    static final int MODE_ALTITUDE_HOLD = 4;
    static final int MODE_POSITION_HOLD = 5;

    static final String DEFAULT_HOST = AircraftEndpoint.DEFAULT_HOST;
    private static final int MAVLINK_PORT = 14550;
    private static final int GCS_SYSTEM_ID = 255;
    private static final int GCS_COMPONENT_ID = 190;
    private static final long HEARTBEAT_TIMEOUT_MS = 3000;
    private static final long OPERATION_TIMEOUT_MS = 2500;
    private static final long COMMAND_RETRY_INTERVAL_MS = 350;
    private static final int COMMAND_MAX_ATTEMPTS = 3;
    private static final int EMERGENCY_DISARM_REPEATS = 3;
    private static final int PREARM_CHECK_BIT = 1 << 28;
    private static final int RC_RECEIVER_BIT = 1 << 16;
    private static final int LASER_POSITION_BIT = 1 << 8;

    interface Listener {
        void onState(DroneLink source, State state);
        void onLinkFailure(DroneLink source, String message);
    }

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final AtomicInteger sequence = new AtomicInteger();
    private final AtomicBoolean linkFailureReported = new AtomicBoolean();
    private final Network aircraftNetwork;
    private final String aircraftHost;
    private final Listener listener;

    private final CommandTracker commandTracker = new CommandTracker(OPERATION_TIMEOUT_MS);
    private volatile FlightControlMapper.ManualControl controls = FlightControlMapper.groundSafe();
    private volatile boolean running;
    private volatile long lastHeartbeatMs;
    private volatile long lastGcsHeartbeatMs;
    private volatile long lastPublishMs;
    private volatile int targetSystem = 1;
    private volatile int targetComponent = 1;
    private volatile boolean armed;
    private volatile boolean ready;
    private volatile boolean readyKnown;
    private volatile boolean physicalRcActive;
    private volatile boolean heightSensorHealthy;
    private volatile boolean landed = true;
    private volatile int mode = -1;
    private volatile int batteryPercent = -1;
    private volatile float voltage = Float.NaN;
    private volatile float roll = Float.NaN;
    private volatile float pitch = Float.NaN;
    private volatile float yaw = Float.NaN;
    private volatile float altitude = Float.NaN;
    private volatile String status = "";
    private volatile int automaticCommand = -1;
    private volatile long udpTxPackets;
    private volatile long udpRxPackets;
    private volatile long mavlinkFrames;
    private volatile CommandRequest pendingCommandRequest;

    private DatagramSocket socket;
    private InetAddress remoteAddress;
    private ScheduledExecutorService transmitter;
    private ExecutorService receiver;

    DroneLink(Network aircraftNetwork, String aircraftHost, Listener listener) {
        this.aircraftNetwork = aircraftNetwork;
        this.aircraftHost = AircraftEndpoint.normalizeIpv4(aircraftHost);
        this.listener = listener;
    }

    synchronized void start() {
        if (running) return;
        linkFailureReported.set(false);
        try {
            if (aircraftNetwork == null) {
                throw new IOException("aircraft Wi-Fi route unavailable");
            }
            remoteAddress = aircraftNetwork.getByName(aircraftHost);
            socket = new DatagramSocket(null);
            // Fail clearly if another controller already owns 14550. Sharing this
            // safety-critical socket can split telemetry between two operator clients.
            socket.setReuseAddress(false);
            socket.bind(new InetSocketAddress(MAVLINK_PORT));
            // Android may keep the no-Internet aircraft hotspot connected while
            // routing application traffic through cellular data. Bind this
            // safety-critical UDP socket to the actual Wi-Fi network so a route
            // preference change cannot silently stop MANUAL_CONTROL.
            aircraftNetwork.bindSocket(socket);
            socket.setBroadcast(true);
            socket.setSoTimeout(500);
        } catch (IOException error) {
            closeSocket();
            status = "UDP 14550 unavailable: " + error.getMessage();
            publishState();
            reportLinkFailure(status);
            return;
        }

        running = true;
        transmitter = Executors.newSingleThreadScheduledExecutor();
        receiver = Executors.newSingleThreadExecutor();
        transmitter.scheduleWithFixedDelay(this::transmitTick, 0, 40, TimeUnit.MILLISECONDS);
        receiver.execute(this::receiveLoop);
    }

    synchronized void stop() {
        FlightControlMapper.ManualControl finalControls = armed
                ? FlightControlMapper.ManualControl.neutral()
                : FlightControlMapper.groundSafe();
        controls = finalControls;
        ScheduledExecutorService currentTransmitter = transmitter;
        ExecutorService currentReceiver = receiver;
        DatagramSocket currentSocket = socket;
        InetAddress currentAddress = remoteAddress;
        running = false;
        commandTracker.clear();
        pendingCommandRequest = null;
        transmitter = null;
        receiver = null;
        socket = null;
        remoteAddress = null;
        if (currentReceiver != null) currentReceiver.shutdownNow();

        if (currentTransmitter != null) {
            // DatagramSocket.send must never run on Android's main thread. Queue the
            // final centered packets behind any in-flight transmitter tick, then close
            // the captured socket. The firmware timeout remains the authoritative
            // airborne fallback if these best-effort packets cannot be delivered.
            currentTransmitter.execute(() -> {
                for (int i = 0; i < 3; i++) {
                    sendNow(currentSocket, currentAddress,
                            MavlinkCodec.manualControl(nextSequence(), GCS_SYSTEM_ID,
                                    GCS_COMPONENT_ID, targetSystem, finalControls.pitch,
                                    finalControls.roll, finalControls.throttle,
                                    finalControls.yaw));
                }
                if (currentSocket != null) currentSocket.close();
            });
            currentTransmitter.shutdown();
        } else if (currentSocket != null) {
            currentSocket.close();
        }
    }

    void updateControls(FlightControlMapper.ManualControl value) {
        controls = value;
    }

    void emergencyDisarm() {
        if (!linkHealthy()) {
            status = "Emergency disarm not sent: flight controller offline";
            publishState();
            return;
        }
        // Emergency stop supersedes a mode/takeoff/landing request. Track the new
        // command so a late ACK cannot be mistaken for a newly initiated arm command.
        commandTracker.clear();
        pendingCommandRequest = null;
        commandTracker.begin(MavlinkCodec.CMD_COMPONENT_ARM_DISARM,
                System.currentTimeMillis());
        automaticCommand = -1;
        controls = FlightControlMapper.groundSafe();
        status = "Emergency disarm sent";
        // Disarm is idempotent. Send three independent UDP datagrams so a single
        // dropped packet does not turn the visible emergency action into a no-op.
        for (int attempt = 0; attempt < EMERGENCY_DISARM_REPEATS; attempt++) {
            send(MavlinkCodec.commandLong(nextSequence(), GCS_SYSTEM_ID, GCS_COMPONENT_ID,
                    targetSystem, targetComponent, MavlinkCodec.CMD_COMPONENT_ARM_DISARM,
                    0.0f, 0.0f));
        }
        publishState();
    }

    void takeoffPosition(float heightMeters) {
        if (sendCommand(MavlinkCodec.CMD_NAV_TAKEOFF,
                Float.NaN, Float.NaN, Float.NaN, Float.NaN, Float.NaN, Float.NaN,
                heightMeters)) {
            // The last datagram already sent on the ground remains z=0 for pre-arm.
            // While TAKEOFF is pending transmitTick withholds MANUAL_CONTROL. Prime
            // the next packet here, on the command thread, so an ACK cannot expose a
            // receiver/UI-thread race that sends one more z=0 and aborts takeoff.
            controls = FlightControlMapper.ManualControl.neutral();
        }
    }

    void land() {
        sendSupersedingLand();
    }

    private boolean sendCommand(int command, float... parameters) {
        if (!linkHealthy()) {
            status = "Command not sent: flight controller offline";
            publishState();
            return false;
        }
        long now = System.currentTimeMillis();
        if (!commandTracker.begin(command, now)) {
            status = "Wait for the current operation";
            publishState();
            return false;
        }
        CommandRequest request = new CommandRequest(command, parameters);
        pendingCommandRequest = request;
        status = commandName(command) + " request sent";
        sendTrackedCommand(request, now);
        publishState();
        return true;
    }

    private void sendSupersedingLand() {
        if (!linkHealthy()) {
            status = "Landing not sent: flight controller offline";
            publishState();
            return;
        }
        // LAND must remain available while TAKEOFF is pending. The FCU implements
        // LAND idempotently and validates armed state.
        commandTracker.clear();
        pendingCommandRequest = null;
        long now = System.currentTimeMillis();
        commandTracker.begin(MavlinkCodec.CMD_NAV_LAND, now);
        CommandRequest request = new CommandRequest(MavlinkCodec.CMD_NAV_LAND, new float[0]);
        pendingCommandRequest = request;
        status = "Landing command sent";
        sendTrackedCommand(request, now);
        publishState();
    }

    private void sendTrackedCommand(CommandRequest request, long nowMs) {
        int confirmation = commandTracker.recordSend(request.command, nowMs);
        if (confirmation < 0) return;
        send(MavlinkCodec.commandLongWithConfirmation(nextSequence(), GCS_SYSTEM_ID,
                GCS_COMPONENT_ID, targetSystem, targetComponent, request.command,
                confirmation, request.parameters));
    }

    private void transmitTick() {
        if (!running) return;
        long now = System.currentTimeMillis();
        CommandRequest request = pendingCommandRequest;
        if (request != null && commandTracker.retryDue(
                now, COMMAND_RETRY_INTERVAL_MS, COMMAND_MAX_ATTEMPTS)) {
            sendTrackedCommand(request, now);
        }
        int expiredCommand = commandTracker.expire(now);
        if (expiredCommand >= 0) {
            pendingCommandRequest = null;
            status = commandName(expiredCommand) + " acknowledgement timeout";
            if (expiredCommand == MavlinkCodec.CMD_NAV_TAKEOFF && !armed && mode != 3) {
                controls = FlightControlMapper.groundSafe();
            }
        }
        if (now - lastGcsHeartbeatMs >= 1000) {
            lastGcsHeartbeatMs = now;
            send(MavlinkCodec.heartbeat(nextSequence(), GCS_SYSTEM_ID, GCS_COMPONENT_ID));
        }

        // Do not keep a one-way link falsely alive with stale controls. If flight-controller
        // heartbeats stop, withholding MANUAL_CONTROL lets its 500 ms safety timeout own the
        // descent while the GCS heartbeat can still be used to recover telemetry.
        // The firmware starts automatic takeoff before its COMMAND_ACK returns.
        // A ground-safe z=0 packet in that short interval is an explicit takeoff
        // cancellation, so withhold MANUAL_CONTROL until ACK resolves the command.
        // The last packet before the command remains the required safe pre-arm z=0.
        boolean takeoffPending = commandTracker.pendingCommand()
                == MavlinkCodec.CMD_NAV_TAKEOFF;
        if (linkHealthy() && !takeoffPending) {
            FlightControlMapper.ManualControl current = controls;
            send(MavlinkCodec.manualControl(nextSequence(), GCS_SYSTEM_ID, GCS_COMPONENT_ID,
                    targetSystem, current.pitch, current.roll, current.throttle, current.yaw));
        }

        if (now - lastPublishMs >= 200) {
            lastPublishMs = now;
            publishState();
        }
    }

    private void receiveLoop() {
        byte[] buffer = new byte[4096];
        while (running) {
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
            try {
                socket.receive(packet);
                // A shared router can carry telemetry from several aircraft on the
                // same UDP port. Only the explicitly selected aircraft is allowed
                // to update mode, ownership, target IDs, or command state.
                InetAddress expectedAddress = remoteAddress;
                if (expectedAddress == null || !expectedAddress.equals(packet.getAddress())) {
                    continue;
                }
                udpRxPackets++;
                List<MavlinkCodec.Frame> frames = MavlinkCodec.parseDatagram(packet.getData(), packet.getLength());
                mavlinkFrames += frames.size();
                for (MavlinkCodec.Frame frame : frames) handleFrame(frame);
            } catch (SocketTimeoutException ignored) {
                // The transmitter publishes the disconnected state after the heartbeat timeout.
            } catch (IOException error) {
                if (running) {
                    status = "UDP receive failed: " + error.getMessage();
                    publishState();
                    reportLinkFailure(status);
                }
                break;
            }
        }
    }

    private void handleFrame(MavlinkCodec.Frame frame) {
        byte[] payload = frame.payload;
        switch (frame.messageId) {
            case MavlinkCodec.MSG_HEARTBEAT:
                if (payload.length >= 9 && MavlinkCodec.unsigned(payload[5]) != 8) {
                    targetSystem = frame.systemId;
                    targetComponent = frame.componentId;
                    mode = MavlinkCodec.littleInt(payload, 0);
                    armed = (MavlinkCodec.unsigned(payload[6]) & 0x80) != 0;
                    lastHeartbeatMs = System.currentTimeMillis();
                }
                break;
            case MavlinkCodec.MSG_SYS_STATUS:
                if (payload.length >= 31) {
                    int health = MavlinkCodec.littleInt(payload, 8);
                    heightSensorHealthy = (health & LASER_POSITION_BIT) != 0;
                    // READY describes the FCU's generic pre-arm state. ToF remains a
                    // visible diagnostic, but the firmware is the sole authority for
                    // assisted takeoff: it checks the freshest range/blind-zone packet
                    // atomically when MAV_CMD_NAV_TAKEOFF arrives and returns an ACK.
                    // Duplicating that gate here can permanently block a healthy FCU
                    // because this client only sees the slower SYS_STATUS snapshot.
                    ready = (health & PREARM_CHECK_BIT) != 0;
                    physicalRcActive = (health & RC_RECEIVER_BIT) != 0;
                    readyKnown = true;
                    int millivolts = MavlinkCodec.unsignedShort(payload, 14);
                    voltage = millivolts == 0xFFFF ? Float.NaN : millivolts / 1000.0f;
                    batteryPercent = MavlinkCodec.signedByte(payload, 30);
                }
                break;
            case MavlinkCodec.MSG_ATTITUDE:
                if (payload.length >= 16) {
                    roll = (float) Math.toDegrees(MavlinkCodec.littleFloat(payload, 4));
                    pitch = (float) Math.toDegrees(MavlinkCodec.littleFloat(payload, 8));
                    yaw = (float) Math.toDegrees(MavlinkCodec.littleFloat(payload, 12));
                }
                break;
            case MavlinkCodec.MSG_ATTITUDE_QUATERNION:
                float[] euler = MavlinkCodec.quaternionEulerDegrees(payload);
                roll = euler[0];
                pitch = euler[1];
                yaw = euler[2];
                break;
            case MavlinkCodec.MSG_LOCAL_POSITION_NED:
                if (payload.length >= 16) altitude = -MavlinkCodec.littleFloat(payload, 12);
                break;
            case MavlinkCodec.MSG_EXTENDED_SYS_STATE:
                if (payload.length >= 2) landed = MavlinkCodec.unsigned(payload[1]) == 1;
                break;
            case MavlinkCodec.MSG_CURRENT_MODE:
                if (payload.length >= 9) {
                    int reportedMode = MavlinkCodec.currentCustomMode(payload);
                    int standardMode = MavlinkCodec.currentStandardMode(payload);
                    mode = reportedMode;
                    int previousAutomaticCommand = automaticCommand;
                    if (standardMode == MavlinkCodec.STANDARD_MODE_TAKEOFF) {
                        automaticCommand = MavlinkCodec.CMD_NAV_TAKEOFF;
                    } else if (standardMode == MavlinkCodec.STANDARD_MODE_LAND) {
                        automaticCommand = MavlinkCodec.CMD_NAV_LAND;
                    } else {
                        // AUTO without TAKEOFF/LAND is Offboard or a failsafe, not
                        // the previous vertical command. Clear the stale label so
                        // its diagnostic/status and control policy are visible.
                        automaticCommand = -1;
                    }
                    if (automaticCommand != previousAutomaticCommand) publishState();
                }
                break;
            case MavlinkCodec.MSG_STATUSTEXT:
                String text = MavlinkCodec.statusText(payload);
                if (!text.isEmpty()) {
                    status = text;
                }
                break;
            case MavlinkCodec.MSG_COMMAND_ACK:
                if (payload.length >= 3) {
                    int command = MavlinkCodec.unsignedShort(payload, 0);
                    int result = MavlinkCodec.unsigned(payload[2]);
                    if (!commandTracker.acknowledge(command)) break;
                    pendingCommandRequest = null;
                    if (result == 0 && (command == MavlinkCodec.CMD_NAV_TAKEOFF
                            || command == MavlinkCodec.CMD_NAV_LAND)) {
                        automaticCommand = command;
                    } else if (automaticCommand == command) {
                        automaticCommand = -1;
                    }
                    if (command == MavlinkCodec.CMD_NAV_TAKEOFF && result != 0
                            && !armed && mode != 3) {
                        controls = FlightControlMapper.groundSafe();
                    }
                    status = commandName(command) + ": " + commandResult(result);
                    publishState();
                }
                break;
            default:
                break;
        }
    }

    private void send(byte[] bytes) {
        ScheduledExecutorService currentTransmitter = transmitter;
        if (!running || currentTransmitter == null || currentTransmitter.isShutdown()) return;
        try {
            currentTransmitter.execute(() -> {
                if (running) sendNow(socket, remoteAddress, bytes);
            });
        } catch (RejectedExecutionException ignored) {
            // A concurrent lifecycle stop already owns socket shutdown.
        }
    }

    private void sendNow(DatagramSocket currentSocket, InetAddress currentAddress, byte[] bytes) {
        if (currentSocket == null || currentAddress == null) return;
        try {
            currentSocket.send(new DatagramPacket(bytes, bytes.length, currentAddress, MAVLINK_PORT));
            udpTxPackets++;
        } catch (IOException | RuntimeException error) {
            if (running) {
                status = "UDP send failed: " + error.getMessage();
                publishState();
                reportLinkFailure(status);
            }
        }
    }

    private void reportLinkFailure(String message) {
        if (!linkFailureReported.compareAndSet(false, true)) return;
        mainHandler.post(() -> listener.onLinkFailure(this, message));
    }

    private int nextSequence() {
        return sequence.getAndIncrement() & 0xFF;
    }

    private void publishState() {
        long age = lastHeartbeatMs == 0 ? Long.MAX_VALUE : System.currentTimeMillis() - lastHeartbeatMs;
        boolean connected = age <= HEARTBEAT_TIMEOUT_MS;
        Phase phase = !connected ? Phase.DISCONNECTED
                : armed ? Phase.ARMED
                : !readyKnown ? Phase.CHECKING
                : ready ? Phase.READY : Phase.NOT_READY;
        State snapshot = new State(phase, connected, age, targetSystem, targetComponent,
                readyKnown, ready, physicalRcActive, heightSensorHealthy, armed, landed, mode,
                batteryPercent, voltage,
                roll, pitch, yaw, altitude, commandTracker.pendingCommand(), automaticCommand,
                udpTxPackets, udpRxPackets, mavlinkFrames, status);
        mainHandler.post(() -> listener.onState(this, snapshot));
    }

    private boolean linkHealthy() {
        return lastHeartbeatMs != 0 && System.currentTimeMillis() - lastHeartbeatMs <= HEARTBEAT_TIMEOUT_MS;
    }

    private static String commandResult(int result) {
        switch (result) {
            case 0: return "accepted";
            case 1: return "temporarily rejected";
            case 2: return "denied";
            case 3: return "unsupported";
            case 4: return "failed";
            case 5: return "in progress";
            case 6: return "cancelled";
            default: return "result " + result;
        }
    }

    private static String commandName(int command) {
        switch (command) {
            case MavlinkCodec.CMD_NAV_TAKEOFF: return "Takeoff";
            case MavlinkCodec.CMD_NAV_LAND: return "Landing";
            case MavlinkCodec.CMD_COMPONENT_ARM_DISARM: return "Emergency stop";
            default: return String.format(Locale.US, "Command %d", command);
        }
    }

    private synchronized void closeSocket() {
        if (socket != null) socket.close();
        socket = null;
    }

    enum Phase {
        DISCONNECTED,
        CHECKING,
        NOT_READY,
        READY,
        ARMED
    }

    private static final class CommandRequest {
        final int command;
        final float[] parameters;

        CommandRequest(int command, float[] parameters) {
            this.command = command;
            this.parameters = parameters.clone();
        }
    }

    static final class State {
        final Phase phase;
        final boolean connected;
        final long heartbeatAgeMs;
        final int targetSystem;
        final int targetComponent;
        final boolean readyKnown;
        final boolean ready;
        final boolean physicalRcActive;
        final boolean heightSensorHealthy;
        final boolean armed;
        final boolean landed;
        final int mode;
        final int batteryPercent;
        final float voltage;
        final float roll;
        final float pitch;
        final float yaw;
        final float altitude;
        final int pendingCommand;
        final int automaticCommand;
        final long udpTxPackets;
        final long udpRxPackets;
        final long mavlinkFrames;
        final String status;

        State(Phase phase, boolean connected, long heartbeatAgeMs, int targetSystem,
              int targetComponent, boolean readyKnown, boolean ready, boolean physicalRcActive,
              boolean heightSensorHealthy, boolean armed, boolean landed, int mode,
              int batteryPercent, float voltage,
              float roll, float pitch, float yaw, float altitude, int pendingCommand,
              int automaticCommand, long udpTxPackets, long udpRxPackets,
              long mavlinkFrames, String status) {
            this.phase = phase;
            this.connected = connected;
            this.heartbeatAgeMs = heartbeatAgeMs;
            this.targetSystem = targetSystem;
            this.targetComponent = targetComponent;
            this.readyKnown = readyKnown;
            this.ready = ready;
            this.physicalRcActive = physicalRcActive;
            this.heightSensorHealthy = heightSensorHealthy;
            this.armed = armed;
            this.landed = landed;
            this.mode = mode;
            this.batteryPercent = batteryPercent;
            this.voltage = voltage;
            this.roll = roll;
            this.pitch = pitch;
            this.yaw = yaw;
            this.altitude = altitude;
            this.pendingCommand = pendingCommand;
            this.automaticCommand = automaticCommand;
            this.udpTxPackets = udpTxPackets;
            this.udpRxPackets = udpRxPackets;
            this.mavlinkFrames = mavlinkFrames;
            this.status = status;
        }
    }
}
