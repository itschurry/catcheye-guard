## CatchEye Guard

Raspberry Pi ARM64 환경을 대상으로 빌드하고 배포하는 감시 애플리케이션이다.
개발 PC(Windows 포함)에서는 Docker buildx/compose로 `linux/arm64` 개발 이미지를 빌드하고, 컨테이너 안에서 ARM64 타겟 빌드를 수행한다.
실행 대상 Raspberry Pi는 `Raspberry Pi OS + hailo-all + 카메라 런타임 + catcheye-guard` 구성을 기준으로 한다.

### 빠른 시작

개발 PC에서:

```bash
# x86_64 PC에서 arm64 컨테이너를 빌드/실행할 때 한 번만 필요하다.
docker run --privileged --rm tonistiigi/binfmt --install arm64

docker compose -f docker/docker-compose.dev.yml build
docker compose -f docker/docker-compose.dev.yml run --rm catcheye-guard-dev bash

cmake -S /home/user/catcheye-guard -B /home/user/catcheye-guard/build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release

cmake --build /home/user/catcheye-guard/build/release -j$(nproc)
cmake --install /home/user/catcheye-guard/build/release --prefix /home/user/catcheye-guard/install/release

rsync -av /home/user/catcheye-guard/install/release/ user@arm-device:/opt/catcheye-guard/
```

ARM 장비에서:

```bash
cd /opt/catcheye-guard
./bin/catcheye-guard --camera
```

## 빌드/배포 개요

- Docker 이미지는 `linux/arm64` 플랫폼으로 빌드한다.
- `libcamera`는 Raspberry Pi용 소스를 arm64 컨테이너 안에서 네이티브 빌드해 설치한다.
- 개발 PC에서는 QEMU/binfmt 기반 arm64 컨테이너 안에서 앱을 빌드한다.
- Raspberry Pi 런타임 기준은 `hailo-all`이다.
- `cmake --install` 결과물에는 앱, 모델, 설정, 그리고 직접 빌드한 `ncnn` 런타임만 포함한다.
- `OpenCV`, `GStreamer`, `libcamera`, `HailoRT`, `libstdc++` 등 시스템 스택은 Raspberry Pi에 설치된 패키지를 사용한다.

## ARM 장비 준비

장비에는 `hailo-all`과 카메라 런타임, 의존 라이브러리 들이 먼저 설치되어 있어야 한다.
Hailo가 없는 장비도 같은 기준으로 맞춘다. 저장공간보다 런타임 충돌 줄이는 게 더 싸다.

```bash
sudo apt update
sudo apt install -y hailo-all

sudo apt install -y libyaml-cpp-dev libgstrtspserver-1.0-dev libspdlog-dev
```

설치 확인:

```bash
gst-inspect-1.0 libcamerasrc
python3 - <<'PY'
import cv2
print(cv2.__version__)
PY
hailortcli fw-control identify || true
```

### libcamera 설치

기본은 배포판 패키지를 먼저 쓴다.

```bash
sudo apt update
sudo apt install -y \
  libcamera0.7 \
  libcamera-tools \
  gstreamer1.0-libcamera
```

설치 확인:

```bash
cam --list
gst-inspect-1.0 libcamerasrc
```

Raspberry Pi 카메라가 패키지 버전과 맞지 않으면 `raspberrypi/libcamera`를 직접 빌드한다.
Docker 이미지는 아래와 같은 옵션으로 같은 저장소를 arm64 네이티브 빌드한다.

ARM 장비에서 직접 빌드할 때:

```bash
sudo apt update
sudo apt install -y \
  git meson ninja-build pkg-config \
  g++ python3-yaml python3-ply python3-jinja2 python3-pyelftools \
  libgnutls28-dev openssl libtiff5-dev libevent-dev \
  libdrm-dev libexif-dev libjpeg-dev libpng-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev

git clone --depth 1 https://github.com/raspberrypi/libcamera.git
meson setup libcamera/build libcamera \
  --buildtype=release \
  --prefix /usr \
  --libdir lib/aarch64-linux-gnu \
  -Dpipelines=rpi/vc4,rpi/pisp \
  -Dipas=rpi/vc4,rpi/pisp \
  -Dv4l2=enabled \
  -Dgstreamer=enabled \
  -Dtest=false \
  -Dlc-compliance=disabled \
  -Dcam=enabled \
  -Dqcam=disabled \
  -Ddocumentation=disabled \
  -Dpycamera=disabled

ninja -C libcamera/build -j$(nproc)
sudo meson install -C libcamera/build
sudo ldconfig
```

