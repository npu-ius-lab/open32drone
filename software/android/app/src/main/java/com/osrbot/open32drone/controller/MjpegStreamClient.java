package com.osrbot.open32drone.controller;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Network;
import android.os.Process;

import java.io.BufferedInputStream;
import java.io.EOFException;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.atomic.AtomicBoolean;

final class MjpegStreamClient {
    enum State { CONNECTING, STREAMING, RETRYING }

    interface Listener {
        void onState(State state, String detail);
        void onFrame(Bitmap bitmap);
        void onStats(float sourceFps, float averageFrameKilobytes);
    }

    private static final int CONNECT_TIMEOUT_MS = 2000;
    private static final int READ_TIMEOUT_MS = 4000;
    private static final int RETRY_DELAY_MS = 1000;
    private static final int MAX_FRAME_BYTES = 512 * 1024;
    private static final long MIN_DECODE_INTERVAL_MS = 100;

    private final Network network;
    private final String streamUrl;
    private final Listener listener;
    private final AtomicBoolean running = new AtomicBoolean();
    private volatile HttpURLConnection connection;
    private Thread worker;

    MjpegStreamClient(Network network, String streamUrl, Listener listener) {
        this.network = network;
        this.streamUrl = streamUrl;
        this.listener = listener;
    }

    synchronized void start() {
        if (!running.compareAndSet(false, true)) return;
        worker = new Thread(this::run, "open32drone-mjpeg");
        worker.start();
    }

    synchronized void stop() {
        running.set(false);
        HttpURLConnection active = connection;
        if (active != null) active.disconnect();
        if (worker != null) worker.interrupt();
        worker = null;
    }

    private void run() {
        // Video is observational. Keep JPEG parsing/decoding below the control
        // transmitter and UI threads so a busy frame cannot increase stick or
        // command latency on a low-end phone.
        Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
        while (running.get()) {
            listener.onState(State.CONNECTING, null);
            try {
                streamOnce();
            } catch (IOException error) {
                if (!running.get()) break;
                listener.onState(State.RETRYING, error.getMessage());
                try {
                    Thread.sleep(RETRY_DELAY_MS);
                } catch (InterruptedException interrupted) {
                    Thread.currentThread().interrupt();
                    break;
                }
            } finally {
                HttpURLConnection active = connection;
                connection = null;
                if (active != null) active.disconnect();
            }
        }
    }

    private void streamOnce() throws IOException {
        HttpURLConnection active = (HttpURLConnection) network.openConnection(new URL(streamUrl));
        connection = active;
        active.setConnectTimeout(CONNECT_TIMEOUT_MS);
        active.setReadTimeout(READ_TIMEOUT_MS);
        active.setUseCaches(false);
        active.setRequestProperty("Accept", "multipart/x-mixed-replace");
        int response = active.getResponseCode();
        if (response != HttpURLConnection.HTTP_OK) {
            throw new IOException("MJPEG HTTP " + response);
        }

        listener.onState(State.STREAMING, null);
        long lastDecodeMs = 0;
        long statsStartMs = System.currentTimeMillis();
        int sourceFrames = 0;
        long sourceBytes = 0;
        try (BufferedInputStream input =
                     new BufferedInputStream(active.getInputStream(), 32 * 1024)) {
            while (running.get()) {
                byte[] jpeg = MjpegFrameReader.readFrame(input, MAX_FRAME_BYTES);
                if (jpeg == null) throw new EOFException("MJPEG stream ended");
                sourceFrames++;
                sourceBytes += jpeg.length;

                long now = System.currentTimeMillis();
                if (now - lastDecodeMs >= MIN_DECODE_INTERVAL_MS) {
                    BitmapFactory.Options options = new BitmapFactory.Options();
                    options.inPreferredConfig = Bitmap.Config.RGB_565;
                    Bitmap bitmap = BitmapFactory.decodeByteArray(jpeg, 0, jpeg.length, options);
                    if (bitmap != null) listener.onFrame(bitmap);
                    lastDecodeMs = now;
                }

                long statsElapsed = now - statsStartMs;
                if (statsElapsed >= 1000 && sourceFrames > 0) {
                    float fps = sourceFrames * 1000.0f / statsElapsed;
                    float averageKb = sourceBytes / (1024.0f * sourceFrames);
                    listener.onStats(fps, averageKb);
                    statsStartMs = now;
                    sourceFrames = 0;
                    sourceBytes = 0;
                }
            }
        }
    }
}
