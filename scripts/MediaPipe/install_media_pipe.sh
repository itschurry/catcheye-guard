#!/bin/bash
# ============================================================
# MediaPipe Person Detection 설치 스크립트
# 대상: 라즈베리파이5 (Raspberry Pi OS 64-bit Bookworm)
# ============================================================
set -e

echo ">>> [1/5] 시스템 패키지 업데이트"
sudo apt update && sudo apt upgrade -y

echo ">>> [2/5] 시스템 의존성 설치"
sudo apt install -y \
    python3-pip \
    python3-venv \
    libcamera-dev \
    libopencv-dev \
    python3-opencv \
    libatlas-base-dev \
    libjpeg-dev \
    libopenblas-dev \
    ffmpeg

echo ">>> [3/5] Python 가상환경 생성"
python3 -m venv --system-site-packages venv
# --system-site-packages: picamera2가 시스템 패키지이므로 접근 허용

echo ">>> [4/5] Python 패키지 설치"
source venv/bin/activate

pip install --upgrade pip
pip install \
    mediapipe \
    numpy \
    opencv-python-headless

echo ">>> [5/5] 모델 파일 다운로드"
# EfficientDet-Lite0: 사람 포함 80개 클래스 검출 (빠름)
wget -q -O efficientdet_lite0.tflite \
    https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/int8/1/efficientdet_lite0.tflite

# BlazePose Full: 사람 bounding box + 33개 관절 keypoint
wget -q -O pose_landmarker_lite.task \
    https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task

echo ""
echo "✅ 설치 완료!"
echo "   실행: source venv/bin/activate"
echo "   검출: python3 person_detect.py"
echo "   확인: python3 check_install.py"