설치 후 확인:

```bash
cam --list
gst-inspect-1.0 libcamerasrc
ls /usr/lib/aarch64-linux-gnu/libcamera.so*
ls /usr/share/libcamera
```

`raspberrypi/libcamera`를 `/usr/local` 기준으로 설치한 장비라면 GStreamer가 `libcamerasrc` 플러그인을 찾도록 아래 경로가 필요하다.
배포용 `catcheye-guard` 런처는 이 값을 자동으로 잡는다.

```bash
export GST_PLUGIN_PATH=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0:/usr/lib/aarch64-linux-gnu/gstreamer-1.0:$GST_PLUGIN_PATH
```

### Hailo PCIe 드라이버

Hailo 백엔드를 쓰려면 Raspberry Pi에서 Hailo-8/8L PCIe 드라이버가 먼저 잡혀야 한다.
`hailo-all`은 런타임 스택을 준비하지만, PCIe 장치가 실제로 잡혔는지는 반드시 확인해야 한다.

커널 헤더와 DKMS 준비:

```bash
sudo apt install -y linux-image-raspi linux-headers-raspi
sudo reboot
```

재부팅 후 드라이버 빌드 도구 설치:

```bash
sudo apt install -y dkms build-essential
```

Hailo PCIe 드라이버 패키지 설치:

```bash
sudo dpkg -i ~/Downloads/hailort-pcie-driver_5.3.0_all.deb
```

설치 확인:

```bash
lsmod | grep hailo
lspci | grep -i hailo
ls -l /dev/hailo*
hailortcli fw-control identify
```

`lspci`에는 Hailo가 보이는데 `/dev/hailo*`나 `hailortcli fw-control identify`가 안 되면 앱 문제가 아니다.
드라이버, DKMS, PCIe, 전원, 커널 헤더 쪽부터 다시 봐야 한다.

참고:

- 이 프로젝트는 Docker 이미지 빌드 시 `raspberrypi/libcamera`를 arm64 컨테이너 안에서 소스 빌드해 `/usr/lib/aarch64-linux-gnu` 아래에 설치한다.
- `cmake --install` 결과물에는 `libcamera`, `OpenCV`, `GStreamer`, `HailoRT` 런타임을 번들하지 않는다.
- Raspberry Pi에서는 `hailo-all`과 로컬 카메라 런타임을 기준으로 실행한다.

## 개발 PC에서 빌드

개발 PC에서는 `docker compose`로 이미지를 빌드한 뒤 컨테이너에 들어가 작업한다.
`docker/docker-compose.base.yml`에서 `platform: linux/arm64`를 고정하므로 Docker는 arm64 이미지를 빌드한다.

x86_64 PC라면 QEMU/binfmt가 필요하다. 이거 안 해두면 `exec /bin/bash: exec format error`가 난다.

```bash
docker run --privileged --rm tonistiigi/binfmt --install arm64
docker buildx inspect --bootstrap
```

```bash
docker compose -f docker/docker-compose.dev.yml build
docker compose -f docker/docker-compose.dev.yml run --rm catcheye-guard-dev bash
```

이미지 빌드 과정에서 아래 항목이 자동으로 준비된다.

- `ncnn`: `/usr/local`
- `libcamera`: `/usr/lib/aarch64-linux-gnu`

Hailo 백엔드를 빌드하려면 HailoRT arm64 개발 패키지가 컨테이너에 있어야 한다.
Hailo 패키지는 Docker 이미지가 자동으로 내려받지 않는다.

