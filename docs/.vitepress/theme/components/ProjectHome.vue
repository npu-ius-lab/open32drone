<script setup lang="ts">
import { withBase } from 'vitepress'

const buildSteps = [
  {
    number: '01',
    title: '打印机架',
    text: '下载 3MF，按原尺寸切片并打印主机架、电池固定件和配套结构件。',
  },
  {
    number: '02',
    title: '下单 PCB 裸板',
    text: '使用与硬件版本匹配的生产文件向板厂下单 Open32Drone PCB 底板。',
  },
  {
    number: '03',
    title: '手工焊接',
    text: '按照 BOM 与位号图焊接电机驱动、连接器、供电小板、排母和 IMU。',
  },
  {
    number: '04',
    title: '装成飞机',
    text: '安装 XIAO、光流/ToF、电机、电池和桨叶，完成方向与重心检查。',
  },
  {
    number: '05',
    title: '飞行与编程',
    text: '刷入固件完成首飞，再用 ROS 2 控制，并在数值模型中练习策略训练。',
  },
]

const chapters = [
  {
    number: '01',
    title: '认识 Open32Drone',
    text: '了解项目来源、模块关系和从飞控闭环到强化学习的完整技术路线。',
    link: '/guide/01-project',
  },
  {
    number: '02',
    title: '确定制作目标',
    text: '准备工具和基础知识，明确每一阶段要得到的可检查结果。',
    link: '/guide/02-goals',
  },
  {
    number: '03',
    title: '开始制作',
    text: '查看单台采购清单，获取打印和 PCB 资料，按照片完成焊接与装配。',
    link: '/guide/03-hardware',
  },
  {
    number: '04',
    title: '刷写与首飞',
    text: '完成 USB 刷写、传感器校准和拆桨检查，选择遥控器或 Android 起飞。',
    link: '/guide/04-firmware-flight',
  },
  {
    number: '05',
    title: '调参与问题反馈',
    text: '检查抖动、漂移和高度变化，学习调参，并提供可复现的问题信息。',
    link: '/guide/05-tuning',
  },
  {
    number: '06',
    title: '接入 ROS 2',
    text: '订阅传感器和里程计，执行起降、速度控制、位置控制与方形航线。',
    link: '/guide/06-ros',
  },
  {
    number: '07',
    title: '训练强化学习策略',
    text: '建立 81 g 粗动力模型，训练残差 PPO，并完成穿环、螺旋与抗扰演示。',
    link: '/guide/07-rl',
  },
]
</script>

