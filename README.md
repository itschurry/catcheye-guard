# CatchEye Guard

Raspberry Pi ARM64 + Hailo 환경에서 카메라 프레임을 처리하고, 사람/파렛트 검출 결과를 ROI 규칙으로 판정하는 감시 애플리케이션이다.

지원 빌드 환경:

- `amd64`: 개발 PC용. 기본 빌드는 `libcamera`, `HailoRT` 없이 빌드한다.
- `arm64`: Raspberry Pi + Hailo 하드웨어용. 기본 빌드는 `libcamera`, `HailoRT`를 켠다.

## 목적

- 카메라, 이미지, 동영상 입력 처리
- HailoRT 기반 YOLO 검출
- 위험 ROI 진입 판정
- 파렛트 필수 영역 존재 판정
- GPIO 상태 유지 출력
- GPIO 입력 기반 사람 ROI 알림 비활성화
- WebSocket 프레임 송출
- HTTP API 기반 ROI 조회/교체, 카메라 제어, 녹화 제어

## 기본 포트

| 기능 | 기본 포트 | 주소 |
| --- | ---: | --- |
| WebSocket | `8080` | `ws://<host>:8080/` |
| HTTP API | `8090` | `http://<host>:8090/api/` |

## 디렉터리 구조

```text
.
├── CMakeLists.txt
├── config/
│   ├── camera_properties.json
│   ├── roi_cam_default.json
│   └── pallet_roi_cam_default.json
├── docs/
│   └── API.md
├── docker/
│   ├── amd64/
│   │   ├── Dockerfile
│   │   ├── docker-compose.base.yml
│   │   └── docker-compose.dev.yml
│   └── arm64/
│       ├── Dockerfile
│       ├── docker-compose.base.yml
│       └── docker-compose.dev.yml
├── models/
│   ├── yolo26m.hef
│   └── metadata.yaml
├── scripts/
│   ├── cmake.sh
│   ├── run.sh
│   └── run-hailo.sh
├── src/
│   ├── main.cpp
│   ├── guard/
│   └── hardware/
└── third_party/catcheye-vision-sdk/
```

## 설치

ARM 장비에는 HailoRT, 카메라 런타임, GStreamer, OpenCV, yaml-cpp, spdlog, libgpiod가 필요하다.

```bash
sudo apt update
sudo apt install -y \
  hailo-all \
  gstreamer1.0-libcamera \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  libopencv-dev \
  libyaml-cpp-dev \
  libspdlog-dev \
  libgpiod-dev \
  gpiod
```

확인:

```bash
gst-inspect-1.0 libcamerasrc
hailortcli fw-control identify
ls -l /dev/hailo*
```

## 빌드

amd64 개발 컨테이너:

```bash
docker compose -f docker/amd64/docker-compose.dev.yml build
docker compose -f docker/amd64/docker-compose.dev.yml up -d catcheye-guard-develop-amd64
```

amd64 호스트에서 arm64 컨테이너를 빌드하거나 실행하기 전에는 QEMU binfmt를 먼저 등록한다.
이 작업이 없으면 arm64 이미지 빌드 중 `exec /bin/bash: exec format error`가 난다.

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

arm64 하드웨어 컨테이너:

```bash
docker compose -f docker/arm64/docker-compose.dev.yml build
docker compose -f docker/arm64/docker-compose.dev.yml up -d catcheye-guard-develop-arm64
```

컨테이너 빌드:

```bash
scripts/cmake.sh configure release
scripts/cmake.sh build release
scripts/cmake.sh install release
scripts/cmake.sh verify release
```

arm64 컨테이너를 명시해서 빌드:

```bash
DOCKER_ARCH=arm64 scripts/cmake.sh build release
```

`build/release-amd64/compile_commands.json` 또는 `build/release-arm64/compile_commands.json`은 컨테이너 경로 기준이다.
`build/compile_commands.json`은 macOS 호스트 경로 기준으로 변환된 파일이다.
프로젝트 루트에는 `compile_commands.json`을 만들지 않는다.