1. Hailo 개발자 페이지에서 `hailort_x.x.x_arm64.deb` 를 다운로드한다.
   - 다운로드 URL: https://hailo.ai/developer-zone/software-downloads/?product=ai_accelerators&device=hailo_8_8l
   - 예: `hailort_5.3.0_arm64.deb`
2. 컨테이너 안에서 설치한다.

```bash
sudo apt install ./hailort_x.x.x_arm64.deb
```

설치 확인:

```bash
ls /usr/include/hailo/hailort.hpp
ls /usr/lib/libhailort.so*
ls /usr/lib/cmake/HailoRT/HailoRTConfig.cmake
```

GPIO 신호를 쓰려면 빌드 타임에 `libgpiod`가 반드시 필요하다.
`libgpiod` 누락 상태면 설정 단계에서 아래처럼 멈춘다.

```bash
cmake -S /home/user/catcheye-guard -B /home/user/catcheye-guard/build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
```

핵심: 컨테이너 안에 `libgpiod-dev`, `gpiod`가 있어야 한다.

설치 결과 확인:

```bash
ls /usr/lib/aarch64-linux-gnu/pkgconfig
ls /usr/local/lib/aarch64-linux-gnu/cmake/ncnn
ls /usr/lib/aarch64-linux-gnu/libcamera.so*
ls /usr/lib/aarch64-linux-gnu/libcamera
ls /usr/share/libcamera
```

컨테이너 안에서 앱을 빌드한다.

```bash
cmake -S /home/user/catcheye-guard -B /home/user/catcheye-guard/build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release

cmake --build /home/user/catcheye-guard/build/release -j$(nproc)
cmake --install /home/user/catcheye-guard/build/release --prefix /home/user/catcheye-guard/install/release
```

`cmake --install` 단계에서는 앱, 모델, 설정, `libncnn.so*`만 `install/release` 아래로 준비한다.
나머지 런타임은 Raspberry Pi의 `hailo-all`/시스템 패키지를 쓴다.

Hailo 백엔드를 포함하려면 HailoRT 설치 후 별도 빌드 디렉터리를 쓴다.

```bash
cmake -S /home/user/catcheye-guard -B /home/user/catcheye-guard/build/release-hailo -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCATCHEYE_VISION_DETECTION_ENABLE_HAILO=ON

cmake --build /home/user/catcheye-guard/build/release-hailo -j$(nproc)
cmake --install /home/user/catcheye-guard/build/release-hailo --prefix /home/user/catcheye-guard/install/release-hailo
```

VS Code task도 같은 구조다.

- `cmake: configure[Release]` / `cmake: build[Release]` / `cmake: install[Release]`
- `cmake: configure[Release,Hailo]` / `cmake: build[Release,Hailo]` / `cmake: install[Release,Hailo]`

## 배포

개발 PC에서:

```bash
rsync -av install/release/ user@arm-device:/opt/catcheye-guard/
```

ARM 장비에서 복사 결과 확인:

```bash
ls /opt/catcheye-guard
ls /opt/catcheye-guard/lib | grep ncnn
```

## 실행

ARM 장비에서:

```bash
cd /opt/catcheye-guard
./bin/catcheye-guard --camera
```

실행 파일 구조:

- 사용자는 `bin/catcheye-guard` 를 실행하면 된다.
- `bin/catcheye-guard` 는 실행 진입점 역할을 하는 래퍼 스크립트다.
- 래퍼 스크립트는 필요한 라이브러리 경로를 설정한 뒤 실제 바이너리 `bin/catcheye-guard-bin` 을 실행한다.
- `bin/catcheye-guard-bin` 은 내부용 실행 파일이므로, 특별한 이유가 없으면 직접 실행하지 않는 것을 권장한다.
- `libcamera` 관련 런타임은 Raspberry Pi 로컬 환경을 그대로 사용한다.

## 실행 옵션

기본 형식:

```bash
./bin/catcheye-guard [입력 옵션] [부가 옵션] [model.param] [model.bin] [metadata.yaml] [roi.json]
```

입력 옵션:

