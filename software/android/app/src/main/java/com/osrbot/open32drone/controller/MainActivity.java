package com.osrbot.open32drone.controller;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.net.ConnectivityManager;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.RouteInfo;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.TextView;
import android.widget.Toast;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Minimal, position-hold-first controller for Open32Drone. */
public final class MainActivity extends Activity implements DroneLink.Listener {
    private static final String TAG = "Open32DroneController";
    private static final String APP_BUILD_ID = "0.1 (1)";
    private static final String PREFS = "open32drone_controller";
    private static final String PREF_GUIDE_SHOWN = "minimal_guide_shown_0_1";
    private static final String PREF_AIRCRAFT_HOST = "aircraft_ipv4";
    private static final long ACTION_HOLD_MS = 600;
    private static final long STICK_GESTURE_HOLD_MS = 200;
    private static final long WIFI_ROUTE_RETRY_MS = 1000;
    private static final long CAMERA_FRAME_STALE_MS = 2000;
    private static final long CAMERA_WATCHDOG_PERIOD_MS = 500;
    private static final float STICK_EXPO = 0.30f;
    private static final int OTA_FILE_REQUEST = 4108;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private DroneLink droneLink;
    private DroneLink.State latestState;
    private SharedPreferences preferences;
    private WifiManager.WifiLock wifiLock;
    private WifiManager.MulticastLock multicastLock;
    private ConnectivityManager connectivityManager;
    private Network controlNetwork;
    private String aircraftHost = AircraftEndpoint.DEFAULT_HOST;
    private boolean networkCallbackRegistered;
    private boolean resumed;
    private boolean otaInProgress;
    private boolean otaFileSelectionPending;
    private AlertDialog otaProgressDialog;
    private TextView otaProgressText;

    private TextView connectionText;
    private TextView telemetryText;
    private TextView safetyText;
    private TextView modeText;
    private TextView cameraStatsText;
    private ImageView cameraPreview;
    private EditText takeoffHeight;
    private Button toolsButton;
    private Button disarmButton;
    private Button takeoffButton;
    private Button landButton;
    private JoystickView leftJoystick;
    private JoystickView rightJoystick;

    private float leftX;
    private float leftY;
    private float rightX;
    private float rightY;
    private int pendingStickGesture = StickGesture.NONE;
    private boolean stickGestureLatched;

    private volatile MjpegStreamClient cameraStream;
    private volatile int cameraGeneration;
    private MjpegStreamClient.State cameraStreamState;
    private long lastCameraFrameElapsedMs;
    private Bitmap displayedCameraFrame;
    private final AtomicReference<PendingCameraFrame> pendingCameraFrame =
            new AtomicReference<>();
    private final AtomicBoolean cameraFramePosted = new AtomicBoolean();

    private final Runnable retryControlLink = this::refreshControlLink;

    private final Runnable deliverLatestCameraFrame = new Runnable() {
        @Override
        public void run() {
            cameraFramePosted.set(false);
            PendingCameraFrame pending = pendingCameraFrame.getAndSet(null);
            if (pending == null) return;
            if (cameraStream == null || pending.generation != cameraGeneration) {
                pending.bitmap.recycle();
            } else {
                Bitmap previous = displayedCameraFrame;
                displayedCameraFrame = pending.bitmap;
                cameraPreview.setImageBitmap(pending.bitmap);
                cameraPreview.setVisibility(View.VISIBLE);
                lastCameraFrameElapsedMs = SystemClock.elapsedRealtime();
                if (previous != null && previous != pending.bitmap) previous.recycle();
            }
            if (pendingCameraFrame.get() != null
                    && cameraFramePosted.compareAndSet(false, true)) {
                handler.post(this);
            }
        }
    };

    private final Runnable cameraWatchdog = new Runnable() {
        @Override
        public void run() {
            if (cameraStream == null) return;
            long now = SystemClock.elapsedRealtime();
            if (!CameraFramePolicy.shouldShowLiveVideo(
                    lastCameraFrameElapsedMs, now, CAMERA_FRAME_STALE_MS)) {
                hideCameraFrame();
                if (cameraStreamState == MjpegStreamClient.State.STREAMING) {
                    cameraStatsText.setText(R.string.camera_waiting_frame);
                }
            }
            handler.postDelayed(this, CAMERA_WATCHDOG_PERIOD_MS);
        }
    };

