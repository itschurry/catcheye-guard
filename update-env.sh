#!/usr/bin/env bash
set -euo pipefail

HOSTNAME_VALUE="${HOSTNAME:-$(hostname)}"
ROS_NAMESPACE_VALUE="/${HOSTNAME_VALUE//-/_}"
read -r -p "ROS_DOMAIN_ID: " ROS_DOMAIN_ID_VALUE
read -r -p "IS_MASTER (0/1): " IS_MASTER_VALUE
IS_MASTER_VALUE="${IS_MASTER_VALUE:-0}"

cat > .env <<EOF
UID=$(id -u)
GID=$(id -g)
USER=$(id -un)
HOME=$HOME
GST_PLUGIN_PATH=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0
ROS_DOMAIN_ID=${ROS_DOMAIN_ID_VALUE}
ROS_NAMESPACE=${ROS_NAMESPACE_VALUE}
IS_MASTER=${IS_MASTER_VALUE}
EOF