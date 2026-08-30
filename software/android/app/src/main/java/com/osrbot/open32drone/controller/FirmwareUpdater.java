package com.osrbot.open32drone.controller;

import android.content.ContentResolver;
import android.net.Network;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Locale;

final class FirmwareUpdater {
    private static final int CONNECT_TIMEOUT_MS = 3000;
    private static final int UPLOAD_TIMEOUT_MS = 20000;

    interface Listener {
        void onProgress(String message);
        void onComplete(boolean success, String message);
    }

    private FirmwareUpdater() {
    }

    static void start(ContentResolver resolver, Uri imageUri, Network aircraftNetwork,
                      String aircraftHost, Listener listener) {
        Handler mainHandler = new Handler(Looper.getMainLooper());
        Thread worker = new Thread(() -> {
            try {
                runUpdate(resolver, imageUri, aircraftNetwork, aircraftHost,
                        message -> mainHandler.post(() -> listener.onProgress(message)));
                mainHandler.post(() -> listener.onComplete(true,
                        "Firmware accepted and validated in the new A/B slot"));
            } catch (Exception error) {
                String detail = error.getMessage() == null
                        ? error.getClass().getSimpleName() : error.getMessage();
                mainHandler.post(() -> listener.onComplete(false, detail));
            }
        }, "ota-update");
        worker.setPriority(Thread.NORM_PRIORITY);
        worker.start();
    }

    private static void runUpdate(ContentResolver resolver, Uri imageUri,
                                  Network aircraftNetwork, String aircraftHost,
                                  Progress progress) throws Exception {
        if (aircraftNetwork == null) throw new IOException("Aircraft Wi-Fi route unavailable");
        progress.send("Checking firmware image …");
        ImageInfo image = inspectImage(resolver, imageUri);
        JSONObject status = requestStatus(aircraftNetwork, aircraftHost, CONNECT_TIMEOUT_MS);
        if (!status.optBoolean("ready")) {
            throw new IOException("Aircraft is not OTA-ready: " + status.optString("reason", "unknown"));
        }
        if (status.optInt("partition_count", 0) < 2) {
            throw new IOException("A/B partition table is missing; perform the one-time USB migration");
        }
        long maximum = status.optLong("max_image_bytes", 0);
        if (maximum > 0 && image.size > maximum) {
            throw new IOException(String.format(Locale.US,
                    "Image is %,d bytes; inactive slot accepts %,d", image.size, maximum));
        }

        if (status.optBoolean("auth_required", true)) {
            throw new IOException("Install the private development firmware once over USB");
        }

        String nextSlot = status.optString("next_slot", "");
        progress.send("Uploading to " + nextSlot + " · 0%");
        uploadImage(resolver, imageUri, image, aircraftNetwork, aircraftHost,
                nextSlot, progress);
        waitForValidatedBoot(aircraftNetwork, aircraftHost, nextSlot, progress);
    }

    private static ImageInfo inspectImage(ContentResolver resolver, Uri uri)
            throws IOException, NoSuchAlgorithmException {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        long size = 0;
        boolean first = true;
        try (InputStream input = requireInput(resolver, uri)) {
            byte[] buffer = new byte[16 * 1024];
            int count;
            while ((count = input.read(buffer)) >= 0) {
                if (count == 0) continue;
                if (first) {
                    if ((buffer[0] & 0xff) != 0xe9) {
                        throw new IOException("Selected file is not an ESP application image");
                    }
                    first = false;
                }
                digest.update(buffer, 0, count);
                size += count;
            }
        }
        if (first || size < 24) throw new IOException("Firmware image is empty or too small");
        return new ImageInfo(size, hex(digest.digest()));
    }

