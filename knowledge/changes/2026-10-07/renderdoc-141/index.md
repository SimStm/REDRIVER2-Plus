---
type: Change
title: RenderDoc 1.41 native inspection through rdc tools
created: 2026-10-07
tags: [okf, rendering, tooling, validation]
---

# RenderDoc 1.41 native inspection through rdc tools

User-requested tool selection is now recorded in the active rendering gates and
tooling rule. New `scripts/inspect_native_capture.py` runs through the configured
rdc daemon, verifies1.41, exports native colour/ID/depth and hashes bound geometry/
all artwork mips. It preserves initial/per-event messages instead of losing them
when RenderDoc changes events.

Fresh1.41 MCP normal/HD and wide/original captures/replays match previous final
colour/ID/depth bytes and bound resources, with80 backdrop/876 world/10 auxiliary
draws. Five initial HIGH replay barrier warnings remain explicitly unclean;
their exact origin is not fully isolated. Five INIs restored with original hashes.
No C++ renderer/backend change, new region acceptance or OpenGL port is claimed.
See [executed evidence](../../../rendering/renderdoc-141-validation.md),
[current product](../../../product/native-static-region.md) and compatibility VK020.

Subsequent inspection records44 original-tree two-sided/alpha/depth parts and
tests three tree footprints in both frames. Hole/ID/depth/occlusion masks pass;
five normal colour samples keep the combined oracle failing. Diagnostic LOD0/
FP32-fetch replay shaders were restored/freed. No runtime fix or silhouette
acceptance claimed; see [tree evidence](../../../rendering/r03-cutout-validation.md)
and compatibility VK021.

Subsequent capability-probe output includes actual texel/mip/raster precision;
two compiled runs exit0 (RTX3060Ti:8/8/8 bits). Five original-shader debug traces
match framebuffer colours; a texel-centre replay probe isolates the diagnosed
fractional-filtering difference and is restored/freed. Accuracy acceptance is
still pending; see the same tree evidence and compatibility VK022.

The subsequent native GPU fixture reproduces five filtered city colours with
owned 2x2 constant-UV patterns outside replay. Fifteen uniformity/nearest/filter/
ownership/retirement cases pass, and configured1.41 MCP verifies ten exact
native/capture RGB comparisons. Strict ideal accuracy stays explicitly unaccepted.
Native/PSX suites PASS/exit0/Khronos enabled/no VUID; VS build PASS. Fork7616e0d
pushed/gitlink staged, developer report enlarged, six test files hash-restored.
This adds diagnostics only, not a production sampling fix or R03 acceptance;
see the same evidence and compatibility VK023.
The [sampling rule](../../../rules/native-sampling-validation.md) preserves the
distinction between diagnostic invariant PASS and accepted colour accuracy.

The subsequent [production precision correction](../native-sampling-precision/index.md)
uses FP32 decoded-texel interpolation and passes the unchanged strict accuracy
gate in fresh1.41 captures and direct GPU fixtures. This supersedes the unresolved
colour result above for the tested interiors, without accepting all silhouettes
or clearing the five initial replay warnings. Compatibility: VK024.
