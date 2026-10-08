---
type: Evidence
title: RenderDoc 1.41 capture and native region inspection
status: partial
created: 2026-10-07
tags: [okf, rendering, vulkan, validation, tooling]
---

# RenderDoc 1.41 capture and native region inspection

The user instructed new work to use the updated `rdc-cli`/`rdc-tools` with
RenderDoc1.41. Historical1.46 evidence remains historical; it was not relabelled
or replayed through1.41. No renderer code or backend selection changed here.
Parent HEAD2e93f215 remains dirty; built fork23411087 is clean/pushed and the
parent gitlink staged. R03 remains incomplete; R04 is unstarted.

## Verified tool selection

- MCP `rdc_command doctor`: Python3.14.8, renderdoc module/replay/upstream1.41,
  registered Vulkan manifest under `%LOCALAPPDATA%/rdc/renderdoc`.
- `rdc.discover.find_renderdoc()` confirms1.41 and that directory's `renderdoc.pyd`.
- Its `renderdoc.dll`/`renderdoccmd.exe` file versions are1.41.0.0; the latter
  reports buildc9e72e3d706c18601de874bcdb875b0ec977f952.
- `C:/Program Files/RenderDoc` still contains1.46. `doctor` independently reports
  this binary; that line does not describe the configured Python capture path.
- MCP capture uses the configured Python API (no fallback warning), and MCP
  replay/script explicitly reports1.41. `capture --list-apis` is unsupported by
  upstream `renderdoccmd`; its failure is not a device/API capability finding.

## Executed capture and inspection

Two new MCP `rdc_capture` runs use `--frame 500 --timeout 45 --api-validation
--json`, with output below ignored `src_rebuild/build/native-renderer-r03/`.
Application arguments, identical to the previous terrain reproduction:

```text
-nointro -mission 50 -gametype 1 -level 0 -playercar 0
-startpos 6917 -218984 -startdir 2798 -players 1 -nativeregion
```

Window1920x1080; focal256/HD overrides on, then focal128/overrides off;
trilinear enabled, region bounds unchanged. Normal PID49156 and wide PID42556
captured successfully; CLI terminated its owned targets after capture.
Fresh five-INI backup was restored with exact original SHA256 hashes afterwards.
No generated project settings or gameplay/asset files were changed.

The new `scripts/inspect_native_capture.py` uses the daemon's controller instead
of a separate qrenderdoc helper. It rejects versions other than1.41, counts actual
D32 native draws, hashes bound vertex/index buffers and every material mip,
exports colour/IDs/depth before auxiliary UI, and retains observed diagnostics.
Example after `rdc open ABS_CAPTURE`:

```text
rdc script scripts/inspect_native_capture.py --arg output=ABS_IGNORED_DIRECTORY --json
```

Both actual replays report80 backdrop/876 world/10 auxiliary draws, geometry
148320 bytes, native B8G8R8A8_UNORM/R32_UINT/D32 attachments. Normal61 images/
RGBA49926792 bytes; wide62/2174132. Ordered draw geometry and all-mip artwork
hashes match previous final1.46 reports. Full-frame colour pixels, IDs and depth
bytes are exactly equal at both settings. The first comparison mistakenly
compared different report schemas (`attributes` exists only in the older report);
comparison of the shared bound-resource fields passes. No changed geometry was found.
Visual review of the wide native export retains the distinct source artwork
patch boundary; version migration does not establish full region seam acceptance.

## Diagnostic limitation retained

Each fresh1.41 open reports five HIGH messages, event IDs3/17, for
`VUID-vkCmdPipelineBarrier-pBufferMemoryBarriers-02818`: HOST_READ access paired
with ALL_COMMANDS destination stage. `SetFrameEvent` clears these messages;
the first script revision therefore reported zero at its final draw. Corrected
retention and fresh-session reruns preserve all five and set `validationClean=false`.
Final normal/wide pixel/resource success is recorded separately from that result.

Local RenderDoc source at buildc9e72e3 shows its buffer `DoPipelineBarrier` helper
uses ALL_COMMANDS at both ends (`vk_common.cpp:682`), and internal backbuffer
readback uses that helper with HOST_READ (`vk_core.cpp:2972`). This is consistent
with tool-generated initial replay warnings. The game's only matching HOST_READ
barrier uses TRANSFER -> HOST (`PsyX_VkNative.inl:477`), not ALL_COMMANDS.
These facts support a tooling diagnosis but do not prove the precise origin of
every warning. Keep the limitation explicit; do not suppress it or modify game
synchronization merely to silence replay. Prior direct native/PSX Khronos tests
on this unchanged build passed/exit0/no VUID; no new direct run is claimed here.

Private `normal141/`, `wide141/` reports/exported buffers/images/comparisons and
`native-canonical-*141_frame500.rdc` retain reproducible evidence. They contain
local original assets and must not be committed or redistributed. Source/region/
silhouette/drive coverage remains the next R03 work; warning isolation is also
pending. OpenGL port work for this tooling-only change is not applicable (VK020).