- `--camera`: 카메라 입력을 사용한다. 기본 CSI 카메라 모드에서는 180도 회전을 고정 보정한다.
- `--camera-pipeline <pipeline>`: 카메라 입력에서만 사용할 수 있다.
- `--camera-device <path>`: 카메라 입력에서만 사용할 수 있으며, USB 카메라를 GStreamer `v4l2src` 로 연다.
- `--camera-width <width>`: 카메라 입력 해상도 너비를 지정한다.
- `--camera-height <height>`: 카메라 입력 해상도 높이를 지정한다.
- `--image <path>`: 이미지 파일 입력을 사용한다.
- `--video <path>`: 비디오 파일 입력을 사용한다.

부가 옵션:

- `--rtsp [port]`: RTSP 결과 송출을 켠다. 포트를 생략하면 기본 포트를 사용한다.
- `--ws [port]`: WebSocket 결과 송출을 켠다. 포트를 생략하면 기본 포트를 사용한다.
- `--detector <ncnn|hailo>`: detector 백엔드를 선택한다. 기본값은 `ncnn` 이다.
- `--hef <path>`: Hailo 백엔드에서 사용할 HEF 모델 경로를 지정한다.
- `--metadata <path>`: 클래스 이름 메타데이터 YAML 경로를 지정한다.
- `--num-threads <n>`: NCNN 추론 스레드 수를 지정한다.
- `--roi-alert-gpio <line>`: ROI 이탈이 처음 감지될 때 pulse를 보낼 GPIO line 번호를 지정한다. 비활성화하려면 생략하거나 `-1`을 쓴다.
- `--roi-alert-pulse-ms <ms>`: ROI 알림 GPIO pulse 길이를 밀리초 단위로 지정한다.
- `--roi-alert-active-low`: active-low 출력으로 GPIO를 요청한다.
- `--gpio-chip <path>`: 기본 GPIO 칩 경로(`/dev/gpiochip0`)를 덮어쓴다.

위치 인자:

- `model.param`: 기본 NCNN param 경로를 덮어쓴다.
- `model.bin`: 기본 NCNN bin 경로를 덮어쓴다.
- `metadata.yaml`: 기본 메타데이터 경로를 덮어쓴다.
- `roi.json`: 기본 ROI 설정 파일 경로를 덮어쓴다.

제약 사항:

- 입력 모드 `--camera`, `--image`, `--video` 는 서로 동시에 쓸 수 없다.
- `--camera-pipeline`, `--camera-device` 는 `--camera` 와 같이 써야 한다.
- `--camera-pipeline` 과 `--camera-device` 는 함께 쓸 수 없다.
- `--rtsp` 와 `--ws` 는 함께 쓸 수 없다.
- `--camera-width`, `--camera-height` 는 `--camera` 와 함께만 사용할 수 있다.
- `--camera-pipeline` 사용 시 `--camera-width`, `--camera-height` 는 지원하지 않는다.
- `--camera-width`, `--camera-height` 는 짝수 양수여야 한다.
- `--roi-alert-gpio` 는 `-1` 또는 0 이상의 GPIO line 번호여야 한다.
- `--roi-alert-pulse-ms` 는 0 이상의 값이어야 한다.
- Hailo 백엔드는 빌드 시 `-DCATCHEYE_VISION_DETECTION_ENABLE_HAILO=ON` 이 필요하다.
- Hailo 백엔드를 실행하려면 Raspberry Pi에 HailoRT와 PCIe 드라이버가 설치되어 있어야 한다.
- `--headless` 는 더 이상 지원하지 않는다.
- `--rtsp-with-preview` 는 더 이상 지원하지 않는다.
- `--ws-with-preview` 는 더 이상 지원하지 않는다.

입력 소스 선택 규칙:

- `--camera` 만 사용하면 CSI 카메라를 `libcamera` 로 연다.
- `--camera --camera-pipeline ...` 을 사용하면 CSI 카메라를 `gstreamer` 로 연다.
- `--camera --camera-device /dev/videoX` 를 사용하면 USB 카메라를 `gstreamer v4l2src` 로 연다.
- `--image`, `--video` 는 항상 `gstreamer` 입력이다.
- `--rtsp` 는 입력 소스를 바꾸지 않고 출력만 RTSP 송출로 바꾼다.
- `--ws` 는 입력 소스를 바꾸지 않고 출력만 WebSocket 송출로 바꾼다.

