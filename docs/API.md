# CatchEye Guard API

CatchEye Guard의 런타임 제어용 HTTP API 문서다.

기본 주소:

```text
http://<host>:8090/api/
```

포트는 실행 옵션으로 바꾼다.

```bash
./install/release-arm64/bin/catcheye-guard \
  --camera \
  --ws \
  --http-port 8090 \
  --hef ./install/release-arm64/models/yolo26m.hef \
  --metadata ./install/release-arm64/models/metadata.yaml
```

## 공통 규칙

- 모든 응답 body는 JSON이다.
- 쓰기 요청은 `Content-Type: application/json`을 붙인다.
- 정의되지 않은 method는 `405 Method Not Allowed`를 반환한다.
- API 서버는 `0.0.0.0:<port>`에 바인딩된다.
- ROI API는 사람 ROI와 파렛트 ROI config가 둘 다 로드된 경우에만 등록된다.

에러 응답 형식:

```json
{"error":"message"}
```

상세 오류가 있으면 `details`가 같이 온다.

```json
{
  "error": "ROI config failed validation",
  "details": [
    "zone_index=0, point_index=1, message=..."
  ]
}
```

## 엔드포인트 요약

| Method | Path | 설명 |
| --- | --- | --- |
| `GET` | `/api/device-info` | 앱 종류와 GPIO 런타임 상태 조회 |
| `GET` | `/api/roi` | 사람 위험 ROI config 조회 |
| `PUT` | `/api/roi` | 사람 위험 ROI config 교체 |
| `GET` | `/api/pallet-roi` | 파렛트 필수 ROI config 조회 |
| `PUT` | `/api/pallet-roi` | 파렛트 필수 ROI config 교체 |
| `GET` | `/api/rgb-camera/properties` | 런타임 RGB 카메라 속성 조회 |
| `PUT` | `/api/rgb-camera/properties/<key>` | 런타임 RGB 카메라 속성 변경 |
| `GET` | `/api/recording` | 녹화 상태 조회 |
| `POST` | `/api/recording/start` | preview 녹화 시작 |
| `POST` | `/api/recording/pause` | preview 녹화 일시정지 |
| `POST` | `/api/recording/resume` | preview 녹화 재개 |
| `POST` | `/api/recording/save` | preview 녹화 저장 |
| `POST` | `/api/recording/cancel` | preview 녹화 취소 |

## Device Info

### `GET /api/device-info`

앱 종류와 GPIO 런타임 상태를 조회한다.

```bash
curl http://<host>:8090/api/device-info
```

응답:

```json
{
  "app": "catcheye-guard",
  "kind": "guard",
  "person_roi_alert_disabled": false,
  "roi_alert_output_active": false
}
```

필드:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `app` | string | 앱 이름. 항상 `catcheye-guard`다. |
| `kind` | string | 앱 종류. 항상 `guard`다. |
| `person_roi_alert_disabled` | boolean | 사람 ROI 알림 비활성화 GPIO 입력이 active인지 여부다. |
| `roi_alert_output_active` | boolean | ROI 알림 GPIO 출력이 active인지 여부다. |

## ROI API

ROI config는 파일과 메모리에 같이 반영된다. `PUT` 요청이 성공하면 config 파일을 저장하고, 실행 중인 processor에도 즉시 적용한다.

### ROI JSON 형식

```json
{
  "camera_id": "cam_default",
  "image_width": 1920,
  "image_height": 1080,
  "allowed_zones": [
    {
      "id": "zone_main_floor",
      "name": "main_danger_zone",
      "enabled": true,
      "points": [
        [380.71853100328224, 193.48177060232413],
        [792.2079304532956, 188.03157988113193],
        [796.7337886986606, 508.6804754723677],
        [369.81814956089784, 505.5051893905793]
      ]
    }
  ]
}
```

필드:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `camera_id` | string | ROI가 적용되는 카메라 id다. |
| `image_width` | number | ROI 좌표 기준 이미지 폭이다. |
| `image_height` | number | ROI 좌표 기준 이미지 높이다. |
| `allowed_zones` | array | ROI polygon 목록이다. |
| `allowed_zones[].id` | string | zone 식별자다. |
| `allowed_zones[].name` | string | zone 표시 이름이다. |
| `allowed_zones[].enabled` | boolean | zone 활성화 여부다. |
| `allowed_zones[].points` | array | `[x, y]` 좌표 배열이다. |

### `GET /api/roi`

사람 위험 ROI config를 조회한다.

```bash
curl http://<host>:8090/api/roi
```

성공 시 현재 사람 ROI config JSON을 그대로 반환한다.

### `PUT /api/roi`

사람 위험 ROI config를 교체한다.

```bash
curl -X PUT \
  -H 'Content-Type: application/json' \
  --data-binary @config/roi_cam_default.json \
  http://<host>:8090/api/roi
```

성공 시 저장된 ROI config JSON을 반환한다.

실패:

| 상태 | 조건 |
| ---: | --- |
| `400` | JSON parse 실패 |
| `400` | ROI validation 실패 |
| `500` | ROI 파일 읽기/쓰기 실패 |
| `500` | 메모리 적용 실패 |

### `GET /api/pallet-roi`

파렛트 필수 ROI config를 조회한다.

```bash
curl http://<host>:8090/api/pallet-roi
```

### `PUT /api/pallet-roi`

파렛트 필수 ROI config를 교체한다.

