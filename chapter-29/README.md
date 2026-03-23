# 第二十九章：Docker容器化部署

> **本章目标**：掌握使用Docker容器化部署直播系统，包括镜像构建、网络配置和服务编排。

容器化是现代应用部署的标准方式。本章介绍如何将SFU/MCU等直播组件打包为Docker镜像并部署。

---

## 目录

1. [Docker基础](#1-docker基础)
2. [容器vs虚拟机详细对比](#2-容器vs虚拟机详细对比)
3. [Docker镜像分层原理](#3-docker镜像分层原理)
4. [SFU/MCU镜像构建](#4-sfumcu镜像构建)
5. [Docker网络模型详解](#5-docker网络模型详解)
6. [持久化存储原理](#6-持久化存储原理)
7. [Docker Compose编排](#7-docker-compose编排)
8. [生产环境配置](#8-生产环境配置)
9. [本章总结](#9-本章总结)

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

## 2. 容器vs虚拟机详细对比

### 2.1 架构差异

**传统虚拟机架构**：
```
┌─────────────────────────────────────────────┐
│  VM 1: Ubuntu          VM 2: CentOS        │
│  ┌─────────────┐       ┌─────────────┐     │
│  │ App A       │       │ App B       │     │
│  │ Bin/Libs    │       │ Bin/Libs    │     │
│  ├─────────────┤       ├─────────────┤     │
│  │ Guest OS    │       │ Guest OS    │     │
│  │ (内核+系统)  │       │ (内核+系统)  │     │
│  └─────────────┘       └─────────────┘     │
├─────────────────────────────────────────────┤
│  Hypervisor (VMware/KVM/Hyper-V)            │
├─────────────────────────────────────────────┤
│  Host OS (Linux/Windows)                    │
├─────────────────────────────────────────────┤
│  Hardware                                   │
└─────────────────────────────────────────────┘
```

**Docker容器架构**：
```
┌─────────────────────────────────────────────┐
│  Container 1            Container 2         │
│  ┌─────────────┐       ┌─────────────┐     │
│  │ App A       │       │ App B       │     │
│  │ Bin/Libs    │       │ Bin/Libs    │     │
│  └─────────────┘       └─────────────┘     │
│                                             │
│  ┌─────────────────────────────────────────┐│
│  │ Docker Engine (容器运行时管理)           ││
│  │  • 进程隔离 (Namespace)                  ││
│  │  • 资源限制 (Cgroup)                     ││
│  │  • 文件系统隔离 (UnionFS)                ││
│  └─────────────────────────────────────────┘│
├─────────────────────────────────────────────┤
│  Host OS (共享内核)                          │
│  ┌─────────────────────────────────────────┐│
│  │ Kernel (进程调度、内存管理、网络栈、文件系统)││
│  └─────────────────────────────────────────┘│
├─────────────────────────────────────────────┤
│  Hardware                                   │
└─────────────────────────────────────────────┘
```

### 2.2 资源占用对比

| 维度 | 虚拟机 | 容器 | 容器优势 |
|:---|:---:|:---:|:---|
| **启动时间** | 分钟级 (30s-2min) | 秒级 (1-5s) | 快速扩缩容 |
| **磁盘占用** | GB级 (5-20GB) | MB级 (50-500MB) | 存储效率高 |
| **内存开销** | 高（完整OS） | 低（共享内核） | 单机可运行更多实例 |
| **CPU开销** | 5-15%（虚拟化） | 接近原生 | 性能更好 |
| **密度** | 10-100/主机 | 100-1000/主机 | 资源利用率高 |

```
资源占用对比（运行相同应用）：

虚拟机部署:
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│   Guest OS 2GB  │ │   Guest OS 2GB  │ │   Guest OS 2GB  │
│   + App 100MB   │ │   + App 100MB   │ │   + App 100MB   │
│   = 2.1GB       │ │   = 2.1GB       │ │   = 2.1GB       │
└─────────────────┘ └─────────────────┘ └─────────────────┘
总计: 6.3GB内存 + 3个完整内核

容器部署:
┌─────────────────────────────────────────────────────────┐
│  Host OS Kernel (共享)                                  │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐             │
│  │ App 100MB │ │ App 100MB │ │ App 100MB │  (隔离)      │
│  │ + Libs    │ │ + Libs    │ │ + Libs    │             │
│  │ = 150MB   │ │ = 150MB   │ │ = 150MB   │             │
│  └───────────┘ └───────────┘ └───────────┘             │
└─────────────────────────────────────────────────────────┘
总计: ~450MB内存 + 1个内核
```

### 2.3 隔离性对比

| 隔离维度 | 虚拟机 | 容器 | 说明 |
|:---|:---:|:---:|:---|
| **进程隔离** | 强（独立内核） | 中（Namespace） | 容器共享宿主机内核 |
| **文件系统隔离** | 完整 | 完整（UnionFS） | 两者都完整 |
| **网络隔离** | 虚拟网卡 | 虚拟网卡/桥接 | 两者都支持 |
| **资源限制** | 硬限制 | Cgroup软/硬限制 | 容器更灵活 |
| **安全边界** | 强（硬件级） | 中（软件级） | 虚拟机逃逸更难 |

### 2.4 适用场景对比

| 场景 | 推荐 | 理由 |
|:---|:---:|:---|
| **多租户严格隔离** | VM | 更强的安全边界 |
| **不同操作系统需求** | VM | 容器共享宿主机内核 |
| **快速CI/CD** | 容器 | 秒级启动，镜像版本控制 |
| **微服务部署** | 容器 | 轻量、高密度、编排友好 |
| **遗留系统迁移** | VM | 完整OS环境兼容 |
| **云原生应用** | 容器 | 与K8s等编排平台无缝集成 |

---

## 3. Docker镜像分层原理

### 3.1 UnionFS与分层存储

**镜像分层的核心概念**：

Docker镜像由多个只读层（Layer）组成，容器运行时在顶部添加一个可写层（Container Layer）：

```
容器运行时视图:
┌─────────────────────────────────────┐
│  Container Layer (可写层)           │  ← 容器内修改都在这里
│  （写时复制 Copy-on-Write）          │
├─────────────────────────────────────┤
│  Layer 3: ADD app.py /app/          │  ← 应用代码层
├─────────────────────────────────────┤
│  Layer 2: RUN pip install -r ...    │  ← 依赖层
├─────────────────────────────────────┤
│  Layer 1: RUN apt-get install ...   │  ← 系统工具层
├─────────────────────────────────────┤
│  Layer 0: FROM ubuntu:20.04         │  ← 基础镜像层
└─────────────────────────────────────┘

所有层合并后的视图:
/                   ← 文件系统根
├── app/
│   └── app.py      ← Layer 3
├── usr/
│   ├── bin/
│   │   └── python3 ← Layer 2
│   └── lib/...     ← Layer 2
├── bin/...         ← Layer 1
└── etc/...         ← Layer 0
```

### 3.2 写时复制（Copy-on-Write）

**CoW机制工作原理**：

```
场景: 修改基础镜像中的文件 /etc/nginx/nginx.conf

1. 读取阶段:
   容器请求读取 /etc/nginx/nginx.conf
   ↓
   从Layer 0读取（只读）

2. 写入阶段（首次修改）:
   容器请求修改 /etc/nginx/nginx.conf
   ↓
   CoW触发:
   a) 从Layer 0复制文件到Container Layer
   b) 在Container Layer中修改
   c) 后续读写都从Container Layer进行

3. 删除文件:
   容器请求删除 /etc/nginx/nginx.conf
   ↓
   在Container Layer中创建"删除标记"
   （实际文件仍然存在于镜像层，只是被隐藏）
```

### 3.3 分层缓存优化

**Dockerfile指令与层的关系**：

```dockerfile
# 每条指令创建一个新层
FROM ubuntu:20.04              # Layer 0: 基础镜像

RUN apt-get update && \        # Layer 1
    apt-get install -y python3 # 系统依赖

WORKDIR /app                   # 元数据层（通常不计入）

COPY requirements.txt .        # Layer 2: 依赖列表
RUN pip install -r ...         # Layer 3: Python依赖

COPY src/ .                    # Layer 4: 应用代码

CMD ["python", "app.py"]       # 元数据层
```

**缓存优化最佳实践**：

| 原则 | 说明 | 示例 |
|:---|:---|:---|
| **不常变的放上层** | 基础依赖先安装 | FROM → RUN apt → ... |
| **常变的放下层** | 代码最后复制 | COPY src 放最后 |
| **合并RUN指令** | 减少层数 | RUN apt-get ... && ... |
| **精确COPY** | 只复制必要文件 | COPY requirements.txt 而非 . |

```dockerfile
# 优化前（缓存效率低）
FROM ubuntu:20.04
COPY . /app           # 代码变更会 invalidated 所有后续层
RUN apt-get update && apt-get install -y python3
RUN pip install -r /app/requirements.txt
RUN apt-get install -y ffmpeg

# 优化后（缓存效率高）
FROM ubuntu:20.04
# 1. 系统依赖（很少变化）
RUN apt-get update && apt-get install -y \
    python3 python3-pip ffmpeg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 2. Python依赖（偶尔变化）
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# 3. 应用代码（频繁变化）
COPY src/ ./src/

CMD ["python", "-m", "src.main"]
```

### 3.4 镜像大小优化

**多阶段构建**：

```dockerfile
# 构建阶段（包含编译工具，体积大）
FROM gcc:11 AS builder
WORKDIR /build
COPY src/ ./
RUN make -j$(nproc)  # 编译

# 运行阶段（仅包含运行时依赖，体积小）
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 只复制编译结果
COPY --from=builder /build/sfu_server .
COPY config.yaml ./

EXPOSE 8080 3478/udp
CMD ["./sfu_server"]
```

**镜像大小对比**：

| 构建方式 | 基础镜像 | 最终大小 | 说明 |
|:---|:---|:---:|:---|
| 单阶段 | gcc:11 | ~2GB | 包含完整编译环境 |
| 多阶段 | ubuntu:22.04 | ~100MB | 仅运行时依赖 |
| 多阶段+alpine | alpine:latest | ~50MB | 最小化基础镜像 |
| 多阶段+distroless | gcr.io/distroless | ~30MB | Google最小镜像 |

---

## 4. SFU/MCU镜像构建

### 4.1 精简镜像技巧

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

### 4.2 多服务镜像

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

## 5. Docker网络模型详解

### 5.1 Linux网络基础

**容器的网络本质**：

Docker利用Linux内核的网络特性实现容器网络：

```
┌─────────────────────────────────────────────────────────┐
│  Namespace (网络隔离)                                    │
│  每个容器拥有独立的：                                       │
│  • 网络接口 (eth0, lo)                                    │
│  • 路由表                                                  │
│  • iptables规则                                            │
│  • Socket端口空间                                          │
└─────────────────────────────────────────────────────────┘

容器网络 ↔ 宿主机网络的连接方式:
┌─────────────────────────────────────────────────────────┐
│  容器A Namespace          容器B Namespace   宿主机        │
│  ┌─────────┐              ┌─────────┐                   │
│  │ veth0   │──────────────│ veth1   │                   │
│  │ 172.17.0.2            │ 172.17.0.3                   │
│  └────┬────┘              └────┬────┘                   │
│       │                        │                        │
│       └──────────┬─────────────┘                        │
│                  │                                      │
│            ┌─────┴─────┐                                │
│            │  docker0  │ ← Linux Bridge (虚拟交换机)      │
│            │ 172.17.0.1│                                │
│            └─────┬─────┘                                │
│                  │                                      │
│            ┌─────┴─────┐                                │
│            │  eth0     │ ← 物理网卡                      │
│            │ 10.0.0.5  │                                │
│            └───────────┘                                │
└─────────────────────────────────────────────────────────┘
```

**核心网络组件**：

| 组件 | 作用 | Docker应用 |
|:---|:---|:---|
| **Network Namespace** | 网络隔离 | 每个容器独立网络栈 |
| **veth pair** | 虚拟网卡对 | 容器与bridge的连接 |
| **Linux Bridge** | 虚拟交换机 | docker0连接容器 |
| **iptables** | 包过滤/NAT | 端口映射、网络隔离 |
| **路由表** | 包转发决策 | 容器间、容器外通信 |

### 5.2 Docker网络模式

**五种网络模式详解**：

| 模式 | 网络配置 | 适用场景 | 隔离性 |
|:---|:---|:---|:---:|
| **bridge** | 独立IP，通过NAT访问外网 | 单机多容器 | 高 |
| **host** | 共享宿主机网络栈 | 高性能网络 | 无 |
| **none** | 无网络 | 安全隔离 | 最高 |
| **container** | 共享其他容器网络栈 | Sidecar模式 | 共享 |
| **overlay** | 跨主机虚拟网络 | Swarm/K8s集群 | 高 |

**Bridge模式（默认）**：

```
┌─────────────────────────────────────────────────────────┐
│  宿主机                                                   │
│  ┌─────────────┐     ┌─────────────┐                    │
│  │  Container A│     │  Container B│                    │
│  │  172.17.0.2 │     │  172.17.0.3 │                    │
│  │  port 8080  │     │  port 8080  │ ← 容器内端口        │
│  └──────┬──────┘     └──────┬──────┘                    │
│         │                   │                           │
│         └─────────┬─────────┘                           │
│                   │                                     │
│            ┌──────┴──────┐                              │
│            │   docker0   │  172.17.0.1                   │
│            └──────┬──────┘                              │
│                   │                                     │
│            ┌──────┴──────┐                              │
│            │  MASQUERADE │ ← iptables NAT规则            │
│            │  port 8080  │ ← 映射到宿主机8080             │
│            └─────────────┘                              │
└─────────────────────────────────────────────────────────┘

命令:
docker run -d -p 8080:8080 sfu-server  # 映射宿主机8080到容器8080
```

**Host模式**：

```
┌─────────────────────────────────────────────────────────┐
│  宿主机网络栈（与容器共享）                                  │
│  ┌─────────────┐     ┌─────────────┐                    │
│  │  Container A│     │  Container B│                    │
│  │  10.0.0.5   │     │  10.0.0.5   │ ← 使用宿主机IP      │
│  │  port 8080  │     │  port 8081  │ ← 直接使用宿主机端口  │
│  └─────────────┘     └─────────────┘                    │
│  注意: 端口冲突需要手动避免                                   │
└─────────────────────────────────────────────────────────┘

命令:
docker run -d --network host sfu-server  # 无端口映射，直接使用宿主机端口
```

**SFU推荐网络配置**：

```bash
# 方案1: Bridge + 端口映射（一般场景）
docker run -d \
  --name sfu \
  --network bridge \
  -p 8080:8080 \
  -p 3478:3478/tcp \
  -p 3478:3478/udp \
  -p 10000-11000:10000-11000/udp \
  sfu-server

# 方案2: Host网络（高性能场景，避免NAT开销）
docker run -d \
  --name sfu \
  --network host \
  sfu-server

# 方案3: 自定义网络（容器间通信）
docker network create live-network
docker run -d --network live-network --name sfu sfu-server
docker run -d --network live-network --name signaling signaling-server
# 容器间可通过容器名直接通信: http://signaling:8082
```

### 5.3 自定义网络与DNS

**Docker内置DNS**：

```
在自定义网络中，Docker提供内置DNS：

容器A (sfu) ←──DNS查询──→ Docker DNS (127.0.0.11) ←──→ 容器IP
                                               ↓
容器B (signaling) 查询 "sfu" → 返回 172.20.0.2

无需 --link，直接使用容器名作为hostname
```

```bash
# 创建自定义网络
docker network create \
  --driver bridge \
  --subnet=172.20.0.0/16 \
  --gateway=172.20.0.1 \
  live-network

# 查看网络配置
docker network inspect live-network

# 运行容器并指定固定IP
docker run -d \
  --name sfu \
  --network live-network \
  --ip 172.20.0.10 \
  sfu-server
```

---

## 6. 持久化存储原理

### 6.1 容器存储的问题

**容器层的特性**：

| 特性 | 说明 | 影响 |
|:---|:---|:---|
| **写入时复制** | 写操作发生在容器层 | 性能比原生低 |
| **容器删除即丢失** | 容器层随容器删除 | 数据持久化问题 |
| **联合文件系统** | 多层合并视图 | 不适合高频IO |

```
容器存储问题示意:

┌─────────────────────────────────────────┐
│  Container Layer (可写，但...)           │
│  • 性能较低 (CoW开销)                    │
│  • 容器删除 → 数据丢失                   │
├─────────────────────────────────────────┤
│  Image Layers (只读)                    │
│  • 配置文件不可修改                      │
│  • 日志无法持久化                        │
└─────────────────────────────────────────┘
```

### 6.2 Volume绑定挂载

**Volume的三种类型**：

| 类型 | 创建方式 | 存储位置 | 适用场景 |
|:---|:---|:---|:---|
| **Named Volume** | `docker volume create` | /var/lib/docker/volumes | 数据持久化 |
| **Bind Mount** | `-v /host:/container` | 宿主机任意路径 | 开发调试 |
| **tmpfs Mount** | `--tmpfs` | 宿主机内存 | 敏感临时数据 |

```yaml
# docker-compose中的存储配置
version: '3.8'
services:
  sfu:
    image: sfu-server
    volumes:
      # Named Volume - 生产推荐
      - sfu-logs:/app/logs
      - sfu-data:/app/data
      
      # Bind Mount - 开发调试
      - ./config/sfu.yaml:/app/config.yaml:ro
      - /etc/localtime:/etc/localtime:ro
      
      # tmpfs - 临时缓存，容器重启清空
      - type: tmpfs
        target: /app/cache
        tmpfs:
          size: 100M

volumes:
  sfu-logs:
    driver: local
  sfu-data:
    driver: local
```

### 6.3 直播系统存储方案

```yaml
version: '3.8'
services:
  sfu:
    volumes:
      # 配置文件 - 只读绑定
      - ./config/sfu.yaml:/app/config/config.yaml:ro
      
      # 日志文件 - 持久化Volume
      - sfu-logs:/app/logs
      
      # 录制文件（如SFU支持录制）- 大容量Volume
      - type: bind
        source: /data/recordings
        target: /app/recordings
  
  recorder:
    volumes:
      # 录制服务 - 大容量存储
      - recordings:/recordings
      
      # 临时转码文件 - tmpfs加速
      - type: tmpfs
        target: /tmp/transcode
        tmpfs:
          size: 2G
  
  redis:
    volumes:
      # 缓存数据 - 需要持久化
      - redis-data:/data

volumes:
  # 日志（定期清理）
  sfu-logs:
    driver: local
  
  # 录制文件（大容量，可能使用外部存储）
  recordings:
    driver: local
    driver_opts:
      type: nfs
      o: addr=nfs-server,rw
      device: ":/exports/recordings"
  
  # Redis数据
  redis-data:
    driver: local
```

### 6.4 存储性能优化

| 优化策略 | 方法 | 效果 |
|:---|:---|:---|
| **避免频繁写容器层** | 将日志写到Volume | 提升IO性能 |
| **使用volume cache模式** | `consistent/delegated/cached` | 优化一致性开销 |
| **分离读写的存储** | 配置文件ro，数据rw | 减少CoW |
| **外部存储** | NFS/Ceph/S3 | 扩展容量，跨节点共享 |

```bash
# 性能测试: Bind Mount vs Volume
docker run --rm -v /tmp/data:/data alpine sh -c "time dd if=/dev/zero of=/data/test bs=1M count=100"
docker run --rm -v test-volume:/data alpine sh -c "time dd if=/dev/zero of=/data/test bs=1M count=100"
```

---

## 7. Docker Compose编排

### 7.1 完整docker-compose.yml

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

## 8. 生产环境配置

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

## 9. 本章总结

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
