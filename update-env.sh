#!/usr/bin/env bash
set -euo pipefail

HOSTNAME_VALUE="${HOSTNAME:-$(hostname)}"
ROS_NAMESPACE_VALUE="/${HOSTNAME_VALUE//-/_}"
read -r -p "ROS_DOMAIN_ID: " ROS_DOMAIN_ID_VALUE

cat > .env <<EOF
UID=$(id -u)
GID=$(id -g)
USER=$(id -un)
HOME=$HOME
GST_PLUGIN_PATH=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0
ROS_DOMAIN_ID=${ROS_DOMAIN_ID_VALUE}
ROS_NAMESPACE=${ROS_NAMESPACE_VALUE}
EOF