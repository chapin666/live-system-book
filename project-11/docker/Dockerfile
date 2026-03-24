# 构建阶段
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# 运行阶段
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libavcodec58 \
    libavformat58 \
    libavutil56 \
    libswscale5 \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 从构建阶段复制二进制文件
COPY --from=builder /build/build/live-server /app/

# 暴露端口
EXPOSE 7880 7881/udp 7882

ENTRYPOINT ["/app/live-server"]
CMD ["--mode", "all"]