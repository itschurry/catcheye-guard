"""
headless_alert.py — 디스플레이 없는 환경용 사람 감지 알림 예제

모니터 없이 SSH로만 쓸 때 유용합니다.
감지 시 → 터미널 출력 + GPIO 버저/LED 제어 예시 포함

실행: python3 headless_alert.py
"""

import time
import mediapipe as mp
import numpy as np

# GPIO (선택): RPi5는 gpiozero 또는 lgpio 사용
try:
    from gpiozero import LED, Buzzer
    led = LED(17)      # GPIO 17번 핀 → LED
    buzzer = Buzzer(18)   # GPIO 18번 핀 → 버저 (없으면 주석 처리)
    GPIO_AVAILABLE = True
except ImportError:
    GPIO_AVAILABLE = False
    print("[GPIO] gpiozero 없음 — 터미널 출력만 사용")


def create_detector(score_threshold: float = 0.5):
    BaseOptions = mp.tasks.BaseOptions
    ObjectDetector = mp.tasks.vision.ObjectDetector
    ObjectDetectorOptions = mp.tasks.vision.ObjectDetectorOptions
    VisionRunningMode = mp.tasks.vision.RunningMode

    return ObjectDetector.create_from_options(
        ObjectDetectorOptions(
            base_options=BaseOptions(model_asset_path="efficientdet_lite0.tflite"),
            running_mode=VisionRunningMode.IMAGE,
            score_threshold=score_threshold,
            category_allowlist=["person"],
            max_results=5,
        )
    )


def on_person_detected(count: int, scores: list):
    """사람 감지 시 실행되는 콜백"""
    ts = time.strftime("%H:%M:%S")
    print(f"[{ts}] 🔴 사람 {count}명 감지! (신뢰도: {[f'{s:.2f}' for s in scores]})")

    if GPIO_AVAILABLE:
        led.on()
        # buzzer.beep(on_time=0.1, off_time=0.1, n=2)  # 버저 2회
        time.sleep(0.3)
        led.off()


def on_clear():
    """사람 없을 때"""
    if GPIO_AVAILABLE:
        led.off()


def main():
    try:
        from picamera2 import Picamera2
        cam = Picamera2()
        cam.configure(cam.create_preview_configuration(
            main={"format": "RGB888", "size": (320, 320)}  # 작은 해상도로 속도↑
        ))
        cam.start()
        time.sleep(0.5)

        def get_frame():
            return cam.capture_array()

        def release():
            cam.stop()

    except ImportError:
        import cv2
        cap = cv2.VideoCapture(0)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 320)

        def get_frame():
            ret, f = cap.read()
            return cv2.cvtColor(f, cv2.COLOR_BGR2RGB) if ret else None

        def release():
            cap.release()

    detector = create_detector(score_threshold=0.5)
    print("[시작] headless 사람 감지 모드 (Ctrl+C로 종료)")

    last_state = False  # 이전 프레임에 사람 있었는지

    try:
        while True:
            frame = get_frame()
            if frame is None:
                continue

            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=frame)
            result = detector.detect(mp_image)

            persons = [d for d in result.detections if d.categories[0].category_name == "person"]
            detected = len(persons) > 0

            if detected:
                scores = [d.categories[0].score for d in persons]
                on_person_detected(len(persons), scores)
                last_state = True
            else:
                if last_state:
                    print(f"[{time.strftime('%H:%M:%S')}] ⚪ 사람 없음")
                on_clear()
                last_state = False

            time.sleep(0.1)  # 초당 ~10회 체크 (전력 절약)

    except KeyboardInterrupt:
        print("\n[종료]")
    finally:
        release()
        if GPIO_AVAILABLE:
            led.close()


if __name__ == "__main__":
    main()