수동 빌드:

```bash
cmake -S . -B build/release-amd64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCATCHEYE_GUARD_ENABLE_LIBCAMERA=OFF \
  -DCATCHEYE_VISION_DETECTION_ENABLE_HAILO=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake -S . -B build/release-arm64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCATCHEYE_GUARD_ENABLE_LIBCAMERA=ON \
  -DCATCHEYE_VISION_DETECTION_ENABLE_HAILO=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

주요 CMake 옵션:

- `CATCHEYE_GUARD_ENABLE_LIBCAMERA`: libcamera 입력 백엔드 빌드 여부다. amd64 기본값은 `OFF`, arm64 기본값은 `ON`이다.
- `CATCHEYE_VISION_DETECTION_ENABLE_HAILO`: HailoRT detector 백엔드 빌드 여부다. amd64 기본값은 `OFF`, arm64 기본값은 `ON`이다.
- `CATCHEYE_GUARD_BUILD_APP`: 실행 파일 빌드 여부다. 기본값은 `ON`이다.
- `CATCHEYE_GUARD_BUILD_TESTS`: 테스트 빌드 여부다. 기본값은 `OFF`다.

## 실행

CSI 카메라 + WebSocket + Hailo:

```bash
./install/release-arm64/bin/catcheye-guard \
  --camera \
  --ws \
  --hef ./install/release-arm64/models/yolo26m.hef \
  --metadata ./install/release-arm64/models/metadata.yaml
```

설치 패키지의 실행 스크립트:

```bash
./install/release-arm64/scripts/run-hailo.sh
```

뷰어 전용:

```bash
./install/release-arm64/bin/catcheye-guard \
  --viewer-only \
  --camera \
  --ws
```

이미지 파일 입력:

```bash
./install/release-arm64/bin/catcheye-guard \
  --image ./frame.jpg \
  --ws \
  --hef ./models/yolo26m.hef \
  --metadata ./models/metadata.yaml
```

ROI 파일을 직접 지정:

```bash
./install/release-arm64/bin/catcheye-guard \
  --camera \
  --ws \
  --hef ./models/yolo26m.hef \
  --metadata ./models/metadata.yaml \
  ./config/roi_cam_default.json
```

사람 ROI 알림 비활성화 입력 GPIO를 같이 지정:

```bash
./install/release-arm64/bin/catcheye-guard \
  --camera \
  --ws \
  --hef ./models/yolo26m.hef \
  --metadata ./models/metadata.yaml \
  --roi-alert-gpio 14 \
  --person-roi-alert-disable-gpio 15 \
  --gpio-chip /dev/gpiochip4
