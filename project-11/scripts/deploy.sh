#!/bin/bash
# deploy.sh - Deploy script

set -e

ENV=${1:-production}
VERSION=${2:-latest}

echo "Deploying to $ENV with version $VERSION"

cd "$(dirname "$0")/../kubernetes"

# Apply using kustomize
kubectl apply -k "overlays/$ENV"

# Wait for rollout
kubectl rollout status statefulset/sfu -n live
kubectl rollout status deployment/signaling -n live

echo "Deployment complete!"
