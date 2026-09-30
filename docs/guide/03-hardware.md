---
outline: [2, 3]
pageClass: build-guide
---

# 02 · 开始制作

这一章从采购器件开始，依次完成机架打印、PCB 制作、焊接和整机装配。

## 2.1 动手前准备 {#preparation}

先按下表准备器件和工具

| 类别 | 需要准备什么 |
|---|---|
| 飞机零件 | 按下方采购清单准备；用手机控制不必购买遥控器和接收机 |
| 焊接与装配工具 | 恒温烙铁、焊锡、助焊剂、细头镊子、吸锡带、放大镜、万用表、螺丝刀、电子秤和非导电垫；用焊膏时另备可控温热台 |
| 电脑与连接线 | Windows、macOS 或 Linux 电脑，以及支持数据传输的 USB-C 线 |
| 手机 | Android 8.0 及以上，首飞使用已打包的 APK |
| 刷写与校准软件 | Chrome 或 Edge 打开网页刷写工具；CoolTerm 用于串口检查和校准 |
| 打印条件 | 自行打印时用切片软件，也可委托打印；不需要安装 CAD 建模软件 |

固件和 APK 都提供现成文件，首次制作不用安装 Arduino IDE、ESP-IDF、Android Studio、ROS 或仿真环境。后续需要修改源码、编程控制时，再到对应章节安装工具。

## 2.2 采购清单

