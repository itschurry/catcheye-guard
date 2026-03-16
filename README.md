## CatchEye Guard ROI Engine Module

This repository now includes a reusable ROI engine under `include/catcheye/guard/roi` and `src/guard/roi`.

### What it provides
- Domain models: point, ROI polygon, camera ROI config.
- JSON repository: load/save + parse/serialize with a lightweight engine-side JSON parser.
- Validation: structured issues for invalid inputs.
- Geometry: point-in-polygon (concave support), polygon area, bounds, self-intersection checks.
- Evaluation helpers for intrusion candidates:
  - `evaluate_reference_point`
  - `evaluate_bbox_bottom_center`

### Quick usage
```cpp
#include "catcheye/guard/roi/roi_repository.hpp"
#include "catcheye/guard/roi/roi_evaluator.hpp"

using namespace catcheye::guard::roi;

auto parsed = RoiRepository::load_from_file("roi_cam_01.json");
if (!parsed.success) {
    // handle parse errors
}

EvaluationResult decision = evaluate_bbox_bottom_center(100, 50, 80, 150, parsed.config);
// Allowed / Restricted / Invalid
```

### Notes
- ROI points are interpreted in original image coordinates.
- Disabled zones are retained in config but ignored during evaluation.
- Normal invalid input is reported via result structs, not exceptions.