```bash
curl -X PUT \
  -H 'Content-Type: application/json' \
  --data-binary @config/pallet_roi_cam_default.json \
  http://<host>:8090/api/pallet-roi
```

성공/실패 규칙은 `/api/roi`와 같다.

## RGB Camera Properties

런타임 GStreamer 카메라 속성을 조회하거나 바꾼다. 카메라 입력이 없거나 속성 owner를 찾지 못하면 실패한다.

### `GET /api/rgb-camera/properties`

지원 속성 중 현재 pipeline에서 조회 가능한 값만 반환한다.

```bash
curl http://<host>:8090/api/rgb-camera/properties
```

응답 예:

```json
{
  "ae-enable": true,
  "exposure-time": 12000,
  "analogue-gain": 1.5,
  "awb-mode": "auto"
}
```

카메라가 없으면:

```json
{"error":"RGB camera is not enabled"}
```

상태 코드는 `409 Conflict`다.

### `PUT /api/rgb-camera/properties/<key>`

속성 하나를 바꾼다. body는 항상 `value` 하나만 보낸다.

```bash
curl -X PUT \
  -H 'Content-Type: application/json' \
  -d '{"value":false}' \
  http://<host>:8090/api/rgb-camera/properties/ae-enable
```

성공 시 `/api/rgb-camera/properties`와 같은 전체 속성 JSON을 반환한다.

지원 key:

| Key | 타입 |
| --- | --- |
| `ae-enable` | boolean |
| `ae-metering-mode` | string |
| `ae-flicker-period` | integer |
| `exposure-time-mode` | string |
| `exposure-time` | integer |
| `exposure-value` | number |
| `analogue-gain-mode` | string |
| `analogue-gain` | number |
| `awb-enable` | boolean |
| `awb-mode` | string |
| `af-mode` | string |
| `lens-position` | number |
| `brightness` | number |
| `contrast` | number |
| `saturation` | number |
| `sharpness` | number |
| `gamma` | number |

타입별 요청 예:

```bash
curl -X PUT -H 'Content-Type: application/json' \
  -d '{"value":true}' \
  http://<host>:8090/api/rgb-camera/properties/ae-enable

curl -X PUT -H 'Content-Type: application/json' \
  -d '{"value":12000}' \
  http://<host>:8090/api/rgb-camera/properties/exposure-time

curl -X PUT -H 'Content-Type: application/json' \
  -d '{"value":1.5}' \
  http://<host>:8090/api/rgb-camera/properties/analogue-gain

curl -X PUT -H 'Content-Type: application/json' \
  -d '{"value":"auto"}' \
  http://<host>:8090/api/rgb-camera/properties/awb-mode
```

실패:

| 상태 | 조건 |
| ---: | --- |
| `400` | 지원하지 않는 key |
| `400` | body에 `value`가 없거나 JSON 형식이 맞지 않음 |
| `400` | key 타입과 `value` 타입이 맞지 않음 |
| `409` | RGB 카메라 입력이 활성화되지 않음 |
| `500` | GStreamer 속성 적용 실패 |

## Recording API

Preview frame을 파일로 녹화한다. 녹화 파일은 `--recording-dir` 아래에 생성된다. 기본 디렉터리는 `recordings`다.

녹화 API 응답 형식:

```json
{
  "state": "idle",
  "active_path": "",
  "saved_path": "",
  "error": "",
  "written_frames": 0
}
```

필드:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `state` | string | `idle`, `recording`, `paused` 중 하나다. |
| `active_path` | string | 현재 쓰는 녹화 파일 경로다. |
| `saved_path` | string | 마지막으로 저장된 파일 경로다. |
| `error` | string | 마지막 녹화 오류 메시지다. |
| `written_frames` | number | 현재 녹화 파일에 기록된 frame 수다. |

녹화가 비활성화된 상태면 `409 Conflict`를 반환한다.

```json
{"error":"recording is not enabled"}
```

### `GET /api/recording`

현재 녹화 상태를 조회한다.

```bash
curl http://<host>:8090/api/recording
```

### `POST /api/recording/start`

녹화를 시작한다.

```bash
curl -X POST http://<host>:8090/api/recording/start
```

이미 녹화 중이면 응답의 `error`에 `recording is already active`가 들어간다.

### `POST /api/recording/pause`

녹화를 일시정지한다.

```bash
curl -X POST http://<host>:8090/api/recording/pause
```

녹화 중이 아니면 응답의 `error`에 `recording is not running`이 들어간다.

### `POST /api/recording/resume`

일시정지된 녹화를 재개한다.

```bash
curl -X POST http://<host>:8090/api/recording/resume
```

일시정지 상태가 아니면 응답의 `error`에 `recording is not paused`가 들어간다.

### `POST /api/recording/save`

현재 녹화를 저장하고 상태를 `idle`로 돌린다.

```bash
curl -X POST http://<host>:8090/api/recording/save
```

저장할 frame이 없거나 파일 생성에 실패하면 응답의 `error`에 이유가 들어간다.

### `POST /api/recording/cancel`

현재 녹화를 취소한다.

```bash
curl -X POST http://<host>:8090/api/recording/cancel
```

녹화 중이 아니면 응답의 `error`에 `recording is not active`가 들어간다.

## 빠른 점검

```bash
curl http://<host>:8090/api/device-info
curl http://<host>:8090/api/roi
curl http://<host>:8090/api/pallet-roi
curl http://<host>:8090/api/rgb-camera/properties
curl http://<host>:8090/api/recording
```
