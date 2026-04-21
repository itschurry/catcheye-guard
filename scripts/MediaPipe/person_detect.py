"""
MediaPipe Person Detection
라즈베리파이5 + 카메라 모듈3 (libcamera / GStreamer)

RPi5 Bookworm 카메라 스택: libcamera (picamera2 없음)
OpenCV 가 GStreamer 파이프라인으로 libcamera 에 직접 접근합니다.

실행:
    python3 person_detect.py               # 기본 (libcamera, 카메라 모듈3)
    python3 person_detect.py --source 0    # USB 웹캠 (V4L2)
    python3 person_detect.py --no-display  # 디스플레이 없이 터미널만 출력
    python3 person_detect.py --mode pose   # 포즈 keypoint 포함

GStreamer 설치 확인:
    python3 -c "import cv2; print(cv2.getBuildInformation())" | grep GStreamer
    → GStreamer: YES 여야 합니다.
    → NO 이면: sudo apt install -y python3-opencv  (GStreamer 포함 빌드)
"""

import argparse
import time
from dataclasses import dataclass, field
from typing import List, Optional

import cv2
import numpy as np

# ── 인수 파싱 ─────────────────────────────────────────────────
parser = argparse.ArgumentParser(description="MediaPipe 사람 검출")
parser.add_argument("--source", default="libcamera",
                    help="libcamera (기본) | 0,1... (USB 웹캠) | /path/to/video.mp4")
parser.add_argument("--mode", choices=["detect", "pose"], default="detect",
                    help="detect: bbox만 / pose: bbox + 관절 keypoint")
parser.add_argument("--width",  type=int, default=640)
parser.add_argument("--height", type=int, default=480)
parser.add_argument("--score-threshold", type=float, default=0.5,
                    help="검출 신뢰도 임계값 (0~1)")
parser.add_argument("--no-display", action="store_true",
                    help="화면 출력 없이 FPS/검출 결과만 터미널 출력")
parser.add_argument("--detect-model", default="efficientdet_lite0.tflite")
parser.add_argument("--pose-model",   default="pose_landmarker_lite.task")
args = parser.parse_args()


# ── 데이터 클래스 ─────────────────────────────────────────────
@dataclass
class Detection:
    """검출된 사람 1명의 정보"""
    bbox: tuple           # (x1, y1, x2, y2) 픽셀 절대 좌표
    score: float          # 신뢰도 0~1
    landmarks: List = field(default_factory=list)  # 포즈 keypoints (mode=pose 시)


# ── 카메라 초기화 ──────────────────────────────────────────────
def _gstreamer_pipeline(width: int, height: int, framerate: int = 30) -> str:
    """
    RPi5 + libcamera 용 GStreamer 파이프라인 문자열 생성.

    libcamerasrc → BGRA 디코딩 → BGR(OpenCV) 변환 → appsink
    drop=true: 처리가 느릴 때 오래된 프레임 버림 (지연 방지)
    """
    return (
        f"libcamerasrc ! "
        f"video/x-raw,width={width},height={height},framerate={framerate}/1 ! "
        f"videoconvert ! "
        f"video/x-raw,format=BGR ! "
        f"appsink drop=true max-buffers=2 sync=false"
    )


def create_camera(source: str, width: int, height: int):
    """
    카메라 소스에 따라 VideoCapture 반환.

    source 우선순위:
      "libcamera"  → GStreamer libcamerasrc 파이프라인 (RPi5 기본)
      "0","1",...  → V4L2 인덱스 (USB 웹캠)
      기타 문자열  → GStreamer 파이프라인 문자열 또는 파일 경로로 직접 전달
    """
    if source == "libcamera":
        pipeline = _gstreamer_pipeline(width, height)
        print(f"[카메라] GStreamer + libcamera 파이프라인 시작")
        print(f"         {pipeline}")
        cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)
        if not cap.isOpened():
            raise RuntimeError(
                "libcamera 카메라를 열 수 없습니다.\n"
                "확인 사항:\n"
                "  1) libcamera-hello --list-cameras   # 카메라 인식 여부\n"
                "  2) python3 -c \"import cv2; print(cv2.getBuildInformation())\" | grep GStreamer\n"
                "     → GStreamer: YES 여야 함. NO 이면:\n"
                "       sudo apt install -y python3-opencv libgstreamer1.0-dev\n"
                "       sudo apt install -y gstreamer1.0-libcamera\n"
                "  3) 가상환경 pip 버전 OpenCV 는 GStreamer 미포함일 수 있음\n"
                "     → 시스템 OpenCV 사용: python3 -m venv --system-site-packages venv"
            )
        print(f"[카메라] libcamera 정상 연결 ({width}x{height})")
        return ("opencv", cap)

    # USB 웹캠 또는 파일
    open_arg = int(source) if source.isdigit() else source
    cap = cv2.VideoCapture(open_arg)
    if not cap.isOpened():
        raise RuntimeError(f"카메라/파일 열기 실패: {source}")
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    print(f"[카메라] VideoCapture 시작 (source={source})")
    return ("opencv", cap)