机架文件见下方“打印机架”，PCB 设计与板载电子件 BOM 从[嘉立创工程](https://oshwhub.com/fanchewang/open32drone)获取。其余模块和机械件按下面的清单准备，装配工具见上一节。

<a id="purchasing"></a>

<a id="一台飞机的采购清单"></a>

表中数量为**一台飞机的用量**。打开商品链接后，按“规格参考”选择型号；商家的整包数量可能不同。橡胶圈等机械件的详细尺寸见[装配规格](#mechanical-specs)。

<div class="purchase-table">

| 序号 | 部件 | 规格参考 | 单台用量 | 采购入口 |
| :---: | --- | --- | :---: | --- |
| 1 | 飞控底板 | 配套版本的生产文件、电子 BOM 与位号图 | 1 | [嘉立创工程](https://oshwhub.com/fanchewang/open32drone) |
| 2 | 打印机架 | 完整机架一套；主体约 103.3 × 103.3 mm，按 100% 比例打印 | 1 | [MakerWorld](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842) |
| 3 | 主控板 | Seeed Studio XIAO ESP32-S3 Sense（含相机） | 1 | [商品页](https://item.taobao.com/item.htm?id=796226570709) |
| 4 | IMU 模块 | MPU9250；引脚排列与安装方向须匹配飞控底板 | 1 | [商品页](https://item.taobao.com/item.htm?id=867297908775) |
| 5 | 排母 | 1 × 7P；间距、高度须匹配飞控底板和主控板 | 2 | [商品选项](https://item.taobao.com/item.htm?id=1040276180385&skuId=6058024109270) |
| 6 | 跳线帽 | 间距 2.54 mm | 1 | [商品页](https://item.taobao.com/item.htm?id=1037786359471) |
| 7 | 升压板 | 输出标称 5 V / 1 A；输入适配 1S 电池 | 1 | [商品选项](https://item.taobao.com/item.htm?id=1020492920926&skuId=6194359034311) |
| 8 | 光流/ToF模块 | CORVON 纵川 TF-0850；UART 版，向下安装 | 1 | [商品页](https://item.taobao.com/item.htm?id=825567548453) |
| 9 | 光流线束 | 4P 双头反向线，长 60 mm；接口及引脚定义须匹配模块 | 1 | [商品页](https://item.taobao.com/item.htm?id=561435308484) |
| 10 | 8520 电机 | 机身 8 × 20 mm；轴径 1 mm；MX1.25 端子；线长 ≥ 100 mm | 4 | 暂未提供 |
| 11 | 桨叶 | 直径 60 mm；CW、CCW 各 2 | 4 | [商品页](https://item.taobao.com/item.htm?id=651317554058) |
| 12 | 电机橡胶圈 | Ø8 × 2 mm；建议两黑两白；[详细尺寸](#mechanical-specs) | 4 | [商品页](https://detail.tmall.com/item.htm?id=923643961535) |
| 13 | 固定螺丝 | 1 × 4 × 4 mm | 10 | [商品页](https://item.taobao.com/item.htm?id=658713209127&skuId=4755138613087) |
| 14 | 电池 | 1S 18350；JST 引出线须匹配飞控底板接口及极性 | 1 | [商品页](https://item.taobao.com/item.htm?id=900687087724) |
| 15 | 电池固定皮筋 | 直径 25 mm，宽 5 mm | 1 | [商品页](https://item.taobao.com/item.htm?id=583635067170) |
| 16 | SBUS 接收机 | 使用物理遥控器时需要，须与遥控器配套 | | 选配 |

</div>

板载电子元件（电阻、电容、MOSFET、二极管、连接器等）按[嘉立创工程](https://oshwhub.com/fanchewang/open32drone)中对应版本的电子 BOM 采购。

### 器件配图 {#parts-gallery}

点击图片可查看大图，型号与数量见上方采购清单。

<div class="purchase-grid">

<div class="purchase-card">

[![主控板](/media/purchasing/xiao-sense.webp)](/media/purchasing/xiao-sense.webp)

**主控板**

XIAO ESP32-S3 Sense · 1 个

</div>

<div class="purchase-card">

[![IMU 模块](/media/purchasing/imu.webp)](/media/purchasing/imu.webp)

**IMU 模块**

MPU9250 · 1 个

</div>

<div class="purchase-card">

[![排母](/media/purchasing/headers.webp)](/media/purchasing/headers.webp)

**排母**

1×7P · 2 个

</div>

<div class="purchase-card">

[![跳线帽](/media/purchasing/jumper.webp)](/media/purchasing/jumper.webp)

**跳线帽**

2.54 mm · 1 个

</div>

<div class="purchase-card">

[![升压板](/media/purchasing/power-module.webp)](/media/purchasing/power-module.webp)

**升压板**

5 V / 1 A · 1 个

</div>

<div class="purchase-card">

[![桨叶](/media/purchasing/propellers.webp)](/media/purchasing/propellers.webp)

**桨叶**

60 mm · 4 个

</div>

<div class="purchase-card">

[![固定螺丝](/media/purchasing/screws.webp)](/media/purchasing/screws.webp)

**固定螺丝**

1×4×4 mm · 10 个

</div>

<div class="purchase-card">

[![光流/ToF 模块](/media/purchasing/flow-tof.webp)](/media/purchasing/flow-tof.webp)

**光流/ToF 模块**

CORVON 纵川光流测距 · 1 个

</div>

<div class="purchase-card">

[![电机橡胶圈](/media/purchasing/grommets.webp)](/media/purchasing/grommets.webp)

**电机橡胶圈**

Ø8×2 mm · 4 个，建议两黑两白

</div>

<div class="purchase-card">

[![电池](/media/purchasing/battery-clean.png)](/media/purchasing/battery-clean.png)

**电池**

18350 · 1 个，配 JST 引出线

</div>

<div class="purchase-card">

[![光流线束](/media/purchasing/flow-cable.webp)](/media/purchasing/flow-cable.webp)

**光流线束**

4P 双头反向，60 mm · 1 根

</div>

<div class="purchase-card">

[![电池固定皮筋](/media/purchasing/battery-band.webp)](/media/purchasing/battery-band.webp)

**电池固定皮筋**

直径 25 mm × 宽 5 mm · 1 个

</div>

</div>

### 装配规格 {#mechanical-specs}

- **电机橡胶圈：** 规格 Ø8×2 mm，开孔 10 mm、卡槽高 2 mm、总厚 6 mm、外径 15 mm，共 4 个。建议两黑两白，四个使用相同材料和硬度。
- **螺丝与桨叶：** 固定螺丝 1×4×4 mm，共 10 个；60 mm 桨叶共 4 个，CW、CCW 各 2 个。
- **光流线束：** 购买 4P、60 mm 配套线束。接线时按模块和底板的 GND、电源、TX、RX 定义对应连接，具体引脚见[固定主控板](#固定主控板)。
- **电池接口：** 选择与底板匹配的 JST 插头和引出线；首次插接前用万用表确认正负极。

## 2.3 机架与 PCB

### 打印机架

拓竹用户可以直接进入 [MakerWorld 机架打印页面](https://makerworld.com.cn/zh/models/2922108-open32drone-wu-ren-ji-8520kong-xin-bei-ji-jia-ros2#profileId-3425842)，选择“在 Bambu Studio 中打开”。页面提供的配置为 **0.2 mm 层高、6 层墙、25% 填充**；切片前选择自己的打印机和材料。其他切片软件可使用下方仓库文件。

仓库中的 `hardware/3d-model/open32drone-frame.3mf` 是推荐的打印工程文件。导入切片软件后保持 100% 比例，主机架外形应约为 103.3 × 103.3 mm。根据实际打印机、喷嘴和材料检查层高、壁厚、支撑与首层附着；打印完成后清理支撑，确认四个电机安装位没有变形，PCB 安装孔能够自然对齐。

`hardware/3d-model/open32drone-frame.stp` 用于修改结构或在其他 CAD 软件中检查尺寸。导入 STEP 后同样以 103.3 mm 左右的主机架外形复核单位，不要凭软件默认单位直接缩放。

### PCB 制作

1. **获取工程。** 打开[嘉立创开源硬件 PCB 工程](https://oshwhub.com/fanchewang/open32drone)，打开或克隆设计图，确认配套硬件版本。如果介绍页没有预览图或电子 BOM，先在工程中检查并导出。
2. **准备制作资料。** 使用同一版本的 Gerber 与钻孔包、电子 BOM、正反面位号图，以及接口和电压定义。板厚、铜厚和表面处理等工艺按工程要求填写，不根据照片估计。
3. **验收裸板。** 收到 PCB 后检查板框、槽孔、通孔、阻焊、焊盘与丝印，确认实物版本对应 BOM 和位号图，再开始焊接。

![主控 PCB 的正反面，丝印和接口清晰可见](/media/photos/pcb-bare-front-back.jpg)

<p class="figure-caption">图 2-1　Open32Drone PCB 底板正反面。底板负责电源、电机驱动和模块连接，XIAO、IMU 与光流/ToF 需要另行安装。</p>

## 2.4 电路焊接

### 1. 器件分组

把电阻、电容、二极管、MOSFET、连接器、排针和模块分开放好。每次只拿出一组，装好后在贴装图上勾掉。有极性的器件先确认 Pin 1、阴极或连接器开口方向。

![PCB、连接器与模块展开](/media/photos/parts-layout.jpg)

<p class="figure-caption">图 2-2　焊接前的 PCB、连接器、供电小板和 IMU。</p>

### 2. 贴片器件

清洁焊盘，均匀涂上焊膏或预先上锡。先装低矮、小封装的器件，再装连接器和模块：

1. 电阻、电容和小信号器件；
2. MOSFET、二极管和其他有方向器件；
3. 电机接口、电源开关等连接器；
4. 排针、排母、供电小板和 IMU。

器件放下后先从正上方看是否居中，再从侧面看两端是否都落在焊盘上。偏移的器件在加热前调整；已经形成锡桥时，用助焊剂和吸锡带处理，不要反复用烙铁推挤相邻器件。

![贴片器件放置过程](/media/photos/smd-placement.jpg)

<p class="figure-caption">图 2-3　贴片器件完成定位后的状态。板上的机头箭头始终作为方向基准。</p>

### 3. 完成焊接

使用热台时，让 PCB 平整贴在工作面上，按焊料规定的预热、回流和冷却过程操作。观察焊料熔化后器件是否回正；焊完自然冷却，再移动电路板。使用烙铁时，先固定一个引脚，复查方向和位置，然后完成其余焊点。

![连接器与贴片器件的焊接状态](/media/photos/connectors-soldered.jpg)

<p class="figure-caption">图 2-4　连接器装好后的主板。连接器开口朝向要与外部线束的出线方向一致。</p>

### 4. 焊点检查

用放大镜沿着电源入口、四路电机驱动、排针、连接器逐区检查。合格焊点应完整润湿焊盘和引脚，没有相邻短路、虚焊、翘脚或多余锡珠。

![焊接后的主板正面](/media/photos/pcb-soldered.jpg)

<p class="figure-caption">图 2-5　焊后正面。检查重点是四路电机输出与中央器件区。</p>

断电后用万用表检查电池正负极是否短路，并核对电源开关前后的连接。第一次供电使用限流电源或带保护的 1S 电池；发现异常发热、气味或电流快速上升时立即断电。

### 5. 插件与模块

先装背面的供电小板，确认输入、输出和 GND 与主板丝印一致。再焊接 XIAO 使用的排母，让两排保持平行，XIAO 能够自然插入。

![背面供电小板](/media/photos/power-board.jpg)

<p class="figure-caption">图 2-6　背面供电小板与主板的安装关系。</p>

![排母与板间连接](/media/photos/headers.jpg)

<p class="figure-caption">图 2-7　排母焊接完成后，从侧面检查高度和垂直度。</p>

IMU 需要单独安装到主控板上。按板上的轴向标识摆正，焊好后应固定牢固，不要垫厚软泡棉，以免模块晃动。标准固件的 IMU 安装旋转为 `roll=π`、`pitch=0`、`yaw=π/2`；使用配套 PCB 并按图安装，就与这一设置对应。

![IMU 模块的丝印与针脚](/media/photos/imu-module.jpg)

![IMU 安装到主控板](/media/photos/imu-installed.jpg)

<p class="figure-caption">图 2-8　IMU 模块及安装完成的主控板。</p>

到这里，主控板应包含电机驱动、电源部分、XIAO 排母和 IMU。光流/ToF 通过线束连接，在下一步随机架安装。

## 2.5 整机装配

### 方向与电机编号 {#motor-layout}

从机顶向下看，机头为 +X，机体左侧为 +Y。先在机架上标记机头，再按图连接四个电机。

![机顶视图：前左 M3、前右 M2、后左 M0、后右 M1](/media/figures/motor-layout.svg)

电机测试命令在下一章的[起飞前准备](04-firmware-flight.md#preflight)中使用；GPIO 和仿真接口见[参数与接口](../reference/firmware.zh-CN.md#硬件约定)。

### 光流与 ToF

把机架翻到底面朝上，将光流/ToF 模块放入前部安装位。镜头和测距窗口朝地面，窗口不能被螺丝、胶带或线束遮挡。模块平面应与四个电机的推力平面平行；标准位置位于机体偏航中心前方约 24 mm，固件会补偿这段偏置。

![光流与 ToF 一体模块的安装位置](/media/photos/flow-tof-install.jpg)

<p class="figure-caption">图 2-9　光流/ToF 一体模块固定在机架前部，线束穿入中央区域。</p>

### 固定主控板

把机架翻回正面朝上。整理好光流/ToF 线束，放上主控板，让板上的机头箭头与机架机头一致。四个安装孔先全部带上螺丝，再按对角顺序轻轻拧到贴合。主控板应保持平整，下面不能压住导线。

![主控板固定到机架](/media/photos/mainboard-install.jpg)

<p class="figure-caption">图 2-10　主控板、IMU 和光流/ToF 的相对位置。</p>

光流/ToF 使用 UART：模块 TX 接飞控 RX（GPIO8），模块 RX 接飞控 TX（GPIO7），波特率 115200。IMU 使用 I²C：SDA 为 GPIO2，SCL 为 GPIO43。使用配套线束时按 PCB 丝印插接，插拔时握住插头本体。

### XIAO 与接收机

检查排针无弯折后，把 XIAO ESP32-S3 垂直插入两排排母。USB-C 口应留在机架外侧可接近的位置。使用 SBUS 时，将接收机固定到预留区域并连接 RX/TX 与供电；只使用手机或 ROS 时可以不装接收机。

![XIAO 安装到主控板](/media/photos/xiao-install.jpg)

<p class="figure-caption">图 2-11　XIAO 插入主控板排母。</p>

### 橡胶圈与电机

把四个 Ø8 mm 电机橡胶圈压入机架卡槽，沿一圈检查边缘完全就位。再把 8520 电机从正确方向压入橡胶圈，四个电机保持同一高度，轴线彼此平行。操作时握住电机外壳，不推压 1 mm 转轴，也不拉扯电机线。

![橡胶圈装入机架](/media/photos/motor-grommets.jpg)

![8520 电机与橡胶圈的侧面关系](/media/photos/motor-install.jpg)

<p class="figure-caption">图 2-12　橡胶圈与电机。橡胶圈既固定电机，也隔离部分振动。</p>

把电机线沿机臂引到对应接口，依照 M0—M3 逐条连接。保留轻微活动余量，并把所有线束移出桨盘。此时仍然不要安装桨叶。

![四路电机线束接入主控板](/media/photos/motor-wiring.jpg)

<p class="figure-caption">图 2-13　电机线束接好后的状态。</p>

### 电池固定

参考样机使用 18350 1300 mAh 电池，实测重量为 25 g。把电池固定在机体中央，让前后、左右的重心都接近几何中心，并确认电源线不会碰到桨叶或压住光流/ToF 窗口。称重时要包括电池、桨叶和实际安装的附件，参考整机重量约为 81 g。

![圆柱电池的中央安装方式](/media/photos/battery-install.jpg)

<p class="figure-caption">图 2-14　圆柱电池安装在中央区域。每次换电后保持相同位置。</p>

如果增加相机、支架或更换软包电池，重新移动电池来恢复水平重心。相机的镜头朝向与排线弯曲半径按相机模块要求处理。

## 2.6 装桨前检查 {#propellers}

电压采样的标准接法是 `VBAT_SW → 100 kΩ → GPIO1/A0 → 100 kΩ → GND`。
电池 3.70 V 时 ADC 引脚应约为 1.85 V。不能把电池或 5 V 直接接到 ESP32-S3 GPIO；
未装分压电路的旧板在软件里设 `PWR_VOLT_PIN=-1`。上电前用万用表检查电源短路、
供电电压和地线连接，再装入主控模块。

先保持无桨，进入下一章完成[固件刷写和电机检查](04-firmware-flight.md#preflight)，确认四个电机的位置与转向后，再断电装桨。用一小条纸带或手机慢动作观察，从机顶向下记录每个电机是 CW 还是 CCW。M0 与 M2 应为同一方向，M1 与 M3 为相反方向；在四个橡胶圈旁贴上 `M0 CW`、`M1 CCW` 这样的可移除标签。

桨叶上的 CW/CCW 表示它设计的旋转方向。把 CW 桨装到实测 CW 的电机，把 CCW 桨装到实测 CCW 的电机。四只桨必须同一直径，桨毂压到位但不摩擦电机外壳。

![桨叶安装位置参考](/media/photos/prop-install.jpg)

<p class="figure-caption">图 2-15　桨叶与四个电机的安装关系。最终方向以拆桨实测标签为准。</p>

装桨前再检查一遍：主板方向正确，IMU 和光流/ToF 不松动，四个电机轴平行，电池居中，全部线束离开桨盘。如果还没刷写和校准，先拆桨完成下一章的操作，再回到这里。

![完成组装的 Open32Drone 参考样机](/media/photos/drone-complete.jpg)

<p class="figure-caption">图 2-16　完成组装的参考样机。相机为可选模块；普通定点飞行使用 IMU 与向下安装的光流/ToF。</p>
