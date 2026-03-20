# 图表绘制指南

本书使用 **Mermaid** 绘制图表，GitHub 会自动渲染。

如需更精美的图表，可使用 **Draw.io**（免费）手动绘制。

---

## 推荐的图表工具

| 工具 | 适用场景 | 文件格式 |
|-----|---------|---------|
| **Mermaid** | 流程图、时序图、架构图 | Markdown 内嵌 |
| **Draw.io** | 精美架构图、自定义样式 | PNG/SVG + 源文件 |
| **Excalidraw** | 手绘风格、草图 | PNG/SVG |

---

## 配色方案

本书统一使用以下配色：

```css
/* 输入/输出 */
#e3f2fd (浅蓝)

/* 处理模块 */
#fff3e0 (浅橙) - Demuxer
#e8f5e9 (浅绿) - Decoder  
#fce4ec (浅粉) - Renderer

/* 数据流 */
#333333 (深灰) - 连接线
```

---

## 第1章关键图表

### 1. Pipeline 架构图

```
[视频文件] → [Demuxer] → [Decoder] → [Renderer] → [屏幕]
   浅蓝        浅橙         浅绿         浅粉        浅蓝
```

### 2. 数据流时序图

参与者：
- File (浅蓝)
- Demuxer (浅橙)
- Decoder (浅绿)
- Renderer (浅粉)
- Screen (浅蓝)

### 3. YUV 内存布局

```
┌─────────────────┐
│   Y 平面        │  1920×1080  浅绿
│  (完整分辨率)    │
├─────────────────┤
│   U 平面        │   960×540   浅黄
│  (1/4 分辨率)    │
├─────────────────┤
│   V 平面        │   960×540   浅紫
│  (1/4 分辨率)    │
└─────────────────┘
```

---

## 绘制步骤

1. 打开 [Draw.io](https://draw.io)
2. 创建新图表
3. 使用上述配色方案
4. 导出为 PNG（分辨率 300dpi）
5. 保存源文件到 `docs/diagrams/*.drawio`
6. 导出图片到 `docs/images/*.png`

---

## 文件存放规范

```
docs/
├── images/          # 导出的 PNG/SVG 图片
│   ├── pipeline-arch.png
│   ├── data-flow.png
│   └── ...
└── diagrams/        # Draw.io 源文件（可编辑）
    ├── pipeline-arch.drawio
    ├── data-flow.drawio
    └── ...
```

---

## Markdown 引用方式

```markdown
![架构图](../images/pipeline-arch.png)

<!-- 或者使用 Mermaid -->
```mermaid
flowchart LR
    ...
```
```