def read_frame(cam_tuple) -> Optional[np.ndarray]:
    """카메라에서 RGB 프레임 읽기 (내부는 BGR → 추론 전에 RGB 변환)"""
    _, cap = cam_tuple
    ret, frame = cap.read()
    if not ret or frame is None:
        return None
    # GStreamer appsink 가 BGR 로 내보내므로 그대로 RGB 변환
    return cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)


def release_camera(cam_tuple):
    _, cap = cam_tuple
    cap.release()


# ── MediaPipe 검출기 초기화 ───────────────────────────────────
def create_detector(mode: str, score_threshold: float):
    import mediapipe as mp

    if mode == "detect":
        # ObjectDetector: 80 COCO 클래스 검출, person = 카테고리명 "person"
        BaseOptions = mp.tasks.BaseOptions
        ObjectDetector = mp.tasks.vision.ObjectDetector
        ObjectDetectorOptions = mp.tasks.vision.ObjectDetectorOptions
        VisionRunningMode = mp.tasks.vision.RunningMode

        options = ObjectDetectorOptions(
            base_options=BaseOptions(model_asset_path=args.detect_model),
            running_mode=VisionRunningMode.IMAGE,
            score_threshold=score_threshold,
            category_allowlist=["person"],  # 사람만 필터링
            max_results=10,
        )
        detector = ObjectDetector.create_from_options(options)
        print(f"[모델] ObjectDetector 로드 완료 ({args.detect_model})")
        return ("detect", detector)

    else:  # pose
        # PoseLandmarker: 33개 관절 keypoint + 사람 bbox
        BaseOptions = mp.tasks.BaseOptions
        PoseLandmarker = mp.tasks.vision.PoseLandmarker
        PoseLandmarkerOptions = mp.tasks.vision.PoseLandmarkerOptions
        VisionRunningMode = mp.tasks.vision.RunningMode

        options = PoseLandmarkerOptions(
            base_options=BaseOptions(model_asset_path=args.pose_model),
            running_mode=VisionRunningMode.IMAGE,
            min_pose_detection_confidence=score_threshold,
            min_pose_presence_confidence=score_threshold,
            min_tracking_confidence=0.5,
            num_poses=5,
        )
        detector = PoseLandmarker.create_from_options(options)
        print(f"[모델] PoseLandmarker 로드 완료 ({args.pose_model})")
        return ("pose", detector)


# ── 추론 ──────────────────────────────────────────────────────
def run_inference(detector_tuple, rgb_frame: np.ndarray) -> List[Detection]:
    import mediapipe as mp
    det_type, detector = detector_tuple
    h, w = rgb_frame.shape[:2]

    mp_image = mp.Image(
        image_format=mp.ImageFormat.SRGB,
        data=rgb_frame
    )

    detections: List[Detection] = []

    if det_type == "detect":
        result = detector.detect(mp_image)
        for det in result.detections:
            bb = det.bounding_box
            x1 = bb.origin_x
            y1 = bb.origin_y
            x2 = x1 + bb.width
            y2 = y1 + bb.height
            score = det.categories[0].score
            # 프레임 범위 클리핑
            x1, y1 = max(0, x1), max(0, y1)
            x2, y2 = min(w, x2), min(h, y2)
            detections.append(Detection(bbox=(x1, y1, x2, y2), score=score))

    else:  # pose
        result = detector.detect(mp_image)
        for i, pose_landmarks in enumerate(result.pose_landmarks):
            # 관절 좌표에서 bounding box 계산
            xs = [lm.x * w for lm in pose_landmarks]
            ys = [lm.y * h for lm in pose_landmarks]
            pad = 20
            x1 = max(0, int(min(xs)) - pad)
            y1 = max(0, int(min(ys)) - pad)
            x2 = min(w, int(max(xs)) + pad)
            y2 = min(h, int(max(ys)) + pad)

            # presence score: 코(landmark 0)의 presence 값 사용
            score = pose_landmarks[0].presence if hasattr(pose_landmarks[0], "presence") else 1.0

            lm_list = [(int(lm.x * w), int(lm.y * h)) for lm in pose_landmarks]
            detections.append(Detection(bbox=(x1, y1, x2, y2), score=score, landmarks=lm_list))

    return detections


