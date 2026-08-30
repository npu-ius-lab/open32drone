package com.osrbot.open32drone.controller;

final class CameraFramePolicy {
    private CameraFramePolicy() {}

    static boolean shouldShowLiveVideo(long lastFrameMs, long nowMs, long staleAfterMs) {
        return lastFrameMs > 0 && nowMs >= lastFrameMs
                && nowMs - lastFrameMs <= staleAfterMs;
    }
}
