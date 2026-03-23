# 第二十九章：Docker容器化部署

> **本章目标**：掌握使用Docker容器化部署直播系统，包括镜像构建、网络配置和服务编排。

容器化是现代应用部署的标准方式。本章介绍如何将SFU/MCU等直播组件打包为Docker镜像并部署。

---

## 目录

1. [Docker基础](#1-docker基础)
2. [SFU/MCU镜像构建](#2-sfumcu镜像构建)
3. [Docker网络配置](#3-docker网络配置)
4. [Docker Compose编排](#4-docker-compose编排)
5. [生产环境配置](#5-生产环境配置)
6. [本章总结](#6-本章总结)

---

## 1. Docker基础

### 1.1 核心概念

| 概念 | 说明 | 类比 |
|:---|:---|:---|
| **镜像 (Image)** | 应用运行环境模板 | 类 |
| **容器 (Container)** | 镜像的运行实例 | 对象 |
| **仓库 (Registry)** | 存储和分发镜像 | GitHub |
| **Dockerfile** | 定义镜像构建步骤 | Makefile |
| **Volume** | 持久化数据存储 | 外部硬盘 |

### 1.2 Dockerfile最佳实践

```dockerfile
# SFU Dockerfile
FROM ubuntu:22.04

# 避免交互式提示
ENV DEBIAN_FRONTEND=noninteractive

# 安装依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    && rm -rf /var/lib/apt/lists/*

# 创建应用目录
WORKDIR /app

# 复制源码
COPY src/ ./src/
COPY CMakeLists.txt .

# 编译
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc)

# 运行时阶段（多阶段构建，减小镜像）
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    libavcodec58 \
    libavformat58 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 从构建阶段复制二进制
COPY --from=0 /app/build/sfu_server .
COPY --from=0 /app/build/config.yaml ./config/

# 暴露端口
EXPOSE 8080/tcp  # HTTP API
EXPOSE 3478/tcp  # TURN TCP
EXPOSE 3478/udp  # TURN UDP
EXPOSE 10000-20000/udp  # RTP端口范围

# 健康检查
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# 运行
ENTRYPOINT ["./sfu_server"]
CMD ["-c", "config/config.yaml"]
```

### 1.3 镜像构建与推送

```bash
# 构建镜像
docker build -t your-registry/sfu-server:v1.0.0 .

# 本地测试运行
docker run -d \
  --name sfu \
  -p 8080:8080 \
  -p 3478:3478/tcp \
  -p 3478:3478/udp \
  -p 10000-10100:10000-10100/udp \
  your-registry/sfu-server:v1.0.0

# 推送镜像到仓库
docker login your-registry.com
docker push your-registry/sfu-server:v1.0.0
```

---

## 2. SFU/MCU镜像构建

### 2.1 精简镜像技巧

```dockerfile
# 使用更小的基础镜像
FROM alpine:3.18 AS runtime

# Alpine使用musl libc，体积更小
RUN apk add --no-cache \
    libstdc++ \
    openssl \
    ca-certificates

# 或者使用distroless（Google维护的最小镜像）
FROM gcr.io/distroless/cc-debian11

# distroless没有shell，更安全
COPY --from=builder /app/sfu_server /
ENTRYPOINT ["/sfu_server"]
```

**镜像大小对比**：
| 基础镜像 | 最终大小 | 特点 |
|:---|:---:|:---|
| ubuntu:22.04 | ~200MB | 功能完整 |
| alpine:3.18 | ~50MB | 轻量 |
| distroless | ~30MB | 最小攻击面 |

### 2.2 多服务镜像

```dockerfile
# 多服务统一部署
FROM ubuntu:22.04

WORKDIR /app

# 复制所有服务
COPY sfu/sfu_server ./
COPY mcu/mcu_server ./
COPY signaling/signaling_server ./
COPY dashboard/dashboard ./

# 复制启动脚本
COPY scripts/start.sh ./
RUN chmod +x start.sh

# 环境变量指定启动哪个服务
ENV SERVICE_TYPE=sfu

EXPOSE 8080 8081 8082 3000

ENTRYPOINT ["./start.sh"]
```

```bash
# start.sh
#!/bin/bash

case $SERVICE_TYPE in
    sfu)
        exec ./sfu_server "$@"
        ;;
    mcu)
        exec ./mcu_server "$@"
        ;;
    signaling)
        exec ./signaling_server "$@"
        ;;
    dashboard)
        exec ./dashboard "$@"
        ;;
    *)
        echo "Unknown service type: $SERVICE_TYPE"
        exit 1
        ;;
esac
```

---

## 3. Docker网络配置

### 3.1 网络模式选择

| 模式 | 说明 | 适用场景 |
|:---|:---|:---|
| bridge | 默认私有网络 | 单机多容器通信 |
| host | 共享主机网络 | 高性能网络要求 |
| overlay | 跨主机网络 | Swarm/K8s集群 |
| macvlan | 直接接入物理网络 | 需要独立IP |

```bash
# 创建自定义网络
docker network create \
  --driver bridge \
  --subnet=172.20.0.0/16 \
  --gateway=172.20.0.1 \
  live-network

# 运行容器并加入网络
docker run -d \
  --name sfu \
  --network live-network \
  --ip 172.20.0.10 \
  sfu-server:v1.0.0
```

### 3.2 RTP端口映射

```bash
# 方式1: 端口范围映射（简单但效率低）
docker run -d \
  -p 10000-20000:10000-20000/udp \
  sfu-server

# 方式2: host网络（性能最好）
docker run -d \
  --network host \
  sfu-server

# 方式3: 使用docker-compose动态配置
```

---

## 4. Docker Compose编排

### 4.1 docker-compose.yml

```yaml
version: '3.8'

services:
  # SFU服务
  sfu:
    build:
      context: ./sfu
      dockerfile: Dockerfile
    image: live-system/sfu:latest
    container_name: live-sfu
    restart: unless-stopped
    ports:
      - "8080:8080"      # HTTP API
      - "3478:3478/tcp"  # TURN TCP
      - "3478:3478/udp"  # TURN UDP
      - "10000-10100:10000-10100/udp"  # RTP
    environment:
      - SERVICE_TYPE=sfu
      - LOG_LEVEL=info
      - TURN_REALM=live.example.com
      - TURN_SHARED_SECRET=${TURN_SECRET}
    volumes:
      - ./config/sfu.yaml:/app/config/config.yaml:ro
      - sfu-logs:/app/logs
    networks:
      - live-net
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 40s
    deploy:
      resources:
        limits:
          cpus: '4.0'
          memory: 4G
        reservations:
          cpus: '2.0'
          memory: 2G

  # MCU服务
  mcu:
    build:
      context: ./mcu
      dockerfile: Dockerfile
    image: live-system/mcu:latest
    container_name: live-mcu
    restart: unless-stopped
    ports:
      - "8081:8081"
    environment:
      - SERVICE_TYPE=mcu
      - GPU_ENABLED=true
    volumes:
      - ./config/mcu.yaml:/app/config/config.yaml:ro
      - mcu-logs:/app/logs
    networks:
      - live-net
    # GPU支持（需要nvidia-docker）
    runtime: nvidia
    environment:
      - NVIDIA_VISIBLE_DEVICES=all
    deploy:
      resources:
        limits:
          cpus: '8.0'
          memory: 16G

  # 信令服务
  signaling:
    build:
      context: ./signaling
      dockerfile: Dockerfile
    image: live-system/signaling:latest
    container_name: live-signaling
    restart: unless-stopped
    ports:
      - "8082:8082"
    environment:
      - SERVICE_TYPE=signaling
      - REDIS_URL=redis://redis:6379
      - DATABASE_URL=postgres://postgres:${DB_PASSWORD}@postgres:5432/live
    depends_on:
      - redis
      - postgres
    volumes:
      - ./config/signaling.yaml:/app/config/config.yaml:ro
    networks:
      - live-net

  # 录制服务
  recorder:
    build:
      context: ./recorder
      dockerfile: Dockerfile
    image: live-system/recorder:latest
    container_name: live-recorder
    restart: unless-stopped
    environment:
      - SERVICE_TYPE=recorder
      - STORAGE_PATH=/recordings
    volumes:
      - ./recordings:/recordings
      - ./config/recorder.yaml:/app/config/config.yaml:ro
    networks:
      - live-net

  # Redis缓存
  redis:
    image: redis:7-alpine
    container_name: live-redis
    restart: unless-stopped
    volumes:
      - redis-data:/data
    networks:
      - live-net

  # PostgreSQL数据库
  postgres:
    image: postgres:15-alpine
    container_name: live-postgres
    restart: unless-stopped
    environment:
      - POSTGRES_DB=live
      - POSTGRES_USER=postgres
      - POSTGRES_PASSWORD=${DB_PASSWORD}
    volumes:
      - postgres-data:/var/lib/postgresql/data
    networks:
      - live-net

  # Nginx反向代理
  nginx:
    image: nginx:alpine
    container_name: live-nginx
    restart: unless-stopped
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx/nginx.conf:/etc/nginx/nginx.conf:ro
      - ./nginx/ssl:/etc/nginx/ssl:ro
      - ./nginx/html:/usr/share/nginx/html:ro
    depends_on:
      - sfu
      - signaling
    networks:
      - live-net

volumes:
  sfu-logs:
  mcu-logs:
  redis-data:
  postgres-data:

networks:
  live-net:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.0.0/16
```

### 4.2 环境变量配置

```bash
# .env文件
TURN_SECRET=your-turn-secret-key-here
DB_PASSWORD=your-database-password
REDIS_PASSWORD=your-redis-password

# Let's Encrypt证书
SSL_CERT_PATH=/etc/letsencrypt/live/your-domain.com/fullchain.pem
SSL_KEY_PATH=/etc/letsencrypt/live/your-domain.com/privkey.pem
```

### 4.3 常用命令

```bash
# 启动所有服务
docker-compose up -d

# 查看日志
docker-compose logs -f sfu

# 重启服务
docker-compose restart sfu

# 停止所有服务
docker-compose down

# 完全清理（包括数据卷）
docker-compose down -v

# 扩展SFU实例
docker-compose up -d --scale sfu=3

# 更新镜像并重启
docker-compose pull
docker-compose up -d
```

---

## 5. 生产环境配置

### 5.1 日志收集

```yaml
# docker-compose.logging.yml
version: '3.8'

services:
  # Fluentd日志收集
  fluentd:
    build: ./fluentd
    volumes:
      - ./fluentd/conf:/fluentd/etc
      - /var/lib/docker/containers:/var/lib/docker/containers:ro
    ports:
      - "24224:24224"
      - "24224:24224/udp"
    networks:
      - live-net

  # Elasticsearch
  elasticsearch:
    image: elasticsearch:8.5.0
    environment:
      - discovery.type=single-node
      - xpack.security.enabled=false
    volumes:
      - es-data:/usr/share/elasticsearch/data
    networks:
      - live-net

  # Kibana可视化
  kibana:
    image: kibana:8.5.0
    ports:
      - "5601:5601"
    environment:
      - ELASTICSEARCH_HOSTS=http://elasticsearch:9200
    depends_on:
      - elasticsearch
    networks:
      - live-net
```

### 5.2 监控告警

```yaml
# 添加Prometheus监控
  prometheus:
    image: prom/prometheus:latest
    volumes:
      - ./prometheus/prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus-data:/prometheus
    ports:
      - "9090:9090"
    networks:
      - live-net

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    volumes:
      - grafana-data:/var/lib/grafana
      - ./grafana/dashboards:/etc/grafana/provisioning/dashboards:ro
    networks:
      - live-net
```

### 5.3 自动更新

```bash
#!/bin/bash
# deploy.sh - 自动部署脚本

set -e

# 拉取最新代码
git pull origin master

# 重新构建镜像
docker-compose build

# 滚动更新（零停机）
docker-compose up -d --no-deps --scale sfu=2 sfu
docker-compose up -d --no-deps --scale sfu=1 sfu
docker-compose up -d

# 健康检查
sleep 10
if ! curl -f http://localhost:8080/health; then
    echo "Health check failed! Rolling back..."
    git revert HEAD
    docker-compose up -d
    exit 1
fi

echo "Deployment successful!"
```

---

## 6. 本章总结

### 6.1 容器化要点

| 方面 | 最佳实践 |
|:---|:---|
| 镜像构建 | 多阶段构建、选择合适基础镜像 |
| 网络配置 | 根据场景选择bridge/host/overlay |
| 数据持久化 | 使用Volume管理配置和日志 |
| 服务编排 | Docker Compose管理多服务依赖 |
| 生产部署 | 健康检查、资源限制、日志收集 |

### 6.2 课后思考

1. **镜像优化**：如何进一步减小Docker镜像体积？分析各层的作用。

2. **网络选择**：SFU使用host网络性能最好，但失去了容器隔离性。如何权衡？

3. **热更新**：如何在不中断服务的情况下更新SFU代码？

4. **多机房部署**：当用户分布在多个地区时，如何设计容器化部署架构？
