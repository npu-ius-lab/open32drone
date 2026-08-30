package com.osrbot.open32drone.controller;

/** Validated network endpoint shared by MAVLink, camera, and OTA clients. */
final class AircraftEndpoint {
    static final String DEFAULT_HOST = "192.168.4.1";

    private AircraftEndpoint() {
    }

    static String normalizeIpv4(String value) {
        if (value == null) throw new IllegalArgumentException("Aircraft IP is empty");
        String candidate = value.trim();
        String[] octets = candidate.split("\\.", -1);
        if (octets.length != 4) throw new IllegalArgumentException("Use an IPv4 address");
        StringBuilder normalized = new StringBuilder();
        for (int index = 0; index < octets.length; index++) {
            if (octets[index].isEmpty() || octets[index].length() > 3) {
                throw new IllegalArgumentException("Use an IPv4 address");
            }
            int octet;
            try {
                octet = Integer.parseInt(octets[index]);
            } catch (NumberFormatException error) {
                throw new IllegalArgumentException("Use an IPv4 address", error);
            }
            if (octet < 0 || octet > 255) {
                throw new IllegalArgumentException("IPv4 octets must be 0..255");
            }
            if (index > 0) normalized.append('.');
            normalized.append(octet);
        }
        return normalized.toString();
    }

    static String cameraUrl(String host) {
        return "http://" + normalizeIpv4(host) + "/stream";
    }

    static String otaStatusUrl(String host) {
        return "http://" + normalizeIpv4(host) + ":8080/api/ota/status";
    }

    static String otaUpdateUrl(String host) {
        return "http://" + normalizeIpv4(host) + ":8080/api/ota/update";
    }
}
