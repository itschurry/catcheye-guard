# FROM ghcr.io/itschurry/ros:jazzy
FROM ubuntu:24.04
LABEL authors="itschurry"
LABEL maintainer="itschurry"
ARG DEBIAN_FRONTEND=noninteractive

RUN id -u && id -g
ARG UID=1000
ARG GID=1000
ARG USERNAME=appuser
ARG HOME=/home/${USERNAME}
# ----------------------------------------------------------------------------------------------
# [시스템 패키지]
RUN set -eux; \
    dpkg --add-architecture arm64; \
    printf '%s\n' \
    'Types: deb' \
    'URIs: http://ftp.kaist.ac.kr/ubuntu/' \
    'Suites: noble noble-updates noble-backports' \
    'Components: main universe restricted multiverse' \
    'Architectures: amd64' \
    'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' \
    '' \
    'Types: deb' \
    'URIs: http://ftp.kaist.ac.kr/ubuntu/' \
    'Suites: noble-security' \
    'Components: main universe restricted multiverse' \
    'Architectures: amd64' \
    'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' \
    > /etc/apt/sources.list.d/ubuntu.sources; \
    printf '%s\n' \
    'Types: deb' \
    'URIs: http://ports.ubuntu.com/ubuntu-ports/' \
    'Suites: noble noble-updates noble-backports' \
    'Components: main universe restricted multiverse' \
    'Architectures: arm64' \
    'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' \
    '' \
    'Types: deb' \
    'URIs: http://ports.ubuntu.com/ubuntu-ports/' \
    'Suites: noble-security' \
    'Components: main universe restricted multiverse' \
    'Architectures: arm64' \
    'Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg' \
    > /etc/apt/sources.list.d/ubuntu-arm64.sources; \
    apt-get update; \
    \
    # [기본 유틸리티] 시스템 / 권한 / 패키지 관리
    apt-get install -y --no-install-recommends \
    apt-utils gettext gnupg2 lsb-release software-properties-common \
    sudo udev; \
    \
    # [기본 유틸리티] 다운로드 / 압축 / 전송
    apt-get install -y --no-install-recommends \
    curl tar unzip wget xz-utils; \
    \
    # [기본 유틸리티] 터미널 / 편집 / 검색
    apt-get install -y --no-install-recommends \
    expect htop python3-tk ripgrep tmux tree universal-ctags vim xclip; \
    \
    # [개발 / 디버깅 도구] 컴파일 / 빌드 기본
    apt-get install -y --no-install-recommends \
    build-essential clang clang-format clangd cmake file g++ git pkg-config; \
    \
    # [개발 / 디버깅 도구] 디버깅 / 네트워크 / 장치 확인
    apt-get install -y --no-install-recommends \
    can-utils gdb iproute2 kmod net-tools usbutils v4l-utils; \
    \
    # [개발 / 디버깅 도구] autotools / 보조 라이브러리
    apt-get install -y --no-install-recommends \
    autoconf automake doxygen libelf-dev libmuparser-dev \
    libpopt-dev libtool; \
    \
    # [X11 / GUI] 표시 / 렌더링 / 로그
    apt-get install -y --no-install-recommends \
    guvcview libglvnd-dev libspdlog-dev libx11-dev libxext-dev \
    mesa-utils x11-apps; \
    \
    # [빌드 시스템 / 크로스 컴파일] 빌드 툴체인
    apt-get install -y --no-install-recommends \
    meson ninja-build; \
    \
    # [빌드 시스템 / 크로스 컴파일] ARM64 크로스 컴파일러 / sysroot
    apt-get install -y --no-install-recommends \
    gcc-13-aarch64-linux-gnu g++-13-aarch64-linux-gnu \
    libc6-dev-arm64-cross libstdc++-13-dev-arm64-cross; \
    \
    # [빌드 시스템 / 크로스 컴파일] ARM64 대상 라이브러리
    # Ubuntu 24.04에서는 libglib2.0-dev:arm64 계열이 co-install되지 않아
    # gstreamer/glib ARM64 dev 패키지는 별도 sysroot 또는 ARM64 환경에서 준비한다.
    apt-get install -y --no-install-recommends \
    libopencv-dev:arm64 libyaml-cpp-dev:arm64 libspdlog-dev:arm64 \
    libjpeg-dev:arm64 libpng-dev:arm64 libtiff5-dev:arm64; \
    \
    # [libcamera / rpicam] Python 보조 패키지
    apt-get install -y --no-install-recommends \
    pybind11-dev python3-gpiozero python3-jinja2 python3-pip \
    python3-ply python3-yaml; \
    \
    # [libcamera / rpicam] 카메라 / 멀티미디어 / 이미지 코덱
    apt-get install -y --no-install-recommends \
    gstreamer1.0-libav gstreamer1.0-libcamera \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good gstreamer1.0-tools \
    libboost-dev libboost-program-options-dev libdrm-dev \
    libepoxy-dev libexif-dev libglib2.0-dev libgnutls28-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libjpeg-dev libopencv-dev libpng-dev libtiff5-dev openssl; \
    \
    # [libcamera / rpicam] Qt UI / 배포 보조 도구
    apt-get install -y --no-install-recommends \
    libqt5core5a libqt5gui5 libqt5widgets5 qtbase5-dev \
    rsync sshpass; \
    \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*

