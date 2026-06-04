# CatchEye Guard

Raspberry Pi ARM64 + Hailo 환경에서 카메라 프레임을 처리하고, 사람/파렛트 검출 결과를 ROI 규칙으로 판정하는 감시 애플리케이션이다.

## 목적

- 카메라, 이미지, 동영상 입력 처리
- HailoRT 기반 YOLO 검출
- 위험 ROI 진입 판정
- 파렛트 필수 영역 존재 판정
- GPIO pulse 출력
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
│   ├── roi_cam_default.json
│   └── pallet_roi_cam_default.json
├── docker/
│   ├── Dockerfile
│   ├── docker-compose.base.yml
│   └── docker-compose.dev.yml
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

개발 PC에서 ARM64 컨테이너를 쓸 때:

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
docker compose -f docker/docker-compose.dev.yml build
docker compose -f docker/docker-compose.dev.yml up -d catcheye-guard-dev
```

컨테이너 빌드:

```bash
scripts/cmake.sh configure release
scripts/cmake.sh build release
scripts/cmake.sh install release
scripts/cmake.sh verify release
```

`build/release/compile_commands.json`은 컨테이너 경로 기준이다.
`build/compile_commands.json`은 macOS 호스트 경로 기준으로 변환된 파일이다.
프로젝트 루트에는 `compile_commands.json`을 만들지 않는다.

수동 빌드:

```bash
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCATCHEYE_VISION_DETECTION_ENABLE_HAILO=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build/release -- -j$(nproc)
cmake --install build/release --prefix install/release
```

## 실행

CSI 카메라 + WebSocket + Hailo:

```bash
./install/release/bin/catcheye-guard \
  --camera \
  --ws \
  --hef ./install/release/models/yolo26m.hef \
  --metadata ./install/release/models/metadata.yaml
```

설치 패키지의 실행 스크립트:

```bash
./install/release/scripts/run-hailo.sh
```

뷰어 전용:

```bash
./install/release/bin/catcheye-guard \
  --viewer-only \
  --camera \
  --ws
```

이미지 파일 입력:

```bash
./install/release/bin/catcheye-guard \
  --image ./frame.jpg \
  --ws \
  --hef ./models/yolo26m.hef \
  --metadata ./models/metadata.yaml
```

ROI 파일을 직접 지정:

```bash
./install/release/bin/catcheye-guard \
  --camera \
  --ws \
  --hef ./models/yolo26m.hef \
  --metadata ./models/metadata.yaml \
  ./config/roi_cam_default.json
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
| `--roi-alert-pulse-ms <ms>` | GPIO pulse 시간을 지정한다. 기본값은 `100`이다. |
| `--gpio-chip <path>` | GPIO chip 경로를 지정한다. 기본값은 `/dev/gpiochip4`다. |
| `--roi-alert-active-low` | GPIO 출력을 active-low로 쓴다. |
| `--recording-dir <path>` | preview 녹화 디렉터리를 지정한다. |

## HTTP API

기본 주소는 `http://<host>:8090/api/`다.

```bash
curl http://<host>:8090/api/roi
curl http://<host>:8090/api/pallet-roi
curl http://<host>:8090/api/info
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
