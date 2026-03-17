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

## ARM64 크로스 컴파일 가이드

이 프로젝트는 Ubuntu 24.04 (`noble`) x86_64 호스트에서 ARM64 (`aarch64`) 타깃으로 크로스 컴파일할 수 있습니다.

현재 빌드 구성은 다음을 사용합니다.
- CMake 3.20 이상
- C++20
- AArch64 GNU 크로스 컴파일러
- `OpenCV`
- `ncnn`
- `yaml-cpp`
- `spdlog`

현재 [`CMakeLists.txt`](/home/user/ros_ws/CMakeLists.txt#L1)에서는 다음 패키지들을 찾습니다.
- `find_package(OpenCV REQUIRED)`
- `find_package(ncnn REQUIRED)`
- `find_package(yaml-cpp REQUIRED)`
- `find_package(spdlog REQUIRED)`

사용 중인 툴체인 파일:
- [`toolchains/aarch64-linux-gnu.cmake`](/home/user/ros_ws/toolchains/aarch64-linux-gnu.cmake)

### 1. ARM64 apt 저장소 추가

Ubuntu 24.04에서 ARM64 패키지를 설치할 때 `archive.ubuntu.com`을 그대로 사용하면 `404 Not Found`가 발생할 수 있습니다. ARM64 패키지는 `ports.ubuntu.com/ubuntu-ports`를 사용하도록 별도 `.sources` 파일을 추가하는 방식을 권장합니다.

아래 명령으로 ARM64 전용 저장소 파일을 생성합니다.

```bash
sudo tee /etc/apt/sources.list.d/ubuntu-arm64.sources > /dev/null <<'EOF'
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: noble-security
Components: main universe restricted multiverse
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
```

그 다음 패키지 메타데이터를 갱신합니다.

```bash
sudo apt update
```

참고:
- 기존 호스트용 `amd64` 저장소는 그대로 두고, ARM64용 저장소만 별도 추가하는 방식입니다.
- ARM64 패키지 조회가 `archive.ubuntu.com`이 아니라 `ports.ubuntu.com/ubuntu-ports`로 향해야 합니다.

### 2. 크로스 컴파일러와 기본 빌드 도구 설치

아래 패키지 목록은 현재 [`docker/Dockerfile`](/home/user/ros_ws/docker/Dockerfile#L14) 내용과 일치하도록 정리한 것입니다.

```bash
sudo apt install -y \
  build-essential \
  clang \
  clang-format \
  clangd \
  cmake \
  file \
  git \
  ninja-build \
  pkg-config \
  gcc-13-aarch64-linux-gnu \
  g++-13-aarch64-linux-gnu \
  libc6-dev-arm64-cross \
  libstdc++-13-dev-arm64-cross
```

참고:
- 툴체인 파일은 `aarch64-linux-gnu-g++`와 `aarch64-linux-gnu-g++-13`를 모두 지원합니다.
- 크로스 빌드 시 제너레이터는 `Ninja` 사용을 권장합니다.

### 3. ARM64 개발 패키지 설치

이 저장소 기준으로 최소한 아래 패키지부터 준비하는 것이 좋습니다.

```bash
sudo apt install -y \
  libopencv-dev:arm64 \
  libyaml-cpp-dev:arm64 \
  libspdlog-dev:arm64
```

코드 경로나 타깃 런타임에 따라 아래 패키지도 추가로 필요할 수 있습니다.

```bash
sudo apt install -y \
  libgstreamer1.0-dev:arm64 \
  libgstreamer-plugins-base1.0-dev:arm64 \
  libjpeg-dev:arm64 \
  libpng-dev:arm64 \
  libtiff5-dev:arm64 \
  libglib2.0-dev:arm64
```

참고:
- ARM64용 `OpenCVConfig.cmake`는 일반적으로 `/usr/lib/aarch64-linux-gnu/cmake/opencv4`에 설치됩니다.
- `yaml-cpp` 또는 `spdlog`를 찾지 못하면 `:arm64` 개발 패키지가 실제로 설치되었는지 먼저 확인해야 합니다.
- configure 단계에서 호스트 `amd64` 라이브러리 경로와 타깃 `arm64` 경로가 섞이지 않도록 주의해야 합니다.

### 4. ARM64용 `ncnn` 준비

[`CMakeLists.txt`](/home/user/ros_ws/CMakeLists.txt#L57)에서 `ncnn`은 필수 의존성입니다. 다만 `OpenCV`, `spdlog`와 달리 Ubuntu 기본 저장소에서 바로 타깃 환경에 맞는 형태로 준비되지 않는 경우가 많습니다.

보통 아래 두 가지 방식 중 하나를 사용합니다.

1. 동일한 툴체인으로 `ncnn`을 ARM64용으로 직접 빌드하고 `/opt/ncnn-aarch64` 같은 prefix에 설치
2. 이미 `ncnnConfig.cmake`를 포함한 ARM64용 사전 빌드 패키지나 sysroot 사용

예시 크로스 컴파일 configure 명령:

```bash
cmake -S /path/to/ncnn -B /path/to/ncnn/build-aarch64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/home/user/ros_ws/toolchains/aarch64-linux-gnu.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/ncnn-aarch64 \
  -DNCNN_VULKAN=OFF \
  -DNCNN_BUILD_TOOLS=OFF \
  -DNCNN_BUILD_EXAMPLES=OFF
```

설치 후 메인 프로젝트 configure 시 아래처럼 지정합니다.

```bash
-Dncnn_DIR=/opt/ncnn-aarch64/lib/cmake/ncnn
```

### 5. ARM64용 프로젝트 configure

권장 configure 예시는 다음과 같습니다.

```bash
cmake -S /home/user/ros_ws -B /home/user/ros_ws/build/aarch64 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/home/user/ros_ws/toolchains/aarch64-linux-gnu.cmake \
  -DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4 \
  -Dncnn_DIR=/opt/ncnn-aarch64/lib/cmake/ncnn
```

`yaml-cpp`나 `spdlog`가 기본 타깃 경로가 아닌 곳에 설치된 경우에는 아래 옵션을 추가할 수 있습니다.

```bash
-DCMAKE_PREFIX_PATH="/usr/lib/aarch64-linux-gnu/cmake;/opt/ncnn-aarch64/lib/cmake"
```

빌드는 다음과 같이 수행합니다.

```bash
cmake --build /home/user/ros_ws/build/aarch64 -j$(nproc)
```

### 6. 현재 툴체인 파일 동작 방식

현재 툴체인 파일은 다음과 같이 동작합니다.
- `CMAKE_SYSTEM_NAME`을 `Linux`로 설정
- `CMAKE_SYSTEM_PROCESSOR`를 `aarch64`로 설정
- `aarch64-linux-gnu-gcc` 또는 `aarch64-linux-gnu-gcc-13` 탐색
- `aarch64-linux-gnu-g++` 또는 `aarch64-linux-gnu-g++-13` 탐색
- `CMAKE_FIND_ROOT_PATH`로 `/usr/aarch64-linux-gnu` 사용

파일 참고:
- [`toolchains/aarch64-linux-gnu.cmake`](/home/user/ros_ws/toolchains/aarch64-linux-gnu.cmake)

이 프로젝트는 CMake 패키지 설정 파일에 많이 의존하므로, 실제로 가장 자주 실패하는 지점은 컴파일러 자체보다 타깃 아키텍처용 패키지 탐색입니다.

### 7. 자주 발생하는 오류

`The CMAKE_CXX_COMPILER ... was not found in the PATH`
- `g++-13-aarch64-linux-gnu`를 설치합니다.
- 툴체인 파일을 지정해서 configure를 다시 실행합니다.

`Could not find OpenCVConfig.cmake`
- `libopencv-dev:arm64`를 설치합니다.
- `-DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4`를 지정합니다.

`Could not find yaml-cpp`
- `libyaml-cpp-dev:arm64`를 설치합니다.
- ARM64 패키지 경로 아래에서 config 파일이 보이는지 확인합니다.

`Could not find ncnn`
- `ncnn`을 ARM64용으로 별도 빌드 및 설치합니다.
- `-Dncnn_DIR=<arm64-ncnn-config-dir>`를 지정합니다.

`apt update` 시 ARM64 패키지에서 `404 Not Found` 발생
- `arm64` 저장소가 `archive.ubuntu.com`을 가리키고 있을 가능성이 큽니다.
- ARM 저장소를 `http://ports.ubuntu.com/ubuntu-ports`로 변경합니다.

### 8. Docker 참고

현재 [`docker/Dockerfile`](/home/user/ros_ws/docker/Dockerfile#L48)에는 크로스 컴파일에 필요한 주요 도구들이 이미 포함되어 있습니다.
- `gcc-13-aarch64-linux-gnu`
- `g++-13-aarch64-linux-gnu`
- `libc6-dev-arm64-cross`
- `libstdc++-13-dev-arm64-cross`
- `cmake`
- `ninja-build`
- `pkg-config`

다만 `Dockerfile`에 있는 `libopencv-dev` 같은 패키지는 호스트용 패키지일 수 있으므로, 이 프로젝트를 ARM64로 크로스 컴파일하려면 CMake가 참조할 ARM64 대상 개발 패키지 또는 ARM64 sysroot를 별도로 준비해야 합니다.
