// Selected milestones on public main. Dates are commit dates, not release tags.
// Keep the README News sections aligned with this list. Minor copy/layout edits
// are omitted. Evidence uses committed code, including its former software/ path.
export const updates = [
  {
    id: 'documentation-home', date: '2026-10-26', commit: '7b0a428',
    zh: { title: '新增中英文教程首页', items: ['集中查找制作、首飞、日志调参与 ROS 2 开发文档。'] },
    en: { title: 'Bilingual documentation home', items: ['Find assembly, first-flight, log-based tuning and ROS 2 guides in one place.'] },
  },
  {
    // Added in f1c8d8b vs 1d4ebfc: ros2/.../keyboard_teleop.py and keyboard_core.py.
    id: 'ros2-keyboard', date: '2026-09-30', commit: 'f1c8d8b',
    zh: { title: '新增 ROS 2 键盘遥控', items: ['支持起飞、速度操控、位置步进、保持和降落。'] },
    en: { title: 'ROS 2 keyboard control', items: ['Take off, fly with velocity commands, step between position targets, hold and land.'] },
  },
  {
    // Added in f1c8d8b: firmware/loop_watchdog.ino and Android EmergencyStopState.java.
    id: 'flight-protection', date: '2026-09-30', commit: 'f1c8d8b',
    zh: { title: '改进飞控保护与 App 急停确认', items: ['主循环超时后停止电机输出，App 根据飞控回传确认急停结果。'] },
    en: { title: 'Flight-controller protection and app emergency-stop confirmation', items: ['Stop motor output on a control-loop timeout and use flight-controller telemetry to confirm emergency-stop results.'] },
  },
  {
    // 1d4ebfc adds software/simulation/{course,rl_demo}; absent from c0e6615.
    id: 'simulation-exercises', date: '2026-09-22', commit: '1d4ebfc',
    zh: { title: '新增仿真与强化学习练习', items: ['提供 PD 控制、日志分析和 PPO 训练示例。'] },
    en: { title: 'Simulation and reinforcement-learning exercises', items: ['Added PD control, log analysis and PPO training examples.'] },
  },
  {
    // c0e6615 adds software/android, including its manual-control and OTA clients.
    id: 'android-controller', date: '2026-08-30', commit: 'c0e6615',
    zh: { title: '新增 Android 控制端', items: ['通过 Wi-Fi 使用虚拟摇杆、起降控制、状态查看与 OTA 更新。'] },
    en: { title: 'Android controller app', items: ['Connect over Wi-Fi for virtual sticks, takeoff and landing, telemetry and OTA updates.'] },
  },
]
