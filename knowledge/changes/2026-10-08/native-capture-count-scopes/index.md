---
type: Change
title: Separate static native inventory from auxiliary draw baselines
created: 2026-10-08
tags: [okf, rendering, tooling, validation]
---

# Separate static native inventory from auxiliary draw baselines

Native capture reports now expose static logical inventory/index-partition and
historical auxiliary batching checks separately. The original combined field,
ten-draw baseline and replay warnings remain unchanged. Retained car/wheel and
screen-space commands remain migration debt, not native-world success evidence.

Three host tests/eight scenarios and both configured RenderDoc1.41 SDK replays
pass. Every prior report field remains byte/value-identical; static inventory
matches, while auxiliary-baseline/combined/clean-validation remain false.
See [executed evidence](../../../rendering/r03-count-contract-validation.md).
No runtime/build/settings change or full R03 acceptance is claimed.
