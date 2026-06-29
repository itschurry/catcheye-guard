#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CATHEYE_GUARD_PATH="$(cd -- "$SCRIPT_DIR/.." && pwd)"
RECORDING_DIR="${CATCHEYE_GUARD_RECORDING_DIR:-/home/user/catcheye-guard/recordings}"

exec "$CATHEYE_GUARD_PATH/bin/catcheye-guard" \
  --camera \
  --camera-pipeline "libcamerasrc ! video/x-raw,width=2304,height=1296,framerate=15/1,format=NV12 ! queue leaky=downstream max-size-buffers=1 ! videoflip method=rotate-180" \
  --ws \
  --hef "$CATHEYE_GUARD_PATH/models/yolo26m.hef" \
  --metadata "$CATHEYE_GUARD_PATH/models/metadata.yaml" \
  --roi-alert-gpio 14 \
  --person-roi-alert-disable-gpio 15 \
  --person-roi-alert-disable-debounce-ms 200 \
  --recording-dir "$RECORDING_DIR"