```

## 주요 옵션

| 옵션 | 설명 |
| --- | --- |
| `--camera` | 카메라 입력을 쓴다. 기본 입력이다. |
| `--image <path>` | 이미지 파일 입력을 쓴다. |
| `--video <path>` | 동영상 파일 입력을 쓴다. |
| `--camera-pipeline <pipe>` | GStreamer 카메라 pipeline을 직접 지정한다. |
| `--camera-device <path>` | USB 카메라 장치 경로를 지정한다. |
| `--camera-width <pixels>` | 카메라 폭을 지정한다. |
| `--camera-height <pixels>` | 카메라 높이를 지정한다. |
| `--viewer-only` | 검출 없이 프레임만 송출한다. `--camera --ws`와 같이 쓴다. |
| `--ws [port]` | WebSocket 송출을 켠다. 포트 생략 시 `8080`이다. |
| `--http-port <port>` | HTTP API 포트를 지정한다. 기본값은 `8090`이다. |
| `--hef <path>` | Hailo HEF 모델 경로를 지정한다. |
| `--metadata <path>` | detector metadata YAML 경로를 지정한다. |
| `--pallet-roi <path>` | 파렛트 ROI config 경로를 지정한다. |
| `--pallet-class-id <id>` | 파렛트 클래스 id를 지정한다. 기본값은 `1`이다. |
| `--roi-alert-gpio <line>` | ROI 알림 GPIO line을 지정한다. `-1`이면 비활성화한다. |
| `--gpio-chip <path>` | GPIO chip 경로를 지정한다. 기본값은 `/dev/gpiochip4`다. |
| `--roi-alert-active-low` | GPIO 출력을 active-low로 쓴다. |
| `--person-roi-alert-disable-gpio <line>` | 입력이 active인 동안 사람 ROI 알림만 비활성화한다. 기본값은 `-1`이다. |
| `--person-roi-alert-disable-active-low` | 사람 ROI 알림 비활성화 입력을 active-low로 해석한다. |
| `--camera-properties <path>` | RGB 카메라 속성 JSON 경로다. 기본값은 `config/camera_properties.json`이다. |
| `--person-roi-alert-disable-debounce-ms <ms>` | 사람 ROI 알림 비활성화 입력 디바운스 시간을 지정한다. 기본값은 `200`이다. |
| `--recording-dir <path>` | preview 녹화 디렉터리를 지정한다. |

## GPIO 연결

- `--roi-alert-gpio`는 사람 ROI 위반 또는 파렛트 미검출 시 알림을 출력한다.
- `--person-roi-alert-disable-gpio` 입력이 active인 동안에는 사람 ROI 위반 알림만 출력에서 제외한다.
- 파렛트 미검출 알림은 사람 ROI 비활성화 입력의 영향을 받지 않는다.
- PLC 24V를 Raspberry Pi GPIO에 직접 넣으면 안 된다. PLC 출력은 릴레이나 절연 접점으로 넘기고, Raspberry Pi GPIO는 3.3V 입력으로 구성한다.
- PLC가 pulse를 내면 pulse 동안만 사람 ROI 알림이 비활성화된다. 작업 중 계속 끄려면 PLC 출력도 유지 출력이어야 한다.

## 카메라 속성 설정

`--camera-properties` 파일은 앱 시작 시 먼저 파싱되고, 카메라 source가 열린 뒤 적용된다. 파일이 JSON object가 아니면 카메라를 열기 전에 실패한다. 기본 파일은 Studio에서 조절하는 전체 RGB property key를 포함한다. Studio나 HTTP API로 값을 바꾸면 같은 파일에 즉시 저장되고, 다음 실행 때 다시 적용된다.

```json
{
  "ae-enable": false,
  "exposure-time-mode": "manual",
  "exposure-time": 12000,
  "analogue-gain-mode": "manual",
  "analogue-gain": 1.5,
  "awb-enable": true,
  "awb-mode": "auto"
}
```

## HTTP API

기본 주소는 `http://<host>:8090/api/`다. 자세한 엔드포인트, 요청/응답 예시, 에러 형식은 [docs/API.md](docs/API.md)에 따로 정리했다.

```bash
curl http://<host>:8090/api/roi
curl http://<host>:8090/api/pallet-roi
curl http://<host>:8090/api/device-info
curl http://<host>:8090/api/rgb-camera/properties
curl http://<host>:8090/api/recording
```

ROI 교체는 JSON config 파일 형식을 그대로 보낸다.

```bash
curl -X PUT \
  -H 'Content-Type: application/json' \
  --data-binary @config/roi_cam_default.json \
  http://<host>:8090/api/roi
```

## 런타임 전제

- detector는 Hailo만 쓴다.
- 송출은 WebSocket만 쓴다.
- `models/yolo26m.hef`와 `models/metadata.yaml`이 기본 모델 파일이다.
- `config/roi_cam_default.json`이 기본 위험 ROI다.
- `config/pallet_roi_cam_default.json`이 기본 파렛트 ROI다.
- HEF나 metadata가 없으면 실행을 실패시킨다.
