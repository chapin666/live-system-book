# 第十五章：美颜与滤镜

> **本章目标**：实现 GPU 图像处理管线，理解美颜算法原理，掌握滤镜链设计。

采集的原始画面往往不够理想——肤色暗沉、皮肤瑕疵、光线不均。美颜处理是直播的标配功能，它能显著提升画面观感。

本章将学习：
- **GPU 图像处理**：Shader 编程、离屏渲染
- **美颜算法**：磨皮、美白、瘦脸原理
- **滤镜链**：多效果组合、参数调节

**阅读指南**：
- 第 1-2 节：GPU 图像处理基础
- 第 3-5 节：美颜算法详解（磨皮、美白、调色）
- 第 6-7 节：滤镜链设计、性能优化
- 第 8 节：本章总结

---

## 目录

1. [GPU 图像处理基础](#1-gpu-图像处理基础)
2. [OpenGL/Metal 离屏渲染](#2-openglmetal-离屏渲染)
3. [磨皮算法：双边滤波](#3-磨皮算法双边滤波)
4. [美白与调色](#4-美白与调色)
5. [高级美颜：瘦脸、大眼](#5-高级美颜瘦脸大眼)
6. [滤镜链设计](#6-滤镜链设计)
7. [性能优化](#7-性能优化)
8. [本章总结](#8-本章总结)

---

## 1. GPU 图像处理基础

### 1.1 为什么要用 GPU

CPU 处理图像的问题：
- 1080p 每帧 207 万像素
- 30fps 时每秒处理 6220 万像素
- CPU 像素级操作太慢

GPU 优势：
- 数千个并行计算单元
- 专门优化图像/矩阵运算
- 1080p 美颜处理 < 2ms

### 1.2 图像处理管线

```
┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│ 输入纹理  │ → │  滤镜1   │ → │  滤镜2   │ → │ 输出纹理  │
│ (YUV/RGB)│    │ (Shader) │    │ (Shader) │    │ (YUV/RGB)│
└──────────┘    └──────────┘    └──────────┘    └──────────┘
```

### 1.3 色彩空间转换

视频采集通常是 YUV，GPU 处理常用 RGB：

```glsl
// YUV to RGB 转换 (Fragment Shader)
vec3 YUVtoRGB(vec3 yuv) {
    float y = yuv.x;
    float u = yuv.y - 0.5;
    float v = yuv.z - 0.5;
    
    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;
    
    return vec3(r, g, b);
}
```

---

## 2. OpenGL/Metal 离屏渲染

### 2.1 OpenGL ES 渲染流程

```cpp
class GLImageProcessor {
public:
    bool Init(int width, int height) {
        width_ = width;
        height_ = height;
        
        // 创建 FBO（帧缓冲对象）
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        
        // 创建输出纹理
        glGenTextures(1, &output_texture_);
        glBindTexture(GL_TEXTURE_2D, output_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // 绑定纹理到 FBO
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, output_texture_, 0);
        
        // 检查 FBO 完整性
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            return false;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }
    
    void Process(GLuint input_texture, GLuint program) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width_, height_);
        
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(program);
        
        // 绑定输入纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input_texture);
        glUniform1i(glGetUniformLocation(program, "inputTexture"), 0);
        
        // 绘制全屏四边形
        DrawQuad();
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    GLuint GetOutputTexture() const { return output_texture_; }
    
private:
    void DrawQuad() {
        // 顶点数据：位置 + 纹理坐标
        static const float vertices[] = {
            // 位置          // 纹理坐标
            -1.0f, -1.0f,   0.0f, 0.0f,
             1.0f, -1.0f,   1.0f, 0.0f,
            -1.0f,  1.0f,   0.0f, 1.0f,
             1.0f,  1.0f,   1.0f, 1.0f
        };
        
        // 使用 VBO 绘制
        // ...
    }
    
    GLuint fbo_ = 0;
    GLuint output_texture_ = 0;
    int width_, height_;
};
```

### 2.2 基础 Shader

**顶点着色器**：
```glsl
#version 300 es
in vec2 aPosition;
in vec2 aTexCoord;
out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
```

**片段着色器（直通）**：
```glsl
#version 300 es
precision mediump float;
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D inputTexture;

void main() {
    fragColor = texture(inputTexture, vTexCoord);
}
```

---

## 3. 磨皮算法：双边滤波

### 3.1 磨皮原理

磨皮目标：
- 平滑皮肤（去除瑕疵）
- 保留边缘（眼睛、嘴巴轮廓清晰）

**双边滤波（Bilateral Filter）**：
- 空间权重：距离中心像素越近权重越大
- 颜色权重：颜色差异越小权重越大
- 结果：平滑皮肤但保留边缘

### 3.2 双边滤波 Shader

```glsl
#version 300 es
precision mediump float;
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D inputTexture;
uniform vec2 texSize;
uniform float sigmaSpace;  // 空间 sigma (5.0 ~ 15.0)
uniform float sigmaColor;  // 颜色 sigma (0.1 ~ 0.3)

vec4 bilateralFilter(sampler2D tex, vec2 uv, vec2 size) {
    vec4 centerColor = texture(tex, uv);
    vec4 sumColor = vec4(0.0);
    float sumWeight = 0.0;
    
    int radius = 5;  // 滤波半径
    
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            vec2 offset = vec2(float(x), float(y)) / size;
            vec2 sampleUV = uv + offset;
            vec4 sampleColor = texture(tex, sampleUV);
            
            // 空间权重
            float spatialDist = length(vec2(x, y));
            float spatialWeight = exp(-(spatialDist * spatialDist) / (2.0 * sigmaSpace * sigmaSpace));
            
            // 颜色权重
            float colorDist = length(sampleColor.rgb - centerColor.rgb);
            float colorWeight = exp(-(colorDist * colorDist) / (2.0 * sigmaColor * sigmaColor));
            
            float weight = spatialWeight * colorWeight;
            sumColor += sampleColor * weight;
            sumWeight += weight;
        }
    }
    
    return sumColor / sumWeight;
}

void main() {
    fragColor = bilateralFilter(inputTexture, vTexCoord, texSize);
}
```

### 3.3 磨皮强度混合

```glsl
uniform float smoothStrength;  // 0.0 ~ 1.0

void main() {
    vec4 original = texture(inputTexture, vTexCoord);
    vec4 smoothed = bilateralFilter(inputTexture, vTexCoord, texSize);
    
    // 皮肤检测（简单版本：根据颜色范围）
    float skinMask = isSkin(original.rgb) ? 1.0 : 0.0;
    
    // 只在皮肤区域应用磨皮
    fragColor = mix(original, smoothed, smoothStrength * skinMask);
}

float isSkin(vec3 rgb) {
    // 简化的皮肤检测
    float max_val = max(rgb.r, max(rgb.g, rgb.b));
    float min_val = min(rgb.r, min(rgb.g, rgb.b));
    float rg_diff = abs(rgb.r - rgb.g);
    
    // 皮肤通常：R > G > B，且 R-G 差异适中
    return (rgb.r > rgb.g && rgb.g > rgb.b && rg_diff > 0.05 && rg_diff < 0.4) ? 1.0 : 0.0;
}
```

---

## 4. 美白与调色

### 4.1 美白算法

```glsl
uniform float whiteningStrength;  // 0.0 ~ 1.0

vec3 whitening(vec3 color, float strength) {
    // 提高亮度但避免过曝
    vec3 hsl = rgbToHsl(color);
    hsl.z = mix(hsl.z, 1.0, strength * 0.3);  // 增加明度
    hsl.y = mix(hsl.y, hsl.y * 0.9, strength); // 略微降低饱和度
    return hslToRgb(hsl);
}

void main() {
    vec4 original = texture(inputTexture, vTexCoord);
    vec3 whitened = whitening(original.rgb, whiteningStrength);
    fragColor = vec4(whitened, original.a);
}
```

### 4.2 色彩调整

```glsl
uniform float brightness;   // -1.0 ~ 1.0
uniform float contrast;     // 0.0 ~ 2.0
uniform float saturation;   // 0.0 ~ 2.0

vec3 adjustBrightnessContrast(vec3 color, float b, float c) {
    return (color - 0.5) * c + 0.5 + b;
}

vec3 adjustSaturation(vec3 color, float s) {
    float gray = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(gray), color, s);
}
```

---

## 5. 高级美颜：瘦脸、大眼

### 5.1 瘦脸原理

瘦脸是**图像变形（Image Warping）**：
- 定义控制点（脸颊两侧）
- 向中心移动控制点
- 周围像素根据距离插值变形

### 5.2 局部变形 Shader

```glsl
uniform vec2 faceCenter;     // 脸部中心
uniform vec2 slimPoint;      // 要收缩的点
uniform float slimRadius;    // 影响半径
uniform float slimStrength;  // 收缩强度

vec2 slimFace(vec2 uv) {
    vec2 pos = uv * texSize;
    vec2 delta = pos - slimPoint;
    float dist = length(delta);
    
    if (dist < slimRadius) {
        float t = dist / slimRadius;  // 0.0 ~ 1.0
        // 越靠近中心收缩越明显
        float factor = (1.0 - t) * slimStrength;
        vec2 direction = normalize(faceCenter - slimPoint);
        pos += direction * factor * slimRadius;
    }
    
    return pos / texSize;
}

void main() {
    vec2 warpedUV = slimFace(vTexCoord);
    fragColor = texture(inputTexture, warpedUV);
}
```

### 5.3 人脸关键点检测

瘦脸需要知道脸部位置：
- **离线检测**：使用 dlib、MediaPipe 检测 68/468 个关键点
- **实时检测**：ML Kit、Face++ 等 SDK
- **简化方案**：手动设置控制点位置

```cpp
struct FaceKeypoints {
    vec2 leftCheek;    // 左脸颊
    vec2 rightCheek;   // 右脸颊
    vec2 leftEye;      // 左眼中心
    vec2 rightEye;     // 右眼中心
    vec2 jawCenter;    // 下巴中心
};
```

---

## 6. 滤镜链设计

### 6.1 滤镜链架构

```cpp
class FilterChain {
public:
    void AddFilter(std::unique_ptr<ImageFilter> filter) {
        filters_.push_back(std::move(filter));
    }
    
    GLuint Process(GLuint input_texture) {
        GLuint current = input_texture;
        
        for (size_t i = 0; i < filters_.size(); i++) {
            filters_[i]->Apply(current, pingpong_[i % 2]);
            current = pingpong_[i % 2].GetTexture();
        }
        
        return current;
    }
    
private:
    std::vector<std::unique_ptr<ImageFilter>> filters_;
    FrameBuffer pingpong_[2];  // 双缓冲
};
```

### 6.2 预设滤镜

```cpp
class PresetFilters {
public:
    static FilterChain CreateNaturalBeauty() {
        FilterChain chain;
        chain.AddFilter(std::make_unique<BilateralFilter>(8.0, 0.15, 0.3));  // 轻度磨皮
        chain.AddFilter(std::make_unique<WhiteningFilter>(0.2));             // 轻度美白
        chain.AddFilter(std::make_unique<BrightnessFilter>(0.05, 1.1));      // 提亮
        return chain;
    }
    
    static FilterChain CreateVintage() {
        FilterChain chain;
        chain.AddFilter(std::make_unique<SepiaFilter>(0.8));      // 复古色调
        chain.AddFilter(std::make_unique<VignetteFilter>(0.5));  // 暗角
        chain.AddFilter(std::make_unique<GrainFilter>(0.1));     // 颗粒感
        return chain;
    }
};
```

### 6.3 参数实时调节

```cpp
class BeautyController {
public:
    void SetSmoothStrength(float value) {
        smooth_strength_ = std::clamp(value, 0.0f, 1.0f);
        smooth_filter_-SetStrength(smooth_strength_);
    }
    
    void SetWhiteningStrength(float value) {
        whitening_strength_ = std::clamp(value, 0.0f, 1.0f);
        whitening_filter_-SetStrength(whitening_strength_);
    }
    
    void SetFaceSlim(float value) {
        face_slim_ = std::clamp(value, 0.0f, 1.0f);
        warp_filter_-SetSlimStrength(face_slim_);
    }
    
    // 获取当前参数用于 UI 显示
    struct Params {
        float smooth;
        float whitening;
        float faceSlim;
    };
    Params GetParams() const {
        return {smooth_strength_, whitening_strength_, face_slim_};
    }
    
private:
    float smooth_strength_ = 0.3f;
    float whitening_strength_ = 0.2f;
    float face_slim_ = 0.0f;
};
```

---

## 7. 性能优化

### 7.1 分辨率降级处理

美颜不需要在全分辨率处理：

```cpp
class OptimizedBeautyProcessor {
public:
    void Process(GLuint input_texture) {
        // 1. 降采样到 540p 处理美颜
        Downsample(input_texture, half_res_fbo_);
        
        // 2. 在 540p 上应用磨皮（更快）
        bilateral_filter_.Apply(half_res_fbo_.GetTexture(), temp_fbo_);
        
        // 3. 升采样回 1080p
        Upsample(temp_fbo_.GetTexture(), full_res_fbo_);
        
        // 4. 在全分辨率上应用其他效果
        whitening_filter_.Apply(full_res_fbo_.GetTexture(), output_fbo_);
    }
    
private:
    FrameBuffer half_res_fbo_;   // 540p
    FrameBuffer full_res_fbo_;   // 1080p
};
```

### 7.2 多_pass 合并

将多个简单 Shader 合并为单个复杂 Shader：

```glsl
// 合并美白 + 调色 + 对比度
void main() {
    vec3 color = texture(inputTexture, vTexCoord).rgb;
    
    // 美白
    color = whitening(color, whiteningStrength);
    
    // 调色
    color = color * tintColor;
    
    // 对比度
    color = (color - 0.5) * contrast + 0.5 + brightness;
    
    fragColor = vec4(color, 1.0);
}
```

### 7.3 性能统计

```cpp
class FilterProfiler {
public:
    void BeginFrame() {
        glFinish();  // 确保 GPU 空闲
        start_time_ = GetHighResTime();
    }
    
    void EndFrame(const char* filter_name) {
        glFinish();
        auto elapsed = GetHighResTime() - start_time_;
        stats_[filter_name] = elapsed;
    }
    
    void PrintStats() {
        for (const auto& [name, time] : stats_) {
            std::cout << name << ": " << time << " ms" << std::endl;
        }
    }
    
private:
    std::unordered_map<std::string, float> stats_;
    double start_time_;
};
```

---

## 8. 本章总结

### 核心技能

| 技能 | 实现要点 |
|:---|:---|
| GPU 处理 | OpenGL FBO、Shader 编程 |
| 磨皮 | 双边滤波、皮肤检测 |
| 美白 | HSL 调整、亮度/饱和度控制 |
| 滤镜链 | 双缓冲、Pass 合并 |
| 性能优化 | 分辨率降级、Shader 合并 |

### 美颜参数建议

| 参数 | 自然 | 轻度 | 重度 |
|:---|:---:|:---:|:---:|
| 磨皮 | 0.2-0.3 | 0.4-0.5 | 0.6-0.8 |
| 美白 | 0.1-0.2 | 0.3-0.4 | 0.5-0.7 |
| 瘦脸 | 0.0 | 0.1-0.2 | 0.3-0.5 |

> 提示：过度美颜会显得不真实，建议保持自然。

### 下一步

第十六章将整合所有组件，实现完整的主播端架构。
