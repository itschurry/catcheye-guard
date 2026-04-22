#!/usr/bin/env bash
set -euo pipefail

ENV_CONTENT=$(cat <<EOF
UID=$(id -u)
GID=$(id -g)
USER=$(id -un)
HOME=$HOME
GST_PLUGIN_PATH=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0
EOF
)

printf '%s\n' "$ENV_CONTENT" > docker/.env
