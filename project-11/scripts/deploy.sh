#!/bin/bash

# 生产级部署脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "========================================"
echo "  直播系统生产部署脚本"
echo "========================================"
echo ""

# 检查参数
DEPLOY_MODE=${1:-docker-compose}

if [ "$DEPLOY_MODE" != "docker-compose" ] && [ "$DEPLOY_MODE" != "k8s" ]; then
    echo "用法: $0 [docker-compose|k8s]"
    exit 1
fi

echo "部署模式: $DEPLOY_MODE"
echo ""

# Docker Compose 部署
if [ "$DEPLOY_MODE" == "docker-compose" ]; then
    echo "[1/4] 构建 Docker 镜像..."
    cd "$PROJECT_ROOT/project-10"
    docker build -t live-system/server:latest -f ../project-11/docker/Dockerfile .
    
    cd "$PROJECT_ROOT/project-09"
    docker build -t live-system/recorder:latest -f ../project-11/docker/Dockerfile .
    
    echo "[2/4] 创建数据目录..."
    mkdir -p "$SCRIPT_DIR/docker-compose/data/recordings"
    
    echo "[3/4] 启动服务..."
    cd "$SCRIPT_DIR/docker-compose"
    docker-compose up -d
    
    echo "[4/4] 检查服务状态..."
    docker-compose ps
    
    echo ""
    echo "部署完成!"
    echo "  信令服务: ws://localhost:7880"
    echo "  HLS回放: http://localhost:8080"
    echo "  Nginx代理: http://localhost"
fi

# Kubernetes 部署
if [ "$DEPLOY_MODE" == "k8s" ]; then
    echo "[1/3] 构建并推送镜像..."
    # 假设使用阿里云容器镜像服务
    REGISTRY=${REGISTRY:-registry.cn-hangzhou.aliyuncs.com/live-system}
    
    cd "$PROJECT_ROOT/project-10"
    docker build -t $REGISTRY/server:latest -f ../project-11/docker/Dockerfile .
    docker push $REGISTRY/server:latest
    
    echo "[2/3] 创建命名空间和部署..."
    kubectl apply -f "$SCRIPT_DIR/k8s/namespace.yaml" 2>/dev/null || true
    kubectl apply -f "$SCRIPT_DIR/k8s/deployment.yaml"
    
    echo "[3/3] 检查部署状态..."
    kubectl get pods -n live-system -w
    
    echo ""
    echo "部署完成!"
    echo "  查看日志: kubectl logs -f deployment/live-signaling -n live-system"
    echo "  查看服务: kubectl get svc -n live-system"
fi

echo ""
echo "监控面板:"
echo "  Prometheus: http://localhost:9090 (docker-compose)"
echo "  Grafana: http://localhost:3000 (docker-compose)"
echo ""
echo "停止服务:"
echo "  docker-compose: cd docker-compose && docker-compose down"
echo "  k8s: kubectl delete namespace live-system"