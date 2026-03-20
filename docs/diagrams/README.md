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

```mermaid
%%{init: {'theme': 'base', 'themeVariables': { 'primaryColor': '#e3f2fd', 'primaryTextColor': '#333', 'primaryBorderColor': '#333', 'lineColor': '#333', 'secondaryColor': '#fff3e0', 'tertiaryColor': '#e8f5e9'}}}%%
pie showData
    title 配色使用分布
    "浅蓝 #e3f2fd (输入/输出)" : 25
    "浅橙 #fff3e0 (Demuxer)" : 25
    "浅绿 #e8f5e9 (Decoder)" : 25
    "浅粉 #fce4ec (Renderer)" : 25
```

---

## 第1章关键图表

### 1. Pipeline 架构图

```mermaid
flowchart LR
    A[视频文件] -->|读取| B[Demuxer]
    B -->|压缩数据| C[Decoder]
    C -->|原始像素| D[Renderer]
    D -->|显示| E[屏幕]
    
    style A fill:#e3f2fd
    style B fill:#fff3e0
    style C fill:#e8f5e9
    style D fill:#fce4ec
    style E fill:#e3f2fd
```

### 2. 数据流时序图

```mermaid
sequenceDiagram
    participant File as 视频文件
    participant Demuxer as Demuxer
    participant Decoder as Decoder
    participant Renderer as Renderer
    participant Screen as 屏幕
    
    File->>Demuxer: 读取容器数据
    Note over Demuxer: 解析 MP4/FLV 格式
    Demuxer->>Decoder: AVPacket<br/>(压缩数据 H.264)
    Note over Decoder: 解码运算
    Decoder->>Renderer: AVFrame<br/>(YUV 像素)
    Note over Renderer: 颜色转换+渲染
    Renderer->>Screen: RGB 显示
```

### 3. YUV 内存布局

```mermaid
flowchart TB
    subgraph YUV420P布局
        direction TB
        Y[Y 平面<br/>1920×1080<br/>完整分辨率]
        U[U 平面<br/>960×540<br/>1/4分辨率]
        V[V 平面<br/>960×540<br/>1/4分辨率]
        
        Y --> U
        U --> V
    end
    
    style Y fill:#e8f5e9
    style U fill:#fff9c4
    style V fill:#e1bee7
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
