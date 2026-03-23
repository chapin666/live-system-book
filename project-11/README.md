# Project 11: 生产级部署

Docker Compose和Kubernetes生产部署配置。

## 项目概述

本项目提供完整的生产级部署方案：
- Docker Compose本地编排
- Kubernetes集群部署
- 监控告警配置
- CI/CD流水线

## 项目结构

```
project-11/
├── README.md
├── docker-compose/              # Docker Compose配置
│   ├── docker-compose.yml
│   ├── docker-compose.prod.yml
│   ├── docker-compose.monitoring.yml
│   └── .env.example
├── kubernetes/                  # K8s配置
│   ├── base/                    # 基础配置
│   │   ├── namespace.yaml
│   │   ├── configmap.yaml
│   │   ├── secret.yaml
│   │   ├── sfu-statefulset.yaml
│   │   ├── sfu-service.yaml
│   │   ├── signaling-deployment.yaml
│   │   ├── signaling-service.yaml
│   │   ├── mcu-deployment.yaml
│   │   └── ingress.yaml
│   ├── overlays/
│   │   ├── development/
│   │   └── production/
│   └── kustomization.yaml
├── monitoring/                  # 监控配置
│   ├── prometheus/
│   ├── grafana/
│   └── loki/
├── scripts/                     # 部署脚本
│   ├── deploy.sh
│   ├── rollback.sh
│   └── health-check.sh
└── ci-cd/                       # CI/CD配置
    ├── github-actions.yml
    └── gitlab-ci.yml
```

## Docker Compose部署

### 开发环境

```bash
cd docker-compose
cp .env.example .env
# 编辑.env配置
docker-compose up -d
```

### 生产环境

```bash
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d
```

## Kubernetes部署

### 使用kubectl

```bash
# 创建命名空间
kubectl apply -f kubernetes/base/namespace.yaml

# 部署配置
kubectl apply -f kubernetes/base/configmap.yaml
kubectl apply -f kubernetes/base/secret.yaml

# 部署SFU
kubectl apply -f kubernetes/base/sfu-statefulset.yaml
kubectl apply -f kubernetes/base/sfu-service.yaml

# 部署信令服务
kubectl apply -f kubernetes/base/signaling-deployment.yaml
kubectl apply -f kubernetes/base/signaling-service.yaml

# 部署入口
kubectl apply -f kubernetes/base/ingress.yaml
```

### 使用Kustomize

```bash
# 开发环境
cd kubernetes
kubectl apply -k overlays/development

# 生产环境
kubectl apply -k overlays/production
```

## 核心配置文件

### SFU StatefulSet

```yaml
# kubernetes/base/sfu-statefulset.yaml
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
        image: registry/live-sfu:v1.0.0
        ports:
        - containerPort: 8080
          name: http
        - containerPort: 3478
          protocol: UDP
          name: turn
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
  volumeClaimTemplates:
  - metadata:
      name: sfu-data
    spec:
      accessModes: ["ReadWriteOnce"]
      resources:
        requests:
          storage: 50Gi
```

### HPA自动扩缩容

```yaml
# kubernetes/base/sfu-hpa.yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: sfu-hpa
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
  - type: Pods
    pods:
      metric:
        name: concurrent_streams
      target:
        type: AverageValue
        averageValue: "100"
```

### Ingress配置

```yaml
# kubernetes/base/ingress.yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: live-ingress
  annotations:
    nginx.ingress.kubernetes.io/ssl-redirect: "true"
    cert-manager.io/cluster-issuer: "letsencrypt-prod"
spec:
  tls:
  - hosts:
    - api.live.example.com
    - sfu.live.example.com
    secretName: live-tls
  rules:
  - host: api.live.example.com
    http:
      paths:
      - path: /
        pathType: Prefix
        backend:
          service:
            name: signaling
            port:
              number: 8080
```

## 监控配置

### Prometheus ServiceMonitor

```yaml
# monitoring/prometheus/servicemonitor.yaml
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: live-metrics
spec:
  selector:
    matchLabels:
      app: sfu
  endpoints:
  - port: metrics
    interval: 15s
```

### 告警规则

```yaml
# monitoring/prometheus/rules.yaml
apiVersion: monitoring.coreos.com/v1
kind: PrometheusRule
metadata:
  name: live-alerts
spec:
  groups:
  - name: sfu
    rules:
    - alert: SFUHighCPU
      expr: |
        rate(container_cpu_usage_seconds_total{pod=~"sfu-.*"}[5m]) > 0.8
      for: 5m
      labels:
        severity: warning
      annotations:
        summary: "SFU CPU usage high"
```

## 部署脚本

```bash
#!/bin/bash
# scripts/deploy.sh

set -e

ENV=${1:-production}
VERSION=${2:-latest}

echo "Deploying to $ENV with version $VERSION"

# 更新镜像版本
cd kubernetes/overlays/$ENV
kustomize edit set image sfu=registry/live-sfu:$VERSION
kustomize edit set image signaling=registry/live-signaling:$VERSION

# 部署
kubectl apply -k .

# 等待就绪
kubectl rollout status statefulset/sfu -n live
kubectl rollout status deployment/signaling -n live

echo "Deployment complete!"
```

## 运维命令

```bash
# 查看Pod状态
kubectl get pods -n live -o wide

# 查看日志
kubectl logs -f sfu-0 -n live

# 进入容器调试
kubectl exec -it sfu-0 -n live -- /bin/sh

# 扩容
kubectl scale statefulset sfu --replicas=5 -n live

# 滚动重启
kubectl rollout restart statefulset/sfu -n live

# 查看事件
kubectl get events -n live --sort-by='.lastTimestamp'
```
