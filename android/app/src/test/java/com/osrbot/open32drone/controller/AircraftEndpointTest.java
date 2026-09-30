package com.osrbot.open32drone.controller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import org.junit.Test;

public final class AircraftEndpointTest {
    @Test
    public void normalizesIpv4AndBuildsAllServiceUrls() {
        assertEquals("192.0.2.42", AircraftEndpoint.normalizeIpv4(" 192.0.002.42 "));
        assertEquals("http://192.0.2.42/stream",
                AircraftEndpoint.cameraUrl("192.0.2.42"));
        assertEquals("http://192.0.2.42:8080/api/ota/status",
                AircraftEndpoint.otaStatusUrl("192.0.2.42"));
        assertEquals("http://192.0.2.42:8080/api/ota/update",
                AircraftEndpoint.otaUpdateUrl("192.0.2.42"));
    }

    @Test
    public void rejectsHostnamesAndInvalidIpv4() {
        for (String value : new String[] {"", "open32drone.local", "192.0.2", "192.0.2.256"}) {
            try {
                AircraftEndpoint.normalizeIpv4(value);
                fail("Expected invalid endpoint: " + value);
            } catch (IllegalArgumentException expected) {
                // Expected.
            }
        }
    }
}
