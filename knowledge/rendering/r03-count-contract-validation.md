---
type: Validation
title: R03 static inventory and auxiliary count scopes
created: 2026-10-08
tags: [okf, rendering, tooling, validation]
---

# R03 static inventory and auxiliary count scopes

The original capture report combined the fixed80-backdrop/876-logical-part
inventory and a historical ten-auxiliary-draw baseline into `regionCountsMatch`.
The [producer investigation](r03-auxiliary-producer-validation.md) proves that
retained car/wheel commands and HD/VRAM batching yield14 versus11 auxiliary
draws in the two west frames. That variation cannot establish missing static
geometry, and neither total proves native pixel correctness or migration.

`inspect_native_capture.py` now reports the two scopes independently:

- `staticRegionCountsMatch`:80 backdrop draws,876 logical native parts and
  complete consecutive index partitions. Physical world draws may exceed876.
- `auxiliaryBaselineCountsMatch`:comparison with the unchanged historical10.
  This is a batching baseline, not producer attribution or a migration gate.
- `regionCountsMatch`:the existing combined field remains unchanged for current
  consumers, with `regionCountsScope` explicitly describing its limited meaning.
- `validationClean`:all observed replay diagnostics remain independently retained.

No baseline is silently changed to14/11. Native pixel/resource acceptance still
requires source-backed checks, and R06 car/wheel and R10 screen-space migration
debt remain visible. Complete native replacement ultimately requires zero PSX
draws; this R03 inventory report cannot accept that requirement.

## Actual verification

`C:/Python314/python.exe scripts/test_native_capture_contract.py` passes three
host tests/eight scenarios:10/11/14 auxiliary batches preserve static inventory;
missing/extra backdrop or logical world parts fail; correct counts cannot hide
an incomplete index partition. The tests load only the inspector's pure helper;
actual replay below verifies top-level report/result integration. Both scripts
also pass `py_compile`.

Configured RenderDoc1.41 local SDK inspection of existing uninstrumented normal
and wide captures completes in14.136s and16.091s respectively. Both report:
static=true, auxiliary-baseline=false, combined=false and validation-clean=false.
Four HIGH messages remain in each. Normal actual counts80/1023/14 and wide
80/1023/11 retain876 logical parts and full range coverage.

Every pre-existing report field is exactly equal to its corresponding prior
report, including colour/ID/D32 hashes, geometry/full-mip resources, draw bindings,
cutout policy and all messages. Only the three new diagnostic fields are added.
The failed combined field is not converted into success. This is replay evidence,
not a new game launch, build, GPU self-test or full-region source-pixel oracle.

Private outputs and exact comparisons stay in ignored
`src_rebuild/build/native-renderer-r03/`: `count-contract-{normal,wide}141/`,
their SDK summaries, `check-count-contract.py` and
`count-contract-replay-equivalence.json`. No settings or production renderer
source change; the fork remains clean.

## Acceptance and next action

The count diagnostic now distinguishes a matching fixed static inventory from
the known auxiliary batching difference. It does not remove retained consumers,
waive source/pixel gates or claim that old dynamic commands never affect other
views. Exact producer evidence remains scoped to the two tested west frames.

Subsequent [part605 clipping evidence](r03-clipped-static-validation.md) accepts
its scoped horizontal interiors only. Continue within R03 with vertical ranges,
excluded edges/silhouettes, remaining surface/region coverage and GPU cost.
R03 remains incomplete; R04 is unstarted. Compatibility entry: VK032.