# ── 시각화 ────────────────────────────────────────────────────
# 포즈 스켈레톤 연결 (MediaPipe Pose 33 keypoints)
POSE_CONNECTIONS = [
    (11, 12), (11, 13), (13, 15), (12, 14), (14, 16),  # 팔
    (11, 23), (12, 24), (23, 24),                        # 몸통
    (23, 25), (25, 27), (24, 26), (26, 28),              # 다리
]


def draw_detections(frame_bgr: np.ndarray, detections: List[Detection]) -> np.ndarray:
    out = frame_bgr.copy()
    for i, det in enumerate(detections):
        x1, y1, x2, y2 = det.bbox
        color = (0, 200, 60)   # 초록 bbox

        # Bounding box
        cv2.rectangle(out, (x1, y1), (x2, y2), color, 2)

        # 라벨
        label = f"person {det.score:.2f}"
        (tw, th), _ = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.55, 1)
        cv2.rectangle(out, (x1, y1 - th - 8), (x1 + tw + 6, y1), color, -1)
        cv2.putText(out, label, (x1 + 3, y1 - 4),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 0, 0), 1, cv2.LINE_AA)

        # 포즈 keypoints
        if det.landmarks:
            # 스켈레톤 선
            for a, b in POSE_CONNECTIONS:
                if a < len(det.landmarks) and b < len(det.landmarks):
                    cv2.line(out, det.landmarks[a], det.landmarks[b],
                             (255, 180, 0), 2, cv2.LINE_AA)
            # 관절 점
            for lx, ly in det.landmarks:
                cv2.circle(out, (lx, ly), 4, (0, 120, 255), -1)

    return out


# ── 메인 루프 ─────────────────────────────────────────────────
def main():
    print("=" * 50)
    print("  MediaPipe Person Detection on RPi5")
    print("=" * 50)

    cam = create_camera(args.source, args.width, args.height)
    detector = create_detector(args.mode, args.score_threshold)

    fps_list = []
    frame_count = 0
    last_print = time.time()

    print("\n[실행] 시작 (종료: q 또는 Ctrl+C)")

    try:
        while True:
            t0 = time.perf_counter()

            rgb = read_frame(cam)
            if rgb is None:
                print("[종료] 스트림 끝")
                break

            # 추론
            detections = run_inference(detector, rgb)

            # FPS 계산
            elapsed = time.perf_counter() - t0
            fps = 1.0 / elapsed if elapsed > 0 else 0
            fps_list.append(fps)
            frame_count += 1

            # 1초마다 터미널 출력
            if time.time() - last_print >= 1.0:
                avg_fps = sum(fps_list) / len(fps_list)
                fps_list.clear()
                n = len(detections)
                status = f"사람 {n}명" if n else "미검출"
                print(f"  FPS: {avg_fps:5.1f}  |  {status}  |  프레임: {frame_count}")
                last_print = time.time()

            # 화면 표시
            if not args.no_display:
                bgr = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
                vis = draw_detections(bgr, detections)

                # FPS 오버레이
                avg_fps = fps  # 즉시값
                cv2.putText(vis, f"FPS: {avg_fps:.1f}", (10, 28),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 100), 2)
                cv2.putText(vis, f"Mode: {args.mode}", (10, 56),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (200, 200, 200), 1)

                cv2.imshow("MediaPipe Person Detection", vis)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break

    except KeyboardInterrupt:
        print("\n[종료] Ctrl+C")
    finally:
        release_camera(cam)
        cv2.destroyAllWindows()
        print(f"[통계] 총 {frame_count}프레임 처리 완료")


if __name__ == "__main__":
    main()
