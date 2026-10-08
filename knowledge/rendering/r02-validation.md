---
type: Evidence
title: R02 original asset import acceptance
status: validated-windows-slice
created: 2026-10-07
tags: [okf, rendering, validation, vulkan, assets]
---

# R02 original asset import acceptance

R02 is accepted for the tested Windows original-import slice. It provides owned
original mesh/material adapters and lifetime-safe caches, not a complete static
region. R03 supplies that next coverage gate. The umbrella renderer remains
planned; Linux/macOS runtime and complete web/Android builds are unexecuted.

## Source, environment and scope

Parent HEAD remains `2e93f215c54e61dd1bbb5d26e439c25303466709` with extensive
pre-existing changes, local data and documentation archival work. R02 extends
accepted PsyCross `99fa6418e03c95f4db71e07c2f09adf554669f51` in the project fork.
R02 fork `9ff48afee0384f92157568f6a2a3400b06b55fbd` is committed/pushed to
origin/master; the parent gitlink is staged. See the
[product](../product/native-original-assets.md). No parent commit or cleanup of
unrelated files occurred.

Windows 11, NVIDIA RTX 3060 Ti/driver610.88, NTSC, VS MCP `Release_dev|x64`;
1920x1080 actual city window, 1280x720 GPU self-test. Khronos validation DLL was
loaded. RenderDoc capture/replay both1.46; the configured1.41 MCP cannot replay
these captures. Installed qrenderdoc Python API was used without changing layers.
Hardware/API limits are recorded in [R00](r00-measured-baseline.md).

Repeatable original-city command:

```text
-nointro -mission 50 -gametype 1 -level 0 -playercar 0 -startpos 9046 -217980 -startdir 2785 -players 1 -nativeassetview
```

This explicitly incomplete fixture chooses original road source37/logical36 and
one observed tall facade after tick30 (actual source/logical273). It preserves
real placements and selected LOD keys, counts omitted objects and leaves complete
original rendering selectable. The black omitted world is intentional evidence
scope, not R03 coverage. Source1176 is an explicitly reported unsupported resource.

## Decoder, catalog and ownership checks

- `pwsh -NoProfile -File scripts/run_native_asset_tests.ps1`: **1,202 PASS**.
  Bounded MODEL families/quad diagonals, placements, 4/8/16-bit textures, corrupt
  and unsupported whole-resource rejection, owned source data, catalog model and
  parent revision/generation reuse, material/filter/mod invalidation, late failure
  rollback and zero ownership. Interrupted scene creation now has a regression.
- Fresh inspector runner: catalog145/export120 PASS, including17 native PNG
  precedence/CLUT/content/reload cases. Fresh directory used; export not repeated
  in the same directory. Scene18/C ABI, camera8/material11 and host excluded-
  platform guard cases PASS. These are not full other-target builds.
- Owned single-primitive bollard:2220 vertices,1479 triangles/4437 indices,
  bounds/UV/embedded PNG checked; SHA256 unchanged. Shared primitive contract
  exercised; full GLB topology/production support remains R09.
- Shader generation and VS build PASS, `FailedProjects=0`; existing compiler/
  linker warnings remain. Parent/fork `git diff --check` passed.

Original atlas coverage differs from named inspector bounds: Chicago1180 has
width24 but UV extent32. Original VRAM crops include source texels; active PNGs
keep their declared rectangle and clamp semantics. Tests verify both policies.

## Actual GPU and source inspection

RenderDoc frame430 has **14 native D32 draws/534 indices**, then **10 original
D24S8 UI draws/318 indices**, with zero debug messages. Direct city validation
also has no VUID/error or cache retirement failure in inspected VS output.

Road37:12 source vertices/3 polygons/6 triangles, parts6+12 indices; facade273:
30 source vertices/20 polygons/40 triangles, parts30+90 indices. Three road and
four facade placements therefore draw178 triangles. Four resident mesh/material
resources own7176 copied geometry bytes/4247296 RGBA bytes. Expanded triangle
vertices retain face attributes; this is not a unique-vertex deduplication claim.

