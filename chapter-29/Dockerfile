# Chapter 29: Docker 部署配置

# 多阶段构建 Dockerfile
# 构建阶段
FROM ubuntu:22.04 AS builder

# 安装编译依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    libssl-dev \
    libsdl2-dev \
    && rm -rf /var/lib/apt/lists/*

# 复制源码
WORKDIR /build
COPY . .

# 编译
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# 运行阶段
FROM ubuntu:22.04

# 安装运行时依赖
RUN apt-get update && apt-get install -y \
    libavcodec58 \
    libavformat58 \
    libavutil56 \
    libswscale5 \
    libswresample3 \
    libssl3 \
    libsdl2-2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# 创建非 root 用户
RUN useradd -m -s /bin/bash liveuser

# 复制二进制文件
WORKDIR /app
COPY --from=builder /build/build/live-server /app/
COPY --from=builder /build/config /app/config

# 设置权限
RUN chown -R liveuser:liveuser /app
USER liveuser

# 健康检查
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD /app/live-server --health-check || exit 1

# 暴露端口
EXPOSE 7880 7881/udp 7882 8080

# 启动命令
ENTRYPOINT ["/app/live-server"]
CMD ["--config", "/app/config/server.yaml"]
