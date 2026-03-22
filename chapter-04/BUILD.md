# Chapter 4 编译指南

## 性能分析播放器

### 编译

```bash
cd /root/.openclaw/workspace/live-system-book/chapter-04

# 编译性能分析播放器
g++ -std=c++14 -O2 profiler_player.cpp -o profiler_player \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil sdl2)

# 编译卡顿实验程序
g++ -std=c++14 lag_experiment.cpp -o lag_experiment \
    $(pkg-config --cflags --libs sdl2)
```

### 运行

```bash
# 创建测试视频
ffmpeg -f lavfi -i testsrc=duration=10:size=1280x720:rate=30 \
       -pix_fmt yuv420p -c:v libx264 test.mp4

# 运行性能分析播放器
./profiler_player test.mp4

# 运行卡顿实验（参数为模拟解码耗时，单位毫秒）
./lag_experiment 50    # 模拟50ms解码
./lag_experiment 100   # 模拟100ms解码
```

### 观察要点

1. **profiler_player**: 观察解码耗时、渲染耗时、帧率预算检查
2. **lag_experiment**: 拖动窗口感受不同解码耗时对事件响应的影响

### perf 火焰图生成

```bash
# 1. 记录性能数据
perf record -g --call-graph=dwarf ./profiler_player test.mp4

# 2. 生成火焰图（需要 FlameGraph 工具）
git clone https://github.com/brendangregg/FlameGraph.git
perf script | ./FlameGraph/stackcollapse-perf.pl | \
    ./FlameGraph/flamegraph.pl > flamegraph.svg

# 3. 查看
firefox flamegraph.svg
```
