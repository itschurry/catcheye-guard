# FROM ghcr.io/itschurry/ros:jazzy
FROM ubuntu:24.04
LABEL authors="itschurry"
LABEL maintainer="itschurry"
ARG DEBIAN_FRONTEND=noninteractive
ENV NCNN_ROOT=/opt/ncnn-aarch64

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
    apt-get install -y --no-install-recommends \
    # [기본 유틸리티] 패키지 다운로드 / 인증
    ca-certificates curl git sudo unzip wget zip \
    # [빌드 도구] 호스트용
    build-essential cmake file ninja-build pkg-config \
    # [크로스 컴파일러 / sysroot]
    gcc-13-aarch64-linux-gnu g++-13-aarch64-linux-gnu \
    libc6-dev-arm64-cross libstdc++-13-dev-arm64-cross \
    # [ARM64 대상 개발 패키지] 앱 링크용
    libfmt-dev:arm64 libopencv-dev:arm64 libyaml-cpp-dev:arm64 libspdlog-dev:arm64 \
    libjpeg-dev:arm64 libpng-dev:arm64 libtiff5-dev:arm64; \
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
    usermod -aG sudo,video,dialout,plugdev $USERNAME

# COPY --chown=$USERNAME:$USERNAME ros_settings.sh /ros_settings.sh
# # User가 소유자이므로 sudo 불필요
# RUN chmod +x /ros_settings.sh

# COPY --chown=$USERNAME:$USERNAME entrypoint.sh /entrypoint.sh
# RUN chmod +x /entrypoint.sh

USER $USERNAME
WORKDIR $HOME

# ----------------------------------------------------------------------------------------------
# 설치 prefix 준비
RUN sudo mkdir -p ${NCNN_ROOT}

RUN printf '%s\n' \
    "export NCNN_ROOT=${NCNN_ROOT}" \
    'export PATH=/usr/local/bin:$PATH' \
    >> ~/.bashrc


# .bashrc 설정
# RUN echo 'source ~/ros_ws/ros_settings.sh' >> ~/.bashrc

# 컨테이너 시작
# ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
