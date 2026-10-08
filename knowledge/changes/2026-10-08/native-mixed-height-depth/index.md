---
type: ChangeRecord
title: Preserve native depth on mixed-height terrain tops
created: 2026-10-08
tags: [okf, rendering, vulkan, native, depth]
---

# Preserve native depth on mixed-height terrain tops

Original Chicago grass569 and sidewalk681 have identical worldY17 tops, confirmed
from copied source/native transforms. The sidewalk's vertical side previously
disabled whole-mesh canonical depth, producing actual one-unit depth failures.
Owned consecutive indexed ranges now distinguish the top from its side without
changing source vertices/indices/materials, triangle order or logical IDs.
Physical draw and CPU metadata budgets reject atomically; mesh fences also own
and release the metadata. No ABI/shader change or old-world recovery is involved.

PsyCross four renderer/test files are committed/pushed as `ae83dd2` to the project
fork; parent gitlink staged. Parent CPU tests and RenderDoc1.41 inspector verify
range bounds/lifetimes and exact logical source coverage. Matching product:
[native static region](../../../product/native-static-region.md).

Actual CPU17 new checks/remaining host suites, asset1536, VS Release_dev/x64,
native and PSX GPU tests pass. Two native orders each check11685 top pixels,
plus paint/holes/nearer occlusion/IDs/retirement. Two actual1.41 city frame500
captures reduce wrong ownership4462/2111 ->0 over11456/2639 normal/wide visible
overlap pixels. Independent sidewalk56030/tree542851 colour samples pass.
Source hashes and upload payloads are unchanged;876 logical parts become1023
physical world draws. Failed initial test accounting, four replay warnings and
the Pixel History/attachment discrepancy remain explicit in
[executed evidence](../../../rendering/r03-mixed-height-validation.md).

Five INIs/`.user` restore byte/hash-exactly; all10 VS pairs/Design/game startup/
zero breakpoints verified. Existing OpenGL preserved; VK027 records future port
work. R03 remains incomplete: vertical-side city pixels, west camera/ground/
source coverage, full silhouettes/region/drive, replay warnings and GPU cost
remain pending. Diagnose west coverage next; R04 is unstarted.
