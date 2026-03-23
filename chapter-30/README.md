# 第三十章：Kubernetes生产部署

> **本章目标**：掌握使用Kubernetes部署直播系统，包括有状态服务管理、自动扩缩容和服务网格。

Kubernetes已成为云原生应用部署的事实标准。本章介绍如何在K8s上部署完整的直播系统。

---

## 目录

1. [K8s核心概念](#1-k8s核心概念)
2. [SFU有状态部署](#2-sfu有状态部署)
3. [自动扩缩容](#3-自动扩缩容)
4. [服务网格](#4-服务网格)
5. [生产最佳实践](#5-生产最佳实践)
6. [本章总结](#6-本章总结)

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

## 2. SFU有状态部署

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

## 3. 自动扩缩容

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

## 4. 服务网格

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

## 5. 生产最佳实践

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

## 6. 本章总结

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