    private final ConnectivityManager.NetworkCallback aircraftWifiCallback =
            new ConnectivityManager.NetworkCallback() {
                @Override
                public void onAvailable(Network network) {
                    handler.post(MainActivity.this::refreshControlLink);
                }

                @Override
                public void onLinkPropertiesChanged(Network network, LinkProperties properties) {
                    handler.post(MainActivity.this::refreshControlLink);
                }

                @Override
                public void onLost(Network network) {
                    handler.post(() -> {
                        if (network.equals(controlNetwork)) stopControlLink();
                        scheduleControlLinkRetry();
                    });
                }
            };

    private final Runnable confirmStickGesture = () -> {
        int gesture = StickGesture.classify(leftX, leftY);
        if (gesture == StickGesture.NONE || gesture != pendingStickGesture) return;
        pendingStickGesture = StickGesture.NONE;
        stickGestureLatched = true;
        performStickGesture(gesture);
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        enterImmersiveMode();
        preferences = getSharedPreferences(PREFS, MODE_PRIVATE);
        try {
            aircraftHost = AircraftEndpoint.normalizeIpv4(
                    preferences.getString(PREF_AIRCRAFT_HOST, AircraftEndpoint.DEFAULT_HOST));
        } catch (IllegalArgumentException ignored) {
            aircraftHost = AircraftEndpoint.DEFAULT_HOST;
            preferences.edit().remove(PREF_AIRCRAFT_HOST).apply();
        }

        connectionText = findViewById(R.id.connectionText);
        telemetryText = findViewById(R.id.telemetryText);
        safetyText = findViewById(R.id.safetyText);
        modeText = findViewById(R.id.modeText);
        cameraStatsText = findViewById(R.id.cameraStatsText);
        cameraPreview = findViewById(R.id.cameraPreview);
        takeoffHeight = findViewById(R.id.takeoffHeight);
        toolsButton = findViewById(R.id.toolsButton);
        disarmButton = findViewById(R.id.disarmButton);
        takeoffButton = findViewById(R.id.takeoffButton);
        landButton = findViewById(R.id.landButton);
        leftJoystick = findViewById(R.id.leftJoystick);
        rightJoystick = findViewById(R.id.rightJoystick);

        leftJoystick.setListener((x, y) -> {
            leftX = x;
            leftY = y;
            updateStickGesture();
            updateManualControl();
        });
        rightJoystick.setListener((x, y) -> {
            rightX = x;
            rightY = y;
            updateManualControl();
        });

        toolsButton.setOnClickListener(this::showToolsMenu);
        configureHold(disarmButton, () -> withLink(DroneLink::emergencyDisarm));
        configureHold(takeoffButton, this::requestTakeoff);
        configureHold(landButton, () -> withLink(DroneLink::land));
        setControlsEnabled(null);

        if (!preferences.getBoolean(PREF_GUIDE_SHOWN, false)) {
            connectionText.post(this::showGuide);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        enterImmersiveMode();
        acquireWifiLock();
        registerAircraftWifiCallback();
        latestState = null;
        if (!otaInProgress) refreshControlLink();
        updateManualControl();
    }

    @Override
    protected void onPause() {
        resumed = false;
        handler.removeCallbacks(retryControlLink);
        unregisterAircraftWifiCallback();
        resetStickGesture();
        stopControlLink();
        if (!otaInProgress) releaseWifiLock();
        super.onPause();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != OTA_FILE_REQUEST) return;
        if (resultCode != RESULT_OK || data == null || data.getData() == null
                || !otaFileSelectionPending) {
            otaFileSelectionPending = false;
            return;
        }
        Uri imageUri = data.getData();
        otaFileSelectionPending = false;
        startFirmwareUpdate(imageUri);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) enterImmersiveMode();
    }

    @Override
    public void onState(DroneLink source, DroneLink.State state) {
        if (source != droneLink) return;
        latestState = state;
        int accent = getColor(R.color.accent);
        int warning = getColor(R.color.warning);
        connectionText.setText(state.connected
                ? getString(R.string.connection_state, phaseName(state.phase))
                : getString(R.string.waiting));
        connectionText.setTextColor(state.ready || state.armed ? accent : warning);

        String battery = state.batteryPercent >= 0
                ? String.format(Locale.US, "%d%%", state.batteryPercent) : "--";
        String voltage = Float.isFinite(state.voltage)
                ? String.format(Locale.US, "%.2f V", state.voltage) : "--";
        String altitude = Float.isFinite(state.altitude)
                ? String.format(Locale.US, "%.2f m", state.altitude) : "--";
        String controlSource = getString(state.physicalRcActive
                ? R.string.control_sbus : R.string.control_phone);
        telemetryText.setText(getString(
                R.string.flight_hud, altitude, battery, voltage, controlSource));
        modeText.setText(getString(R.string.mode_status, displayModeName(state.mode)));

        updateSafetyHint(state);
        setControlsEnabled(state);
        updateManualControl();
    }

    @Override
    public void onLinkFailure(DroneLink source, String message) {
        if (source != droneLink) return;
        Log.w(TAG, message);
        stopControlLink();
        if (resumed && !otaInProgress) {
            handler.removeCallbacks(retryControlLink);
            handler.postDelayed(retryControlLink, WIFI_ROUTE_RETRY_MS);
        }
    }

    private void startControlLink() {
        if (droneLink != null) return;
        Network aircraftNetwork = findAircraftWifiNetwork();
        if (aircraftNetwork == null) {
            scheduleControlLinkRetry();
            return;
        }
        handler.removeCallbacks(retryControlLink);
        controlNetwork = aircraftNetwork;
        droneLink = new DroneLink(aircraftNetwork, aircraftHost, this);
        droneLink.start();
        startCameraStream(aircraftNetwork);
    }

    private void refreshControlLink() {
        if (!resumed || otaInProgress) return;
        Network aircraftNetwork = findAircraftWifiNetwork();
        if (aircraftNetwork == null) {
            scheduleControlLinkRetry();
            return;
        }
        if (droneLink != null && aircraftNetwork.equals(controlNetwork)) return;
        stopControlLink();
        startControlLink();
    }

    private void scheduleControlLinkRetry() {
        if (!resumed || otaInProgress) return;
        handler.removeCallbacks(retryControlLink);
        handler.postDelayed(retryControlLink, WIFI_ROUTE_RETRY_MS);
    }

    private void stopControlLink() {
        stopCameraStream();
        DroneLink link = droneLink;
        droneLink = null;
        controlNetwork = null;
        latestState = null;
        if (link != null) link.stop();
        setControlsEnabled(null);
    }

    private void startCameraStream(Network aircraftNetwork) {
        stopCameraStream();
        final int generation = ++cameraGeneration;
        MjpegStreamClient stream = new MjpegStreamClient(
                aircraftNetwork, AircraftEndpoint.cameraUrl(aircraftHost),
                new MjpegStreamClient.Listener() {
                    @Override
                    public void onState(MjpegStreamClient.State state, String detail) {
                        handler.post(() -> {
                            if (!cameraSessionCurrent(generation)) return;
                            cameraStreamState = state;
                            if (state == MjpegStreamClient.State.CONNECTING) {
                                cameraStatsText.setText(R.string.camera_connecting);
                            } else if (state == MjpegStreamClient.State.RETRYING) {
                                cameraStatsText.setText(detail == null || detail.isEmpty()
                                        ? getString(R.string.camera_retrying)
                                        : getString(R.string.camera_retrying_detail, detail));
                            } else if (!CameraFramePolicy.shouldShowLiveVideo(
                                    lastCameraFrameElapsedMs, SystemClock.elapsedRealtime(),
                                    CAMERA_FRAME_STALE_MS)) {
                                cameraStatsText.setText(R.string.camera_waiting_frame);
                            }
                        });
                    }

                    @Override
                    public void onFrame(Bitmap bitmap) {
                        if (!cameraSessionCurrent(generation)) {
                            bitmap.recycle();
                            return;
                        }
                        PendingCameraFrame next = new PendingCameraFrame(generation, bitmap);
                        PendingCameraFrame replaced = pendingCameraFrame.getAndSet(next);
                        if (replaced != null) replaced.bitmap.recycle();
                        if (cameraFramePosted.compareAndSet(false, true)) {
                            handler.post(deliverLatestCameraFrame);
                        }
                    }

                    @Override
                    public void onStats(float sourceFps, float averageFrameKilobytes) {
                        handler.post(() -> {
                            if (!cameraSessionCurrent(generation)) return;
                            cameraStatsText.setText(getString(
                                    R.string.camera_stats, sourceFps, averageFrameKilobytes));
                        });
                    }
                });
        cameraStream = stream;
        cameraStreamState = MjpegStreamClient.State.CONNECTING;
        lastCameraFrameElapsedMs = 0;
        cameraStatsText.setText(R.string.camera_connecting);
        handler.removeCallbacks(cameraWatchdog);
        handler.postDelayed(cameraWatchdog, CAMERA_WATCHDOG_PERIOD_MS);
        stream.start();
    }

    private boolean cameraSessionCurrent(int generation) {
        return cameraStream != null && generation == cameraGeneration;
    }

    private void stopCameraStream() {
        cameraGeneration++;
        MjpegStreamClient stream = cameraStream;
        cameraStream = null;
        if (stream != null) stream.stop();
        handler.removeCallbacks(cameraWatchdog);
        handler.removeCallbacks(deliverLatestCameraFrame);
        cameraFramePosted.set(false);
        PendingCameraFrame pending = pendingCameraFrame.getAndSet(null);
        if (pending != null) pending.bitmap.recycle();
        cameraStreamState = null;
        lastCameraFrameElapsedMs = 0;
        hideCameraFrame();
        if (cameraStatsText != null) cameraStatsText.setText(R.string.camera_waiting_frame);
    }

    private void hideCameraFrame() {
        if (cameraPreview == null) return;
        cameraPreview.setVisibility(View.GONE);
        cameraPreview.setImageDrawable(null);
        Bitmap previous = displayedCameraFrame;
        displayedCameraFrame = null;
        if (previous != null) previous.recycle();
    }

    private Network findAircraftWifiNetwork() {
        ConnectivityManager manager = (ConnectivityManager) getApplicationContext()
                .getSystemService(Context.CONNECTIVITY_SERVICE);
        if (manager == null) return null;

        final InetAddress aircraftAddress;
        try {
            aircraftAddress = InetAddress.getByName(aircraftHost);
        } catch (UnknownHostException error) {
            Log.e(TAG, "Invalid aircraft address", error);
            return null;
        }

        for (Network network : manager.getAllNetworks()) {
            NetworkCapabilities capabilities = manager.getNetworkCapabilities(network);
            if (capabilities == null
                    || !capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                continue;
            }
            LinkProperties properties = manager.getLinkProperties(network);
            if (properties == null) continue;
            for (RouteInfo route : properties.getRoutes()) {
                // A default route matches every IPv4 address and can therefore
                // select an unrelated Wi-Fi. Require a connected subnet route
                // that actually contains the configured aircraft address.
                if (!route.isDefaultRoute() && route.matches(aircraftAddress)) return network;
            }
        }
        return null;
    }

    private void registerAircraftWifiCallback() {
        if (networkCallbackRegistered) return;
        connectivityManager = (ConnectivityManager) getApplicationContext()
                .getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivityManager == null) return;
        NetworkRequest request = new NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build();
        try {
            connectivityManager.registerNetworkCallback(request, aircraftWifiCallback);
            networkCallbackRegistered = true;
        } catch (RuntimeException error) {
            Log.w(TAG, "Wi-Fi network callback unavailable; polling remains active", error);
        }
    }

    private void unregisterAircraftWifiCallback() {
        if (!networkCallbackRegistered || connectivityManager == null) return;
        try {
            connectivityManager.unregisterNetworkCallback(aircraftWifiCallback);
        } catch (RuntimeException error) {
            Log.w(TAG, "Wi-Fi network callback release failed", error);
        }
        networkCallbackRegistered = false;
        connectivityManager = null;
    }

    private void updateManualControl() {
        DroneLink link = droneLink;
        if (link == null) return;
        DroneLink.State state = latestState;
        if (state != null && state.pendingCommand == MavlinkCodec.CMD_NAV_TAKEOFF) {
            // DroneLink has already queued TAKEOFF and is withholding MANUAL_CONTROL.
            // Preserve the primed neutral packet that will be sent after ACK; writing
            // ground-safe z=0 here would recreate the takeoff-abort race.
            link.updateControls(FlightControlMapper.ManualControl.neutral());
            return;
        }
        if (state == null || !state.connected || !state.armed || state.physicalRcActive) {
            if (state != null && state.connected && !state.physicalRcActive) {
                FlightControlPolicy.Decision policy = controlPolicy(state);
                if (policy.manualControlEnabled) {
                    link.updateControls(FlightControlMapper.map(
                            leftX, leftY, rightX, rightY, STICK_EXPO));
                    return;
                }
            }
            link.updateControls(FlightControlMapper.groundSafe());
        } else if (!controlPolicy(state).manualControlEnabled) {
            link.updateControls(FlightControlMapper.ManualControl.neutral());
        } else {
            link.updateControls(FlightControlMapper.map(
                    leftX, leftY, rightX, rightY, STICK_EXPO));
        }
    }

    private void requestTakeoff() {
        DroneLink.State state = latestState;
        if (state == null || !state.connected || state.physicalRcActive) {
            showToast(R.string.takeoff_not_ready);
            return;
        }
        final float height;
        try {
            height = Float.parseFloat(takeoffHeight.getText().toString());
        } catch (NumberFormatException error) {
            showToast(R.string.invalid_height);
            return;
        }
        if (!Float.isFinite(height) || height < 0.20f || height > 5.80f) {
            showToast(R.string.invalid_height);
            return;
        }
        withLink(link -> link.takeoffPosition(height));
    }

    private void updateStickGesture() {
        int gesture = StickGesture.classify(leftX, leftY);
        if (gesture == StickGesture.NONE) {
            resetStickGesture();
            return;
        }
        if (stickGestureLatched || gesture == pendingStickGesture) return;
        handler.removeCallbacks(confirmStickGesture);
        pendingStickGesture = gesture;
        handler.postDelayed(confirmStickGesture, STICK_GESTURE_HOLD_MS);
    }

    private void performStickGesture(int gesture) {
        DroneLink.State state = latestState;
        if (state == null || !state.connected) return;
        boolean flightControlActive = controlPolicy(state).flightControlActive;
        if (gesture == StickGesture.DISARM && flightControlActive) {
            withLink(DroneLink::emergencyDisarm);
            leftJoystick.performHapticFeedback(
                    android.view.HapticFeedbackConstants.LONG_PRESS);
        }
    }

    private void resetStickGesture() {
        handler.removeCallbacks(confirmStickGesture);
        pendingStickGesture = StickGesture.NONE;
        stickGestureLatched = false;
    }

    @SuppressLint("ClickableViewAccessibility")
    private void configureHold(Button button, Runnable action) {
        CharSequence label = button.getText();
        button.setOnClickListener(view -> showToast(R.string.hold_hint));
        Runnable confirm = () -> {
            if (!button.isPressed() || !button.isEnabled()) return;
            action.run();
            button.performHapticFeedback(android.view.HapticFeedbackConstants.LONG_PRESS);
            button.setText(label);
            button.setPressed(false);
        };
        button.setOnTouchListener((view, event) -> {
            if (!button.isEnabled()) return false;
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                button.setPressed(true);
                button.setText("…");
                handler.postDelayed(confirm, ACTION_HOLD_MS);
            } else if (event.getActionMasked() == MotionEvent.ACTION_UP
                    || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                handler.removeCallbacks(confirm);
                if (button.isPressed() && event.getActionMasked() == MotionEvent.ACTION_UP) {
                    view.performClick();
                }
                button.setPressed(false);
                button.setText(label);
            }
            return true;
        });
    }

    private void setControlsEnabled(DroneLink.State state) {
        if (otaInProgress) {
            toolsButton.setEnabled(false);
            takeoffHeight.setEnabled(false);
            disarmButton.setEnabled(false);
            takeoffButton.setEnabled(false);
            landButton.setEnabled(false);
            leftJoystick.setEnabled(false);
            rightJoystick.setEnabled(false);
            leftJoystick.reset();
            rightJoystick.reset();
            return;
        }

        toolsButton.setEnabled(true);
        boolean connected = state != null && state.connected;
        boolean automatic = state != null && state.mode == 3;
        FlightControlPolicy.Decision policy = state == null ? null : controlPolicy(state);
        boolean pending = policy != null && policy.operationPending;
        boolean phoneOwns = state == null || !state.physicalRcActive;
        boolean flightControlActive = policy != null && policy.flightControlActive;
        boolean verticalAutomation = policy != null && policy.verticalAutomation;

        takeoffHeight.setEnabled(connected && !flightControlActive && !automatic
                && !verticalAutomation && !pending);
        takeoffButton.setEnabled(connected && phoneOwns
                && !flightControlActive && !automatic && !verticalAutomation && !pending);
        // LAND is a recoverable safety action and may supersede a pending takeoff;
        // emergency disarm is idempotent. Do not hide either behind stale armed/mode
        // telemetry—the firmware remains authoritative for command acceptance.
        landButton.setEnabled(connected);
        disarmButton.setEnabled(connected);

        boolean leftEnabled = policy != null && policy.leftStickEnabled;
        boolean rightEnabled = policy != null && policy.rightStickEnabled;
        leftJoystick.setEnabled(leftEnabled);
        rightJoystick.setEnabled(rightEnabled);
        if (!leftEnabled) {
            leftJoystick.reset();
            resetStickGesture();
        }
        if (!rightEnabled) rightJoystick.reset();
        leftJoystick.setAlpha(leftEnabled ? 1.0f : 0.45f);
        rightJoystick.setAlpha(rightEnabled ? 1.0f : 0.45f);
    }

    private void updateSafetyHint(DroneLink.State state) {
        int warning = getColor(R.color.warning);
        FlightControlPolicy.Decision policy = controlPolicy(state);
        if (!state.connected) {
            safetyText.setText(state.status == null || state.status.isEmpty()
                    ? getString(R.string.link_waiting_warning) : state.status);
            safetyText.setTextColor(warning);
        } else if (state.physicalRcActive) {
            safetyText.setText(R.string.rc_priority_warning);
            safetyText.setTextColor(warning);
        } else if (state.pendingCommand == MavlinkCodec.CMD_NAV_LAND) {
            safetyText.setText(R.string.automatic_landing_hint);
            safetyText.setTextColor(warning);
        } else if (state.pendingCommand == MavlinkCodec.CMD_NAV_TAKEOFF) {
            safetyText.setText(R.string.automatic_takeoff_hint);
            safetyText.setTextColor(warning);
        } else if (policy.operationPending) {
            safetyText.setText(R.string.operation_pending);
            safetyText.setTextColor(warning);
        } else if (isActionableStatus(state.status)) {
            // Command ACK/denial and FCU STATUSTEXT must not be hidden behind the
            // persistent near-ground ToF advisory. This is the operator's reason
            // when an action is rejected or a failsafe starts.
            safetyText.setText(state.status);
            safetyText.setTextColor(warning);
        } else if (state.automaticCommand == MavlinkCodec.CMD_NAV_LAND) {
            safetyText.setText(R.string.automatic_landing_hint);
            safetyText.setTextColor(warning);
        } else if (state.automaticCommand == MavlinkCodec.CMD_NAV_TAKEOFF) {
            safetyText.setText(R.string.automatic_takeoff_hint);
            safetyText.setTextColor(warning);
        } else if (!state.heightSensorHealthy) {
            safetyText.setText(R.string.height_sensor_warning);
            safetyText.setTextColor(warning);
        } else if (state.mode == 3) {
            safetyText.setText(R.string.automatic_control_locked_hint);
            safetyText.setTextColor(warning);
        } else {
            safetyText.setText(R.string.safety_hint);
            safetyText.setTextColor(getColor(R.color.text_secondary));
        }
    }

    private static boolean isActionableStatus(String status) {
        if (status == null || status.isEmpty()) return false;
        String normalized = status.toLowerCase(Locale.US);
        return normalized.contains("denied")
                || normalized.contains("rejected")
                || normalized.contains("failed")
                || normalized.contains("timeout")
                || normalized.contains("offline")
                || normalized.contains("unavailable")
                || normalized.contains("failsafe");
    }

    private static FlightControlPolicy.Decision controlPolicy(DroneLink.State state) {
        return FlightControlPolicy.evaluate(state.connected, state.armed,
                state.physicalRcActive, state.mode, state.pendingCommand,
                state.automaticCommand);
    }

    private void showToolsMenu(View anchor) {
        PopupMenu menu = new PopupMenu(this, anchor);
        menu.getMenu().add(0, 1, 0, R.string.guide);
        menu.getMenu().add(0, 3, 1, R.string.aircraft_address)
                .setEnabled(latestState == null || !latestState.armed);
        menu.getMenu().add(0, 2, 2, R.string.firmware_update)
                .setEnabled(!otaInProgress && latestState != null && latestState.connected
                        && !latestState.armed && latestState.landed);
        menu.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == 1) {
                showGuide();
                return true;
            }
            if (item.getItemId() == 2) {
                showFirmwareUpdateDialog();
                return true;
            }
            if (item.getItemId() == 3) {
                showAircraftAddressDialog();
                return true;
            }
            return false;
        });
        menu.show();
    }

    private void showAircraftAddressDialog() {
        if (latestState != null && latestState.armed) {
            showToast(R.string.aircraft_address_ground_only);
            return;
        }
        EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setSelectAllOnFocus(true);
        input.setText(aircraftHost);
        int padding = (int) (20 * getResources().getDisplayMetrics().density);
        LinearLayout container = new LinearLayout(this);
        container.setPadding(padding, 0, padding, 0);
        container.addView(input, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        new AlertDialog.Builder(this)
                .setTitle(R.string.aircraft_address)
                .setMessage(R.string.aircraft_address_help)
                .setView(container)
                .setPositiveButton(R.string.save, (dialog, which) -> {
                    try {
                        String nextHost = AircraftEndpoint.normalizeIpv4(input.getText().toString());
                        if (nextHost.equals(aircraftHost)) return;
                        aircraftHost = nextHost;
                        preferences.edit().putString(PREF_AIRCRAFT_HOST, aircraftHost).apply();
                        stopControlLink();
                        if (resumed && !otaInProgress) startControlLink();
                    } catch (IllegalArgumentException error) {
                        Toast.makeText(this, error.getMessage(), Toast.LENGTH_LONG).show();
                    }
                })
                .setNeutralButton(R.string.use_direct_ap_address, (dialog, which) -> {
                    aircraftHost = AircraftEndpoint.DEFAULT_HOST;
                    preferences.edit().remove(PREF_AIRCRAFT_HOST).apply();
                    stopControlLink();
                    if (resumed && !otaInProgress) startControlLink();
                })
                .setNegativeButton(R.string.close, null)
                .show();
    }

    private void showGuide() {
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.guide_title) + " · " + APP_BUILD_ID)
                .setMessage(R.string.guide_body)
                .setPositiveButton(R.string.understood, (dialog, which) ->
                        preferences.edit().putBoolean(PREF_GUIDE_SHOWN, true).apply())
                .show();
    }

    private void showFirmwareUpdateDialog() {
        DroneLink.State state = latestState;
        if (state == null || !state.connected || state.armed || !state.landed) {
            showToast(R.string.ota_requires_safe_ground_state);
            return;
        }
        new AlertDialog.Builder(this)
                .setTitle(R.string.firmware_update)
                .setMessage(R.string.ota_select_message)
                .setPositiveButton(R.string.ota_select_file, (dialog, which) -> {
                    otaFileSelectionPending = true;
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("application/octet-stream");
                    startActivityForResult(intent, OTA_FILE_REQUEST);
                })
                .setNegativeButton(R.string.close, null)
                .show();
    }

    private void startFirmwareUpdate(Uri imageUri) {
        if (otaInProgress) return;
        DroneLink.State state = latestState;
        if (state == null || state.armed || !state.landed) {
            showToast(R.string.ota_requires_safe_ground_state);
            return;
        }
        otaInProgress = true;
        stopCameraStream();
        if (droneLink != null) {
            droneLink.stop();
            droneLink = null;
        }
        setControlsEnabled(null);

        otaProgressText = new TextView(this);
        int padding = (int) (20 * getResources().getDisplayMetrics().density);
        otaProgressText.setPadding(padding, padding, padding, padding);
        otaProgressText.setText(R.string.ota_checking);
        otaProgressDialog = new AlertDialog.Builder(this)
                .setTitle(R.string.firmware_update)
                .setView(otaProgressText)
                .setCancelable(false)
                .create();
        otaProgressDialog.show();

        FirmwareUpdater.start(getContentResolver(), imageUri, controlNetwork, aircraftHost,
                new FirmwareUpdater.Listener() {
                    @Override
                    public void onProgress(String message) {
                        if (otaProgressText != null) otaProgressText.setText(message);
                    }

                    @Override
                    public void onComplete(boolean success, String message) {
                        finishFirmwareUpdate(success, message);
                    }
                });
    }

    private void finishFirmwareUpdate(boolean success, String message) {
        if (otaProgressDialog != null) otaProgressDialog.dismiss();
        otaProgressDialog = null;
        otaProgressText = null;
        otaInProgress = false;
        new AlertDialog.Builder(this)
                .setTitle(success ? R.string.ota_success_title : R.string.ota_failure_title)
                .setMessage(message)
                .setPositiveButton(R.string.close, null)
                .show();
        if (resumed) {
            latestState = null;
            startControlLink();
        } else {
            releaseWifiLock();
        }
        setControlsEnabled(latestState);
    }

    @SuppressLint("WakelockTimeout")
    private void acquireWifiLock() {
        WifiManager manager = (WifiManager) getApplicationContext()
                .getSystemService(Context.WIFI_SERVICE);
        if (manager == null) return;
        try {
            wifiLock = manager.createWifiLock(
                    WifiManager.WIFI_MODE_FULL_HIGH_PERF, "Open32Drone:control-link");
            wifiLock.setReferenceCounted(false);
            wifiLock.acquire();
        } catch (RuntimeException error) {
            Log.w(TAG, "Wi-Fi performance lock unavailable", error);
            wifiLock = null;
        }
        try {
            multicastLock = manager.createMulticastLock("Open32Drone:mavlink-broadcast");
            multicastLock.setReferenceCounted(false);
            multicastLock.acquire();
        } catch (RuntimeException error) {
            Log.w(TAG, "Wi-Fi broadcast lock unavailable", error);
            multicastLock = null;
        }
    }

    private void releaseWifiLock() {
        try {
            if (multicastLock != null && multicastLock.isHeld()) multicastLock.release();
        } catch (RuntimeException error) {
            Log.w(TAG, "Wi-Fi broadcast lock release failed", error);
        }
        multicastLock = null;
        try {
            if (wifiLock != null && wifiLock.isHeld()) wifiLock.release();
        } catch (RuntimeException error) {
            Log.w(TAG, "Wi-Fi performance lock release failed", error);
        }
        wifiLock = null;
    }

    @SuppressWarnings("deprecation")
    private void enterImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller =
                    getWindow().getDecorView().getWindowInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars()
                        | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
            return;
        }
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    private void withLink(LinkAction action) {
        if (droneLink != null) action.run(droneLink);
    }

    private void showToast(int message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }

    private String displayModeName(int mode) {
        switch (mode) {
            case DroneLink.MODE_STABILIZE: return getString(R.string.mode_stab);
            case DroneLink.MODE_ALTITUDE_HOLD: return getString(R.string.mode_alt);
            case DroneLink.MODE_POSITION_HOLD: return getString(R.string.mode_pos);
            case 3: return getString(R.string.mode_auto);
            default: return getString(R.string.mode_unknown);
        }
    }

    private String phaseName(DroneLink.Phase phase) {
        switch (phase) {
            case CHECKING: return getString(R.string.phase_checking);
            case NOT_READY: return getString(R.string.phase_not_ready);
            case READY: return getString(R.string.phase_ready);
            case ARMED: return getString(R.string.phase_armed);
            default: return getString(R.string.waiting);
        }
    }

    private interface LinkAction {
        void run(DroneLink link);
    }

    private static final class PendingCameraFrame {
        final int generation;
        final Bitmap bitmap;

        PendingCameraFrame(int generation, Bitmap bitmap) {
            this.generation = generation;
            this.bitmap = bitmap;
        }
    }
}
