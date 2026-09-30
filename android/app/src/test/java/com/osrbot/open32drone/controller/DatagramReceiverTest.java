package com.osrbot.open32drone.controller;

import org.junit.Test;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import static org.junit.Assert.*;

public class DatagramReceiverTest {
    @Test public void closeDuringReceiveExitsWithoutFailure() throws Exception {
        InetAddress address = InetAddress.getLoopbackAddress();
        try (DatagramSocket socket = new DatagramSocket(0, address)) {
            socket.setSoTimeout(50);
            AtomicReference<Throwable> failure = new AtomicReference<>();
            CountDownLatch entered = new CountDownLatch(1);
            Thread worker = new Thread(() -> {
                entered.countDown();
                try {
                    DatagramReceiver.run(socket, address, () -> true,
                            packet -> fail("unexpected packet"), failure::set);
                } catch (Throwable error) { failure.set(error); }
            });
            worker.start();
            assertTrue(entered.await(1, TimeUnit.SECONDS));
            socket.close();
            worker.join(1000);
            assertFalse(worker.isAlive());
            assertNull(failure.get());
        }
    }

    @Test public void obsoleteConnectionCannotDeliverIntoNewConnection() throws Exception {
        InetAddress address = InetAddress.getLoopbackAddress();
        try (DatagramSocket oldSocket = new DatagramSocket(0, address);
             DatagramSocket newSocket = new DatagramSocket(0, address);
             DatagramSocket sender = new DatagramSocket()) {
            oldSocket.setSoTimeout(50);
            newSocket.setSoTimeout(50);
            AtomicBoolean oldActive = new AtomicBoolean(true);
            AtomicBoolean newActive = new AtomicBoolean(true);
            AtomicReference<Throwable> failure = new AtomicReference<>();
            CountDownLatch oldWaiting = new CountDownLatch(1);
            CountDownLatch received = new CountDownLatch(1);
            Thread oldWorker = new Thread(() -> DatagramReceiver.run(oldSocket, address,
                    () -> { oldWaiting.countDown(); return oldActive.get(); },
                    packet -> failure.set(new AssertionError("old session delivered")), failure::set));
            Thread newWorker = new Thread(() -> DatagramReceiver.run(newSocket, address,
                    newActive::get, packet -> received.countDown(), failure::set));
            try {
                oldWorker.start();
                assertTrue(oldWaiting.await(1, TimeUnit.SECONDS));
                oldActive.set(false);
                newWorker.start();
                byte[] data = {1};
                sender.send(new DatagramPacket(data, 1, address, oldSocket.getLocalPort()));
                sender.send(new DatagramPacket(data, 1, address, newSocket.getLocalPort()));
                assertTrue(received.await(1, TimeUnit.SECONDS));
            } finally {
                oldActive.set(false);
                newActive.set(false);
                oldSocket.close();
                newSocket.close();
                oldWorker.join(1000);
                newWorker.join(1000);
            }
            assertFalse(oldWorker.isAlive());
            assertFalse(newWorker.isAlive());
            assertNull(failure.get());
        }
    }
}
