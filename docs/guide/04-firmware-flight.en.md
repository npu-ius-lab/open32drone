# 03 · Firmware and first flight

After assembly, remove the propellers, flash the firmware and complete the preflight checks before flying with an Android phone. These steps do not require a transmitter, ROS or a development environment.

## 3.1 Download the firmware and APK {#downloads}

Open [GitHub Releases](https://github.com/npu-ius-lab/open32drone/releases) and download the **full firmware full.bin** and **Android APK** from the same release. Save the firmware on your computer and the APK on your phone.

| File | Purpose |
|---|---|
| `Open32Drone-20260928-190250-full.bin` | First installation: flash over USB at address `0x0` |
| `Open32Drone-20260928-190250-android.apk` | Install on Android 8.0 or later |
| `Open32Drone-20260928-190250-app.bin` | Later OTA updates through the APK; not needed for the first flash |

Verify the downloads against `SHA256SUMS` from the same release. Check that the firmware matches your controller and IMU before flashing.

## 3.2 Flash the firmware {#flashing}

Open the [Espressif web flasher](https://espressif.github.io/esptool-js/) in **Chrome or Edge** on a computer. It works on Windows, macOS and Linux; Safari and Firefox do not support this method. The web tool does not require Python, Arduino IDE or ESP-IDF.

### Connect the aircraft

1. Remove the propellers and connect the XIAO to your computer with a USB-C **data cable**.
2. Hold **BOOT**, press **RESET** once, then release **BOOT** to enter download mode.
3. In the **Program** section, set Baudrate to `460800`, click **Connect** and select the XIAO serial port. Leave `WebUSB (CH340)` unchecked for a standard XIAO.

![Espressif web flasher: select the baud rate and connect a serial port in the Program section](/media/figures/web-flasher.png)

The screenshot shows the tool before connection. File and flash-address settings appear after it connects.

### Select the file and flash

Use the settings below. For a first installation, click **Erase Flash** and wait for it to finish. This removes existing calibration, parameters and Wi-Fi settings.

| Web setting | Value |
|---|---|
| File | Select `Open32Drone-20260928-190250-full.bin` |
| Flash Address | **`0x0`**; do not leave the initial `0x1000` value |
| Flash Mode / Frequency / Size | Select `keep` to preserve the image settings |

Click **Program** and wait for writing and verification to succeed. Click **Disconnect** to release the serial port, then press RESET to boot normally. Do not proceed to flight if flashing fails. If the web tool is unavailable, use the [optional command-line method](../reference/source-build.md#usb-flash).

| Problem | First steps |
|---|---|
| No serial port listed | Try a known data cable, connect directly to the computer and enter download mode again |
| Serial port busy | Close other serial programs or browser tabs using the port |
| Stuck on Connecting or interrupted write | Enter download mode again; retry at `115200` baud |
| Permission denied on Linux | Configure serial access for your distribution; on Ubuntu, this usually means joining `dialout` and logging in again |

## 3.3 Preflight preparation {#preflight}

### Open a serial terminal

[CoolTerm](https://freeware.the-meiers.org/#CoolTerm) is recommended. Download the version for your operating system. Keep USB connected, disconnect the web flasher from the port and set these values in CoolTerm **Options**:

| Setting | Value |
|---|---|
| Port | The XIAO port after normal boot; often `COM…` on Windows or `usbmodem…` on macOS |
| Baudrate | `115200` |
| Data Bits / Parity / Stop Bits | `8 / None / 1` |
| Flow Control | Disabled |
| Terminal Mode | `Line Mode` |
| Enter Key Emulation | `LF` |

Click **Connect**, type commands in the bottom input field and press Enter. Press RESET once, leave the aircraft level and still, and wait for `Gyro calibration complete` and `Initializing complete`. See [CoolTerm Help](https://freeware.the-meiers.org/CoolTermHelp/) for terminal settings.

Flashing and calibration both use the **USB serial port**. Your computer does not need to join the aircraft hotspot. Connect the phone to it in the next section.

### Check the sensors

Run one command at a time and inspect the result:

| Command | What to check |
|---|---|
| `sys` | Firmware identifies as Open32Drone; main loop is around 300 Hz |
| `imu` | Sensor status is healthy; `gyro calibrated: 1` after remaining still |
| `flow` | Packets keep updating; lift the aircraft steadily to 20–60 cm and check that range changes with height |
| `pw` | Battery voltage is available; calibrate it against a multimeter below |

Keep the propellers removed. The ToF sensor may be in its blind zone close to the ground. For takeoff, place the aircraft flat on the ground; do not arm it in your hand.

### Six-face accelerometer calibration

Run `ca` after the first assembly, an IMU replacement or a full erase. Follow the serial prompts in this order:

1. Level.
2. Nose up.
3. Nose down.
4. Right side down.
5. Left side down.
6. Upside down.

Release the aircraft after positioning each face and let it rest still on a rigid surface during sampling. After `Accelerometer calibration accepted`, return it to level, wait for gyro calibration to finish again and run `imu`. Acceleration magnitude at rest should be close to `9.81 m/s²`.

### Battery-voltage calibration

Keep the propellers removed and connect the battery. Measure the battery terminals with a multimeter as `V_DMM`, then run `pw` and record the firmware reading as `V_FW`. Read the current scale with `p PWR_VOLT_SCALE` and calculate:

```text
new scale = old scale × V_DMM ÷ V_FW
```

Write the value, wait one second and check with `pw`:

```text
p PWR_VOLT_SCALE YOUR_NEW_VALUE
```

For example, with an old scale of 2.000, a multimeter reading of 4.10 V and a firmware reading of 4.00 V, the new scale is `2.000 × 4.10 ÷ 4.00 = 2.050`.

### Check the motors

Keep the aircraft disarmed with propellers removed. Send one command at a time:

| Command | Motor that should turn (viewed from above) |
|---|---|
| `mrl` | Rear left M0 |
| `mrr` | Rear right M1 |
| `mfr` | Front right M2 |
| `mfl` | Front left M3 |

Only the selected motor should turn at low speed for about 1 second. If its position or direction is wrong, check the [motor diagram](03-hardware.en.md#motor-layout). After checking all four motors, disconnect power and fit the matching propellers as described in [Propellers](03-hardware.en.md#propellers).

Calibration is stored on the aircraft and survives a normal restart. Repeat it after a full erase. Phone-only control does not require transmitter calibration with `cr`. See [Parameter storage and reset](../reference/firmware.md#nvs-behavior) for details.

## 3.4 First flight with an Android phone {#android-first-flight}

### Install and connect

1. Install the APK downloaded above, allowing installation from that source when Android prompts you.
2. Connect the aircraft battery and place it on evenly lit, textured ground with at least 2 m of clear space around it. Unplug USB before takeoff.
3. Connect the phone to Wi-Fi **`open32drone`**, password **`12345678`**. Choose to stay connected if Android reports no internet access.
4. Open the Open32Drone app. If the address was changed, set **Tools → Aircraft address** to **`192.168.4.1`**. Wait for the connection indicator at the top left to update; joining Wi-Fi does not by itself mean the app is connected to the flight controller.

Use the aircraft hotspot for the first flight; router setup can wait. Close ROS and other MAVLink clients, and keep the APK in the foreground.

### Controls

| Location / button | Purpose |
|---|---|
| Connection and mode at the top left | Check that telemetry is arriving; the mode reflects the aircraft's current state |
| Height field at the bottom center | Use `0.65` m for the first flight |
| **Hold for Position takeoff** | Runs preflight checks, arms, climbs and enters position hold |
| **Hold to land** | Descends automatically and stops the motors after touchdown |
| **Emergency stop** | Stops the motors immediately; not a normal landing button |
| Left stick | Climb/descent and yaw |
| Right stick | Forward/backward and left/right movement |

After an emergency-disarm request, the status bar first reports that the request was sent. A command acknowledgement must still be followed by aircraft feedback confirming disarm. An “unconfirmed” result does not mean the propellers have stopped. Closing the app does not send another stick command; the flight controller handles the interrupted control stream.

These correspond to the buttons in the current APK. Position-hold takeoff enters position hold automatically; there is no need to arm or change modes separately.

### Take off, hover and land

1. Enter `0.65` and hold **Hold for Position takeoff** for about 0.6 seconds.
2. Release the sticks and observe about 5 seconds of hovering. Once stable, try small forward, backward and sideways movements.
3. Hold **Hold to land**, then wait for touchdown and all four motors to stop before disconnecting power.

Land promptly if the aircraft tilts noticeably, shakes continuously or drifts quickly. If it is about to hit someone, become entangled or cannot be controlled normally, hold **Emergency stop**. This stops the motors directly and an airborne aircraft will fall.

If the buttons are greyed out, read the message at the top and check the connection, aircraft address and whether another client has control. Do not repeatedly press takeoff. See [Tuning and diagnosis](05-tuning.en.md) for troubleshooting.

## 3.5 Optional: use a transmitter {#sbus}

A phone is enough for the first flight. Buy a matching SBUS receiver and transmitter only if you want physical sticks.

Connect the receiver as described in [Assembly](03-hardware.en.md#xiao-and-receiver). Close the APK and ROS and remove the propellers before calibration.

### Connect and calibrate

With a receiver installed, switch on the transmitter and run `cr`. Complete the eight stick and switch actions prompted over serial, then check with `rc`:

| Action | Expected reading |
|---|---|
| Roll, pitch and yaw centered | Near 0 |
| Throttle minimum / maximum | Near 0 / 1 |
| Three-position mode switch | Near 0 / 0.5 / 1 |

Aircraft controlled only through Android or ROS do not need `cr`.

### Takeoff and landing with the transmitter

The three-position switch selects these modes:

| Switch position | Mode | Behavior |
|---|---|---|
| Low | STAB | Throttle directly controls thrust; suited to experienced pilots |
| Middle | ALT_HOLD | Center the throttle to hold height |
| High | POS_HOLD | Optical flow holds horizontal position; recommended for the first flight |

Select the high, position-hold setting. Minimum throttle and full-right yaw arm the aircraft; motors idle at about 10%. Hold throttle above 62.5% for about 0.2 seconds to start assisted takeoff to the default 0.60 m height. After takeoff, center the throttle and make small horizontal corrections.

To land, hold throttle below 5% for about 0.3 seconds. The aircraft descends automatically and stops the motors after touchdown. Raise throttle above 60% to cancel descent.

For emergency stop, hold minimum throttle and full-left yaw for at least 150 ms. This immediately stops the motors, so an airborne aircraft will fall. Use it only when a collision, entanglement or loss of attitude control is imminent.

## 3.6 Optional: connect to a router {#router}

Once familiar with the direct phone connection, use router STA mode if you need internet access on the computer or want to join a lab network. Connect the aircraft, phone and ROS 2 computer to the same LAN so you do not have to switch between the aircraft hotspot and the lab network. Initial setup still uses USB serial; keep the aircraft disarmed with propellers removed.

Use a 2.4 GHz Wi-Fi network the aircraft can reach. The SSID must contain 1–32 characters and the password 8–63 characters. Open the serial port at 115200 baud and check the current state:

```text
wifi
```

The default full image reports AP mode and address `192.168.4.1`. Replace the example name and password below with the router's credentials:

```text
sta LAB_SSID LAB_PASSWORD
reboot
```

`sta` saves the credentials and selects STA for the next boot. The running network does not switch immediately; `reboot` applies the change. After rebooting, run this over USB serial:

```text
wifi
```

A successful connection shows these key fields:

```text
Configured mode: STA (2)
Mode: Client (STA)
Connected: 1
SSID: LAB_SSID
IP: <aircraft address assigned by the router>
MAVLink UDP: bound 1 local 14550
```

The router assigns `IP` through DHCP; use the address actually printed by the aircraft. Join the same router with your phone or ROS 2 computer, then test reachability:

```bash
printf 'Enter the aircraft IP shown by wifi: '
read -r AIRCRAFT_IP
ping -c 3 "$AIRCRAFT_IP"
```

Set a DHCP reservation in the router using the aircraft's Wi-Fi MAC address so it receives the same IP at each boot. Disable guest-network or client isolation that blocks communication between LAN devices. Store real SSIDs and passwords only on the aircraft, not in source code, tutorials or flight logs.

In Android, open **Tools → Aircraft address** and enter the `IP` from `wifi`. For programmatic control, follow the [ROS 2 tutorial](06-ros.en.md); use either Android or ROS 2 as the control client at any one time.

If the aircraft cannot join the router within 8 seconds of startup, it opens its saved hotspot for recovery. `wifi` reports `Mode: Access Point (AP) - STA fallback`. Correct the credentials with `sta ...` and `reboot`. To return permanently to direct AP mode, run:

```text
ap open32drone 12345678
reboot
```

Once comfortable with basic flight, continue to [ROS 2 control](06-ros.en.md) to fly through your own programs.
