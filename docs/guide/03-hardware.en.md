---
outline: [2, 3]
pageClass: build-guide
---

# 03 · Start Building

Build the aircraft in this order: print the frame, obtain the PCB, solder and
inspect it, mount the modules, then identify the motors before fitting propellers.
The purple board supplies power, brushed-motor drivers and interconnects; the
XIAO, IMU and flow/ToF are separate modules.

## 3.1 Frame and PCB

### Frame printing

Bambu users can open the [MakerWorld frame profile](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) in Bambu Studio. The listed profile uses **0.2 mm layers, 6 walls and 25% infill**. Select your printer and material before slicing. For other slicers, use the repository files below.

Use [the 3MF print project](../../software/hardware/3d-model/open32drone-frame.3mf) at
100% scale. The main frame is approximately **103.3 × 103.3 mm**. Check layer
height, walls, supports and bed adhesion for your printer. Clean supports and
verify that all four motor mounts and PCB holes align without forcing parts.
Use [STEP](../../software/hardware/3d-model/open32drone-frame.stp) for CAD editing; its
stored units are centimetres, so verify the physical dimensions after import.

### PCB fabrication

1. **Get the design.** Open or clone the [JLC Open Hardware PCB project](https://oshwhub.com/fanchewang/open32drone) and verify the matching revision. If the landing page has no preview or electronic BOM, inspect and export them from the design.
2. **Prepare fabrication data.** Use Gerbers, drill files, the electronic BOM, placement drawings and interface/voltage definitions from the same revision. Select board thickness, copper and finish according to the design, not photographs.
3. **Inspect the bare PCB.** Check the outline, slots, holes, solder mask, pads and silkscreen against the matching BOM and placement drawings before soldering.

![PCB front and back](/media/photos/pcb-bare-front-back.jpg)

## 3.2 Parts and tools

<a id="purchasing"></a>

### Shopping list {#shopping-list-for-one-aircraft}

Quantities are **per aircraft**, not seller pack sizes; optional parts have a blank quantity. Choose the model using the specification reference when opening a listing. See [mechanical dimensions and wiring](#mechanical-specs) for additional details.

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
| 10 | Brushed motor | 8 × 20 mm body; 1 mm shaft; MX1.25 connector; wire length ≥ 100 mm | 4 | Not provided |
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

[![Battery](/media/purchasing/battery.webp)](/media/purchasing/battery.webp)

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
- **Flow cable:** use a matching 4-pin, 60 mm cable. Connect according to the GND, supply, TX and RX definitions on the module and flight-controller board, not the seller's cable-orientation label.
- **Battery connector:** match the JST connector and lead to the board. Check polarity with a multimeter before the first connection.

### Tools

- **Soldering:** temperature-controlled iron, solder, flux, fine tweezers and desoldering braid; add a controlled hot plate when using solder paste.
- **Inspection:** multimeter and magnifier.
- **Assembly:** suitable screwdriver, scale and non-conductive work mat.

Set the soldering temperature according to the solder or paste instructions.

## 3.3 PCB soldering

![Parts before assembly](/media/photos/parts-layout.jpg)

1. Sort components by the electronic BOM and placement drawing. Check values,
   packages and polarity before applying solder.
2. Start with low-profile SMD parts. Align pads and avoid bridges, especially
   around the motor-driver and power components.
3. Follow the solder manufacturer's temperature profile for reflow, or tack
   one pin, check alignment, then solder the remaining pins with an iron.
4. Let the board cool. Inspect for bridges, missing joints, displaced parts and
   reversed polarized components. Check power-to-ground resistance before power.
5. Fit the power module, headers and IMU according to the board revision.
   Do not flex the PCB to make modules fit.

![Soldered board](/media/photos/pcb-soldered.jpg)

## 3.4 Aircraft assembly

### Directions

Label the nose first. Motor names refer to the aircraft looking forward:

| Motor | Position | GPIO |
|---|---|---:|
| M0 | Rear left | 4 |
| M1 | Rear right | 3 |
| M2 | Front right | 6 |
| M3 | Front left | 5 |

The four motor axes must be parallel and all grommets fully seated at equal
height. Shafts must turn freely. Keep leads clear of propeller discs and fully
seat the connectors. Do not over-tighten the screws into printed plastic.

### IMU

Fix the IMU rigidly and parallel to the four-motor thrust plane. It need not
be at the same vertical height as the motors. The standard firmware mounting
rotation is `roll=π`, `pitch=0`, `yaw=π/2`. I²C uses SDA GPIO2 and SCL GPIO43.
Follow the board's supply-voltage requirements; ESP32-S3 GPIO is not 5 V tolerant.
The flight estimator uses acceleration and angular rate, not magnetic heading.

### Flow/ToF

Point the window straight downward, keep it clean and avoid cables in view.
The standard module is approximately 24 mm forward of the yaw centre; firmware
compensates that fixed offset during yaw. A tilted module can turn height or
rotation into false horizontal motion.

UART uses aircraft RX GPIO8, TX GPIO7, at 115200 baud. Connect module TX to
aircraft RX and module RX to aircraft TX. A reflective, transparent, uniformly
coloured or dark floor is unsuitable for optical flow.

### Battery and optional receiver

Secure the battery in the central documented position and repeat its placement
after every change. The reference build uses a 25 g 18350 cell; weigh your own
assembly rather than assuming that every battery has the same mass. SBUS is
optional for Android/ROS control.

The voltage input is a 100 kΩ / 100 kΩ divider from `VBAT_SW` to GPIO1/A0.
A 3.70 V battery should produce approximately 1.85 V at the ADC pin. Never wire
the battery directly to GPIO1. On a board without the divider, use
`PWR_VOLT_PIN=-1` rather than sampling a floating pin.

## 3.5 Motors and propellers

Keep propellers off. Continue to [flashing and calibration](04-firmware-flight.en.md),
then run `mrl`, `mrr`, `mfr` and `mfl` individually. Exactly the named motor should
run for approximately one second. Record its actual direction from above and
fit the matching CW/CCW propeller only after all four checks pass.

A wrong motor location, rotation or propeller pairing must be corrected before
flight. Calibration does not repair a twisted frame, bent shaft or reversed motor.