RUN userdel -r ubuntu || true && groupdel ubuntu || true

RUN if getent group $GID; then \
    groupmod -n $USERNAME $(getent group $GID | cut -d: -f1); \
    else \
    groupadd -g $GID $USERNAME; \
    fi

RUN useradd -m -u $UID -g $GID -d $HOME -s /bin/bash $USERNAME && \
    echo "$USERNAME ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/$USERNAME && \
    chmod 0440 /etc/sudoers.d/$USERNAME && \
    usermod -aG sudo,video,render,dialout,plugdev $USERNAME

# COPY --chown=$USERNAME:$USERNAME ros_settings.sh /ros_settings.sh
# # User가 소유자이므로 sudo 불필요
# RUN chmod +x /ros_settings.sh

# COPY --chown=$USERNAME:$USERNAME entrypoint.sh /entrypoint.sh
# RUN chmod +x /entrypoint.sh

USER $USERNAME
WORKDIR $HOME
# ------------------------------------------------------------------------------------------------------------------------------------------------
# libcamera & rpicam 설치 & 기타 의존성 설치

RUN git clone https://github.com/raspberrypi/libcamera.git ~/libcamera && \
    cd ~/libcamera && \
    meson setup build --buildtype=release \
    -Dpipelines=rpi/vc4,rpi/pisp \
    -Dipas=rpi/vc4,rpi/pisp \
    -Dv4l2=enabled \
    -Dgstreamer=enabled \
    -Dtest=false \
    -Dlc-compliance=disabled \
    -Dcam=disabled \
    -Dqcam=disabled \
    -Ddocumentation=disabled \
    -Dpycamera=enabled && \
    ninja -C build && \
    sudo ninja -C build install

WORKDIR $HOME

RUN git clone https://github.com/raspberrypi/rpicam-apps.git ~/rpicam-apps && \
    cd ~/rpicam-apps && \
    meson setup build \
    -Denable_libav=disabled \
    -Denable_drm=enabled \
    -Denable_egl=enabled \
    -Denable_qt=enabled \
    -Denable_opencv=enabled \
    -Denable_tflite=disabled \
    -Denable_hailo=disabled && \
    meson compile -C build && \
    sudo meson install -C build && \
    sudo ldconfig

RUN echo 'export GST_PLUGIN_PATH=/usr/local/lib/aarch64-linux-gnu/gstreamer-1.0:$GST_PLUGIN_PATH' >> ~/.bashrc
# ------------------------------------------------------------------------------------------------------------------------------------------------
# ncnn 빌드 / 설치
WORKDIR $HOME
RUN git clone --depth 1 https://github.com/Tencent/ncnn.git && \
    cd ncnn && mkdir -p build-aarch64 && cd build-aarch64 && \
    cmake -G Ninja \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc-13 \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++-13 \
    -DCMAKE_FIND_ROOT_PATH=/usr/aarch64-linux-gnu \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/ncnn-aarch64 \
    -DNCNN_VULKAN=OFF \
    -DNCNN_BUILD_TOOLS=OFF \
    -DNCNN_BUILD_EXAMPLES=OFF \
    .. && \
    cmake --build . -j$(nproc) && sudo cmake --install . && sudo ldconfig


# .bashrc 설정
# RUN echo 'source ~/ros_ws/ros_settings.sh' >> ~/.bashrc

# 컨테이너 시작
# ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
