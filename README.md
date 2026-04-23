## CatchEye Guard

Raspberry Pi ARM64 환경을 대상으로 빌드하고 배포하는 감시 애플리케이션이다.
개발 PC에서는 Docker 기반 sysroot를 사용해 크로스컴파일하고, ARM 장비에서는 설치 결과물을 그대로 실행한다.

### 빠른 시작

개발 PC에서:

```bash
docker compose -f docker/docker-compose.dev.yml build
docker compose -f docker/docker-compose.dev.yml run --rm catcheye-guard-dev bash

cmake -S /home/user/catcheye-guard -B /home/user/catcheye-guard/build/aarch64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/home/user/catcheye-guard/toolchains/aarch64-linux-gnu.cmake \
  -DTARGET_SYSROOT=/opt/sysroots/raspi

cmake --build /home/user/catcheye-guard/build/aarch64 -j$(nproc)
cmake --install /home/user/catcheye-guard/build/aarch64 --prefix /home/user/catcheye-guard/install/aarch64

rsync -av /home/user/catcheye-guard/install/aarch64/ user@arm-device:/opt/catcheye-guard/
```

ARM 장비에서:

```bash
cd /opt/catcheye-guard
./bin/catcheye-guard --camera
```

## 빌드/배포 개요

- Docker 이미지는 ARM64 sysroot 안에 앱 의존성을 미리 준비한다.
- `libcamera`는 Ubuntu 패키지 대신 Raspberry Pi용 소스를 직접 크로스빌드해 sysroot에 넣는다.
- 개발 PC에서는 그 sysroot를 사용해 앱을 크로스컴파일한다.
- `cmake --install` 결과물에는 앱과 함께 `ncnn`, `OpenCV`, `yaml-cpp`, `spdlog`, `fmt` 런타임을 포함한다.
- Raspberry Pi에는 `libcamera`와 카메라/미디어 런타임이 로컬에 준비되어 있어야 한다.

사용 중인 툴체인 파일:

- [`toolchains/aarch64-linux-gnu.cmake`](/home/user/catcheye-guard/toolchains/aarch64-linux-gnu.cmake)

## ARM 장비 준비

장비에는 아래 구성 요소가 먼저 설치되어 있어야 한다.

- `GStreamer`
- `gstreamer-rtsp-server`
- 카메라 / 인코더 관련 GStreamer 플러그인

Ubuntu 24.04 계열 예시:

```bash
sudo apt update
sudo apt install -y \
  gstreamer1.0-libav \
  gstreamer1.0-libcamera \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-ugly \
  libgstrtspserver-1.0-0 \
  gstreamer1.0-tools \
  libgpiod-dev \
  gpiod
```

설치 확인:

```bash
gst-inspect-1.0 libcamerasrc
```

참고:

- 이 프로젝트는 Docker 이미지 빌드 시 `raspberrypi/libcamera`를 소스 크로스빌드해 `${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu` 아래에 설치한다.
- 크로스빌드는 이 sysroot를 사용하지만, `cmake --install` 결과물에는 `libcamera` 런타임을 번들하지 않는다.
- Raspberry Pi에는 로컬 `libcamera`, IPA 모듈, tuning/config 데이터가 별도로 준비되어 있어야 한다.
- `gstreamer`의 `libcamerasrc`는 `libcamera` 런타임 세트와 강하게 결합되어 있으므로, Raspberry Pi에서 직접 구성한 `libcamera` 환경 기준으로 검증하는 것을 권장한다.

## 개발 PC에서 빌드

개발 PC에서는 `docker compose`로 이미지를 빌드한 뒤 컨테이너에 들어가 작업한다.

```bash
docker compose -f docker/docker-compose.dev.yml build
docker compose -f docker/docker-compose.dev.yml run --rm catcheye-guard-dev bash
```

이미지 빌드 과정에서 아래 항목이 자동으로 준비된다.

- target sysroot: `/opt/sysroots/raspi`
- `ncnn`: `/opt/sysroots/raspi/usr`
- `libcamera`: `/opt/sysroots/raspi/usr/lib/aarch64-linux-gnu`

GPIO 신호를 쓰려면 빌드 타임에 `libgpiod`가 반드시 필요하다.
`libgpiod` 누락 상태면 설정 단계에서 아래처럼 멈춘다.

```bash
cmake -S /home/user/catcheye-guard -B /home/user/catcheye-guard/build/aarch64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/home/user/catcheye-guard/toolchains/aarch64-linux-gnu.cmake \
  -DTARGET_SYSROOT=/opt/sysroots/raspi
```

핵심: 호스트와 `${TARGET_SYSROOT}` 둘 다 `libgpiod-dev`, `gpiod`가 있어야 한다.

설치 결과 확인:

```bash
ls /opt/sysroots/raspi/usr/lib/aarch64-linux-gnu/pkgconfig
ls /opt/sysroots/raspi/usr/lib/aarch64-linux-gnu/cmake/ncnn
ls /opt/sysroots/raspi/usr/lib/aarch64-linux-gnu/libcamera.so*
ls /opt/sysroots/raspi/usr/lib/aarch64-linux-gnu/libcamera
ls /opt/sysroots/raspi/usr/share/libcamera
```

컨테이너 안에서 앱을 크로스컴파일한다.

```bash
cmake -S /home/user/catcheye-guard -B /home/user/catcheye-guard/build/aarch64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/home/user/catcheye-guard/toolchains/aarch64-linux-gnu.cmake \
  -DTARGET_SYSROOT=/opt/sysroots/raspi

cmake --build /home/user/catcheye-guard/build/aarch64 -j$(nproc)
cmake --install /home/user/catcheye-guard/build/aarch64 --prefix /home/user/catcheye-guard/install/aarch64
```

`cmake --install` 단계에서는 `ncnn`, `OpenCV`, `yaml-cpp`, `spdlog`, `fmt` 런타임도 `install/aarch64` 아래로 함께 복사된다.

## 배포

개발 PC에서:

```bash
rsync -av install/aarch64/ user@arm-device:/opt/catcheye-guard/
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
libcamerasrc ! video/x-raw,width=640,height=480,framerate=15/1,format=NV12 ! videoflip video-direction=vert
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
./bin/catcheye-guard --camera --camera-pipeline "libcamerasrc ! video/x-raw,width=640,height=480,framerate=15/1,format=NV12 ! videoflip video-direction=vert" --ws 8080
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

## 문제 생기면 확인

`Could not find OpenCVConfig.cmake`

- `-DTARGET_SYSROOT=/opt/sysroots/raspi`
- `/opt/sysroots/raspi/usr/lib/aarch64-linux-gnu/cmake/opencv4`

`Could not find ncnn`

- `/opt/sysroots/raspi/usr/lib/aarch64-linux-gnu/cmake/ncnn/ncnnConfig.cmake`

`pkg-config`로 `libcamera` / `gstreamer`를 못 찾음

- `/opt/sysroots/raspi/usr/lib/aarch64-linux-gnu/pkgconfig`
- `toolchains/aarch64-linux-gnu.cmake`의 `PKG_CONFIG_LIBDIR`

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