<template>
  <main class="project-home">
    <section class="project-hero" aria-labelledby="project-title">
      <div class="project-hero__copy">
        <p class="project-kicker">OPEN HARDWARE · DIY QUADROTOR · ROS 2 / ISAAC SIM</p>
        <h1 id="project-title">从裸板开始，亲手做一架能飞、能编程的无人机</h1>
        <p class="project-hero__lead">
          打印机架、向板厂下单 PCB 底板、焊接器件和接口，再安装分立的 XIAO ESP32-S3、
          IMU、光流/ToF、电机与电池。完成真实首飞以后，继续进入 ROS 2 控制和强化学习。
        </p>
        <div class="project-actions">
          <a class="project-button project-button--primary" :href="withBase('/guide/03-hardware')">
            开始制作
          </a>
          <a class="project-button" :href="withBase('/guide/01-project')">查看文档导航</a>
        </div>
        <dl class="project-specs" aria-label="参考样机参数">
          <div><dt>81 g</dt><dd>含电池起飞重量</dd></div>
          <div><dt>4 × 8520</dt><dd>空心杯电机</dd></div>
          <div><dt>60 mm</dt><dd>CW / CCW 桨叶</dd></div>
          <div><dt>300 Hz</dt><dd>飞行控制循环</dd></div>
        </dl>
      </div>
      <figure class="project-hero__media">
        <img
          :src="withBase('/media/photos/drone-complete.jpg')"
          alt="红色 3D 打印机架、紫色 Open32Drone PCB、四个电机和桨叶组成的参考样机"
          width="1600"
          height="1200"
        />
        <figcaption>
          <span>REFERENCE BUILD</span>
          81 g 参考样机 · 3D 打印机架 · 模块化电子系统
        </figcaption>
      </figure>
    </section>

    <section class="build-route" aria-labelledby="build-route-title">
      <header class="section-heading">
        <p class="section-index">BUILD ROUTE</p>
        <div>
          <h2 id="build-route-title">制作过程就是项目本身</h2>
          <p>打印、下单、焊接、检查和装配构成完整学习路径，每一步都有真实产物。</p>
        </div>
      </header>
      <ol class="build-steps">
        <li v-for="step in buildSteps" :key="step.number">
          <span>{{ step.number }}</span>
          <h3>{{ step.title }}</h3>
          <p>{{ step.text }}</p>
        </li>
      </ol>
    </section>

    <section class="modular-system" aria-labelledby="modular-title">
      <div class="modular-system__media">
        <img
          :src="withBase('/media/photos/pcb-bare-front-back.jpg')"
          alt="Open32Drone 紫色 PCB 底板正反面"
          width="1600"
          height="1185"
          loading="lazy"
        />
        <p>PCB 裸板正反面。焊盘、接口和模块安装位置完整保留。</p>
      </div>
      <div class="modular-system__copy">
        <p class="section-index">MODULAR ELECTRONICS</p>
        <h2 id="modular-title">分立模块，亲手完成电子系统</h2>
        <p>
          Open32Drone 的紫色 PCB 是电源、四路有刷电机驱动和连接接口的底板。
          计算、惯性测量和对地感知分别由独立模块承担，制作者需要把它们焊接或连接到正确位置。
        </p>
        <ul class="module-list">
          <li><strong>主控：</strong>XIAO ESP32-S3 插装到 PCB 排母，负责飞控计算与 Wi-Fi。</li>
          <li><strong>姿态：</strong>独立 IMU 模块按固定轴向焊接在主控板上。</li>
          <li><strong>定点定高：</strong>光流与 ToF 位于同一模块，通过线束向下安装。</li>
          <li><strong>动力：</strong>四个 8520 电机由板上 MOSFET 直接驱动，桨叶按 M0—M3 配对。</li>
          <li><strong>能源：</strong>1S 电池固定在机体中央，参考样机使用 25 g 的 18350 电池。</li>
        </ul>
        <p><a class="text-link" :href="withBase('/guide/03-hardware#purchasing')">查看单台采购清单与商品链接 →</a></p>
        <a class="text-link" :href="withBase('/guide/03-hardware')">查看完整焊接与装机步骤 →</a>
      </div>
    </section>

    <section class="chapter-route" aria-labelledby="chapter-route-title">
      <header class="section-heading">
        <p class="section-index">PROJECT GUIDE</p>
        <div>
          <h2 id="chapter-route-title">从第一块裸板走到自主飞行</h2>
          <p>七章按真实制作顺序衔接，上一章的结果会直接成为下一章的起点。</p>
        </div>
      </header>
      <ol class="chapter-list">
        <li v-for="chapter in chapters" :key="chapter.number">
          <a :href="withBase(chapter.link)">
            <span class="chapter-list__number">{{ chapter.number }}</span>
            <span class="chapter-list__content">
              <strong>{{ chapter.title }}</strong>
              <small>{{ chapter.text }}</small>
            </span>
            <span class="chapter-list__arrow" aria-hidden="true">→</span>
          </a>
        </li>
      </ol>
    </section>

    <section class="showcase" aria-labelledby="showcase-title">
      <div class="showcase__copy">
        <p class="section-index">SIMULATION SHOWCASE</p>
        <h2 id="showcase-title">完成装机以后，控制能力继续生长</h2>
        <p>
          这段演示把 81 g 模型、基础悬停、残差 PPO 对照、八字穿环、螺旋爬升和阵风恢复放在同一条流程中。
          教程正文还提供模型检查与两段固定镜头悬停视频。
        </p>
        <a class="text-link" :href="withBase('/guide/07-rl')">进入强化学习章节 →</a>
      </div>
      <video
        controls
        playsinline
        preload="metadata"
        :poster="withBase('/media/figures/rl-demo-poster.png')"
      >
        <source :src="withBase('/media/videos/rl-demo-60s.mp4')" type="video/mp4" />
      </video>
    </section>
  </main>
</template>
