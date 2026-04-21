"""
설치 확인 스크립트
실행: python3 check_install.py
"""
import os
import sys

errors = []

print("=" * 45)
print("  MediaPipe RPi5 환경 확인")
print("=" * 45)

# Python 버전
print(f"\n[1] Python: {sys.version.split()[0]}", end=" ")
major, minor = sys.version_info[:2]
if major == 3 and minor >= 9:
    print("✅")
else:
    print("⚠️  (3.9 이상 권장)")

# MediaPipe
try:
    import mediapipe as mp
    print(f"[2] MediaPipe: {mp.__version__} ✅")
except ImportError as e:
    print(f"[2] MediaPipe: ❌ ({e})")
    errors.append("mediapipe 미설치 → pip install mediapipe")

# OpenCV
try:
    import cv2
    print(f"[3] OpenCV: {cv2.__version__} ✅")
except ImportError:
    print("[3] OpenCV: ❌")
    errors.append("opencv 미설치 → pip install opencv-python-headless")

# NumPy
try:
    import numpy as np
    print(f"[4] NumPy: {np.__version__} ✅")
except ImportError:
    print("[4] NumPy: ❌")

# picamera2
try:
    from picamera2 import Picamera2
    print("[5] picamera2: ✅")
except ImportError:
    print("[5] picamera2: ⚠️  (카메라 모듈3 없으면 불필요)")

# 모델 파일
models = {
    "efficientdet_lite0.tflite": "사람 검출용",
    "pose_landmarker_lite.task": "포즈+사람 검출용",
}
print()
for fname, desc in models.items():
    exists = os.path.exists(fname)
    status = "✅" if exists else "❌ (install.sh 재실행)"
    print(f"[모델] {fname} ({desc}): {status}")

# 카메라 연결 확인
print()
try:
    import subprocess
    result = subprocess.run(
        ["libcamera-hello", "--list-cameras"],
        capture_output=True, text=True, timeout=5
    )
    if "Available cameras" in result.stdout or result.returncode == 0:
        print("[카메라] libcamera 감지 ✅")
    else:
        print("[카메라] 카메라 미감지 ⚠️  (raspi-config에서 카메라 활성화 필요)")
except Exception:
    print("[카메라] libcamera 확인 불가 (정상일 수도 있음)")

# 결과
print()
if errors:
    print("❌ 해결 필요:")
    for e in errors:
        print(f"   • {e}")
else:
    print("✅ 모든 의존성 정상 — person_detect.py 실행 가능")
print("=" * 45)
