# 第三十章：Kubernetes生产部署

> **本章目标**：掌握使用Kubernetes部署直播系统，包括有状态服务管理、自动扩缩容和服务网格。

Kubernetes已成为云原生应用部署的事实标准。本章介绍如何在K8s上部署完整的直播系统。

---

## 目录

1. [K8s核心概念](#1-k8s核心概念)
2. [Pod生命周期详解](#2-pod生命周期详解)
3. [控制器模式](#3-控制器模式)
4. [服务发现机制](#4-服务发现机制)
5. [存储卷类型详解](#5-存储卷类型详解)
6. [调度算法简介](#6-调度算法简介)
7. [SFU有状态部署](#7-sfu有状态部署)
8. [自动扩缩容](#8-自动扩缩容)
9. [服务网格](#9-服务网格)
10. [生产最佳实践](#10-生产最佳实践)
11. [本章总结](#11-本章总结)

---

## 1. K8s核心概念

### 1.1 核心资源

| 资源 | 作用 | 直播系统应用 |
|:---|:---|:---|
| **Pod** | 最小部署单元 | SFU/MCU进程 |
| **Deployment** | 无状态应用管理 | 信令服务、Dashboard |
| **StatefulSet** | 有状态应用管理 | SFU（固定网络标识）|
| **Service** | 服务发现和负载均衡 | 暴露SFU端口 |
| **Ingress** | HTTP路由 | API网关 |
| **ConfigMap/Secret** | 配置管理 | 配置文件、密钥 |
| **PV/PVC** | 持久化存储 | 录制文件存储 |

### 1.2 架构图

```
┌─────────────────────────────────────────────────┐
│                   Ingress                        │
│         (API网关 / 负载均衡)                      │
└──────────────────┬──────────────────────────────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
┌───┴───┐    ┌────┴────┐   ┌─────┴────┐
│Service│    │ Service │   │  Service │
│信令   │    │  SFU-0  │   │  SFU-1   │
└───┬───┘    └────┬────┘   └─────┬────┘
    │             │              │
┌───┴───┐    ┌────┴────┐   ┌─────┴────┐
│ Pod   │    │ Pod     │   │ Pod      │
└───────┘    │ SFU-0   │   │ SFU-1    │
             │(固定IP) │   │(固定IP)  │
             └─────────┘   └──────────┘
```

---

## 2. Pod生命周期详解

### 2.1 Pod生命周期阶段

Pod从创建到终止经历以下阶段：

```
创建 → Pending → Running → Succeeded/Failed → Terminating → 删除
         │           │           │                │
         │           │           │                └─ 优雅关闭期
         │           │           └─ 正常完成或失败
         │           └─ 至少一个容器运行中
         └─ 调度中/镜像拉取中/卷挂载中
```

**详细生命周期**：

| 阶段 | 状态 | 说明 | 常见原因 |
|:---|:---:|:---|:---|
| **Pending** | 等待中 | Pod已创建但未调度 | 资源不足、节点选择器不匹配 |
| **ContainerCreating** | 容器创建中 | 正在拉取镜像/创建容器 | 镜像拉取慢、卷挂载失败 |
| **Running** | 运行中 | 至少一个容器在运行 | 正常工作状态 |
| **Succeeded** | 成功完成 | 所有容器正常退出(Exit 0) | Job类任务完成 |
| **Failed** | 失败 | 有容器异常退出 | 应用崩溃、健康检查失败 |
| **Unknown** | 未知 | 无法获取Pod状态 | 节点失联 |
| **Terminating** | 终止中 | 正在优雅关闭 | 删除请求、缩容、节点维护 |

### 2.2 容器状态与重启策略

**容器状态转换**：

```
         Waiting
            │
            ▼
      ┌───────────┐    退出码0    ┌─────────┐
      │  Running  │ ────────────→ │Terminated│
      └───────────┘    退出码≠0   └─────────┘
            │                          │
            │                          │
            └────────── 重启 ──────────┘
            
根据restartPolicy决定:
- Always: 总是重启（默认）
- OnFailure: 非0退出码时重启
- Never: 不重启
```

**SFU的Pod配置示例**：

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: sfu-pod
  labels:
    app: sfu
spec:
  restartPolicy: Always  # SFU服务需要持续运行
  
  initContainers:        # 初始化容器，按顺序执行
  - name: init-config
    image: busybox
    command: ['sh', '-c', 'echo "Initializing..."']
    
  containers:
  - name: sfu
    image: sfu-server:v1.0.0
    
    # 生命周期钩子
    lifecycle:
      postStart:         # 启动后执行
        exec:
          command: ["/bin/sh", "-c", "echo SFU started > /tmp/log"]
      preStop:           # 停止前执行（优雅关闭）
        exec:
          command: ["/bin/sh", "-c", "sleep 10 && curl -X POST localhost:8080/drain"]
    
    # 健康检查
    livenessProbe:       # 存活性检查，失败则重启
      httpGet:
        path: /health
        port: 8080
      initialDelaySeconds: 30  # 启动后等待30秒开始检查
      periodSeconds: 10        # 每10秒检查一次
      timeoutSeconds: 5        # 超时5秒
      failureThreshold: 3      # 连续3次失败才判定为不健康
      
    readinessProbe:      # 就绪性检查，失败则从Service摘除
      httpGet:
        path: /ready
        port: 8080
      initialDelaySeconds: 5
      periodSeconds: 5
      successThreshold: 1      # 1次成功即就绪
      failureThreshold: 3
```

### 2.3 优雅关闭流程

**Pod删除时的优雅关闭**：

```
1. API Server接收删除请求，设置DeletionTimestamp
          │
          ▼
2. Pod状态变为Terminating
          │
          ▼
3. kubelet调用preStop钩子（如有）
   同步执行，必须在terminationGracePeriodSeconds内完成
          │
          ▼
4. kubelet发送SIGTERM给容器主进程
   容器应在terminationGracePeriodSeconds内完成清理
          │
          ▼
5. 超时后发送SIGKILL强制终止
          │
          ▼
6. Pod资源释放

默认terminationGracePeriodSeconds = 30秒
```

```yaml
# SFU优雅关闭配置
spec:
  terminationGracePeriodSeconds: 60  # SFU需要更长时间迁移连接
  
  containers:
  - name: sfu
    lifecycle:
      preStop:
        exec:
          command:
          - /bin/sh
          - -c
          - |
            # 1. 通知负载均衡器停止新连接
            curl -X POST localhost:8080/drain
            
            # 2. 等待现有连接完成
            sleep 30
            
            # 3. 强制关闭剩余连接
            curl -X POST localhost:8080/close-all
```

---

## 3. 控制器模式

### 3.1 声明式API与控制器循环

**Kubernetes的核心设计模式**：

```
         用户
          │ kubectl apply -f deployment.yaml
          ▼
    ┌─────────────┐
    │   Etcd      │ ← 期望状态存储
    │ (Desired)   │
    └──────┬──────┘
           │ watch
           ▼
    ┌─────────────┐     差异检测     ┌─────────────┐
    │ Controller  │ ←──────────────→ │  当前状态    │
    │  (控制循环)  │    调谐(Reconcile) │ (Actual)    │
    └──────┬──────┘                  └─────────────┘
           │ 创建/更新/删除资源
           ▼
    ┌─────────────┐
    │    Pod      │
    └─────────────┘
```

**控制器工作原理**：

1. **观察（Observe）**：监听资源变化
2. **差异分析（Diff）**：比较期望状态与当前状态
3. **调谐（Reconcile）**：执行操作使当前状态趋近期望状态
4. **重复（Repeat）**：持续循环

### 3.2 ReplicaSet原理

**ReplicaSet**：确保指定数量的Pod副本始终运行

```yaml
apiVersion: apps/v1
kind: ReplicaSet
metadata:
  name: signaling-rs
spec:
  replicas: 3              # 期望副本数
  selector:
    matchLabels:
      app: signaling       # 选择器，匹配Pod标签
  template:
    metadata:
      labels:
        app: signaling
    spec:
      containers:
      - name: signaling
        image: signaling:v1.0
```

**ReplicaSet的行为**：

| 场景 | 当前状态 | 控制器动作 |
|:---|:---:|:---|
| Pod崩溃 | 2/3运行 | 创建新Pod补充到3个 |
| 手动删除Pod | 2/3运行 | 创建新Pod补充到3个 |
| 节点故障 | 1/3运行（2个在故障节点） | 在其他节点创建2个新Pod |
| 缩容到2 | 3/2运行 | 删除1个Pod |
| 扩容到5 | 3/5运行 | 创建2个新Pod |

### 3.3 Deployment原理

**Deployment**：基于ReplicaSet，提供声明式更新和回滚

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: signaling
spec:
  replicas: 3
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 25%         # 更新时最多超出25%的Pod
      maxUnavailable: 25%   # 更新时最多不可用25%的Pod
  selector:
    matchLabels:
      app: signaling
  template:
    metadata:
      labels:
        app: signaling
    spec:
      containers:
      - name: signaling
        image: signaling:v1.1  # 更新镜像触发滚动更新
```

**滚动更新过程**：

```
初始状态: [Pod-v1] [Pod-v1] [Pod-v1]

第1步: 创建新Pod (maxSurge=1)
        [Pod-v1] [Pod-v1] [Pod-v1] [Pod-v1-new]

第2步: 删除旧Pod (保持maxUnavailable)
        [Pod-v1] [Pod-v1] [Pod-v1-new]

第3步: 继续创建新Pod
        [Pod-v1] [Pod-v1] [Pod-v1-new] [Pod-v1-new]

第4步: 删除旧Pod
        [Pod-v1] [Pod-v1-new] [Pod-v1-new]

... 重复直到全部更新

最终状态: [Pod-v1-new] [Pod-v1-new] [Pod-v1-new]
```

### 3.4 StatefulSet原理

**StatefulSet**：为每个Pod提供稳定标识的有状态服务管理

**与Deployment的关键区别**：

| 特性 | Deployment | StatefulSet |
|:---|:---|:---|
| **Pod命名** | 随机哈希 | 有序序号（-0, -1, -2）|
| **启动顺序** | 同时 | 有序（0→1→2）|
| **停止顺序** | 同时 | 逆序（2→1→0）|
| **网络标识** | 临时IP | 稳定的DNS名 |
| **存储** | 共享/临时 | 独立的PVC绑定 |
| **缩容** | 随机删除 | 从高序号删除 |

**SFU StatefulSet配置**：

```yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: sfu
spec:
  serviceName: sfu-headless  # Headless Service名称
  replicas: 3
  podManagementPolicy: OrderedReady  # 有序管理
  
  selector:
    matchLabels:
      app: sfu
      
  template:
    metadata:
      labels:
        app: sfu
    spec:
      containers:
      - name: sfu
        image: sfu-server:v1.0.0
        env:
        - name: POD_NAME
          valueFrom:
            fieldRef:
              fieldPath: metadata.name
        # Pod名称：sfu-0, sfu-1, sfu-2
        # DNS名称：sfu-0.sfu-headless.live.svc.cluster.local
        
  volumeClaimTemplates:  # 每个Pod独立的PVC
  - metadata:
      name: sfu-data
    spec:
      accessModes: ["ReadWriteOnce"]
      resources:
        requests:
          storage: 10Gi
```

**StatefulSet的PVC绑定**：

```
sfu-0 → PVC: sfu-data-sfu-0 → PV: pv-001
sfu-1 → PVC: sfu-data-sfu-1 → PV: pv-002  
sfu-2 → PVC: sfu-data-sfu-2 → PV: pv-003

Pod删除重建后，PVC重新绑定到相同的PV
保证数据不丢失，网络标识不变
```

---

## 4. 服务发现机制

### 4.1 DNS服务发现

**Kubernetes DNS架构**：

```
Pod A (signaling-xxx)                Pod B (sfu-0)
    │                                      │
    │ 查询: sfu-0.sfu-headless           │
    │ ─────────────────────────────────────→
    │                                      │
    │ ← 返回: 10.244.1.5                   │
    │                                      │
    │ 直接通信                              │
    └──────────────────────────────────────→

DNS解析流程:
1. Pod内查询 /etc/resolv.conf
2. nameserver指向Cluster DNS (kube-dns/CoreDNS)
3. DNS服务器查询Endpoint对象
4. 返回对应Pod IP
```

**DNS记录格式**：

| 记录类型 | 格式 | 示例 |
|:---|:---|:---|
| **Service A记录** | `service.namespace.svc.cluster.local` | `signaling.live.svc.cluster.local` |
| **Headless SRV** | `pod.service.namespace.svc.cluster.local` | `sfu-0.sfu-headless.live.svc.cluster.local` |
| **Pod A记录** | `pod-ip.namespace.pod.cluster.local` | `10-244-1-5.live.pod.cluster.local` |

### 4.2 Endpoints与EndpointSlice

**Endpoints**：Service后端Pod的IP:Port列表

```yaml
# Service定义
apiVersion: v1
kind: Service
metadata:
  name: signaling
spec:
  selector:
    app: signaling  # 选择标签为app=signaling的Pod
  ports:
  - port: 8080

# 自动生成的Endpoints
apiVersion: v1
kind: Endpoints
metadata:
  name: signaling
subsets:
- addresses:
  - ip: 10.244.1.3    # signaling-pod-1
    nodeName: node-1
  - ip: 10.244.2.5    # signaling-pod-2
    nodeName: node-2
  - ip: 10.244.3.7    # signaling-pod-3
    nodeName: node-3
  ports:
  - port: 8080
```

**Service与Pod的关联**：

```
Service: signaling
    │
    ├── selector: app=signaling
    │
    ▼
Pod: signaling-xxx-abc (labels: app=signaling) → 加入Endpoints
Pod: signaling-xxx-def (labels: app=signaling) → 加入Endpoints
Pod: other-yyy-ghi     (labels: app=other)     → 不加入
```

### 4.3 Headless Service

**Headless Service**：不提供ClusterIP，直接返回后端Pod IP

```yaml
apiVersion: v1
kind: Service
metadata:
  name: sfu-headless
spec:
  clusterIP: None  # Headless关键配置
  selector:
    app: sfu
  ports:
  - port: 8080
```

**Headless的用途**：

| 场景 | 说明 | 直播应用 |
|:---|:---|:---|
| **直接Pod通信** | DNS返回所有Pod IP列表 | SFU集群间直接通信 |
| **有状态服务** | 配合Statefulset使用 | 每SFU实例有独立DNS |
| **客户端发现** | 客户端自己选择后端 | WebRTC客户端选择最近SFU |

```yaml
# Headless Service + StatefulSet 的DNS解析
# sfu-0.sfu-headless → 10.244.1.10
# sfu-1.sfu-headless → 10.244.2.15
# sfu-2.sfu-headless → 10.244.3.20

# 应用程序可以通过环境变量获取同组Pod
env:
- name: SFU_PEERS
  value: "sfu-0.sfu-headless,sfu-1.sfu-headless,sfu-2.sfu-headless"
```

---

## 5. 存储卷类型详解

### 5.1 Volume类型对比

| 类型 | 生命周期 | 数据共享 | 适用场景 |
|:---|:---|:---:|:---|
| **emptyDir** | Pod级 | 同Pod多容器 | 临时缓存、共享内存 |
| **hostPath** | 节点级 | 节点内 | 单节点测试、日志收集 |
| **ConfigMap** | K8s对象 | 只读共享 | 配置文件注入 |
| **Secret** | K8s对象 | 只读共享 | 密钥证书注入 |
| **PV/PVC** | 独立于Pod | 可跨Pod | 数据持久化 |

### 5.2 PV与PVC绑定机制

**PersistentVolume（PV）**：集群级别的存储资源
**PersistentVolumeClaim（PVC）**：Pod对存储的请求

```
静态供给（管理员预先创建PV）：

管理员创建:                    用户创建:
┌───────────────┐              ┌───────────────┐
│ PV: pv-001    │  ←──绑定──→ │ PVC: sfu-data │
│ 容量: 10Gi    │              容量: 10Gi      │
│ 类型: SSD     │              匹配: ReadWriteOnce
│ 路径: /data/1 │              
└───────────────┘              └───────┬───────┘
                                       │
                                       ▼
                               ┌───────────────┐
                               │ Pod使用PVC    │
                               │ 读写/data     │
                               └───────────────┘

动态供给（StorageClass自动创建PV）：

用户创建PVC → StorageClass检测到 → 自动 provisioner 创建PV → 绑定
```

**StorageClass配置**：

```yaml
apiVersion: storage.k8s.io/v1
kind: StorageClass
metadata:
  name: fast-ssd
provisioner: kubernetes.io/gce-pd  # GCE持久盘
parameters:
  type: pd-ssd
  replication-type: regional
reclaimPolicy: Retain  # PVC删除后PV保留
allowVolumeExpansion: true  # 支持扩容
volumeBindingMode: WaitForFirstConsumer  # 延迟绑定到Pod所在节点
```

### 5.3 直播系统存储方案

```yaml
# SFU录制存储 - 使用高性能SSD
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name: sfu-recordings
spec:
  accessModes:
    - ReadWriteOnce
  storageClassName: fast-ssd
  resources:
    requests:
      storage: 500Gi
---
# SFU StatefulSet 使用PVC
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: sfu
spec:
  volumeClaimTemplates:
  - metadata:
      name: recordings
    spec:
      accessModes: ["ReadWriteOnce"]
      storageClassName: fast-ssd
      resources:
        requests:
          storage: 100Gi
  - metadata:
      name: logs
    spec:
      accessModes: ["ReadWriteOnce"]
      storageClassName: standard
      resources:
        requests:
          storage: 10Gi
```

---

## 6. 调度算法简介

### 6.1 调度流程

**Pod调度流程**：

```
Pod创建 → 调度队列 → 预选(Filters) → 优选(Scores) → 绑定 → 节点运行
            │            │               │          │
            │            │               │          │
            ▼            ▼               ▼          ▼
         优先级排序   排除不满足    计算得分    更新Pod的
                     条件的节点    排序选择    nodeName
```

### 6.2 预选策略（Predicates）

**排除不符合条件的节点**：

| 预选策略 | 说明 | 示例 |
|:---|:---|:---|
| **PodFitsResources** | 资源是否充足 | CPU/内存/磁盘检查 |
| **PodFitsHost** | 是否匹配nodeName | 指定节点调度 |
| **PodFitsHostPorts** | 端口是否冲突 | HostPort检查 |
| **PodMatchNodeSelector** | 标签选择器匹配 | 节点亲和性 |
| **NoVolumeZoneConflict** | 存储区域匹配 | 云盘可用区 |
| **NoDiskConflict** | 磁盘不冲突 | GCE PD只能挂载一个节点 |
| **PodToleratesNodeTaints** | 容忍污点 | 专用节点调度 |

### 6.3 优选策略（Priorities）

**为剩余节点打分排序**：

| 优选策略 | 权重 | 说明 |
|:---|:---:|:---|
| **LeastRequestedPriority** | 1 | 优先选择资源空闲多的节点 |
| **BalancedResourceAllocation** | 1 | CPU和内存使用平衡 |
| **SelectorSpreadPriority** | 1 | 打散Pod分布（高可用）|
| **InterPodAffinityPriority** | 1 | Pod亲和性偏好 |
| **NodeAffinityPriority** | 1 | 节点亲和性偏好 |
| **TaintTolerationPriority** | 1 | 污点容忍度 |

**SFU调度优化**：

```yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: sfu
spec:
  template:
    spec:
      # 反亲和性：打散到不同节点
      affinity:
        podAntiAffinity:
          preferredDuringSchedulingIgnoredDuringExecution:
          - weight: 100
            podAffinityTerm:
              labelSelector:
                matchExpressions:
                - key: app
                  operator: In
                  values:
                  - sfu
              topologyKey: kubernetes.io/hostname
      
      # 节点亲和性：优先选择网络性能好的节点
      nodeAffinity:
        preferredDuringSchedulingIgnoredDuringExecution:
        - weight: 10
          preference:
            matchExpressions:
            - key: node-type
              operator: In
              values:
              - network-optimized
      
      # 污点容忍：可以调度到专用节点
      tolerations:
      - key: "dedicated"
        operator: "Equal"
        value: "sfu"
        effect: "NoSchedule"
```

---

## 7. SFU有状态部署

### 2.1 StatefulSet配置

```yaml
# sfu-statefulset.yaml
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: sfu
  namespace: live
spec:
  serviceName: sfu-headless
  replicas: 3
  selector:
    matchLabels:
      app: sfu
  template:
    metadata:
      labels:
        app: sfu
    spec:
      containers:
      - name: sfu
        image: your-registry/sfu:v1.0.0
        ports:
        - containerPort: 8080
          name: http
        - containerPort: 3478
          protocol: TCP
          name: turn-tcp
        - containerPort: 3478
          protocol: UDP
          name: turn-udp
        - containerPort: 10000
          protocol: UDP
          name: rtp-start
        env:
        - name: POD_NAME
          valueFrom:
            fieldRef:
              fieldPath: metadata.name
        - name: EXTERNAL_IP
          valueFrom:
            fieldRef:
              fieldPath: status.podIP
        - name: TURN_REALM
          value: "live.example.com"
        - name: CONFIG_PATH
          value: "/config/sfu.yaml"
        volumeMounts:
        - name: config
          mountPath: /config
        - name: logs
          mountPath: /app/logs
        resources:
          requests:
            memory: "2Gi"
            cpu: "1000m"
          limits:
            memory: "4Gi"
            cpu: "4000m"
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 5
      volumes:
      - name: config
        configMap:
          name: sfu-config
  volumeClaimTemplates:
  - metadata:
      name: logs
    spec:
      accessModes: ["ReadWriteOnce"]
      resources:
        requests:
          storage: 10Gi
```

### 2.2 Headless Service

```yaml
# sfu-service.yaml
apiVersion: v1
kind: Service
metadata:
  name: sfu-headless
  namespace: live
spec:
  clusterIP: None  # Headless service
  selector:
    app: sfu
  ports:
  - port: 8080
    name: http
  - port: 3478
    name: turn-tcp
    protocol: TCP
  - port: 3478
    name: turn-udp
    protocol: UDP
```

**为什么SFU需要StatefulSet？**

1. **稳定网络标识**：每个Pod有固定hostname（sfu-0, sfu-1, sfu-2）
2. **有序部署/扩缩容**：避免同时重启所有SFU
3. **持久化存储**：日志等数据需要持久化
4. **ICE候选**：需要稳定的IP地址供客户端连接

### 2.3 NodePort暴露

```yaml
# sfu-nodeport.yaml
apiVersion: v1
kind: Service
metadata:
  name: sfu-external
  namespace: live
spec:
  type: NodePort
  selector:
    app: sfu
  ports:
  - port: 3478
    targetPort: 3478
    nodePort: 30478
    protocol: UDP
    name: turn-udp
  - port: 10000
    targetPort: 10000
    nodePort: 31000
    protocol: UDP
    name: rtp
  externalTrafficPolicy: Local  # 保留客户端真实IP
```

---

## 8. 自动扩缩容

### 3.1 HPA（水平Pod自动伸缩）

```yaml
# sfu-hpa.yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: sfu-hpa
  namespace: live
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: StatefulSet
    name: sfu
  minReplicas: 3
  maxReplicas: 20
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
  - type: Resource
    resource:
      name: memory
      target:
        type: Utilization
        averageUtilization: 80
  - type: Pods
    pods:
      metric:
        name: concurrent_streams
      target:
        type: AverageValue
        averageValue: "100"  # 每Pod平均100路流
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 60
      policies:
      - type: Percent
        value: 100
        periodSeconds: 60
    scaleDown:
      stabilizationWindowSeconds: 300
      policies:
      - type: Percent
        value: 10
        periodSeconds: 60
```

### 3.2 自定义指标

```yaml
# 自定义指标（Prometheus Adapter）
apiVersion: v1
kind: ConfigMap
metadata:
  name: custom-metrics-config
  namespace: monitoring
data:
  config.yaml: |
    rules:
    - seriesQuery: 'sfu_concurrent_streams'
      resources:
        template: <<.Resource.Name>>
      name:
        matches: "^(.*)"
        as: "concurrent_streams"
      metricsQuery: sum(<<.Series>>) by (<<.GroupBy>>)
```

### 3.3 集群自动扩缩容

```yaml
# 集群自动扩缩容配置（Cluster Autoscaler）
apiVersion: autoscaling/v1
kind: ClusterAutoscaler
metadata:
  name: cluster-autoscaler
spec:
  scaleDownEnabled: true
  balanceSimilarNodeGroups: true
  minNodes: 3
  maxNodes: 100
  scaleDownDelayAfterAdd: 10m
  scaleDownDelayAfterDelete: 10s
  scaleDownDelayAfterFailure: 3m
  scaleDownUnneededTime: 10m
```

---

## 9. 服务网格

### 4.1 Istio部署

```yaml
# istio-gateway.yaml
apiVersion: networking.istio.io/v1beta1
kind: Gateway
metadata:
  name: live-gateway
  namespace: live
spec:
  selector:
    istio: ingressgateway
  servers:
  - port:
      number: 80
      name: http
      protocol: HTTP
    hosts:
    - "*.live.example.com"
  - port:
      number: 443
      name: https
      protocol: HTTPS
    tls:
      mode: SIMPLE
      credentialName: live-tls-secret
    hosts:
    - "*.live.example.com"
---
apiVersion: networking.istio.io/v1beta1
kind: VirtualService
metadata:
  name: sfu-route
  namespace: live
spec:
  hosts:
  - "sfu.live.example.com"
  gateways:
  - live-gateway
  http:
  - match:
    - uri:
        prefix: /
    route:
    - destination:
        host: sfu-headless
        port:
          number: 8080
```

### 4.2 流量管理

```yaml
# 金丝雀发布
apiVersion: networking.istio.io/v1beta1
kind: VirtualService
metadata:
  name: sfu-canary
spec:
  hosts:
  - sfu-headless
  http:
  - match:
    - headers:
        canary:
          exact: "true"
    route:
    - destination:
        host: sfu-headless
        subset: v2
      weight: 100
  - route:
    - destination:
        host: sfu-headless
        subset: v1
      weight: 90
    - destination:
        host: sfu-headless
        subset: v2
      weight: 10
---
apiVersion: networking.istio.io/v1beta1
kind: DestinationRule
metadata:
  name: sfu-versions
spec:
  host: sfu-headless
  subsets:
  - name: v1
    labels:
      version: v1
  - name: v2
    labels:
      version: v2
```

### 4.3 可观测性

```yaml
# 分布式追踪
apiVersion: install.istio.io/v1alpha1
kind: IstioOperator
spec:
  profile: default
  values:
    global:
      proxy:
        resources:
          requests:
            cpu: 100m
            memory: 128Mi
    pilot:
      traceSampling: 100.0
  meshConfig:
    enableTracing: true
    accessLogFile: /dev/stdout
    defaultConfig:
      tracing:
        sampling: 100.0
        zipkin:
          address: zipkin.istio-system:9411
```

---

## 10. 生产最佳实践

### 5.1 配置管理

```bash
# 使用Kustomize管理多环境
# base/kustomization.yaml
resources:
- sfu-statefulset.yaml
- sfu-service.yaml
- sfu-hpa.yaml

# overlays/production/kustomization.yaml
bases:
- ../../base

namePrefix: prod-
namespace: live-prod

replicas:
- name: sfu
  count: 5

patchesStrategicMerge:
- resources-patch.yaml
- config-patch.yaml
```

### 5.2 安全加固

```yaml
# Pod安全策略
apiVersion: policy/v1beta1
kind: PodSecurityPolicy
metadata:
  name: live-restricted
spec:
  privileged: false
  allowPrivilegeEscalation: false
  requiredDropCapabilities:
    - ALL
  volumes:
    - 'configMap'
    - 'emptyDir'
    - 'projected'
    - 'secret'
    - 'downwardAPI'
    - 'persistentVolumeClaim'
  runAsUser:
    rule: 'MustRunAsNonRoot'
  seLinux:
    rule: 'RunAsAny'
  fsGroup:
    rule: 'RunAsAny'
---
# NetworkPolicy
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: sfu-network-policy
  namespace: live
spec:
  podSelector:
    matchLabels:
      app: sfu
  policyTypes:
  - Ingress
  - Egress
  ingress:
  - from:
    - podSelector:
        matchLabels:
          app: signaling
    ports:
    - protocol: TCP
      port: 8080
  - from: []  # 允许外部UDP（ICE）
    ports:
    - protocol: UDP
      port: 3478
    - protocol: UDP
      port: 10000
      endPort: 20000
```

### 5.3 备份恢复

```yaml
# Velero备份计划
apiVersion: velero.io/v1
kind: Schedule
metadata:
  name: live-backup
  namespace: velero
spec:
  schedule: "0 2 * * *"  # 每天2点备份
  template:
    includedNamespaces:
    - live
    excludedResources:
    - events
    - pods
    ttl: 720h0m0s  # 保留30天
    storageLocation: default
    volumeSnapshotLocations:
    - aws-default
```

### 5.4 监控告警

```yaml
# PrometheusRule
apiVersion: monitoring.coreos.com/v1
kind: PrometheusRule
metadata:
  name: live-alerts
  namespace: monitoring
spec:
  groups:
  - name: live.rules
    rules:
    - alert: SFUHighCPU
      expr: |
        rate(container_cpu_usage_seconds_total{pod=~"sfu-.*"}[5m]) > 0.8
      for: 5m
      labels:
        severity: warning
      annotations:
        summary: "SFU CPU usage high"
        
    - alert: SFUHighLatency
      expr: |
        histogram_quantile(0.99, 
          rate(sfu_packet_delay_seconds_bucket[5m])) > 0.5
      for: 3m
      labels:
        severity: critical
      annotations:
        summary: "SFU latency too high"
        
    - alert: SFUPodDown
      expr: |
        up{job="sfu"} == 0
      for: 1m
      labels:
        severity: critical
      annotations:
        summary: "SFU pod is down"
```

---

## 11. 本章总结

### 6.1 K8s部署要点

| 方面 | 关键决策 |
|:---|:---|
| 有状态服务 | 使用StatefulSet管理SFU |
| 网络暴露 | NodePort/LoadBalancer暴露UDP端口 |
| 扩缩容 | HPA基于自定义指标（并发流数）|
| 流量管理 | Istio实现金丝雀发布 |
| 可观测性 | Prometheus + Grafana + Jaeger |
| 安全 | NetworkPolicy、PodSecurityPolicy |

### 6.2 部署流程

```
1. 准备镜像 → 2. 配置ConfigMap/Secret → 3. 部署StatefulSet
      ↓                                               ↓
4. 配置Service/Ingress ← 5. 配置HPA ← 6. 验证健康检查
      ↓
7. 配置监控告警 → 8. 配置备份 → 9. 上线
```

### 6.3 课后思考

1. **有状态 vs 无状态**：分析为什么SFU需要使用StatefulSet而不是Deployment。

2. **扩缩容策略**：设计一个更智能的扩缩容策略，考虑房间数、流数、地域分布。

3. **灾备方案**：当一个可用区故障时，如何快速恢复SFU服务？

4. **成本控制**：如何在保证服务质量的前提下优化K8s集群成本？