    private static void uploadImage(ContentResolver resolver, Uri uri,
                                    ImageInfo image, Network aircraftNetwork, String aircraftHost,
                                    String nextSlot, Progress progress)
            throws Exception {
        HttpURLConnection connection = open(aircraftNetwork,
                AircraftEndpoint.otaUpdateUrl(aircraftHost), UPLOAD_TIMEOUT_MS);
        long sent = 0;
        int code = -1;
        JSONObject response = null;
        try {
            connection.setRequestMethod("POST");
            connection.setDoOutput(true);
            connection.setRequestProperty("Content-Type", "application/octet-stream");
            connection.setRequestProperty("X-Firmware-SHA256", image.sha256);
            connection.setFixedLengthStreamingMode(image.size);

            int lastPercent = -1;
            try (InputStream input = requireInput(resolver, uri);
                 OutputStream output = connection.getOutputStream()) {
                byte[] buffer = new byte[16 * 1024];
                int count;
                while ((count = input.read(buffer)) >= 0) {
                    if (count == 0) continue;
                    output.write(buffer, 0, count);
                    sent += count;
                    int percent = (int) (sent * 100 / image.size);
                    if (percent != lastPercent) {
                        progress.send("Uploading to " + nextSlot + " · " + percent + "%");
                        lastPercent = percent;
                    }
                }
            }

            code = connection.getResponseCode();
            response = readJson(connection, code);
        } catch (IOException responseError) {
            // A committed image can reboot before the final response reaches
            // Android. Do not retry the flash; the expected boot slot below is
            // the authoritative outcome.
            if (sent != image.size) throw responseError;
            progress.send("Upload response lost; checking the boot slot …");
        } finally {
            connection.disconnect();
        }
        if (response != null &&
                (code != HttpURLConnection.HTTP_OK || !response.optBoolean("ok"))) {
            throw new IOException("OTA rejected: " + response.optString("error", "HTTP " + code));
        }
    }

    private static void waitForValidatedBoot(Network aircraftNetwork, String aircraftHost,
                                             String expectedSlot, Progress progress)
            throws Exception {
        progress.send("Rebooting and checking flight sensors …");
        long deadline = System.currentTimeMillis() + 55000;
        JSONObject lastStatus = null;
        while (System.currentTimeMillis() < deadline) {
            Thread.sleep(1000);
            try {
                JSONObject status = requestStatus(aircraftNetwork, aircraftHost, 2000);
                lastStatus = status;
                if (expectedSlot.equals(status.optString("active_slot"))
                        && !status.optBoolean("pending_verify", true)) {
                    return;
                }
            } catch (IOException ignored) {
                // Rebooting temporarily closes the OTA HTTP server.
            }
        }
        if (lastStatus != null && !expectedSlot.equals(lastStatus.optString("active_slot"))) {
            throw new IOException("New firmware failed health checks and rolled back to "
                    + lastStatus.optString("active_slot", "the previous slot"));
        }
        throw new IOException("Timed out waiting for post-boot health validation");
    }

    private static JSONObject requestStatus(Network aircraftNetwork, String aircraftHost,
                                            int timeoutMs) throws IOException {
        HttpURLConnection connection = open(aircraftNetwork,
                AircraftEndpoint.otaStatusUrl(aircraftHost), timeoutMs);
        try {
            connection.setRequestMethod("GET");
            int code = connection.getResponseCode();
            JSONObject response = readJson(connection, code);
            if (code != HttpURLConnection.HTTP_OK) {
                throw new IOException("OTA status HTTP " + code);
            }
            return response;
        } finally {
            connection.disconnect();
        }
    }

    private static HttpURLConnection open(Network aircraftNetwork, String address,
                                          int timeoutMs) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) aircraftNetwork.openConnection(
                new URL(address));
        connection.setUseCaches(false);
        connection.setConnectTimeout(timeoutMs);
        connection.setReadTimeout(timeoutMs);
        connection.setRequestProperty("Accept", "application/json");
        return connection;
    }

    private static JSONObject readJson(HttpURLConnection connection, int code) throws IOException {
        InputStream stream = code >= 200 && code < 400
                ? connection.getInputStream() : connection.getErrorStream();
        if (stream == null) return new JSONObject();
        try (InputStream input = stream; ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[2048];
            int count;
            while ((count = input.read(buffer)) >= 0) {
                if (count > 0) output.write(buffer, 0, count);
            }
            try {
                return new JSONObject(output.toString(StandardCharsets.UTF_8.name()));
            } catch (Exception error) {
                throw new IOException("Invalid OTA response", error);
            }
        }
    }

    private static InputStream requireInput(ContentResolver resolver, Uri uri) throws IOException {
        InputStream input = resolver.openInputStream(uri);
        if (input == null) throw new IOException("Unable to open the selected firmware image");
        return input;
    }

    private static String hex(byte[] bytes) {
        StringBuilder output = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) output.append(String.format(Locale.US, "%02x", value & 0xff));
        return output.toString();
    }

    private interface Progress {
        void send(String message);
    }

    private static final class ImageInfo {
        final long size;
        final String sha256;

        ImageInfo(long size, String sha256) {
            this.size = size;
            this.sha256 = sha256;
        }
    }
}