WebSocket 송출 형식:

- 클라이언트는 `ws://<host>:<port>/` 로 접속한다.
- 프레임마다 텍스트 프레임 1개와 바이너리 프레임 1개를 순서대로 받는다.
- 텍스트 프레임에는 `frame_index`, `stream_name`, `width`, `height`, `stride`, `pixel_format`, `timestamp`, `payload_encoding`, `payload_size`, `metadata` 가 JSON으로 담긴다.
- 바이너리 프레임에는 JPEG 인코딩된 이미지 바이트가 담긴다.

## 권장 실행 예시

권장 CSI GStreamer 입력 파이프라인:

```bash
libcamerasrc ! video/x-raw,width=1920,height=1080,framerate=10/1,format=NV12 ! videoflip video-direction=vert
```

참고:

- `--camera-pipeline` 문자열에는 입력 파이프라인만 넣는다.
- 끝에 `appsink` 를 붙이지 않는다.
- 현재 앱이 내부에서 `appsink` 를 자동으로 붙인다.
- 아래 파이프라인 예시는 Raspberry Pi 로컬 `libcamera` 환경에서 검증했던 `NV12 + videoflip` 기준이다.
- `--camera-pipeline` 은 입력 파이프라인 문자열만 주입하므로, 고급 옵션으로 취급하는 것을 권장한다.

CSI 카메라 + `libcamera` 소스:

```bash
./bin/catcheye-guard --camera --camera-width 1280 --camera-height 720
```

CSI 카메라 + `gstreamer` 소스 + RTSP 송출:

```bash
./bin/catcheye-guard --camera --camera-pipeline "libcamerasrc ! video/x-raw,width=640,height=480,framerate=15/1,format=NV12 ! videoflip video-direction=vert" --rtsp 8554
```

CSI 카메라 + `gstreamer` 소스 + WebSocket 송출:

```bash
./bin/catcheye-guard --camera --camera-pipeline "libcamerasrc ! video/x-raw,width=1920,height=1080,framerate=10/1,format=NV12 ! videoflip video-direction=vert" --ws --detector ncnn
```

CSI 카메라 + `gstreamer` 소스 + WebSocket 송출 + Hailo 백엔드:

```bash
./bin/catcheye-guard --camera --camera-pipeline "libcamerasrc ! video/x-raw,width=1920,height=1080,framerate=10/1,format=NV12 ! videoflip video-direction=vert" --ws --detector hailo --hef ./models/model.hef --metadata ./models/metadata.yaml
```

USB 카메라 + `gstreamer` 소스:

```bash
./bin/catcheye-guard --camera --camera-device /dev/video0 --camera-width 960 --camera-height 540
```

USB 카메라 + `gstreamer` 소스 + RTSP 송출:

```bash
./bin/catcheye-guard --camera --camera-device /dev/video0 --camera-width 960 --camera-height 540 --rtsp 8554
```

USB 카메라 + `gstreamer` 소스 + WebSocket 송출:

```bash
./bin/catcheye-guard --camera --camera-device /dev/video0 --camera-width 960 --camera-height 540 --ws 8080
```

ROI 이탈 시 GPIO18로 100ms pulse 출력:

```bash
./bin/catcheye-guard --camera --roi-alert-gpio 18 --roi-alert-pulse-ms 100
```

이미지 파일 입력:

```bash
./bin/catcheye-guard --image ./frame.jpg
```

이미지 파일 입력 + RTSP 송출:

```bash
./bin/catcheye-guard --image ./frame.jpg --rtsp 8554
```

이미지 파일 입력 + WebSocket 송출:

```bash
./bin/catcheye-guard --image ./frame.jpg --ws 8080
```

동영상 파일 입력:

```bash
./bin/catcheye-guard --video ./sample.mp4
```

동영상 파일 입력 + RTSP 송출:

```bash
./bin/catcheye-guard --video ./sample.mp4 --rtsp 8554
```

동영상 파일 입력 + WebSocket 송출:

```bash
./bin/catcheye-guard --video ./sample.mp4 --ws 8080
```

모델/메타데이터/ROI 경로를 함께 넘기는 예시:

```bash
./bin/catcheye-guard --image ./frame.jpg ./models/model.ncnn.param ./models/model.ncnn.bin ./models/metadata.yaml ./models/roi_cam_default.json
```

Hailo HEF 모델을 쓰는 예시:

```bash
./bin/catcheye-guard --camera --camera-pipeline "libcamerasrc ! video/x-raw,width=1920,height=1080,framerate=10/1,format=NV12 ! videoflip video-direction=vert" --ws --detector hailo --hef ./models/model.hef --metadata ./models/metadata.yaml
```

## 문제 생기면 확인

`Could not find OpenCVConfig.cmake`

- `libopencv-dev` 설치 상태를 확인한다.
- `/usr/lib/aarch64-linux-gnu/cmake/opencv4`

`Could not find ncnn`

- `/usr/local/lib/aarch64-linux-gnu/cmake/ncnn/ncnnConfig.cmake`

`Could not find HailoRT`

- HailoRT SDK/런타임 설치 상태를 확인한다.
- `hailort_*.deb` 를 컨테이너에 설치했는지 확인한다.
- 이 패키지는 CMake config를 보통 `/usr/lib/cmake/HailoRT` 아래에 둔다.
- CMake가 못 찾으면 `-DHailoRT_DIR=/usr/lib/cmake/HailoRT` 를 넘긴다.

`pkg-config`로 `libcamera` / `gstreamer`를 못 찾음

- `/usr/local/lib/aarch64-linux-gnu/pkgconfig`
- `/usr/lib/aarch64-linux-gnu/pkgconfig`

`libcamerasrc` 오류

- `gst-inspect-1.0 libcamerasrc`

## ROI 엔진 모듈

이 저장소에는 `include/catcheye/guard/roi` 및 `src/guard/roi` 아래에 재사용 가능한 ROI 엔진이 포함되어 있습니다.

제공 기능:

- 도메인 모델: 포인트, ROI 폴리곤, 카메라 ROI 설정
- JSON 저장소: 경량 엔진 내장 JSON 파서를 이용한 로드/저장 및 파싱/직렬화
- 검증 기능: 잘못된 입력에 대한 구조화된 이슈 정보 제공
- 기하 연산: 점-폴리곤 포함 판정(오목 폴리곤 지원), 폴리곤 면적, 경계 계산, 자기 교차 검사
- 침입 후보 평가 헬퍼: `evaluate_reference_point`, `evaluate_bbox_bottom_center`, `evaluate_bbox_fully_inside`

빠른 사용 예시:

```cpp
#include "catcheye/guard/roi/roi_repository.hpp"
#include "catcheye/guard/roi/roi_evaluator.hpp"

using namespace catcheye::guard::roi;

auto parsed = RoiRepository::load_from_file("roi_cam_01.json");
if (!parsed.success) {
    // 파싱 에러 처리
}

EvaluationResult decision = evaluate_bbox_fully_inside(100, 50, 80, 150, parsed.config);
// Allowed / Restricted / Invalid
```

참고:

- ROI 포인트는 원본 이미지 좌표계를 기준으로 해석된다.
- 비활성화된 영역은 설정에는 유지되지만 평가 시에는 무시된다.
- 일반적인 잘못된 입력은 예외 대신 결과 구조체로 반환된다.
- 라이브 프리뷰 파이프라인에서는 바운딩 박스 전체가 활성 ROI 안에 포함될 때만 허용으로 판단한다.

## 커밋 메시지 규칙

이 저장소는 로컬 git 설정 기준으로 아래 규칙을 사용한다.

- 형식: `<type>(선택적 scope): 한국어 메시지`
- 예시: `feat: ROI 침입 판정 로직 추가`
- 주요 type: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `perf`, `ci`, `build`, `revert`

저장소의 `.githooks/commit-msg` 훅이 형식을 검사하고, `.gitmessage.txt` 템플릿이 기본 입력 예시를 제공한다.