Four captured vertex buffers, index buffers and sampled sRGB RGBA textures match
the FNV64 hashes of the exact CPU bytes submitted by the adapters. This compares
all position/normal/UV/colour attributes, indices and mip0 pixels, rather than
inferring correctness from source counts. Context and captured artwork sizes:

- Road part0: page0/index21, tpage10/CLUT20286, region128,128,64,64,
  active PNG1024x1024; part1: page0/index14, CLUT20159, region128,98,96,10,
  original96x10. Both use the live tpage10.
- Facade part0: page6/index2, tpage21/CLUT26430, region128,64,128,64/image128x64;
  part1: page5/index5, tpage9/CLUT25981, region192,0,64,64/image64x64.

All14 post-VS clip positions agree with their captured matrix/vertices within
relative tolerance1e-5 (observed errors below that limit). All14 captured MVPs
also agree with seven actual pre-GTE placement matrices and the final camera,
using predeclared1e-4 tolerance for debugger text precision; max7.17824e-5.
Translations match original integer placements after Y reflection. This second
comparison uses a later stabilized frame, not a time-matched image difference.
Visual inspection confirms road, facades and the original HUD/minimap.

## Live invalidation and failure recovery

- Bilinear1/PGXP1 initially effective14 draws. Setting both PGXP switches0 and
  filter0 preserves14 draws and ownership; material reports NEAREST. Re-enabling
  filter1 replaces handles and reports LINEAR. Neither PGXP option is a native
  geometry prerequisite.
- Reload clicked through the Mods panel changes registration revision677 to1041,
  replaces handles and returns to the same four resources/budgets. An earlier
  debugger function evaluation of reload timed out; it is not success evidence.
- Disabling PNGs through the panel uses original road64x64, retains14 draws and
  linear filtering, and reduces owned artwork to69376 bytes. Re-enabling PNGs
  restores1024x1024 and4247296 bytes. No asset/schema/directory changes.
- A forced reset immediately before material creation originally leaked two road
  parts. The fix rejects the interrupted transaction and retires newly created
  handles even when reset preceded their creation. Repeating the exact breakpoint
  fault yields scene generation2, four resources,7176/4247296 bytes, zero retiring
  meshes and effective14/OK. The CPU regression also recovers and clears to zero.
- Actual `LoadGameLevel` reload was forced through the normal restart/init path:
  road/facade catalog revisions1 to2, native handle generation3 to4, same counts
  and budgets. A second full reload with a deliberately mismatched catalog
  context exercises normal context reset: catalog generation1 to2, new source
  handles generation2/revision1, native handle generation5, facade reselected273,
  four resources and14 native/world0/UI10 draws. This is controlled catalog/slot
  reuse, not a long multi-city streaming route; R04 retains that acceptance.

Validation-enabled `-vknativetest` PASS includes R01 regressions, eight material
cycles, exact188 linear-light midpoint, cutout/background, legacy-filter
independence and a real submitted-upload wait-failure probe. The latter confirms
legacy pixels/draws, retained command/staging/image/fence, real completion and zero
remaining ownership. Fresh `-vkpsxtest` PASS. Initial upload is synchronous;
async scheduling, GPU timing and peak allocations are not measured here.

## Evidence preservation and next gate

Private logs/captures/helpers are ignored under `src_rebuild/build/native-renderer-r02/`:
`compare-original-buffers146-report.json`, `compare-city-transforms-report.json`,
`live-material-reload-evidence.json`, `live-material-reset-fixed.log`, native GPU
readback reports and capture/replay statuses. Captures/original artwork are not
committed or redistributed. Five runtime INIs and `.vcxproj.user` were restored
byte-exactly from the R02 backups; no test arguments/environment remain.

Next: [R03 complete static region](stages/03-complete-static-region.md), including
all source categories/materials, backdrop/transparency, coverage and picking.
Native OpenGL remains unsupported, [pending and unscheduled](../roadmap/planned/port-vulkan-to-opengl-compatibility.md#vk-009---r02-original-decoders-and-native-materials-2026-10-07).
