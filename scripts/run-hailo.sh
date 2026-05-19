#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CATHEYE_GUARD_PATH="$(cd -- "$SCRIPT_DIR/.." && pwd)"

exec "$CATHEYE_GUARD_PATH/bin/catcheye-guard" \
  --camera \
  --camera-pipeline "libcamerasrc ! video/x-raw,width=2304,height=1296,framerate=15/1,format=NV12 ! queue leaky=downstream max-size-buffers=1 ! videoflip method=rotate-180" \
  --ws \
  --detector hailo \
  --hef "$CATHEYE_GUARD_PATH/models/yolo26m.hef" \
  --metadata "$CATHEYE_GUARD_PATH/models/metadata.yaml"
