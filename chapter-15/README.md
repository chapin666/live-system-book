# 第十五章：美颜与滤镜

> **本章目标**：理解 GPU 图像处理原理，掌握美颜算法，学会设计滤镜链。

采集的原始画面往往不够理想——肤色暗沉、皮肤瑕疵、光线不均。美颜处理是直播的标配功能，它能显著提升画面观感。

本章将学习：
- **GPU 图像处理原理**：为什么美颜必须用 GPU
- **美颜算法**：磨皮、美白的核心原理
- **滤镜链设计**：多效果组合的架构

⚠️ **前置知识**：本章涉及 GPU 和 Shader 概念。如果对这些不熟悉，建议先阅读第 0 节快速入门。

**阅读指南**：
- 第 0 节：GPU 与 Shader 基础（前置知识）
- 第 1-2 节：美颜算法原理（双边滤波、LUT 调色）
- 第 3-4 节：滤镜链架构、性能优化
- 第 5 节：本章总结

---

## 目录

0. [前置知识：GPU 与 Shader](#0-前置知识gpu-与-shader)
1. [磨皮算法：双边滤波](#1-磨皮算法双边滤波)
2. [美白与 LUT 调色](#2-美白与-lut-调色)
3. [滤镜链架构设计](#3-滤镜链架构设计)
4. [性能优化](#4-性能优化)
5. [本章总结](#5-本章总结)

---

## 0. 前置知识：GPU 与 Shader

### 0.1 为什么要用 GPU 做图像处理

**CPU 的问题**：
- 1080p 每帧 207 万像素
- 30fps 时每秒处理 6220 万像素
- CPU 是"串行"处理器，一个像素一个像素处理，太慢

**GPU 的优势**：
- 数千个并行计算单元
- 专门优化图像/矩阵运算
- 1080p 美颜处理 < 2ms

**类比理解**：
- CPU 像一位熟练的画家，画得快但一次只能画一个点
- GPU 像一支画笔阵列，可以同时画数千个点

### 0.2 什么是 Shader

**Shader（着色器）** 是运行在 GPU 上的小程序，专门处理图像像素。

**最简单的 Shader（亮度调节）**：
```glsl
// 输入：每个像素的颜色
// 输出：处理后的颜色

void main() {
    vec4 color = texture(inputImage, textureCoord);  // 读取原像素
    color.rgb *= 1.2;  // 亮度提升 20%
    outputColor = color;  // 输出
}
```

**关键概念**：
- **纹理（Texture）**：GPU 中的图像数据
- **Shader**：处理每个像素的程序
- **管线（Pipeline）**：多个 Shader 串联处理

### 0.3 图像处理管线

<img src="docs/images/gpu-pipeline.svg" width="100%"/>

```
输入纹理 → [滤镜1 Shader] → [滤镜2 Shader] → [滤镜3 Shader] → 输出纹理
   YUV         磨皮          美白           调色         YUV
```

**每一级滤镜**：
1. 读取输入纹理的像素
2. 执行算法（如双边滤波）
3. 写入输出纹理
4. 下一级滤镜读取这个输出作为输入

### 0.4 离屏渲染

美颜处理不需要显示到屏幕，只需要处理后的数据传给编码器。这种"不显示到屏幕"的渲染叫**离屏渲染**。

**核心概念**：
- **FBO（帧缓冲对象）**：离屏渲染的目标
- **纹理绑定**：将 FBO 的输出绑定为纹理，供下一级使用

---

## 1. 磨皮算法：双边滤波

### 1.1 磨皮的核心矛盾

**需求**：
- 皮肤要光滑（去除瑕疵）
- 眼睛、嘴巴轮廓要清晰（不能模糊）

**传统滤波的问题**：
- **均值滤波/高斯滤波**：整张图都模糊，边缘也糊了
- **双边滤波**：皮肤磨平，边缘保留

### 1.2 双边滤波原理

<img src="docs/images/bilateral-filter.svg" width="100%"/>

**双边滤波 = 空间权重 × 颜色权重**

**空间权重**（和高斯滤波一样）：
- 距离越近的像素，影响越大

**颜色权重**（双边滤波的关键）：
- 颜色越相似的像素，影响越大
- 颜色差异大的像素（如皮肤→眼睛边界），影响很小

**效果**：
- 皮肤区域（颜色相似）→ 被平滑，瑕疵消失
- 边缘区域（颜色差异大）→ 被保留，轮廓清晰

### 1.3 双边滤波 Shader 伪代码

```glsl
// 简化版双边滤波逻辑
vec3 BilateralFilter(sampler2D image, vec2 uv) {
    vec3 center = texture(image, uv).rgb;  // 中心像素
    vec3 result = vec3(0.0);
    float weightSum = 0.0;
    
    // 遍历周围 5×5 像素
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 offset = vec2(x, y) / imageSize;
            vec3 sample = texture(image, uv + offset).rgb;
            
            // 空间权重：距离越近权重越大
            float spatialWeight = Gaussian(length(vec2(x, y)));
            
            // 颜色权重：颜色越相似权重越大
            float colorWeight = Gaussian(distance(center, sample));
            
            // 双边权重
            float weight = spatialWeight * colorWeight;
            
            result += sample * weight;
            weightSum += weight;
        }
    }
    
    return result / weightSum;
}
```

> 实际实现需要优化（如使用分离滤波、降采样等），核心逻辑如上。

### 1.4 磨皮强度调节

通过调节双边滤波的参数控制磨皮程度：

| 参数 | 作用 | 推荐值 |
|:---|:---|:---:|
| 滤波半径 | 影响范围 | 3-5 像素 |
| 颜色 sigma | 颜色相似度阈值 | 0.1-0.3 |
| 空间 sigma | 距离衰减速度 | 2-5 像素 |
| 混合系数 | 原图与磨皮图混合比例 | 0.3-0.7 |

---

## 2. 美白与 LUT 调色

### 2.1 亮度与对比度调节

最简单的"美白"就是提升亮度：

```glsl
vec3 Brightness(vec3 color, float level) {
    // level: 0.0~1.0，0.5 为原亮度
    return color * (level * 2.0);
}
```

但单纯提升亮度会让画面"发白"，需要配合对比度：

```glsl
vec3 Contrast(vec3 color, float contrast) {
    // contrast: 0.0~2.0，1.0 为原对比度
    return (color - 0.5) * contrast + 0.5;
}
```

### 2.2 LUT 调色（Lookup Table）

**问题**：如何用同一套代码实现"日系清新"、"复古胶片"、"电影感"等不同风格？

**LUT 方案**：
- 不做实时计算，而是"查表"
- 输入颜色 → 查表 → 输出颜色
- 换一张表 = 换一种风格

<img src="docs/images/lut-color-grading.svg" width="80%"/>

**LUT 的优势**：
1. **速度极快**：O(1) 查找，无计算
2. **效果一致**：同样的 LUT 文件在任何平台效果相同
3. **专业调色**：可以用 Photoshop 制作 LUT，直接应用到直播

### 2.3 LUT Shader 实现

```glsl
uniform sampler3D lutTexture;  // 3D LUT 纹理

vec3 ApplyLUT(vec3 color) {
    // 将 RGB (0.0~1.0) 映射到 LUT 坐标
    vec3 lutCoord = color * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;
    return texture(lutTexture, lutCoord).rgb;
}
```

---

## 3. 滤镜链架构设计

### 3.1 设计目标

- **可组合**：多个滤镜可以任意组合
- **可配置**：每个滤镜参数可调节
- **高性能**：跳过的滤镜不消耗资源

### 3.2 架构图

<img src="docs/images/filter-chain.svg" width="100%"/>

### 3.3 接口设计

```cpp
// 滤镜基类
class IFilter {
public:
    virtual ~IFilter() = default;
    
    // 处理一帧
    virtual void Process(const GpuTexture& input, GpuTexture& output) = 0;
    
    // 是否启用
    virtual bool IsEnabled() const { return enabled_; }
    virtual void SetEnabled(bool enabled) { enabled_ = enabled; }
    
protected:
    bool enabled_ = true;
};

// 具体滤镜
class BilateralFilter : public IFilter {
public:
    void SetStrength(float strength) { strength_ = strength; }
    void Process(const GpuTexture& input, GpuTexture& output) override;
    
private:
    float strength_ = 0.5f;
};

class BrightnessFilter : public IFilter {
public:
    void SetLevel(float level) { level_ = level; }
    void Process(const GpuTexture& input, GpuTexture& output) override;
    
private:
    float level_ = 1.0f;
};
```

### 3.4 滤镜链管理器

```cpp
class FilterChain {
public:
    void AddFilter(std::unique_ptr<IFilter> filter) {
        filters_.push_back(std::move(filter));
    }
    
    void Process(const GpuTexture& input, GpuTexture& output) {
        GpuTexture* current_input = const_cast<GpuTexture*>(&input);
        GpuTexture temp;
        
        for (size_t i = 0; i < filters_.size(); i++) {
            if (!filters_[i]->IsEnabled()) continue;  // 跳过未启用的滤镜
            
            GpuTexture* current_output = (i == filters_.size() - 1) 
                ? &output : &temp;
            
            filters_[i]->Process(*current_input, *current_output);
            current_input = current_output;
        }
    }
    
private:
    std::vector<std::unique_ptr<IFilter>> filters_;
};
```

### 3.5 使用示例

```cpp
// 创建滤镜链
FilterChain chain;
chain.AddFilter(std::make_unique<BilateralFilter>());   // 磨皮
chain.AddFilter(std::make_unique<BrightnessFilter>()); // 美白
chain.AddFilter(std::make_unique<LUTFilter>());        // 调色

// 设置参数
dynamic_cast<BilateralFilter*>(chain[0])->SetStrength(0.6f);
dynamic_cast<BrightnessFilter*>(chain[1])->SetLevel(1.1f);

// 处理视频帧
chain.Process(inputTexture, outputTexture);
```

---

## 4. 性能优化

### 4.1 GPU 处理时间参考

| 分辨率 | 双边滤波 | LUT 调色 | 完整滤镜链 |
|:---|:---:|:---:|:---:|
| 720p | 0.5ms | 0.1ms | 1-2ms |
| 1080p | 1ms | 0.2ms | 2-3ms |
| 4K | 4ms | 0.5ms | 6-8ms |

### 4.2 优化策略

**1. 降采样磨皮**：
- 将 1080p 降到 540p 做双边滤波
- 再放大回原尺寸
- 效果相近，速度快 4 倍

**2. 跳过未启用的滤镜**：
- 滤镜链管理器检查 `IsEnabled()`
- 未启用的滤镜不绑定、不渲染

**3. 合并简单滤镜**：
- 亮度 + 对比度可以合并为一个 Shader
- 减少渲染 Pass 次数

**4. 使用硬件编码器的预处理**：
- 部分硬件编码器（如 NVENC）支持内置降噪
- 可以用硬件功能替代软件磨皮

### 4.3 美颜性能预算

以 1080p@30fps 直播为例：
- **总时间预算**：33ms/帧
- **美颜处理预算**：< 5ms（留给编码和网络更多时间）
- **实际**：优化后的滤镜链 2-3ms，满足要求

---

## 5. 本章总结

### 核心概念

| 概念 | 一句话解释 |
|:---|:---|
| GPU 处理 | 并行处理像素，比 CPU 快 50-100 倍 |
| Shader | 运行在 GPU 上的图像处理程序 |
| 双边滤波 | 磨皮 + 保边的算法，平滑皮肤但保留轮廓 |
| LUT | 查找表调色，速度快、效果可控 |
| 滤镜链 | 多个滤镜串联，灵活组合效果 |

### 美颜算法选择

| 效果 | 算法 | 性能 |
|:---|:---|:---:|
| 基础磨皮 | 双边滤波（降采样） | ⭐⭐⭐ |
| 高级磨皮 | 双边滤波 + 皮肤检测 | ⭐⭐ |
| 美白 | 亮度 + 对比度调节 | ⭐⭐⭐⭐⭐ |
| 调色 | LUT 查找表 | ⭐⭐⭐⭐⭐ |
| 瘦脸/大眼 | 顶点变形（Mesh Warp） | ⭐⭐ |

### 关键技能

- 理解双边滤波"保边"的原理
- 使用 LUT 实现风格化调色
- 设计可扩展的滤镜链架构
- 优化美颜处理性能

### 下一步

第十六章将整合所有组件，实现**完整的主播端 Pipeline**。
