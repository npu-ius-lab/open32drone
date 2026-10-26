// Public main-branch commits; dates are commit dates, not release dates.
export const updates = [
  {
    date: '2026-10-24', commit: '913d1db',
    zh: { title: '新增日志调参教程', items: ['介绍 log dump 导出与 AI 辅助调参，并高亮焊接注意事项。'] },
    en: { title: 'Added a flight-log tuning guide', items: ['Covers log dump export and AI-assisted tuning, with clearer warnings in the assembly guide.'] },
  },
  {
    date: '2026-10-21', commit: '8d19508',
    zh: { title: '补充供电与 IMU 接线说明', items: ['明确电源跳线帽、电压焊桥和针脚接法，增加逐步焊接检查。'] },
    en: { title: 'Clarified power and IMU wiring', items: ['Explains power jumpers, voltage selection and pin connections, with checks after each soldering step.'] },
  },
  {
    date: '2026-10-09', commit: '28750fc',
    zh: { title: '新增文档导航', items: ['集中列出教程、开发参考、硬件文件和软件下载入口。'] },
    en: { title: 'Added a documentation directory', items: ['Brings together tutorials, development references, hardware files and software downloads.'] },
  },
  {
    date: '2026-09-30', commit: '1a7b155',
    zh: { title: '修复下载校验兼容问题', items: ['校验脚本现在可读取采用 Windows 换行格式的校验文件。'] },
    en: { title: 'Fixed checksum file compatibility', items: ['The validation script now accepts checksum files with Windows line endings.'] },
  },
  {
    date: '2026-09-30', commit: 'f1c8d8b',
    zh: { title: '调整仓库目录，更新中英文教程', items: ['固件、Android、ROS 2、硬件、仿真与文档分目录存放，同步教程图片与下载配置。'] },
    en: { title: 'Reorganized source files and updated both guides', items: ['Separated firmware, Android, ROS 2, hardware, simulation and docs into folders; updated illustrations and download settings.'] },
  },
]
