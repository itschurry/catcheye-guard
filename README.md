## CatchEye Guard ROI 엔진 모듈

이 저장소에는 `include/catcheye/guard/roi` 및 `src/guard/roi` 아래에 재사용 가능한 ROI 엔진이 포함되어 있습니다.

### 제공 기능
- 도메인 모델: 포인트, ROI 폴리곤, 카메라 ROI 설정
- JSON 저장소: 경량 엔진 내장 JSON 파서를 이용한 로드/저장 및 파싱/직렬화
- 검증 기능: 잘못된 입력에 대한 구조화된 이슈 정보 제공
- 기하 연산: 점-폴리곤 포함 판정(오목 폴리곤 지원), 폴리곤 면적, 경계 계산, 자기 교차 검사
- 침입 후보 평가 헬퍼:
  - `evaluate_reference_point`
  - `evaluate_bbox_bottom_center`
  - `evaluate_bbox_fully_inside`

### 빠른 사용 예시
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

### 참고 사항
- ROI 포인트는 원본 이미지 좌표계를 기준으로 해석됩니다.
- 비활성화된 영역은 설정에는 유지되지만 평가 시에는 무시됩니다.
- 일반적인 잘못된 입력은 예외 대신 결과 구조체로 반환됩니다.
- 라이브 프리뷰 파이프라인에서는 바운딩 박스 전체가 활성 ROI 안에 포함될 때만 허용으로 판단합니다.

## 커밋 메시지 규칙

이 저장소는 로컬 git 설정 기준으로 아래 규칙을 사용합니다.

- 형식: `<type>(선택적 scope): 한국어 메시지`
- 예시: `feat: ROI 침입 판정 로직 추가`
- 주요 type: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `perf`, `ci`, `build`, `revert`

저장소의 `.githooks/commit-msg` 훅이 형식을 검사하고, `.gitmessage.txt` 템플릿이 기본 입력 예시를 제공합니다.

## ARM64 크로스 컴파일 가이드
요약:
- Docker 이미지는 ARM64 sysroot 안에 앱 의존성을 미리 준비한다.
- `libcamera`는 Ubuntu 패키지를 쓰지 않고 Raspberry Pi용 소스를 직접 크로스빌드해서 sysroot에 넣는다.
- 개발 PC에서는 그 sysroot를 사용해 크로스컴파일한다.
- `cmake --install` 결과물에는 앱과 함께 `ncnn`, `OpenCV`, `yaml-cpp`, `spdlog`, `fmt`, `libcamera` 런타임과 IPA/tuning 데이터를 포함한다.
- Raspberry Pi에는 카메라/미디어 런타임만 최소 설치하고 `install/aarch64`만 복사한다.

사용 중인 툴체인 파일:
- [`toolchains/aarch64-linux-gnu.cmake`](/home/user/catcheye-guard/toolchains/aarch64-linux-gnu.cmake)

### ARM 장비에서 할 일

장비에는 아래가 먼저 있어야 한다.
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
  libgstrtspserver-1.0-0 \
  gstreamer1.0-tools
```

실행 확인:

```bash
gst-inspect-1.0 libcamerasrc
```

이 프로젝트는 Docker 이미지 빌드 시 `raspberrypi/libcamera`를 소스 크로스빌드해서
`${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu` 아래에 설치한다.
`cmake --install` 단계에서 `libcamera` 라이브러리, IPA 모듈, tuning/config 데이터를
함께 번들하고, 설치된 `bin/catcheye-guard` 래퍼가 그 번들을 우선 사용하게 만든다.
그래서 Raspberry Pi에 별도로 같은 `libcamera`를 소스 빌드할 필요는 없다.

### 개발 PC에서 할 일

개발 PC에서는 `docker compose`로 이미지 빌드하고 컨테이너에 들어간다.

```bash
docker compose -f docker/docker-compose.dev.yml build
docker compose -f docker/docker-compose.dev.yml run --rm catcheye-guard-dev bash
```

이미지 빌드 과정에서 아래가 자동 준비된다.
- target sysroot: `/opt/sysroots/raspi`
- `ncnn`: `/opt/sysroots/raspi/usr`
- `libcamera`: `/opt/sysroots/raspi/usr/lib/aarch64-linux-gnu`

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

`cmake --install` 단계에서 `ncnn`, `OpenCV`, `yaml-cpp`, `spdlog`, `fmt`, `libcamera` 런타임과
`libcamera` IPA/tuning 데이터가 `install/aarch64` 아래로 함께 복사된다.

### 배포

개발 PC에서:

```bash
rsync -av install/aarch64/ user@arm-device:/opt/catcheye-guard/
```

ARM 장비에서 복사 결과 확인:

```bash
ls /opt/catcheye-guard
ls /opt/catcheye-guard/lib | grep ncnn
ls /opt/catcheye-guard/lib | grep libcamera
ls /opt/catcheye-guard/lib/aarch64-linux-gnu/libcamera
ls /opt/catcheye-guard/share/libcamera
```

### 실행

ARM 장비에서:

```bash
cd /opt/catcheye-guard
./bin/catcheye-guard --camera
```

설치된 `bin/catcheye-guard` 는 래퍼 스크립트다.
실제 바이너리는 `bin/catcheye-guard-bin` 이고, 래퍼가 아래 환경변수를 설정해서
번들된 `libcamera` 런타임을 우선 사용한다.

- `LD_LIBRARY_PATH`
- `LIBCAMERA_IPA_MODULE_PATH`
- `LIBCAMERA_IPA_PROXY_PATH`
- `LIBCAMERA_IPA_CONFIG_PATH`

### 실행 옵션

기본 형식:

```bash
./bin/catcheye-guard [입력 옵션] [부가 옵션] [model.param] [model.bin] [metadata.yaml] [roi.json]
```

입력 옵션:

- `--camera`
  - 카메라 입력을 사용한다.
- `--camera-pipeline <pipeline>`
  - 카메라 입력에서만 사용 가능하다.
- `--image <path>`
  - 이미지 파일 입력을 사용한다.
- `--video <path>`
  - 비디오 파일 입력을 사용한다.

부가 옵션:

- `--rtsp [port]`
  - RTSP 결과 송출을 켠다.
  - 포트를 생략하면 기본 포트를 사용한다.
- `--num-threads <n>`
  - NCNN 추론 스레드 수를 지정한다.

위치 인자:

- `model.param`
  - 기본 NCNN param 경로를 덮어쓴다.
- `model.bin`
  - 기본 NCNN bin 경로를 덮어쓴다.
- `metadata.yaml`
  - 기본 메타데이터 경로를 덮어쓴다.
- `roi.json`
  - 기본 ROI 설정 파일 경로를 덮어쓴다.

주의:

- 입력 모드 `--camera`, `--image`, `--video` 는 서로 동시에 쓸 수 없다.
- `--camera-pipeline` 은 `--camera` 와 같이 써야 한다.
- `--headless` 는 더 이상 지원하지 않는다.
- `--rtsp-with-preview` 는 더 이상 지원하지 않는다.

예시:

```bash
./bin/catcheye-guard --camera
./bin/catcheye-guard --camera --camera-pipeline "libcamerasrc ! video/x-raw,width=1280,height=720 ! appsink"
./bin/catcheye-guard --camera --rtsp 8554
./bin/catcheye-guard --video ./sample.mp4 --num-threads 4
./bin/catcheye-guard --image ./frame.jpg ./models/model.ncnn.param ./models/model.ncnn.bin ./models/metadata.yaml ./models/roi_cam_default.json
```

### 문제 생기면 확인

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
