package com.osrbot.open32drone.controller;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketTimeoutException;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;

/** One receive loop owns one socket, even when its caller reconnects. */
final class DatagramReceiver {
    private DatagramReceiver() {}

    static void run(DatagramSocket socket, InetAddress expectedAddress,
                    BooleanSupplier active, Consumer<DatagramPacket> onPacket,
                    Consumer<IOException> onError) {
        byte[] buffer = new byte[4096];
        while (active.getAsBoolean() && !socket.isClosed()) {
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
            try {
                socket.receive(packet);
                if (active.getAsBoolean() && expectedAddress.equals(packet.getAddress())) {
                    onPacket.accept(packet);
                }
            } catch (SocketTimeoutException ignored) {
                // Periodically recheck whether this connection is still active.
            } catch (IOException error) {
                if (active.getAsBoolean() && !socket.isClosed()) onError.accept(error);
                return;
            }
        }
    }
}
