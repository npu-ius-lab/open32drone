---
outline: [2, 3]
pageClass: build-guide
---

# 02 · Start building

This chapter covers parts, frame printing, PCB fabrication, soldering and aircraft assembly.

## 2.1 Before you start {#preparation}

Prepare the parts and tools below.

| Item | What you need |
|---|---|
| Aircraft parts | Use the shopping list below; SBUS is optional for phone control |
| Tools | Temperature-controlled soldering iron, solder, flux, fine-tipped tweezers, wick, magnifier, multimeter, screwdriver, scale and a nonconductive mat; a temperature-controlled hotplate if using solder paste |
| Computer and cable | Windows, macOS or Linux computer and a USB-C data cable |
| Phone | Android 8.0 or newer; install the ready-built APK for first flight |
| Flashing and calibration | Chrome or Edge for the web flasher; CoolTerm for serial commands |
| Printing | Use your slicer if printing yourself, or have the supplied model printed for you; CAD software is unnecessary |

Ready-built firmware and APK are provided. Arduino IDE, ESP-IDF, Android Studio, ROS and simulator environments are not prerequisites. Install development tools only when reaching the corresponding chapter.

## 2.2 Parts

Frame files are listed under “Frame printing” below. Get the PCB design and onboard electronic BOM from the [JLC project](https://oshwhub.com/fanchewang/open32drone). Use the list below for other modules and mechanical parts; assembly tools are listed in the previous section.

<a id="purchasing"></a>

<a id="shopping-list-for-one-aircraft"></a>

Quantities are **per aircraft**; seller pack sizes may differ. Choose the model using the specification reference when opening a listing. See [mechanical dimensions and wiring](#mechanical-specs) for additional details.

<div class="purchase-table">

| No. | Part | Specification reference | Qty | Links |
| :---: | --- | --- | :---: | --- |
| 1 | Flight-control baseboard | Production files, electronic BOM and placement drawing from the same revision | 1 | [JLC project](https://oshwhub.com/fanchewang/open32drone) |
| 2 | Frame | One complete frame set; approximately 103.3 × 103.3 mm; print at 100% scale | 1 | [MakerWorld](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) |
| 3 | Controller | Seeed Studio XIAO ESP32-S3 Sense (camera included) | 1 | [Listing](https://item.taobao.com/item.htm?id=796226570709) |
| 4 | IMU | MPU9250; pin layout and mounting orientation must match the flight-control baseboard | 1 | [Listing](https://item.taobao.com/item.htm?id=867297908775) |
| 5 | Female header | 1 × 7 pins; pitch and height must match the baseboard and controller | 2 | [SKU](https://item.taobao.com/item.htm?id=1040276180385&skuId=6058024109270) |
| 6 | Jumper cap | 2.54 mm pitch | 1 | [Listing](https://item.taobao.com/item.htm?id=1037786359471) |
| 7 | Boost module | Rated output 5 V / 1 A; input compatible with a 1S battery | 1 | [SKU](https://item.taobao.com/item.htm?id=1020492920926&skuId=6194359034311) |
| 8 | Optical flow/ToF module | CORVON TF-0850; UART version, downward-facing | 1 | [Listing](https://item.taobao.com/item.htm?id=825567548453) |
| 9 | Flow cable | 4-pin reversed-end cable, 60 mm; connector and pinout must match the module | 1 | [Listing](https://item.taobao.com/item.htm?id=561435308484) |
| 10 | 8520 brushed motor | 8 × 20 mm body; 1 mm shaft; MX1.25 connector; wire length ≥ 100 mm | 4 | Not provided |
| 11 | Propeller | 60 mm diameter; two CW and two CCW | 4 | [Listing](https://item.taobao.com/item.htm?id=651317554058) |
| 12 | Motor grommet | Ø8 × 2 mm; two black and two white recommended; [dimensions](#mechanical-specs) | 4 | [Listing](https://detail.tmall.com/item.htm?id=923643961535) |
| 13 | Mounting screw | 1 × 4 × 4 mm | 10 | [Listing](https://item.taobao.com/item.htm?id=658713209127&skuId=4755138613087) |
| 14 | Battery | 1S 18350; JST lead must match the baseboard connector and polarity | 1 | [Listing](https://item.taobao.com/item.htm?id=900687087724) |
| 15 | Battery retaining band | 25 mm diameter, 5 mm wide | 1 | [Listing](https://item.taobao.com/item.htm?id=583635067170) |
| 16 | SBUS receiver | Required for a physical transmitter and must match it | | Optional |

</div>

Purchase onboard electronic components (resistors, capacitors, MOSFETs, diodes and connectors) using the electronic BOM from the matching [JLC design](https://oshwhub.com/fanchewang/open32drone).

### Component photos {#parts-gallery}

The photos help identify parts; follow the specifications and per-aircraft quantities in the table above. Click a photo to view it larger.

<div class="purchase-grid">

<div class="purchase-card">

[![Controller](/media/purchasing/xiao-sense.webp)](/media/purchasing/xiao-sense.webp)

**Controller**

XIAO ESP32-S3 Sense · 1

</div>

<div class="purchase-card">

[![IMU module](/media/purchasing/imu.webp)](/media/purchasing/imu.webp)

**IMU module**

MPU9250 · 1

</div>

<div class="purchase-card">

[![Female headers](/media/purchasing/headers.webp)](/media/purchasing/headers.webp)

**Female headers**

1×7 pins · 2

</div>

<div class="purchase-card">

[![Jumper cap](/media/purchasing/jumper.webp)](/media/purchasing/jumper.webp)

**Jumper cap**

2.54 mm · 1

</div>

<div class="purchase-card">

[![Power module](/media/purchasing/power-module.webp)](/media/purchasing/power-module.webp)

**Power module**

5 V / 1 A · 1

</div>

<div class="purchase-card">

[![Propellers](/media/purchasing/propellers.webp)](/media/purchasing/propellers.webp)

**Propellers**

60 mm · 4

</div>

<div class="purchase-card">

[![Mounting screws](/media/purchasing/screws.webp)](/media/purchasing/screws.webp)

**Mounting screws**

1×4×4 mm · 10

</div>

<div class="purchase-card">

[![Optical flow / ToF](/media/purchasing/flow-tof.webp)](/media/purchasing/flow-tof.webp)

**Optical flow / ToF**

CORVON optical flow and distance · 1

</div>

<div class="purchase-card">

[![Motor grommets](/media/purchasing/grommets.webp)](/media/purchasing/grommets.webp)

**Motor grommets**

Ø8×2 mm · 4, preferably two black and two white

</div>

<div class="purchase-card">

[![Battery](/media/purchasing/battery-clean.png)](/media/purchasing/battery-clean.png)

**Battery**

18350 · 1, with JST lead

</div>

<div class="purchase-card">

[![Flow cable](/media/purchasing/flow-cable.webp)](/media/purchasing/flow-cable.webp)

**Flow cable**

4-pin reversed-end, 60 mm · 1

</div>

<div class="purchase-card">

[![Battery retaining band](/media/purchasing/battery-band.webp)](/media/purchasing/battery-band.webp)

**Battery retaining band**

25 mm diameter × 5 mm wide · 1

</div>

</div>

### Assembly specifications {#mechanical-specs}

- **Motor grommets:** Ø8×2 mm, 10 mm mounting opening, 2 mm groove height, 6 mm total thickness and 15 mm outer diameter; four required. Two black and two white are recommended, all with the same material and hardness.
- **Screws and propellers:** ten 1×4×4 mm mounting screws and four 60 mm propellers: two CW and two CCW.
- **Flow cable:** use a matching 4-pin, 60 mm cable. Connect according to the GND, supply, TX and RX definitions on the module and flight-controller board, with pin details under [Mounting the controller board](#mounting-the-controller-board).
- **Battery connector:** match the JST connector and lead to the board. Check polarity with a multimeter before the first connection.

## 2.3 Frame and PCB

### Frame printing

Bambu users can open the [MakerWorld frame profile](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) in Bambu Studio. The listed profile uses **0.2 mm layers, 6 walls and 25% infill**. Select your printer and material before slicing. For other slicers, use the repository files below.

The recommended print project is `hardware/3d-model/open32drone-frame.3mf`. Import it at 100% scale; the main frame should measure approximately 103.3 × 103.3 mm. Check layer height, wall thickness, supports and first-layer adhesion for your printer, nozzle and material. After printing, remove supports and check that the four motor mounts are not deformed and the PCB holes align naturally.

Use `hardware/3d-model/open32drone-frame.stp` to modify the structure or inspect dimensions in other CAD software. After importing STEP, check units against a main-frame width of about 103.3 mm rather than scaling from the software's default units.

### PCB fabrication

1. **Get the design.** Open or clone the [JLC Open Hardware PCB project](https://oshwhub.com/fanchewang/open32drone) and verify the matching revision. If the landing page has no preview or electronic BOM, inspect and export them from the design.
2. **Prepare fabrication data.** Use Gerbers, drill files, the electronic BOM, placement drawings and interface/voltage definitions from the same revision. Select board thickness, copper and finish according to the design, not photographs.
3. **Inspect the bare PCB.** Check the outline, slots, holes, solder mask, pads and silkscreen against the matching BOM and placement drawings before soldering.

![PCB front and back](/media/photos/pcb-bare-front-back.jpg)

<p class="figure-caption">Figure 2-1. Front and back of the Open32Drone baseboard. It provides power, motor drivers and module connections; the XIAO, IMU and optical-flow/ToF module are installed separately.</p>

## 2.4 PCB soldering

### 1. Sort the components

Separate resistors, capacitors, diodes, MOSFETs, connectors, headers and modules. Work with one group at a time and mark installed parts on the placement drawing. Check Pin 1, cathode markings and connector openings before placing polarized parts.

![PCB, connectors and modules laid out](/media/photos/parts-layout.jpg)

<p class="figure-caption">Figure 2-2. PCB, connectors, power board and IMU before soldering.</p>

### 2. Place surface-mount components

Clean the pads and apply an even layer of solder paste or pre-tin them. Start with small, low-profile parts, then fit connectors and modules:

1. Resistors, capacitors and small-signal components.
2. MOSFETs, diodes and other directional components.
3. Motor connectors, power switch and other connectors.
4. Male and female headers, power board and IMU.

Check from above that each part is centered, then from the side that both ends sit on their pads. Adjust misaligned parts before heating. Remove bridges with flux and solder wick rather than repeatedly pushing adjacent components with the iron.

![Surface-mount component placement](/media/photos/smd-placement.jpg)

<p class="figure-caption">Figure 2-3. Positioned surface-mount components. Use the nose arrow on the board as the orientation reference throughout assembly.</p>

### 3. Solder the board

On a hotplate, keep the PCB flat against the working surface and follow the solder's specified preheat, reflow and cooling profile. Watch for parts aligning as the solder melts, then let the board cool naturally before moving it. With a soldering iron, tack one pin, recheck orientation and position, then solder the remaining joints.

![Soldered connectors and surface-mount components](/media/photos/connectors-soldered.jpg)

<p class="figure-caption">Figure 2-4. Main board with connectors installed. Connector openings should match the direction of the external wiring.</p>

### 4. Inspect the joints

Use a magnifier to inspect the power input, four motor-driver circuits, headers and connectors section by section. Solder should wet both pad and pin fully, with no bridges, cold joints, lifted pins or loose solder beads.

![Front of the soldered board](/media/photos/pcb-soldered.jpg)

<p class="figure-caption">Figure 2-5. Board after soldering. Pay particular attention to the four motor outputs and central component area.</p>

With power disconnected, use a multimeter to check for a short across the battery terminals and verify connections on both sides of the power switch. For the first power-up, use a current-limited supply or a protected 1S battery. Disconnect immediately if you notice abnormal heat, smell or rapidly rising current.

### 5. Fit headers and modules

Install the power board on the back first, matching input, output and GND to the main-board silkscreen. Then solder the XIAO female headers, keeping both rows parallel so the XIAO fits without force.

![Power board on the back](/media/photos/power-board.jpg)

<p class="figure-caption">Figure 2-6. Mounting the power board on the back of the main board.</p>

![Female headers and board connections](/media/photos/headers.jpg)

<p class="figure-caption">Figure 2-7. Check header height and alignment from the side after soldering.</p>

Install the IMU separately on the controller board, following the axis markings. Secure it rigidly after soldering; thick, soft foam can let it move. The standard firmware uses IMU mounting rotation `roll=π`, `pitch=0`, `yaw=π/2`. The matching PCB and illustrated mounting correspond to these settings.

![IMU silkscreen and pins](/media/photos/imu-module.jpg)

![IMU installed on the controller board](/media/photos/imu-installed.jpg)

<p class="figure-caption">Figure 2-8. IMU module and completed controller board.</p>

The board should now contain the motor drivers, power circuitry, XIAO headers and IMU. The optical-flow/ToF module connects by cable and is fitted to the frame next.

## 2.5 Aircraft assembly

### Orientation and motor numbering {#motor-layout}

Viewed from above, +X points forward and +Y to the aircraft's left. Mark the nose on the frame, then connect the four motors as shown.

![Top view: front left M3, front right M2, rear left M0, rear right M1](/media/figures/motor-layout.en.svg)

Motor-test commands are used in the next chapter's [Preflight preparation](04-firmware-flight.en.md#preflight). See [Parameters and interfaces](../reference/firmware.md#hardware-contract) for GPIO and simulation interfaces.

### Optical flow and ToF

Turn the frame upside down and place the optical-flow/ToF module in the front mount. The lens and ranging window must face the ground, clear of screws, tape and wires. Keep the module parallel to the four-motor thrust plane. Its standard position is about 24 mm ahead of the yaw center; the firmware compensates for this offset.

![Optical-flow/ToF module mounting position](/media/photos/flow-tof-install.jpg)

<p class="figure-caption">Figure 2-9. The module mounts at the front of the frame, with its cable routed into the center.</p>

### Mounting the controller board

Turn the frame upright. Arrange the optical-flow/ToF cable and place the controller board with its nose arrow aligned to the frame's nose. Start all four screws, then tighten gently in a diagonal sequence until seated. Keep the board flat and avoid trapping wires underneath.

![Controller board mounted on the frame](/media/photos/mainboard-install.jpg)

<p class="figure-caption">Figure 2-10. Relative positions of the controller board, IMU and optical-flow/ToF module.</p>

The optical-flow/ToF module uses UART: module TX connects to controller RX (GPIO8), and module RX to controller TX (GPIO7), at 115200 baud. The IMU uses I²C with SDA on GPIO2 and SCL on GPIO43. Connect the matching cable according to the PCB silkscreen, holding the connector body when plugging or unplugging it.

### XIAO and receiver

Check for bent pins, then insert the XIAO ESP32-S3 vertically into both female-header rows. Leave the USB-C port accessible from outside the frame. For SBUS, secure the receiver in the reserved area and connect RX/TX and power. A receiver is optional when using only a phone or ROS.

![XIAO installed on the controller board](/media/photos/xiao-install.jpg)

<p class="figure-caption">Figure 2-11. XIAO inserted into the controller-board headers.</p>

### Grommets and motors

Press the four Ø8 mm motor grommets into the frame slots and check that each edge is seated all the way around. Insert the 8520 motors from the correct side, keeping all four at the same height with parallel shafts. Hold the motor casing; do not press on the 1 mm shaft or pull the wires.

![Grommets inserted into the frame](/media/photos/motor-grommets.jpg)

![Side view of an 8520 motor in its grommet](/media/photos/motor-install.jpg)

<p class="figure-caption">Figure 2-12. Grommets hold the motors and isolate some vibration.</p>

Route the motor wires along the arms and connect M0–M3 one by one. Leave a little slack and keep every cable out of the propeller discs. Do not install the propellers yet.

![Four motor cables connected to the controller board](/media/photos/motor-wiring.jpg)

<p class="figure-caption">Figure 2-13. Completed motor wiring.</p>

### Secure the battery

The reference aircraft uses an 18350 1300 mAh battery weighing 25 g. Mount it near the center so the fore/aft and left/right center of gravity are close to the geometric center. Keep power leads clear of the propellers and optical-flow/ToF window. Include the battery, propellers and installed accessories when weighing the aircraft; the reference takeoff mass is about 81 g.

![Cylindrical battery mounted centrally](/media/photos/battery-install.jpg)

<p class="figure-caption">Figure 2-14. Battery in the central mounting area. Keep it in the same position after each battery change.</p>

If you add a camera or bracket, or change to a pouch battery, reposition the battery to restore horizontal balance. Follow the camera module's requirements for lens direction and ribbon-cable bend radius.

## 2.6 Checks before fitting propellers {#propellers}

The standard voltage-sensing connection is `VBAT_SW → 100 kΩ → GPIO1/A0 → 100 kΩ → GND`. With a 3.70 V battery, the ADC pin should read about 1.85 V. Never connect the battery or 5 V directly to an ESP32-S3 GPIO. Set `PWR_VOLT_PIN=-1` on older boards without the divider. Before power-up, check for supply shorts, verify voltages and ground connections with a multimeter, then fit the controller module.

Keep the propellers off while completing [firmware flashing and motor checks](04-firmware-flight.en.md#preflight) in the next chapter. Confirm motor positions and rotation before disconnecting power to fit propellers. Use a small strip of paper or phone slow-motion video to record CW or CCW rotation from above. M0 and M2 should turn the same way, with M1 and M3 turning the opposite way. Add removable labels such as `M0 CW` and `M1 CCW` beside the grommets.

CW/CCW markings indicate a propeller's intended direction. Fit CW propellers to motors measured as CW and CCW propellers to motors measured as CCW. All four propellers must have the same diameter. Seat the hubs fully without rubbing the motor casings.

![Propeller installation reference](/media/photos/prop-install.jpg)

<p class="figure-caption">Figure 2-15. Propellers and motors. Use the direction labels established during propellers-off testing.</p>

Before fitting propellers, recheck board orientation, secure IMU and optical-flow/ToF mounts, parallel motor shafts, a centered battery and wires clear of all propeller discs. If flashing and calibration are still pending, complete the next chapter with propellers removed, then return here.

![Completed Open32Drone reference aircraft](/media/photos/drone-complete.jpg)

<p class="figure-caption">Figure 2-16. Completed reference aircraft. The camera is optional; ordinary position-hold flight uses the IMU and downward optical-flow/ToF module.</p>
