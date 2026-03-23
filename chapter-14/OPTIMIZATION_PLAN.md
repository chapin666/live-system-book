# 第十四章优化计划：高级采集技术

## 当前问题
1. 没有 SVG 架构图
2. 代码过多（macOS、Linux、Windows 采集代码重复）
3. 理论部分薄弱（缺少采集原理图解）

## 优化方案

### 1. 新增 SVG 图（5个）
- `capture-arch.svg` - 屏幕采集架构图（对比摄像头采集）
- `gpu-texture-sharing.svg` - GPU 纹理共享原理图
- `multi-camera.svg` - 多摄像头管理架构
- `pip-composition.svg` - 画中画合成示意图
- `capture-pipeline.svg` - 完整采集 Pipeline 数据流

### 2. 精简代码
- 删除 macOS CGDisplayStream 完整实现（约 80 行）
- 删除 Linux x11grab 完整实现（约 60 行）
- 删除窗口枚举和区域计算代码（约 50 行）
- 保留核心接口设计和 FFmpeg 命令行示例
- 统一说明：本章提供设计思路，完整实现见配套代码仓库

### 3. 增强理论
- 屏幕采集与摄像头采集的底层差异（帧缓冲 vs 设备驱动）
- GPU 纹理共享原理（避免 CPU 拷贝）
- 多摄像头同步采集的时序问题
- 画中画合成的图层混合算法

### 4. 平台支持说明
- 明确说明：只支持 Linux 和 macOS
- 删除所有 Windows 特定代码和条件编译
- 统一使用 FFmpeg 跨平台方案
