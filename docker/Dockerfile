FROM ubuntu:24.04
LABEL authors="itschurry"
LABEL maintainer="itschurry"
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Seoul
ENV TARGET_SYSROOT=/opt/sysroots/raspi
ARG LIBCAMERA_REF=main

ARG UID=1000
ARG GID=1000
ARG USERNAME=appuser
ARG HOME=/home/${USERNAME}

COPY docker/libcamera-aarch64.ini /tmp/libcamera-aarch64.ini

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
    apt-get install -y --no-install-recommends \
    # [기본 유틸리티] 패키지 다운로드 / 인증
    tzdata ca-certificates curl git sudo unzip wget zip python3 \
    # [빌드 도구] 호스트용
    build-essential cmake file ninja-build pkg-config meson \
    python3-jinja2 python3-ply python3-yaml python3-pyelftools python3-pip \
    flex bison \
    # [크로스 컴파일러 / sysroot]
    gcc-13-aarch64-linux-gnu g++-13-aarch64-linux-gnu \
    libc6-dev-arm64-cross libstdc++-13-dev-arm64-cross \
    dpkg-dev; \
    \
    mkdir -p /tmp/sysroot-debs "${TARGET_SYSROOT}"; \
    apt-get install -y --download-only --no-install-recommends \
    # [ARM64 대상 개발 패키지] 설치하지 않고 sysroot로 추출
    libgstreamer1.0-dev:arm64 \
    libgstreamer-plugins-base1.0-dev:arm64 \
    libgstrtspserver-1.0-dev:arm64 \
    libdrm-dev:arm64 \
    libevent-dev:arm64 \
    libexif-dev:arm64 \
    libgnutls28-dev:arm64 \
    libdw-dev:arm64 \
    libyaml-dev:arm64 \
    libfmt-dev:arm64 \
    libopencv-dev:arm64 \
    libyaml-cpp-dev:arm64 \
    libspdlog-dev:arm64 \
    libblas-dev:arm64 \
    liblapack-dev:arm64 \
    libopenblas0-pthread:arm64 \
    libopenblas-pthread-dev:arm64 \
    libjpeg-dev:arm64 \
    libpng-dev:arm64 \
    libtiff5-dev:arm64; \
    cp /var/cache/apt/archives/*.deb /tmp/sysroot-debs/; \
    for pkg in /tmp/sysroot-debs/*_arm64.deb /tmp/sysroot-debs/*_all.deb; do \
    [ -e "${pkg}" ] || continue; \
    dpkg-deb -x "${pkg}" "${TARGET_SYSROOT}"; \
    done; \
    mkdir -p "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu"; \
    for lib in blas lapack openblas; do \
    for suffix in .so .so.3; do \
    target="${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/openblas-pthread/lib${lib}${suffix}"; \
    link="${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/lib${lib}${suffix}"; \
    if [ -e "${target}" ] && [ ! -e "${link}" ]; then \
    ln -s "openblas-pthread/lib${lib}${suffix}" "${link}"; \
    fi; \
    done; \
    done; \
    if [ -e "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/openblas-pthread/libopenblas.so.0" ] && \
       [ ! -e "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/libopenblas.so.0" ]; then \
    ln -s "openblas-pthread/libopenblas.so.0" \
       "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/libopenblas.so.0"; \
    fi; \
    \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/* /tmp/sysroot-debs

RUN userdel -r ubuntu || true && groupdel ubuntu || true

RUN if getent group $GID; then \
    groupmod -n $USERNAME $(getent group $GID | cut -d: -f1); \
    else \
    groupadd -g $GID $USERNAME; \
    fi

RUN useradd -m -u $UID -g $GID -d $HOME -s /bin/bash $USERNAME && \
    echo "$USERNAME ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/$USERNAME && \
    chmod 0440 /etc/sudoers.d/$USERNAME && \
    usermod -aG sudo,video,dialout,plugdev $USERNAME

USER $USERNAME
WORKDIR $HOME
# 설치 sysroot 준비
RUN sudo mkdir -p ${TARGET_SYSROOT}

RUN printf '%s\n' \
    "export TARGET_SYSROOT=${TARGET_SYSROOT}" \
    'export PATH=/usr/local/bin:$PATH' \
    >> ~/.bashrc

# 이 이미지는 "재현 가능한 크로스빌드 + sysroot + ncnn/libcamera 준비"에 집중한다.
# ARM64 dev 패키지는 sysroot로 추출하고, libcamera는 별도 소스 크로스빌드한다.

# libcamera 빌드 / 설치
WORKDIR $HOME
RUN git clone --branch ${LIBCAMERA_REF} --depth 1 https://github.com/raspberrypi/libcamera.git && \
    meson setup libcamera/build-aarch64 libcamera \
    --cross-file /tmp/libcamera-aarch64.ini \
    --buildtype=release \
    --prefix /usr \
    --libdir lib/aarch64-linux-gnu \
    -Dpipelines=rpi/vc4,rpi/pisp \
    -Dipas=rpi/vc4,rpi/pisp \
    -Dv4l2=enabled \
    -Dgstreamer=enabled \
    -Dtest=false \
    -Dlc-compliance=disabled \
    -Dcam=disabled \
    -Dqcam=disabled \
    -Ddocumentation=disabled \
    -Dpycamera=disabled && \
    ninja -C libcamera/build-aarch64 -j$(nproc) && \
    sudo rm -f ${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/libcamera.so* && \
    sudo rm -f ${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/libcamera-base.so* && \
    sudo DESTDIR=${TARGET_SYSROOT} meson install -C libcamera/build-aarch64

# ncnn 빌드 / 설치
WORKDIR $HOME
RUN git clone --depth 1 https://github.com/Tencent/ncnn.git && \
    cmake -S ncnn -B ncnn/build-aarch64 -G Ninja \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc-13 \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++-13 \
    -DCMAKE_SYSROOT=${TARGET_SYSROOT} \
    -DCMAKE_FIND_ROOT_PATH=${TARGET_SYSROOT} \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${TARGET_SYSROOT}/usr \
    -DCMAKE_INSTALL_LIBDIR=lib/aarch64-linux-gnu \
    -DNCNN_VULKAN=OFF \
    -DNCNN_BUILD_TOOLS=OFF \
    -DNCNN_BUILD_EXAMPLES=OFF \
    && \
    cmake --build ncnn/build-aarch64 -j$(nproc) && \
    sudo cmake --install ncnn/build-aarch64 && \
    sudo ldconfig

CMD ["bash"]
