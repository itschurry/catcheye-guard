# Vision App Template

새 앱 만들 때 이 템플릿 복붙해서 시작하면 돼.

토큰 치환:

- `__APP_NS__`
  - `catcheye::__APP_NS__` 네임스페이스에 들어갈 이름
  - 예: `hole`
- `__APP_CLASS__`
  - 클래스 prefix
  - 예: `Hole`
- `__APP_SLUG__`
  - 로그 / stream 이름 / 표시 문자열에 쓸 slug
  - 예: `hole-inspector`
- `__APP_TITLE__`
  - UI window 제목
  - 예: `CatchEye Hole Inspector`

파일 매핑:

- `main.cpp.tpl` -> `src/main.cpp`
- `app.hpp.tpl` -> `src/__APP_NS__/__APP_NS___app.hpp`
- `app.cpp.tpl` -> `src/__APP_NS__/__APP_NS___app.cpp`
- `processor_config.hpp.tpl` -> `src/__APP_NS__/__APP_NS___processor_config.hpp`
- `processor.hpp.tpl` -> `src/__APP_NS__/__APP_NS___processor.hpp`
- `processor.cpp.tpl` -> `src/__APP_NS__/__APP_NS___processor.cpp`

이 템플릿이 해주는 것:

- `vision-input`로 입력 소스 선택
- `vision-runtime`으로 공통 frame loop 실행
- `vision-transport`로 WebSocket publish 연결
- preview / headless / websocket 옵션 기본 제공

이 템플릿이 안 해주는 것:

- 앱 알고리즘
- 앱별 설정 파일
- 앱별 metadata schema
- 앱별 ROI 정책

즉 새 앱은 `Processor::process()` 안만 채우면 된다.
