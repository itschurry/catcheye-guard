FROM ghcr.io/itschurry/ros:jazzy
LABEL authors="itschurry"
LABEL maintainer="itschurry"
ARG DEBIAN_FRONTEND=noninteractive

RUN id -u && id -g
ARG UID=1000
ARG GID=1000
ARG USERNAME=appuser
ARG HOME=/home/${USERNAME}

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
RUN sudo apt-get update && sudo apt-get install -y \
    python3-pip python3-jinja2 meson cmake ninja-build \
    libboost-dev libgnutls28-dev openssl libtiff5-dev pybind11-dev \
    python3-yaml python3-ply libglib2.0-dev \
    cmake libboost-program-options-dev libdrm-dev libexif-dev \
    libepoxy-dev libjpeg-dev libtiff5-dev libpng-dev \
    qtbase5-dev libqt5core5a libqt5gui5 libqt5widgets5 \
    v4l-utils libopencv-dev \
    gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libcamera gstreamer1.0-libav \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    python3-gpiozero \
    rsync sshpass \
    && sudo apt-get clean && sudo rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/raspberrypi/libcamera.git ~/libcamera && \
    cd ~/libcamera && \
    meson setup build --buildtype=release \
    -Dpipelines=rpi/vc4,rpi/pisp \
    -Dipas=rpi/vc4,rpi/pisp \
    -Dv4l2=true \
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
    cd ncnn && mkdir -p build && cd build && \
    cmake -DNCNN_VULKAN=OFF -DNCNN_BUILD_TOOLS=OFF -DNCNN_BUILD_EXAMPLES=OFF .. && \
    make -j$(nproc) && sudo make install && sudo ldconfig


# .bashrc 설정
RUN echo 'source ~/ros_ws/ros_settings.sh' >> ~/.bashrc

# 컨테이너 시작
# ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]