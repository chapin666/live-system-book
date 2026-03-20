# 图表绘制规范

本书使用 **SVG 外链方案**，平衡美观度和加载速度。

---

## 图表存放

```
docs/
├── images/          # SVG 图片（用于引用）
│   ├── roadmap.svg
│   ├── pipeline-arch.svg
│   └── ...
└── diagrams/        # Draw.io 源文件（可编辑）
    └── *.drawio
```

---

## 使用方式

Markdown 中引用 SVG：

```markdown
<img src="docs/images/xxx.svg" width="100%"/>
```

---

## 配色规范

| 颜色 | 用途 | 十六进制 |
|:---|:---|:---|
| 浅蓝 | 输入/输出 | `#e3f2fd` |
| 浅橙 | Demuxer | `#fff3e0` |
| 浅绿 | Decoder | `#e8f5e9` |
| 浅粉 | Renderer | `#fce4ec` |

---

## 每章图表数量

- **README**：1 个（五阶段演进图）
- **每章**：最多 2 个核心图
- **其他**：Markdown 表格

---

## 绘制工具

1. 用 Draw.io 绘制
2. 导出为 SVG
3. 存放到 `docs/images/`
4. 保留 `.drawio` 源文件到 `docs/diagrams/`
