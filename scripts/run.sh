#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CATHEYE_GUARD_PATH="$(cd -- "$SCRIPT_DIR/.." && pwd)"
RECORDING_DIR="${CATCHEYE_GUARD_RECORDING_DIR:-/home/user/catcheye-guard/recordings}"
CAMERA_PROPERTIES="${CATCHEYE_GUARD_CAMERA_PROPERTIES:-$CATHEYE_GUARD_PATH/config/camera_properties.json}"

exec "$CATHEYE_GUARD_PATH/bin/catcheye-guard" \
  --viewer-only \
  --camera \
  --camera-pipeline "libcamerasrc ! video/x-raw,width=2304,height=1296,framerate=15/1,format=NV12 ! queue leaky=downstream max-size-buffers=1 ! videoflip method=rotate-180" \
  --ws \
  --recording-dir "$RECORDING_DIR" \
  --camera-properties "$CAMERA_PROPERTIES"
