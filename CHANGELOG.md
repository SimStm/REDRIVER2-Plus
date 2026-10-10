# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
where release policy permits it.

## [Unreleased]

### Changed

- E03 block 2 item 4 camera convention: the native scene projection is +Z-forward
  with a [0,1] NDC depth range (`out[10]=F/(F-N)`, `out[11]=+1`,
  `out[14]=-N*F/(F-N)`, Diligent's convention) and +Y-down like the game; the F-A
  fix negates camera-space Y (`BuildPerspective out[5]=-f`) because the game camera
  is +Y-down (GTE `SY=OFY+IR2*h/sz`) while Diligent's D3D-like viewport is +Y-up.
  The reflection is cancelled by the cull pin flip (`CULL_MODE_BACK` +
  `FrontCounterClockwise=False` keeps the same world faces front; `CULL_MODE_NONE`
  is still rejected), and the `SourceModelDecoderTests` projection/winding/unit
  fixtures are re-pinned to view-space +Z (frontArea<0/backArea>0). New
  `NativeRenderer_SetClipPlanes(near, far)` replaces the hardcoded metre-scale
  0.02/300 for raw-unit scenes (the bridge supplies ~100 / ~125000).

### Added

- E03 source MODEL decoder foundation (Block 1, extended in Block 2):
  `utils/SourceModelDecoder.*` is a pure, asset-free decoder of the reconstructed
  MODEL poly block from an explicit `[begin,end)` span (the MODEL structure has no
  byte span; loader span capture is Block 2a). It decodes the allowlist
  `{10,11,20,21,22,23}`: 10 (`PL_POLYFT4`, 16 B, textured triangle, no RGB),
  20/21 (`POLYFT3`/`FT4`, 20 B) and 22/23 (`POLYGT3`/`GT4`, 24 B); 10/20/22 are
  triangles and 11/21/23 quads. It strides by `PolySizes[ptype & 0x1f]`, emits the
  world quad order `(v0,v1,v3,v2)` with UVs `(uv0,uv1,uv3,uv2)`, validates the
  total span/indices/normals with named errors and zero output, rejects instanced
  models, and stores raw stored face colours per submesh with aligned zero
  placeholders and per-submesh `hasColors`/`hasNormals`. The runtime
  `ConvertPolygonTypes` converts every even declared ptype except 8 (10->11,
  20->21, 22->23), so the building loop draws `{11,21,23}` and the declared types
  it can draw are `{10,11,20,21,22,23}`. No GPU/GLB/texture/loader/renderer work.
  Covered by `SourceModelDecoderTests` (extended to 1186 checks) with synthetic
  byte-exact fixtures; all other inspector suites still pass.

- E03 Block 2a (loader span capture, source identity and real-model ptype probe):
  loader-owned `model_byte_sizes[]` is captured at both size-live sites
  (`models.c` `ProcessMDSLump`, `spool.c` `init_spooled_models`) before relocation
  and exposed as catalog `ModelRecord.spanBytes` via
  `AssetCatalog_RegisterModelWithSpan`/`GetModelSpan`, cleared on slot
  invalidation with no stored pointers; `AssetCatalogPlacement.sourceObjectIndex`
  is set from the walker's 14-bit index (dedup key unchanged). New pure
  `Game/C/modelptypeprobe.{h,c}` counts declared ptypes from real `PolySizes`
  (bounds checked; zero-stride reported and never advanced; no draw-path class
  guessed), and the `-modelptypes` developer flag (accepted in `main.c` to avoid
  the modal helper) writes `model_ptypes_level_<N>.txt` through
  `InspectorExport_WriteText` with a per-level accumulator reset at
  `AssetCatalogGame_BeginLevel`. Covered by `ModelPtypeProbeTests` (68 checks) and
  `AssetCatalogTests` (278 checks). Real Chicago histogram (Release_dev,
  `-opengl -nointro -nofmv -mission 1 -modelptypes`): 516 slots, 5538 records;
  building-class 379 slots (ptypes 9x2, 10x30, 11x350, 20x11, 21x28); tile-class
  132 and sprite-class 5 slots contain only ptype 21; pre-policy outside-allowlist
  118 (ptype 10 x116 + 9 x2, all building); after the final allowlist the re-run
  reports 2 outside (ptype 9 only); 0 stride-zero, 0 invalid.

- E03 Block 2.1 (skip-vs-fail policy): `SourceModelDecoder` gains a policy enum -
  `Strict` (default, unchanged hard error/zero output) and `SkipUnsupported`
  (advance by `PolySizes`, count; bounds stay strict; zero-stride is
  `UNSKIPPABLE_PTYPE`) - applied to building-class models only, while tile/sprite
  callers stay `Strict` (Chicago tile/sprite data contains only ptype 21).
  `SourceModelDecoderTests` 1186 checks, 0 failures.

- E03 Block 2.5 (camera/unit/winding fixture): pure fixtures pin yang ==
  `RotMatrixY(yang*64)` (4096-unit turn), view = `R_cam*(R_obj*p + pos - cam)`,
  the NDC signed-area winding of the decoder's `(v0,v1,v3,v2)`->`(0,1,2)+(0,2,3)`
  emission, and the raw-unit policy (0.02/300 is not a valid raw-unit projection).
  Renderer scene pipelines changed from `CULL_MODE_NONE` to `CULL_MODE_BACK`
  (Vulkan negative-viewport emulation), asserted through the real offscreen
  pipeline; the `FrontCounterClockwise` pin was re-derived in item 4 (`False` after
  the F-A +Y-down fix). Native renderer smoke 46 checks (38 preserved + 8 new).
  Near/far and the game camera bridge are covered by item 4.

- E03 Block 2 gate: attempt 1 found one High must-fix (ptype 10 is drawn by the
  runtime, not skipped) plus probe counting/reset items; remediation was applied
  and verified; attempt 2 returned a COMMIT-ready verdict on the code. `Release_dev`
  and `Release_dev_native` builds exit 0 with the batch.

- E03 Block 2.4 (CPU texture decode): `utils/TextureDecode.{h,c}` is a pure,
  C-compatible decoder for indexed 4/8-bit pages plus CLUT bytes and 16-bit direct
  pages into RGBA8. Explicit row stride (the desktop loader paths hand over tight
  pages; the 1024-halfword VRAM stride is also covered); low nibble/byte is the
  leftmost texel; BGR555 `E5TO8(c)=((c*527)+23)>>6`, low bits red, bit 15 STP;
  `0000h` transparent; STP with unknown blend context -> alpha 128; the texture
  window is intentionally not applied (sample-time). Bounded validation with named
  errors and zero partial output; `TextureDecodeTests` 136 checks (synthetic
  fixtures plus an optional read-only real-page probe). No GPU, PsyCross, game
  integration or ODT code.

- E03 Block 2 item 3 (one real building/tile -> `SourceMesh` -> GLB):
  `utils/WorldMeshBuilder.{h,cpp}` is a pure C++11 `SourceMesh` -> `GltfMeshData`
  builder. It synthesizes one flat geometric normal per emitted triangle
  (winding-normalized; degenerate fallback `(0,0,1)`) and expands each triangle to 3
  dedicated vertices so flat normals are unambiguous; base colour is the mean of the
  raw stored face colours (neutral opaque white for PL records with no RGB); the node
  transform is identity; the result is deterministic; full bounds validation yields
  named errors and zero partial output, and more than one submesh yields
  `multiple-submeshes-unsupported` with zero output (never a silent drop). A
  game-side `-convertmodel <slot>` developer flag (accepted in `main.c`, value
  skipped in the arg loop; warn-only with named diagnostics) resolves
  `AssetCatalog_FindModel(slot)` + `AssetCatalog_GetModelSpan`, decodes the span with
  `SkipUnsupported`, builds, and publishes through the existing pipeline:
  `AssetConverter_ConvertMesh` (converter key `world-mesh`, `native_cache/`) writes
  the GLB with `GltfWriter`, re-verifies it with `Gltf_LoadFile`, then publishes
  atomically. Log line: `AssetCatalog: convert-model: slot <slot>, span <bytes>
  bytes, submeshes <n>, vertices <v>, triangles <t>, skipped <k>, source-hash <h>,
  content-hash <h> -> <path>`. Real Chicago evidence (Release_dev, `-opengl -nointro
  -nofmv -mission 1 -convertmodel 0`): slot 0, span 676 bytes, submeshes 1, vertices
  120, triangles 40, skipped 0; source-hash `ed3098e5f1551505`, content-hash
  `e05d775f5e30a78b`; output
  `native_cache/world-mesh_ed3098e5f1551505_0000000000000000_0000000000000000_1.bin`
  (GLB payload, 5256 bytes); two runs produced identical source-hash and content-hash
  (determinism), and slots 1 and 2 also converted with 1 submesh each.
  `WorldMeshBuilderTests` 122 checks (flat normals including a vertical triangle, base
  colour present/absent, the multi-submesh named diagnostic with zero output,
  bounds/argument errors, an uncovered index, determinism: byte-identical GLB + equal
  content hash, and `GltfLoader` reload). All 14 inspector suites pass; `Release_dev`
  and `Release_dev_native` builds exit 0. Limits: no textures/embedded images in this
  block (recorded follow-up: per-face `texture_id`/CLUT + `TEXINF` sub-rect recovery
  from the project decode plus a writer extension); flat normals only (stored gouraud
  point normals not consumed); one material = mean face colour; multi-submesh models
  unsupported in this block.

- E03 Block 2 item 4 (per-instance draw + game camera): `native/NativeRenderer.*`
  gains a bounded resource/instance API (`NATIVE_MAX_RESOURCES`/`NATIVE_MAX_INSTANCES`, raised to 64/4096 for the region path in the item-5 entry); `NativeInstance` carries `resourceKey[160]`, a
  column-major `transform[16]`, and level/variant/sourceObjectIndex/modelSlot/
  modelRevision diagnostics. `RegisterResource`/`UnregisterResource`/`ClearResources`/
  `ResourceResident` are idempotent (capacity is a named error, no eviction);
  `SetInstances`/`ClearInstances` copy by value and skip unresolved keys with a
  counter and a first-key diagnostic, never fatally; `SetClipPlanes`,
  `GetInstanceStats` and `GetFirstInstanceDiagnostic` are added. The no-fallback rule
  holds: with instances set the draw is only the instances (an all-unresolved
  submission clears and reports, never the placeholder), the modes
  placeholder|content|instances are explicit, and `LoadContent` stays the N=1 compat
  path. The projection is +Z-forward with a [0,1] depth range (`out[10]=F/(F-N)`,
  `out[11]=+1`, `out[14]=-N*F/(F-N)`) and +Y-down like the game: the F-A fix negates
  camera-space Y (`BuildPerspective out[5]=-f`) because the game camera is +Y-down
  (GTE `SY=OFY+IR2*h/sz`) while Diligent's D3D-like viewport is +Y-up
  (`DeviceContextVkImpl.cpp:1812-1837`; the project's modern-mesh path already negated
  Y, `PsyX_ModernMesh.cpp:105`). The reflection is cancelled by the cull pin flip
  (`CULL_MODE_BACK` + `FrontCounterClockwise=False` keeps the same world faces front;
  the game-front triangle `(v0,v1,v3)` area is negative after the reflection), pinned
  by the new smoke test `TestVerticalConvention` (a +y marker, which the game maps
  downward, must appear in the lower half; reverting to the old convention fails
  exactly the three vertical checks) and by the re-pinned winding fixture
  (`SourceModelDecoderTests` asserts frontArea<0/backArea>0). New pure helpers
  `utils/NativeInstanceTransform.*` (`T = Translate(pos)*RotY(yang*64)` raw
  column-major; 392 checks) and `utils/NativeCameraView.*` (raw view `[R|-R*cam]`;
  9 checks). The game bridge (`redriver2_psxpc.cpp`) feeds the real game camera
  (`inv_camera_matrix` + `camera_position`) through `NativeCameraView_Compose` with
  raw clip planes 100/125000 (`draw.c farClipLimit`); a one-shot report/dump is gated
  on a valid camera (non-degenerate view + non-zero `camera_position` + 3 consecutive
  frames; a degenerate first frame was the observed cause of an early clear dump) and
  submits the K placements nearest the camera (in-frustum and laterally-visible
  preferred, fallback nearest), logging camera-position, view diag, camera-z
  first/min/max, inFrustum and laterallyVisible, then dumping one 640x360 offscreen
  RGBA frame via the public readback. Stale-level safety: a registry miss clears
  instances/unregisters and resets the one-shot state (F-C). The instance source
  (`Game/C/assetcatalog_game.{h,c}`) adds a bounded slot->{key, glbPath, contentHash}
  registry populated on publish and a bounded resident-placements collector reusing
  the level walker (no re-decode, <=64 placements per slot with the model handle
  revision); the `-drawmodel <slot> [count]` developer flag (accepted in `main.c`;
  warn-only, named diagnostics) drives it. Evidence: native renderer smoke 130 checks
  / 0 failures (was 46 at the start of item 4; adds instance lifecycle, screen regions
  including yang=16, culling, depth, no-fallback, real-style +Z camera, vertical
  convention and placeholder); all inspector suites pass (SourceModelDecoder 1186,
  NativeInstanceTransform 392, NativeCameraView 9, WorldMeshBuilder 122, others
  unchanged); `Release_dev` and `Release_dev_native` builds exit 0. Real bounded run
  (Release_dev, `RED2_NATIVE_RENDERER=1 -nointro -nofmv -mission 1 -drawmodel 169
  32`): slot 169, 18 real placements, submitted 18, drawn 18, skippedUnresolved 0,
  residentResources 1; camera-position 17387,-1050,142670; camera-z first 10824.0, min
  -1481.9, max 77735.4, inFrustum 14/18, laterallyVisible 10/18; offscreen frame
  640x360 rgba-hash `241175f1491c392b` with 4 non-clear pixels at y[286..307] (the
  same x band at y[52..73] before the F-A fix = 360-y, empirically confirming the
  vertical correction). This proves the end-to-end pipeline executes and rasterizes
  the converted resource at its real placements with the game camera; it does NOT
  prove on-screen shape/region beyond what the offscreen suite establishes, because
  the selected model is a micro-prop (parsed POSITION bounds 134x130x68 raw ~3.2 cm)
  and larger buildings are multi-submesh. Limits: one slot + bounded K;
  single-primitive meshes only - bigger buildings fail with the named
  `multiple-submeshes-unsupported`, so the multi-primitive writer extension is the
  next mandatory item before any "visible building/region" claim; unreferenced slots
  have no placements by data; FOV 1.0472 native vs the game's 0.9265 is deferred; the
  GLB node transform is ignored in instance mode (latent, resources are identity
  today); no textures/LOD/streaming/cars/animation/performance. Gate: attempt 1
  verdict was commit-conditional on F-A (vertical) verification/fix and the docs;
  F-A and F-C/F-D were remediated and verified; this documentation update resolves
  F-B.

- E03 multi-primitive GLB support (follow-up A+B): `utils/GltfWriter.{h,cpp}` and
  `utils/GltfLoader.{h,cpp}` gain `GltfPrimitiveData`/`GltfMultiMeshData` records and
  `Gltf_WriteFileMulti`/`Gltf_LoadFileMulti`. The layout is deterministic: one shared BIN
  buffer; per primitive POSITION, then NORMAL/TEXCOORD_0 when present, then the index list;
  material i corresponds to primitive i; accessors are numbered in the same order. Writing
  is strictly validated first (no positions, a partial triangle or an out-of-range index
  yields named errors and removes the partial file). The single-primitive writer API is
  unchanged and byte-identical by construction (deterministic `%.9g` floats), empirically
  verified by compiling the pre-change writer and comparing bytes, and covered by a
  dedicated byte-identity test added with the gate follow-ups. The single loader keeps its
  historical tolerant, image-retaining contract and reads primitive 0 of a multi file; the
  multi loader is strict and rejects texture references with a named error.
  `WorldMeshBuilder_BuildMulti` maps each `SourceMesh` submesh to one `GltfPrimitiveData`
  (flat per-triangle normals; per-submesh base colour = mean of the stored face colours, or
  neutral white for PL records) and validates a contiguous, ordered, complete partition of
  the index list (deterministic, named errors, zero partial output); the single builder
  keeps `MULTIPLE_SUBMESHES_UNSUPPORTED` for compatibility and the game call site uses
  `BuildMulti`. `native/NativeRenderer` represents a multi-primitive resource
  (`ScenePrimitive` with its own vertex/index buffers, window+offscreen SRBs and base
  colour; one shared white texel per item), and an instance draws every primitive
  (UpdateBuffer -> Commit -> Draw per primitive); `drawn` still counts instances and the
  placeholder/content/no-fallback semantics are unchanged. `Game/C/assetcatalog_game.c`
  `ConvertModelSlot` now builds with `BuildMulti` and publishes with a local transactional
  `PublishMultiMesh` (reuse/tmp/verify/rename mirroring `AssetConverter`; same error codes;
  the log line and named diagnostics are unchanged). Recorded follow-ups: promote
  `PublishMultiMesh` into `AssetConverter` (it duplicates ~80 lines and its multi verify
  checks only the primitive count), and `MaterialReferencesTexture` does not cover texture
  references expressed via `extensions`. Capacity change: the renderer's
  `LoadContent`/`RegisterResource` now go through `Gltf_LoadFileMulti`, which rejects
  texture references, so a single-record GLB with an embedded base-colour image is no
  longer accepted by the renderer (named error); the E02 writer never embeds images, so the
  world-mesh pipeline is unaffected (a boundary check was added to the smoke with the gate
  follow-ups). Evidence: all inspector suites are green (`GltfRoundTripTests` 116 checks;
  `WorldMeshBuilderTests` 211; others unchanged); native renderer smoke 154 checks,
  0 failures; `Release_dev` and
  `Release_dev_native` builds exit 0. Real runs: slot 195 (the pack's largest model, span
  2044 B) now converts as multi-submesh (it previously failed with
  `multiple-submeshes-unsupported`) but is unreferenced (no placements); slot 1150
  (referenced) published a GLB parsed with 3 primitives / 3 materials, 22 placements,
  submitted 22 / drawn 22 / skippedUnresolved 0 / residentResources 1, offscreen frame
  640x360 with 42 non-clear pixels. The model is a ~0.3 m sign (bounds 916x1265x56 raw) and
  the pack is modular (max span 2044 B): no large single building meshes exist in the model
  pack. The bounded run proves the multi-primitive pipeline end-to-end (conversion,
  publication, registration, draw of all three primitives, 22/22 instances, one resident
  resource) with the game camera; per-primitive correctness is established by the offscreen
  suite; it does not prove visual fidelity or a large visible building, and there is no
  texture/lighting parity. Must not claim: no "region/building renders correctly"; the
  renderer no longer accepts embedded-image GLBs (capacity change above); no all-model/
  all-slot coverage; no LOD/streaming/cars/animation/performance; single-vs-multi byte
  identity is by construction plus empirical verification and the dedicated test; dev runs
  are not visual acceptance.

- E03 region native draw (`-drawregion`): the native scene draws a bounded snapshot of the
  converted region (all resident placements up to the caps) with the real game camera instead of a single slot. The region path
  raises the bounded capacities: `NATIVE_MAX_RESOURCES` 64, `NATIVE_MAX_INSTANCES` 4096,
  `ASSET_CATALOG_GAME_MAX_MODEL_RESOURCES` 64 and the new
  `ASSET_CATALOG_GAME_MAX_REGION_PLACEMENTS` 4096. `Game/C/assetcatalog_game.{h,c}` prepares
  once after `SpoolSYNC`: the level walker collects all resident placements (bounded at 4096,
  placement truncation flagged), derives the distinct referenced model slots (bounded at 64,
  resource truncation flagged), converts each slot once through `ConvertModelSlot` (a failed
  slot is a named warning and is skipped while the rest continue) and exposes bounded
  accessors for the bridge; state is cleared at `BeginLevel`; non-Windows/PSX stubs.
  `redriver2_psxpc.cpp` registers every converted resource (logs R/T), builds the full
  instance list in a static `NativeInstance[4096]` buffer (~1 MB BSS; `static_assert <=
  NATIVE_MAX_INSTANCES`, never on the stack), calls `SetInstances` once, and reuses the
  camera-validity gate and the one-shot report plus `native_region_draw.rgba` 640x360 dump
  with the real game camera; a level change resets the state; there is no per-frame
  re-collection, no re-decode at draw time and no placeholder fallback when instances are
  set. The bounded `-drawmodel <slot> [count]` path is unchanged. `main.c` accepts
  `-drawregion` in the arg loop, adds the help line and a warn-only post-SpoolSYNC hook.
  Evidence: all inspector suites green; `Release_dev` and `Release_dev_native` builds exit 0.
  Real mission-1 run (Release_dev, `RED2_NATIVE_RENDERER=1 -nointro -nofmv -mission 1
  -drawregion`): prepare log `4096 placements (placement-truncated), 64 distinct slots, 46
  converted, 18 failed (resource-truncated); walker complete`; the 18 failures are named
  `decode failed: instance (instanced models cannot be decoded from a single span)`;
  `registered 46/46 resources`; report `submitted 4096, drawn 2913, skippedUnresolved 1183,
  residentResources 46, placements 4096, placement-truncated 1, resource-truncated 1,
  camera-position 17387,-1050,142670, camera-z first 14956.2, min -49875.2, max 80857.4,
  inFrustum 2588/4096, laterallyVisible 1919/4096`; offscreen frame 640x360 with 64174
  non-clear pixels (~28% of the frame), full-frame bbox, 8 distinct colours. Honest wording:
  this proves the full converted region is drawn by the native renderer with the real game
  camera (2913 instances drawn, 46 resident resources); it is untextured (base colours
  only), the placement/resource caps truncate the level's full content (the E02 inventory
  counted 6740 unique placements), and 18 referenced slots are instanced models the
  single-span decoder rejects (recorded follow-up: base-instance vertex resolution). No
  LOD/streaming, no texture/lighting parity; a dev run is not visual acceptance. Recorded
  follow-ups: instanced-model decode (base-instance vertex resolution) so those slots
  convert; the placement/resource caps remain bounded dev limits; promote `PublishMultiMesh`
  into `AssetConverter` (from the previous block).

- E03 texture phase (T1-T3): the native world pipeline now carries and samples textures end
  to end. **T1 - pipeline images (pure):** `GltfPrimitiveData` gains `imageRgba` (RGBA8
  row-major, `imageWidth*imageHeight*4`; empty means untextured), `imageWidth` and
  `imageHeight`. The multi GLB writer (`Gltf_WriteFileMulti`) embeds one deterministic PNG
  per material through a minimal dependency-free encoder (8-bit RGBA, filter None, zlib with
  stored deflate blocks, CRC32/Adler-32, image bufferViews 4-byte padded); images, textures
  and samplers are numbered in primitive order and the dimensions/data are validated with
  named errors and zero partial output. A conversion with no images stays byte-identical (the
  single-primitive path is unchanged). The multi loader (`Gltf_LoadFileMulti`) decodes the
  embedded stored-block PNG back into `imageRgba`, returning a named error for unsupported
  variants (dynamic/fixed Huffman, other colour types or bit depths, interlace, non-None
  filters, non-embedded data, non-image/png); the single loader keeps its historical tolerant
  WIC path. `WorldSubmeshImage { const unsigned char* rgba; int width; int height; size_t
  byteCount; }` is added as a defaulted trailing parameter of `WorldMeshBuilder_BuildMulti`;
  whole-array validation runs up front and the result is deterministic. **T2 - game-side
  bake:** new `Game/C/texturebake.{h,c}` (guarded by `#ifndef PSX`) reads VRAM once per
  conversion through `GR_ReadVRAM` (1024x512 halfwords) and extracts the page and CLUT for
  each `(textureSet, textureId)` (`format=(tpage>>7)&3; pageX=(tpage&0x0f)<<6;
  pageY=(tpage&0x10)?256:0; clutX=(clut&0x3f)<<4; clutY=clut>>6; rowStrideHalfwords=1024`;
  16/256 CLUT entries; 256x256 full page), decodes it with `TextureDecode`, dedupes identical
  (page,CLUT) pairs and caps unique images at 64 per model (16 MiB). The `TEXINF` sub-rect is
  deliberately not cropped (full-page upload) and the raw 8-bit texel UVs keep their meaning
  (the renderer scales by 1/256). `ConvertModelSlot` bakes before building and passes the
  images to `BuildMulti`; additive `texture-bake:` info/warning diagnostics report the result
  (existing conversion wording unchanged), and per-submesh failures are named reasons
  (texture-not-declared, page-not-resident, unsupported-format, region-out-of-vram,
  decode-error, image-cap) that leave that submesh untextured while the conversion continues.
  The bake is a single VRAM snapshot at prepare, so pages that become resident later (streaming)
  stay untextured for the run and are not re-baked.
  Residency uses the loader's bookkeeping (tpageloaded/tpageslots; sentinel = never
  assigned); a known limit is that if a page's VRAM slot were reused without the bookkeeping
  updating, stale pixels could decode (documented, not guessed). The conversion cache options
  hash is keyed on the baked pixels (`HashBakedImages`; 0 when nothing baked, so the legacy
  untextured key/output stays byte-identical). **T3 - renderer sampling:**
  `native/NativeRenderer` creates a per-primitive RGBA8_UNORM texture from `imageRgba` with a
  NEAREST/NEAREST/CLAMP_TO_EDGE sampler (PSX look, no mips) and binds it in the primitive's
  SRB; untextured primitives keep the shared 1x1 white texel and the UVs are scaled by 1/256
  once at upload (`kContentTexelUvScale`; shaders/pipeline unchanged); no-fallback/stats
  semantics are unchanged. Smoke additions: a stored-block textured GLB now loads and renders
  (the old rejection boundary moved), an unsupported PNG variant is still rejected with a
  named diagnostic, a textured multi-primitive test (a 2x1 red/green image plus an untextured
  blue primitive) asserts the expected regions, and the UV-scale discrimination check
  (scale=1 fails the test) is recorded. Evidence: all inspector suites green
  (`GltfRoundTripTests` 144, `WorldMeshBuilderTests` 239, others unchanged); native renderer
  smoke 214 checks / 0 failures; `Release_dev` and `Release_dev_native` builds exit 0
  (`texturebake.c` in the generated project). A real textured region run (Release_dev,
  `RED2_NATIVE_RENDERER=1 -nointro -nofmv -mission 1 -drawregion`) logged texture-bake lines
  such as `slot 627, 9/9 submeshes textured (7 deduped, 2 images)` and `slot 149, 1/1
  submeshes textured (0 deduped, 1 images)`; the region report is unchanged geometrically
  (46/46 resources, 4096 submitted, 2913 drawn, 1183 unresolved), and the offscreen 640x360
  frame has 64174 non-clear pixels and 400 distinct colours (was 8 untextured), the top
  colours being textured greys/browns. Honest wording: the bounded region now renders
  textured (base-colour sampling, unlit) with the game camera; this does not prove texture
  colour parity with the legacy renderer (no alpha blending, no `TEXINF` sub-rect cropping,
  full-page upload, only base-colour maps consumed, and only the stored-block PNG subset - a
  general compressed PNG is rejected by the loader); the same bounded caps/instanced-slot
  gaps as the region block remain; a dev run is not visual acceptance.

- ODT-to-GLB tooling spike (bounded, external, temp-only; no repository
  integration): OpenDriver2Tools master @
  `89009f7f4f48f37a886b376292432cdab4a136` (2025-10-28) was built headless (Win32)
  and produced a full Chicago export (498 model OBJs, 159 texture pages, 135 region
  OBJs) plus one reference GLB (`CHICAGO_HEAD.glb`) through a stdlib converter.
  The adaptation surface was recorded (lift rgb5a1 conversion/RLE/`PolySizes`/byte
  structs into tooling; reimplement decode/inventory/LOD/writers in the project
  module; ODT divergences: no index dedup, normal-index bug, gouraud colours
  undecoded, `TEXINF` subrect not applied, transparency key). Informs E03 B2.4.

- E04 menu-bank PCM decode and registration seam: `Game/C/sounddecode.{h,c}` is
  an SDK-free PSX SPU ADPCM (VAG) decoder (16 B -> 28 frames, coefficient
  recursion with cross-block predictor carry, filter-nibble validation, inclusive
  block-aligned loop frames, bounded validation with zero partial output and
  deterministic results). It uses the PSX block-start loop convention,
  deliberately diverging from the fork reference's FIXME-marked `(block+1)*28`
  (documented; needs an explicit acceptance decision when looping samples
  migrate, outside the FESound slice). `Game/C/sound.c` glue under
  `#ifdef RED2_AUDIO_FMOD` decodes samples 0-3 at the `SBK_ID_MENU` load into
  `SOUND_BANK_SFX` and registers `SFX(1,0..3)` via
  `AudioService_RegisterSoundMemory` (streaming=0), unregisters on bank-1 reload,
  registers nothing when base!=0 and is a no-op while the service is inactive, so
  the seam is inert until A1 initializes it. A read-only local probe of
  `data/DRIVER2/SOUND/VOICES2.BLK` found the menu lump is 4 samples x 560 frames
  at 4000 Hz with no loop, and the level lump decodes cleanly. Verified by
  `scripts/run_audio_service_smoke.ps1` (pure decode phase without FMOD headers
  plus the service phase): 20 + 87 checks, 0 failures.

- E04 FESound FMOD routing and service lifecycle (A1):
  `Game/C/audiofmod_adapter.{h,c}` is an SDK-free helper module (consumer mode
  parsed once from `RED2_FMOD_AUDIO`, master-gain `clamp((10000+gMasterVolume)/
  10000, 0, 1)`, pitch `gamePitch/4096` clamped to `16383/4096`, and the
  `SFX(1,sample)` event-id builder). In FMOD_ONLY mode `FESound` routes to
  `AudioService_Play` as a 2D one-shot and skips the SPU body entirely, so
  `channels[]` is untouched and there is no double playback; a missing event or
  inactive service logs once and stays silent rather than falling back to SPU per
  call. `LoadSoundBankDynamic` threads the lump identity so only the fresh
  `SBK_ID_MENU` load registers `SFX(1,0..3)` and every other bank-1 load
  unregisters them. The service is initialized before `redriver2_main` (init
  failure stays on SPU for the whole run), updated once per rendered frame in
  `VsyncProc` and shut down at exit; all calls remain under `#ifdef
  RED2_AUDIO_FMOD` and non-FMOD builds are unchanged. Verified by the smoke
  (31 + 98 checks, 0 failures), `cl /Zs` with and without `RED2_AUDIO_FMOD`, and
  regenerated Release_dev/Release_dev_native builds (exit 0); the in-game probe
  (`RED2_FMOD_AUDIO=1`) initialised the service alongside OpenAL and routed 41
  FESound calls as distinct FMOD handles with 0 warnings, while the switch off
  keeps the service inactive with no route lines. The gate accepted the slice
  with two informational items recorded: the linear master-gain mapping (the SPU
  path uses a ~1.4x pre-saturation curve; accepted E04 decision) and the dormant
  lower pitch clamp (precondition documented in the adapter header).

- Production effect-timing tests for both regional clocks, strict replay dump
  inventory comparisons and diagnostic detection of recorded draw-time mutations.

- Display FPS limit beside resolution (15..360, default 30 for NTSC / 25 for PAL)
  and a confirmed NTSC/PAL startup preference in the developer graphics panel.
  FPS and VSync apply live and autosave to `developer_graphics.ini`; region
  changes take effect after application restart, preserving active replay/game
  cadence. Either desktop executable can select the original 30/25 Hz regional
  clock, aspect and sound timing; replay headers record the active region.

- Adopted engine-migration roadmap and five implementation issue briefs covering
  project-owned fixed simulation ticks/replay compatibility, original-content
  conversion, Diligent rendering, FMOD Core and progressive PsyCross removal.
  PopcornFX is an optional last extension, only explicitly requested after full
  game acceptance and removal; existing native effects remain required. Native visual changes use targeted tests while mechanics retain
  deterministic acceptance; no engine migration is implemented by this plan.

- E00 engine-migration inventory: recorded every required PsyCross/Psy-Q
  service contract, the NTSC/PAL tick, replay and RNG facts, and the draw-time
  mutation sites in `knowledge/engine-migration/e00/`, backed by a reproducible
  symbol/include scan (`scripts/engine_migration/e00_symbol_scan.py`) and a
  baseline scenario matrix. Added a per-tick canonical state dump for
  determinism checks (`-statedump` in `utils/DeterministicBaseline.*`, compared
  by `scripts/engine_migration/e00_compare_state.py`) and `RED2_BUILD_DIR` /
  `RED2_BIN_DIR` overrides to `premake5.lua` for isolated regional builds.
  Fresh NTSC and PAL `Release_dev|x64` builds, a Vulkan smoke run and four
  replay/mission dump pairs (0 mismatches between runs) are recorded in the
  inventory. No engine behaviour changed.

- E01 fixed simulation ticks: the desktop tick is now owned by a project clock
  (`utils/ProjectClock.*`, 30 Hz NTSC / 25 Hz PAL) with bounded catch-up and a
  pause API instead of the emulated vblank gate, and gameplay reads of the draw
  counter `FrameCnt` moved to the simulation tick. Authoritative state is
  unchanged versus the pre-E01 baselines (0 mismatches on all shared ticks), and
  presentation now draws every frame: vsync (~30 FPS) and uncapped (~250 FPS)
  runs produced identical authoritative tick state on NTSC replay/mission and
  PAL replay. Smooth interpolated high-FPS presentation (previous/current
  snapshot blend) is still pending.

- E01 versioned replay contract: new replay tapes are written with an explicit
  format version, region, tick rate and period plus a content checksum in the
  header's previously unused reserved fields (on-disk size unchanged), while
  legacy tapes still load. Unknown magic, unsupported version, region/rate
  mismatch and checksum mismatch are now diagnosed explicitly instead of being
  silently accepted or silently resampled to the build cadence. A
  `-savereplay`/`-saveat` developer fixture records a tape without the pause
  menu; NTSC/PAL round trips, the four diagnostics and the legacy replay
  regression were verified.

- E01 presentation interpolation: a project-owned visual snapshot seam
  (`utils/VisualSnapshot.*`) publishes each car's previous and current tick pose,
  and cars are rendered between them using the fixed clock's interpolation
  factor, so high display FPS produces smooth motion instead of repeated tick
  poses. The seam only reads authoritative state; attract replay and mission
  baselines remain identical (0 mismatches). Subsequent complementary fixes share
  car/camera rotation and effect snapshot time; full presentation acceptance
  remains migration work.

- E01 input service: SDL2 keyboard/game-controller polling and the input mapping
  now live in the project (`utils/InputService.*`) instead of PsyCross; the game
  reads pads through it while keeping the `Pads[]` and per-tick sampling
  contract. PsyCross's own pad backend is disabled in the project fork so there
  is a single controller owner. Verified with live keyboard drive tests and
  unchanged NTSC/PAL replay baselines.

- E02 reproducible conversion: exported texture manifest entries now record a
  canonical 64-bit FNV-1a `contentHash` over the converted pixels, so two
  identical conversions produce equal entries. A new pure-data package manifest
  module (`Game/C/assetpackage.*`) parses, validates and canonically serializes a
  readable JSON package with named diagnostics (schema version, missing fields,
  duplicate ids, unknown dependencies, trailing data) and no PsyCross/GPU/game
  dependency. A conversion cache contract (`Game/C/conversioncache.*`) keys a
  conversion by source hash, converter, schema version, options and active mods
  with staleness reasons and deterministic cache paths. A bounded GLB writer
  (`utils/GltfWriter.*`) and the importer now round-trip geometry, UVs, the node
  transform and material flags and write byte-identical GLB for identical
  input. A transactional converter (`utils/AssetConverter.*`) turns a mesh record
  into a cached native GLB keyed by source/converter/schema/options/mods, reusing
  fresh artifacts and rebuilding stale ones, and the package manifest can be
  assembled from the converted hashes (`AssetPackage_AddEntry`). A sound asset
  record (`Game/C/soundasset.*`) carries sample rate, channels, bit depth, frame
  count, loop points, pitch reference, bank/voice ids and music metadata with
  validation and canonical serialization. Verified by new
  `InspectorExportTests` checks (107 passed), `AssetCatalogTests` (145 pass) and
  the new `AssetPackageTests` (39), `ConversionCacheTests` (38),
  `GltfRoundTripTests` (28), `AssetConverterTests` (28), `AssetPipelineTests`
  (25) and `SoundAssetTests` (32) suites.

- E02 real-source conversion: `utils/CarMeshSerializer.*` turns game-side car
  geometry into a deterministic cached GLB through the conversion pipeline
  (source/converter/schema/options/mods key, read-back verify, temp+rename
  publish, content hash), and the developer 3D debug tab can convert the
  selected car model (`Cars_ConvertInspectorModel`) beside the existing OBJ
  export. Covered by the new `CarMeshSerializerTests` (64 checks); all nine
  inspector suites pass.

- E02 Chicago slice inventory: a pure reconciliation contract
  (`AssetCatalog_ReconcileInventory`) and a loader-driven report
  (`AssetCatalogGame_ReconcileLevel`, `-assetinventory` flag and panel button)
  publish `assetcatalog_level_<n>_inventory.txt`. Gate 4A attempt 2 is approved
  as a scoped diagnostic only after fix-4; catalog tests pass 258/0. Fresh
  parent-integrated x64 runtime report scans all four eligible barrels:
  6740 unique placements, 227 straddlers, 0 overlaps/missing model refs, 114
  unknown material classifications and 0 errors/480 warnings (64 displayed,
  truncated). The corrected walker includes terminal objects marked `0x8000`,
  so the initial 6078-placement report is superseded. This remains a scoped
  source-union diagnostic, not conversion or world acceptance. See
  `knowledge/engine-migration/e02/chicago-inventory.md`.

- E04 audio contract: `Game/C/audiocontract.*` defines the project-owned
  gameplay-to-audio boundary (stable event ids, play flags, request validation,
  a fixed voice table with per-slot generations and inclusive loop metadata)
  before any FMOD SDK integration; its initial `AudioContractTests` result (216
  checks) predates fix-5; the reported post-fix result is 221/0 below.
  `scripts/fetch_fmod.ps1` checks the gitignored FMOD Engine SDK, whose local
  header and DLL metadata identify 2.03.15 (0x00020315 / resource 2.3.15), and
  documents the login-gated acquisition, and
  `knowledge/rules/fmod-integration.md` records version/distribution/threading
  and VAG/CD-XA transcode rules.

- E04 FMOD service remediation (fix-5): per-channel 2D/3D replacement,
  concurrent samples, persistent service generations, checked listener updates,
  and explicit unsupported RESTART/stream-loop diagnostics are implemented.
  Inclusive loop endpoints are unchanged and equal endpoints are rejected;
  Doppler remains disabled pending units validation, and an x64 SDK guard
  protects target wiring. The initial post-fix smoke was 49/0, with final
  AudioServiceSmoke 57/0 and AudioContractTests 221/0; SDK fetch check
  exits 0, and nine SDK-version fixtures pass 9/0. A reset-on-restart mutant
  fails 13/57 checks as expected; live-service source hash is unchanged. The
  two nonblocking smoke/version nits are resolved. Gate 4B attempt 2 is approved
  as a bounded SDK-isolated service only—not consumer migration or audible correctness.
  Parent independent builds/suites are complete. Release-failure stream storage
  lifetime, captured output, device loss and stream stress are unverified.
  Earlier 39/0 and 216/0 results are historical.
  The game target links `fmod_vc.lib`, defines `RED2_AUDIO_FMOD` and copies
  `fmod.dll` when the SDK is present; no game consumer calls the service yet.

- E03 SDK pin/build: DiligentEngine is pinned at `v2.5.6` (DiligentCore commit
  `b036337d`) and built Vulkan-only as a static Release library through the new
  `scripts/fetch_diligent.ps1`. The SDK stays in the gitignored
  `dependencies/` tree; new native renderer modules use C++17.
- E03 native renderer owner: `native/NativeRenderer.*` owns one Diligent
  device/context/swap chain for a single native window handle (Diligent-free C
  ABI) with clear-and-present, resize and shutdown, uploads converted mesh
  geometry into immutable Diligent buffers, renders overlapping triangles through
  a depth-tested graphics pipeline, and samples a base-colour texture on a quad
  transformed by a constant-buffer MVP, all pixel-verified. Verified by
  `scripts/run_native_renderer_smoke.ps1` (15 checks). On Windows the native
  renderer and Diligent are linked into the game when the SDK is present, behind
  a gated `RED2_NATIVE_RENDERER` probe over the game's own window. PsyCross can
  now initialize headless (window only, no swap chain or present), so the native
  renderer owns the window's presentation and the probe reports `native renderer
  active on the game window`. A generic post-frame handler then lets the game
  feed its own camera to `NativeRenderer_SetCamera`/`NativeRenderer_RenderScene`,
  which draws a depth-tested, textured scene every frame (`native renderer
  drawing the game window with the game camera`). A `Release_dev_native` build
  configuration launches the game with the native renderer enabled, so it can be
  tested from Visual Studio. The Diligent SDK was upgraded to the official
  repository's master (`API256019`).

- E03 converted content: the native renderer loads a converted GLB through
  `utils/GltfLoader` and draws it in place of the placeholder scene with the
  game camera (`NativeRenderer_LoadContent`/`ClearContent`/`HasContent`/
  `GetContentStats`); the node transform places the mesh and the base-colour
  factor/texture shade it unlit. `NativeRenderer_RenderSceneToRGBA` runs the
  same draw offscreen for pixel checks, and `RED2_NATIVE_CONTENT` loads a
  converted GLB into the in-game native path without aborting on failure. The
  in-game path now releases the Diligent device before the game window is
  destroyed, forwards window resizes to the swap chain and logs scene
  failure/recovery transitions. Verified by `scripts/run_native_renderer_smoke.ps1`
  (38 checks, 0 failures) and an in-game probe (content loaded and drawn, resize
  applied, clean exit).

- E01 bounded backlog policy: the fixed clock caps catch-up at four ticks per
  frame and counts the excess; the game loop now logs the dropped ticks
  (rate-limited) so a stall never silently discards simulation ticks. Pause
  resume and the first presented frame re-base the clock so load/pause intervals
  are not replayed as a burst, and a `RED2_CLOCKJUMP_MS` fixture exercises the
  bounded path.

- The opt-in performance log (`PSYX_PERF_LOG=1`) now reports `submit_ms`, the
  mean CPU time of the frame-submission path (`GR_SwapWindow` plus the OpenGL
  overlay). Measured at 1280x720: ~6.4 ms on Vulkan, where the whole frame is
  recorded at present time, versus ~0.18 ms on OpenGL. Uncapping present
  (`PSYX_VK_PRESENT_MODE=immediate`) and disabling vsync both leave the frame
  rate at 30.0, confirming the ceiling is the game's emulated PSX vblank rather
  than the renderer.
- The modern light set now also shades the legacy scene (renderer roadmap
  legacy-lighting-receptivity): road, cars, trees, barriers and near buildings
  brighten by the modern sun's `N·L` term while the legacy shading itself is
  preserved. Controlled by `legacyLighting` and `legacyLightReceptivity` in
  `developer_modern_mesh.ini`, the Graphics panel and F9; default off. Both
  backends implement it by sampling a copy of the framebuffer colour, because a
  blend on a fixed-point attachment clamps the source to `[0,1]` and could only
  darken. The normal comes from a five-tap cross of the neighbour world
  positions with an edge test (a single-pixel derivative flickered on moving
  vehicles), the term is gated to `depth < 0.999` so the sky/skyline band stays
  untouched, and it fades out with distance at `2x`-`4x` the shadow volume
  half-size. The shadow volume and fade centre follow the player vehicle instead
  of a fixed spawn anchor. Measured `+10%` to `+34%` on legacy surfaces depending
  on the sun angle, with the sky unchanged. The composite needs both PGXP
  options on, because it reads the depth they write; the panel states this on
  both checkboxes. See
  [`knowledge/product/legacy-lighting-receptivity.md`](knowledge/product/legacy-lighting-receptivity.md).
- The Graphics tab exposes the whole modern light set under **Modern sun and
  look**: sun azimuth and height, sun intensity, modern ambient, modern exposure,
  shadow volume size, legacy light strength and a shadow debug view, each with
  its explanation and persisted to `developer_modern_mesh.ini`. The F-key
  controls and the sliders stay in sync.
- The display confirmation is now its own ImGui window at the bottom-left of the
  applied resolution, drawn whether or not the panel is open, with the cursor
  shown and input captured while it is armed, so a mode change can no longer
  hide the Keep/Revert buttons.
- Checkbox and slider help markers in the developer panel moved to the end of the
  control's label (on their own line only when the label fills the line), and the
  Input tab table now has a dedicated wrapping column for shared bindings instead
  of letting a long conflict note widen the table past the panel.
- Three more live Graphics-panel controls, persisted to
  `developer_graphics.ini` (`schemaVersion` 6 -> 7): **Dynamic vehicle lights**,
  **Widescreen overlay alignment** and **Fast loading screens**. Each shows when
  it takes effect and names the equivalent `config.ini` key, which remains the
  shipped default. Runtime settings GUI, milestone 2.
- Display mode in the developer panel (`schemaVersion` 7 -> 8): a Fullscreen
  (desktop) checkbox and a window-size combo that apply immediately through the
  new `PsyX_ApplyWindowMode`. A change is provisional for 15 seconds with
  *Keep*/*Revert now* buttons, the countdown also runs while the panel is
  closed, and an unconfirmed change restores the previous mode. Verified for
  windowed sizes; the viewport and primitive picking follow the new window size.
  Runtime settings GUI, milestone 3.
- Developer panel **Input** tab: click a binding to capture a key, controller
  button or stick push, with Escape/right-click/Cancel/timeout cancellation,
  conflict markers when two actions share a binding, and per-action or
  per-device reset to the `config.ini` defaults. Game and menu tables stay
  distinct, an edit only reaches the mapping that is currently active, input is
  held while capturing, and changes are saved to `developer_input.ini` as
  overrides (a binding equal to the `config.ini` value is not written). Runtime
  settings GUI, milestone 4.
- Developer panel **Content and language** section (Game Debug tab) for
  `overrideContent`, `disableChicagoBridges`, `languageId` and `driver1music`,
  each stating when the change applies, plus the restart-only `freeCamera`
  shown disabled with the reason.
- Developer settings files are now written failure-safely and without losing
  unrelated content: unknown keys, comments and blank lines are preserved, keys
  removed from the owner are dropped, and the previous contents are kept as
  `.bak` (`DeveloperSettingsFile_WriteKeys`). Runtime settings GUI, milestone 5.
- The in-game modern-mesh system now runs on the Vulkan backend (renderer
  roadmap item 14, defect 3, Option A), so it is live in the default
  configuration instead of OpenGL-only. `PsyX_ModernMesh.cpp` dispatches every
  call to a new `PsyX_Vk_GameModernMesh*` module that owns its own load render
  pass, UBO, descriptor sets and pipelines; `vk_shaders/psx_modern.vert/.frag`
  reproduce the GTE-encoded vertex path (GL-to-Vulkan clip conversion, xyScale,
  camera transform) and the GGX/Schlick PBR fragment path, and
  `vk_shaders/fullscreen.vert` plus `psx_composite.frag` composite the modern
  pass over the PSX main pass with the scene-depth shadow term. The developer
  gallery no longer disables itself on Vulkan, so the F10 toggle and the eight
  GLB fixtures behave as on OpenGL. The shared `ModernUBO` layout must be
  declared identically in all three shaders; the vertex shader had been missing
  `ambientExposure`, which shifted `cameraPos` onto the wrong std140 slot and
  clipped every mesh away.
- An experimental native-Vulkan backend for the modern scene and a standalone
  developer window (renderer roadmap item 14, milestone R7, partial):
  `PsyX_Vk_*` in PsyCross owns an SDL window, swapchain, depth buffer, shadow
  map, PBR/depth pipelines and descriptor sets, with texture uploads, resize
  handling, RGBA readback and an ImGui overlay. It negotiates the newest API
  the loader supports (Vulkan 1.4.0 here) and resolves the loader at runtime
  through SDL, so the build needs the Vulkan headers only - no SDK, import
  library or validation layers. Run it with `-vkfixture` (add `-vkcapture N`,
  `-vkshot <path>` or `-vknogui`); it renders the same eight Meshy GLBs plus
  four analytic spheres as the OpenGL fixture. SPIR-V is embedded and
  regenerated with `scripts/compile_vk_shaders.ps1`. The fixture renders
  correctly with zero Khronos validation errors, and `-vkshot` writes an
  upright BMP, and the main render pass is created only after the surface
  format is negotiated (before that fix every draw in the pass was silently
  discarded). Work in progress towards rendering the game itself with Vulkan:
  the emulated PSX path is ported (`vk_shaders/psx.vert`, `vk_shaders/psx.frag`
  with the in-shader CLUT, texture window, dither and bilinear filter), the
  `R32G32_SFLOAT` VRAM image, the RG8 decode table and the five PSX blend
  pipelines are created, and `REDRIVER2_dev.exe -vkpsxtest` renders synthetic
  PSX quads and asserts the readback pixels (`(252,0,0,255)` for a 16-bit
  `0x001F` word, `(0,0,252,255)` for a 4-bit texture resolving to CLUT entry 5)
  with zero validation errors. The game still renders through OpenGL; the GR_*
  contract is mapped onto this path in the next phase.
  On macOS SDL resolves MoltenVK, so the same source is the portability path,
  but no macOS build has been produced or verified.
- The game window can now render through the Vulkan backend (renderer roadmap
  item 14/R7b, partial): `REDRIVER2_dev.exe -vulkan` maps the `GR_*` contract
  onto the native backend, so the in-game scene, HUD and minimap are presented
  by Vulkan while OpenGL stays the default and is run without the flag
  (`Release_dev_gl`). The PSX vertex capacity was raised to 65536 vertices (the
  previous 256 KiB buffer truncated roughly 11500 vertices per frame), the game
  texture-slot limit went from 64 to 512, and `GR_StoreFrameBuffer` now mirrors
  the presented frame into the PSX VRAM mirror so framebuffer-reading effects
  such as the sky lens flare sample it. Verified in an original Chicago launch
  under the Khronos validation layer with zero validation errors and a
  screenshot matching the OpenGL image; the `-playground` fixture and the
  `-vkpsxtest` PSX self-test still pass.
- A fixed fixture gallery for the experimental modern path (renderer roadmap
  item 14): all eight owned Meshy GLBs (six base-colour, two PBR) plus the four
  analytic material spheres are now placed in the same scene, each grounded on
  its own bounding box at the terrain height and laid out once from the spawn
  pose. The update now runs with the frame's final scene camera, which removes
  the slight swimming the imported mesh showed while the camera moved, and
  `PsyX_ModernMesh_SetInstanceWorld` gives the shadow pass the instance world
  transform so casters are rendered where they stand.
- Modern shadows now reach the existing scenery (renderer roadmap item 14,
  milestone R5 completion of the receiver side): the modern shadow map is
  projected over the already rendered legacy frame, so modern objects cast onto
  the road, walls and trees as well as onto themselves. The pass copies the
  scene depth (`GL_DEPTH24_STENCIL8`), reconstructs world positions through the
  inverse `Projection3D` and the inverse camera view, applies a 3x3 PCF term,
  and is gated by the existing shadow toggle. Diagnostic modes for the pass are
  selectable with the `7` key or `shadowdebug=` in `developer_modern_mesh.ini`
  (`1` depth, `2` shadow-volume membership, `3` shadow map, `4` receiver vs
  caster depth). The sun default now faces the follow camera so cast shadows are
  visible without rotating the light.
- A classic/enhanced renderer switch and the R6 Forward-path decision (renderer
  roadmap item 14): the developer Graphics panel gains an **Enhanced renderer
  (modern meshes/PBR)** checkbox plus **Modern shadows** and **Modern ambient
  occlusion** toggles, persisted in `developer_graphics.ini` (schema 6) and also
  reachable with the F10 key live without reloading. The reference Forward path
  is retained as the initial production choice with the recorded rationale.
- Lighting effects for the experimental modern path (renderer roadmap item 14,
  milestone R5, partial): a light set of up to eight directional/point lights
  with colour, intensity and range, ambient and exposure; a 2048x2048
  directional shadow map whose casters are the modern meshes and whose receivers
  are both the modern meshes and the legacy scene (see the shadow-projection
  entry above); glTF emissive factors and maps; and a normal-based
  ambient-occlusion approximation as its own toggle.
  In game `[`/`]` rotate the sun, `;`/`'` change elevation, `-`/`=` change
  exposure, `0` toggles shadows and `9` toggles ambient occlusion; values are
  persisted to `developer_modern_mesh.ini`.
- A reference Forward PBR path for the experimental modern meshes (renderer
  roadmap item 14, milestone R4, partial): the new shader shades
  metallic/roughness with GGX + Schlick Fresnel, one directional light, an
  ambient term and an exposure multiplier, decoding base colour sRGB->linear and
  re-encoding on output. It binds the imported normal map (tangent frame
  reconstructed from screen-space derivatives) and the metallic/roughness map
  (G/B). Two Meshy PBR fixtures (`bollard_pbr.glb`, `oil_barrel_pbr.glb`, 20
  credits) plus four procedural analytic material spheres render with visible
  metal/dielectric response; light direction, intensity, ambient and exposure
  are set from `developer_modern_mesh.ini`. Real-time light control and
  shadows/AO remain R5.
- A bounded glTF 2.0 / GLB static-asset import path (renderer roadmap item 14,
  milestone R3, partial): `utils/GltfLoader.*` reads one mesh primitive
  (float positions/normals/UVs, 16/32-bit indices) and one
  `pbrMetallicRoughness` material with an embedded base-colour image decoded
  through WIC, and the new `PsyX_ModernMesh_CreateEx` draws it through the
  shared-depth modern path. `DeveloperModernMesh` selects a fixture from
  `developer_modern_mesh.ini` (`asset=...`) and keeps the synthetic cube as the
  fallback. Six owned Meshy fixtures (Meshy 6 preview + texture, 180 credits)
  are retained under `assets/modern_fixtures/` with `provenance.md`; verified in
  the playground with `bollard.glb` (2220 verts) and `wooden_crate.glb`
  (1822 verts). PBR maps and lighting remain R4.
- An experimental hybrid modern-mesh path (renderer roadmap item 14, milestones
  R1/R2, partial): PsyCross now exposes a small C-compatible persistent
  VAO/VBO mesh API (`PsyX_ModernMesh_*`) that draws unlit vertex-coloured
  geometry through the legacy `Projection3D` in the shared colour/depth buffer,
  and the game owns a synthetic static fixture
  (  `utils/DeveloperModernMesh.*`). In the playground and in an original Chicago
  launch the fixture mutually occludes with the legacy scene (car, floor,
  skyline and street geometry); a persisted `developer_modern_mesh.ini` flag
  (toggle with `F10`) restores the exact legacy-only baseline. The path is a
  no-op on non-OpenGL targets and is carried by the project PsyCross fork. R3/R4
  asset/PBR work and the initial hardware budgets remain open.
- Palette variant texture exports: a manifest entry may now carry a `"clut"`
  that is part of the override identity, so a car or pedestrian texture drawn
  through several runtime palettes gets one PNG and one registration per
  palette instead of collapsing into a single `(texture, texturePage,
  textureIndex)`. The 3D Debug tab exposes **Export all palette variants (PNG)**
  for car and pedestrian selections, writing `NAME_pP_iI_clutN.png` files. Legacy
  entries without `clut` keep the previous single-override behaviour, and a load
  registers one override per declared CLUT. Measured with two `clut` entries for
  `GRASS01C`: 2 loaded images and 2 active renderer mappings.
- Type/level metadata backfill on re-export: a pre-existing manifest entry that
  predates the descriptive `type`/`level` fields gains them on the next
  re-export with a context, without a duplicate registration, alongside the
  existing model-reference merge.
- A live page-registration cost diagnostic on the Mods tab. Measured on the
  Chicago debug start: 0.176 ms of name registration for 18 pages with no mods,
  versus 927.6 ms when the `inspector-export` mod's PNGs are decoded during the
  pass, so the spool cost is override-image decoding rather than the
  registration walk.
- Opt-in proportional texture-override alpha (roadmap item 16, complete): the
  new persisted `Proportional override alpha` developer setting (Mods tab,
  default off) makes `BM_AVERAGE` override draws blend the imported PNG alpha
  through `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` instead of the binary 0.5 cutout, so
  `alpha 0/64/128/192/255` render five steps while `alpha 0` still punches
  through and `alpha 128` still matches the original `STP=1` blend. Opaque,
  additive and subtractive draws keep the compatibility cutout, so existing mods
  are unaffected until the flag is enabled; the CPU picker mirrors whichever
  mode is active.
- Whole-object selection across renderer categories (roadmap item 06,
  complete): the 3D Debug inspector now resolves a clicked primitive to the
  source that produced it and reads it at four scopes - **Face**, **Material**,
  **Component** and **Logical object** - with game-owned identity for buildings,
  tiles, animated props, car bodies, wheels and pedestrian parts. Keys are
  resource-based and LOD-independent, retained selections carry a catalog
  generation plus a model handle (and a car slot for vehicles) and report
  `valid` / `stale` / `not model-backed` instead of retargeting silently.
  Picking is visibility-aware: ordering-table arrival order is the depth test, a
  registered source is preferred over unidentified overlays so screen-space
  effects no longer shadow the scene, the cursor is projected into the split's
  emulated display area, PGXP is honoured with texture mapping and the Z-buffer
  either way, and override cutout coverage is mirrored on the CPU so transparent
  texels are skipped and the geometry behind resolves. New diagnostics report
  the frame-local pick index, sources that are registered but unreachable,
  masked and cut-out override texels, a   locator for any reachable source or
  component, and the measured pick cost. The identity and picking paths are
  platform-neutral; the diagnostics UI follows the existing developer-panel
  guards (Windows and Linux), and the renderer changes live in the project
  PsyCross fork.
- A resident procedural playground scene (roadmap item 13, milestones P1-P5)
  that reuses the existing renderer and original vehicle simulation: a
  generated flat drivable surface, collidable box obstacles, disabled donor
  traffic/police/missions and camera events, a car reset control, safe return
  to the frontend, a `-playground` developer launch path, and a **Playground**
  choice on the Take a Ride city screen that starts the same launcher from every
  build. A loaded original level only supplies car, texture and sound resources;
  its world geometry, roads and collisions are replaced. The stable fixture
  identity is `playground.flatpad.v1`.
- Batch texture export from the 3D Debug tab (roadmap item 05, complete): one action exports the currently identified texture list, deduplicating by the exact `(name, texturePage, textureIndex)` identity so a texture shared by several models is published once, and reports exported, duplicate-omitted and failed counts with the last failure named. New manifest entries carry a backward-compatible `modelReferences` array and a readable, sanitized model filename suffix derived from the asset catalog, falling back to an explicit `slot<index>` / `unknown:model-slot:<index>` form when no catalog record exists; model references are metadata and never change the override key. Re-exporting writes the PNG to the manifest's existing mapped `file` and merges new references into the same entry, preserving unknown fields and never appending a duplicate registration, and a separate catalog-scoped action exports every material linked to the selected source model, including hidden faces and its high-detail LOD sibling, resolving the VRAM region of materials that are not on a submitted triangle and naming any missing adapter (palette variants and cross-model child parts are not enumerated yet). Large batches run cooperatively across frames with a progress bar, cancel control, per-resource results and retry of failed resources, publishing each file independently so cancelling keeps what was already written. Validated in-game on the Chicago debug start: selecting tile `GRASS01C` (source model 76), the catalog-scoped batch exported 1/1 with 0 duplicates, a repeat run left the manifest at 80 distinct entries with no duplicate identity, and the entry kept all 8 shared model references.
- A source-aware asset catalog (roadmap item 04, complete): a game-owned,
  allocation-free module that separates stable resource identity (models,
  textures, cars, palettes) from runtime instances, keeps the exact
  `(texture, texturePage, textureIndex)` identity the mod manifest matches on,
  records explicit `UNKNOWN`/`DECLARED`/`VERIFIED` provenance, links models to
  textures and LODs many-to-many, and invalidates retained handles when a
  runtime slot is reused, streamed away, or the level changes. It is populated
  from the level loaders (models with names and LOD links, texture sets, car
  resident slots, model material links), re-populated as regions stream, and
  includes the playground's generated assets; the building/tile/car inspector
  keys carry catalog source identity and the city variant. The 3D Debug tab
  shows catalog context, counts vs capacity, the fixed footprint, and the
  selected building/tile's stable id, provenance and source-material list
  (including hidden faces). Covered by the standalone `AssetCatalogTests`.
- Linked playground and renderer-modernization roadmaps, with a validated
  minimum playground preceding modern scene experiments and original-city
  regression checks retained alongside the dedicated test environment.
- A planned Meshy MCP workflow for prompt-generated PBR test assets, with
  export validation and reproducible local fixtures for renderer development.
- A source-backed discussion of a procedural driving playground, declarative
  scene options, collision requirements, and Take a Ride integration.
- An evolving knowledge discussion catalog, with automatic topic maintenance
  guidance and an illustrated renderer-modernization exploration kept separate
  from adopted roadmap work and implemented features.
- Thirteen ordered roadmap records for mod-system PR readiness, texture
  correctness, manifest merging, asset identity, batch exports, inspector
  coverage, camera/previews, tool interoperability, model round trips,
  runtime settings, draw distance, and measured graphics improvements.
- A knowledge roadmap lifecycle for planned and completed features, with
  required product documentation for completed roadmap records.
- Object-level building inspection, LOD-independent world-object keys, bounded
  model-name lookup, and a list of registered textures on the selected object
  with individual PNG export actions.
- Standalone Windows export regression tests covering channel fidelity,
  repeated writes, source validation, and preservation after failed writes.
- Added configurable diagnostic selection highlighting, draw-source labels,
  live car-slot positions, loaded override previews, selection-detail TXT
  export/clipboard copying, and explicit export paths
  and disabled-action explanations in the 3D inspector.
- Added discoverable JSON texture mods under `mods/<id>/manifest.json`, with a
  deterministic `mods/enabled.json` load order and a `3D Debug` inspector for
  their declared texture resources.
- Added click-based PSX primitive inspection, including texture page, CLUT,
  UV, level texture identity, active override, and provenance for car bodies
  and city tiles; selected textures export as PNG and selected cars as OBJ.

- A Windows desktop HD-texture vertical slice: external RGBA PNG replacements
  can be mapped by `TEXINF` name with original TIM/VRAM and CLUT fallback
  preserved, plus manifest documentation and Developer Graphics Panel
  diagnostics.
- A **Reproduce this state** section in the developer panel's Game Debug tab
  that generates and copies the direct-start command line for the current
  mission, vehicle, position, players, and chase, and persists it to
  `developer_debug_start.ini` for automatic startup.
- `scripts/run_debug_start.ps1`, which launches a debug build at a saved or
  explicit session and can capture the same scene with texture overrides on and
  off. Scripted captures use the new `[game] captureAfterSeconds` setting and
  `[render] textureOverrides` instead of injecting keyboard input.
- Added `-gametype <n>`, `-level <n>`, and `-startdir <angle>` debug arguments
  so a direct start can restore the game mode, city, and vehicle heading;
  `GAME_TAKEADRIVE` derives the mission from the level, so `-mission` alone was
  not sufficient, and the position override previously discarded the heading.
- The generated project debugger working directory is now the executable
  folder, so `F5` finds `config.ini`, `mods/`, and `developer_debug_start.ini`.
- Documented deterministic launch arguments: `-mission`, `-gametype`, `-level`,
  `-playercar`, `-startpos`, `-players`, `-chase`, `-replay`, `-nointro`, and
  `-nofmv`.
- Dear ImGui developer panel, opened with F11, with live graphics controls and
  an explained game-debug tab for renderer, streaming, traffic, police,
  mission, vehicle, and road-state telemetry.
- Durable agent guidance, OKF knowledge catalog, and build-file regeneration
  rule for contributor and AI-agent workflows.
- `BUILDING.md`, a dedicated build and run guide, with per-platform
  prerequisites and dependency installation, Windows/Linux/WSL steps, the
  deterministic debug-start workflow, the standalone export tests, screenshots
  in `docs/images/`, and troubleshooting. The README now links to it instead of
  inlining the build steps.
- Inspector exports record what they belong to: every manifest entry now carries
  a `type` (`buildings`, `sprites`, `cars`, `pedestrians`, `tiles`, `props`)
  and the `level` it is used on, and the developer panel gained an opt-in
  **Organize exports by type and level** setting that writes textures under
  `assets/inspector/<type>/<level>/`. The flag is off by default; the manifest
  always stores the real relative path.
- The developer panel can export textures in **base colours** (enabled by
  default). Cars and pedestrians are drawn through runtime palettes, so the
  choice is explicit at export time: enabled, the PNG uses the palette the level
  registered for the texture - the original artwork colours that keep working
  with CLUT recolouring; disabled, it uses the palette of the clicked primitive,
  so each instance exports the colours it currently shows. The tooltip explains
  both cases.
- `scripts/run_inspector_tests.ps1` builds and runs `AssetCatalogTests` and
  `InspectorExportTests` in fresh directories, so the export-path verification
  is reproducible. `InspectorExportTests` is not idempotent and must never run
  twice in the same directory; `AGENTS.md` records the command.
- Double-clicking in pick mode selects the whole object instead of the part: the
  selection keeps the parent instance of the clicked component (a car, a
  pedestrian) and highlights and exports every part together. A single click
  keeps selecting the individual part.
- An MCP tooling workflow in `AGENTS.md`: agents must check for configured MCP
  servers and use context7 for library documentation, visual-studio-ide-mcp to
  build/debug the solution open in Visual Studio (`Release_dev_gl` OpenGL,
  `Release_dev` Vulkan), and computer-control-mcp / windows-mcp to navigate the
  running game and take screenshots. Terminal launches of the game must be
  fire-and-forget, with log and screenshot polling, because the interactive
  process otherwise blocks until the window closes.
- A mandatory **Code quality and structure** section in `AGENTS.md` for new
  code and maintenance edits: readable idiomatic C++, no spaghetti code or
  unnecessary global state, separated responsibilities, small cohesive
  functions and domain-named types, no premature abstractions or out-of-scope
  refactors, explicit error handling, preserved determinism in game logic, and
  a final self-review for mixed responsibilities, duplication, coupling and
  hard-to-follow execution paths.

### Changed

- Project status is now tracked in two complementary files under `knowledge/`:
  [`CURRENT_STATUS.md`](knowledge/CURRENT_STATUS.md) is the cumulative source of
  truth for everything implemented in this fork and is updated whenever a change
  lands, and [`RECENT_CONTEXT.md`](knowledge/RECENT_CONTEXT.md) is the
  session-to-session handoff (moved from `docs/ai/RECENT_CONTEXT.md`, which no
  longer exists). `AGENTS.md` documents both and requires the status update as
  part of finishing a change.
- The native Vulkan backend is now the default renderer for the game (renderer
  roadmap item 14/R7b). It had been opt-in behind `-vulkan` while OpenGL stayed
  the default "until parity is proven"; that condition now holds - same image,
  resize, readback, primitive-mask stencil, offscreen render-to-VRAM and VRAM
  export, zero validation errors, and measured 30 FPS equality with OpenGL - so
  the default is flipped. OpenGL remains fully selectable as the fallback with
  the new `-opengl` flag, `-vulkan` is still accepted, and a machine whose
  driver exposes no usable Vulkan device falls back to OpenGL automatically at
  startup. PSX, Android and Emscripten builds keep OpenGL as their default
  because the backend is compiled out there.
- The emulated-PSX primitive mask bit is now implemented on the Vulkan backend
  (renderer roadmap item 14/R7b): the main pass depth attachment prefers a
  stencil-capable `D24_UNORM_S8_UINT` format, cleared to zero every frame, and
  each PSX blend mode gains a mask-set pipeline variant. `GR_SetStencilMode`'s
  OpenGL semantics are reproduced exactly: the mask-bit draw (`DrawPrim`,
  `overmap.c` map tiles, `loadview.c`) always passes and writes stencil bit 4,
  while every other draw only passes where that bit is clear. On a driver
  without a combined depth-stencil format the mask degrades to a no-op, as
  before. Verified in an original-city Vulkan launch with zero validation
  errors and the minimap/map overlay drawn correctly.
- The emulated-PSX offscreen render-to-VRAM target is now implemented on the
  Vulkan backend (renderer roadmap item 14/R7b): `PsyX_Vk_GameSetOffscreen` /
  `PsyX_Vk_GameResolveOffscreen` render the draws queued by
  `GR_SetOffscreenState(enable)` into a colour-only target, read them back and
  pack them into the PSX VRAM mirror before the frame is submitted, so effects
  that sample a `dfe=0` region in the same frame (the Tanner pedestrian shadow)
  see it. Groups are bound to the double-buffered frame index, matching the
  game's one-frame-ahead display-list build. The `-vkpsxtest` PSX self-test now
  also covers the offscreen path (a 32x32 quad rendered offscreen and resolved
  into VRAM at (64,64)).
- The PsyCross submodule now tracks this project's own fork
  (`git@github.com:SimStm/PsyCross.git`) instead of the upstream repository, so
  project-specific changes are committed and reviewed there and recorded by the
  parent gitlink.
- `AGENTS.md` and the PsyCross fork rule now state explicitly that
  `src_rebuild/PsyCross` must always be the SimStm fork (`origin`, tracked
  branch `origin/master`) and never `OpenDriver2/PsyCross`: the fork carries this
  project's Vulkan backend, MoltenVK/portability work and other renderer
  changes that upstream does not have. The `upstream` remote remains a read-only
  source for rebasing fork work. The `patches/psycross/` delta and
  `scripts/apply_psycross_patches.ps1` were removed, and the Windows and Linux
  prepare scripts now initialise the submodule instead of applying a patch.
- Reorganised the developer panel: modding information (override toggle, mod
  diagnostics, mods root, reload, enabled mod order, declared override list)
  moved from the Graphics and 3D Debug tabs into a dedicated **Mods** tab, and
  the **3D Debug** tab is now render-debug only (picking, highlight, selection
  details, asset-catalog diagnostics, and model/texture export plus the mod
  reload action). The **Graphics** tab keeps renderer controls.
- Fully opaque texture overrides now generate mipmaps and use mipmap
  minification, which reduces the minification shimmer reported for large
  upscales such as `GRASS01C`. Overrides that contain transparency keep plain
  filtering so mip averaging cannot bleed the transparent colour into cutout
  edges.
- Mipmapped overrides now use anisotropic filtering (capped at 4x) when the
  driver exposes `GL_EXT_texture_filter_anisotropic`, reducing grazing-angle
  shimmer on roads and ground. The PSX nearest-filtered path is unchanged.
- Exported texture alpha now follows the PSX transparency rules and the
  consuming draw's blend context (roadmap item 15, complete). `0000h` always
  exports as a cutout (`0`), because PSX treats it as fully transparent in every
  context and Driver 2 writes opaque black as `8000h`. The `STP` flag exports
  `128` on a semi-transparent draw and `255` on an opaque draw, where PSX
  ignores it, so an opaque texture now exports `255` everywhere instead of a
  half-alpha black that could be mistaken for a blend. Batch and catalog exports
  have no primitive context and keep the conservative `128`. The 3D Debug tab
  shows the effective rule for the current selection.
- An enabled `developer_debug_start.ini` now also skips the intro movie, as if
  `-nointro` had been passed.

- Documented how to reconstruct explicit texture manifest entries from
  inspector-export filenames, including renamed upscaled variants.
- Developer-panel text wraps within the available window width. Texture
  exports use the full registered texture region, retain up to 4096 known
  textures independently of the mod-entry limit.
- Inspector PNG, TXT and car OBJ exports can be repeated at the same path.
  Completed nonempty temporary files replace the previous export; PNG output
  is decoded and dimension-checked before publication.
- Kept the previous `mods/hd_textures/manifest.ini` mapping as a compatibility
  fallback when no JSON mod manifests are present.
- Exporting a texture into an existing texture mod now appends a missing
  `(texture, texturePage, textureIndex)` registration instead of only reporting
  the entry to add. Unknown manifest fields, existing file references and user
  edits are preserved, repeated exports stay idempotent, and an unreadable
  `textures` array is reported without rewriting the manifest.
- Texture manifests and PNG overrides can now be reloaded for texture pages
  already present in the current level without changing original game assets.

- Rewrote the project README with fork scope, legal-data guidance, setup,
  build, run, debug, technology, and agent-workflow documentation.
- The F11 developer-panel hotkey now activates on key press and supports both
  SDL F11 identifiers.
- Project-owned PsyCross changes are distributed as an idempotent patch applied
  after submodule initialisation, avoiding a fork solely for this integration.
- The panel's texture-lookup explanation (`Explain texture lookup`) refreshes
  automatically when the selection changes, instead of describing the previous
  pick until the button is pressed again.

### Fixed

- Desktop smoke, exhaust, debris, birds, hubcaps, rain and light phases no longer
  advance per presentation frame. Effect poses interpolate locally at the shared
  car/camera snapshot time; drawing does not age their simulation pools.
- Mission-title input unlock, bomb flash and pause-message expiry follow regional
  ticks; replay UI draws every presentation frame. Existing replay trajectory
  faults on local original recordings are deferred at the user's request.
- Night-time palette cycling uses per-frame upload packets, preventing catch-up
  ticks from creating a cyclic ordering table and hanging replay rendering.

- Vulkan now honors live VSync changes by selecting a supported presentation
  mode and recreating its buffers at a safe frame boundary. FPS/VSync autosaves
  preserve unconfirmed resolution and unrelated session edits; small timer
  overruns no longer accumulate drift in the presentation FPS limiter.

- Car/camera flick and jumps between simulation ticks: original camera control
  now runs at the regional tick rate and camera/car translation and rotation
  share one presentation interval. Wheel transforms and shadow corners use the
  visual car pose; pause, cuts and resets delimit history without changing car
  physics or replay data. The clock preserves exact NTSC/PAL cadence and the
  fractional interpolation phase after bounded catch-up. Covered by production
  timing/snapshot tests at varied FPS and live deterministic replay comparisons.

- The reported Vulkan/OpenGL difference in shadow reception was a measurement
  artefact, not a renderer defect: the two installs had different developer
  state (sun direction, shadow extent 2500 vs 6991, spawn) and the street scene
  contains moving traffic whose capture noise exceeded the metric. With aligned
  settings in the traffic-free playground the two backends' shadowed-pixel sets
  reach IoU 0.985, within the capture noise floor; the OpenGL install keeps the
  aligned state.
- The modern shadow volume is snapped to the light-space texel grid on both
  backends. The volume follows the player vehicle, so its sub-texel motion made
  `NEAREST` shadow edges swim; the centre is now rounded to whole
  `2 * extent / shadowSize` steps in the plane perpendicular to the light,
  which moves it by at most half a texel and leaves the depth along the light
  exact.
- Minimizing the window no longer leaves the Vulkan backend rendering nothing:
  the frame fence is reset immediately before the submit that signals it again,
  so an out-of-date acquire (which returns from the frame early) can no longer
  leave the next frame waiting forever on a fence nothing would signal. A
  failed submit signals the fence too.
- Minimizing the game window no longer crashes the Vulkan backend with an
  access violation in the graphics driver. A minimized window has a `0x0`
  surface, so an out-of-date present triggered a recreation that destroyed the
  current swapchain and then failed to create a replacement, leaving
  `VK_NULL_HANDLE` for the next `vkAcquireNextImageKHR`. Recreation now checks
  the surface extent before destroying anything, the frame path skips
  minimized windows and never acquires from a null swapchain, and the
  swapchain is re-created normally when the window is restored.
- The developer panel's display-mode confirmation can no longer be spent by a
  single stalled frame: the countdown still runs while the window is minimized
  (an unconfirmed mode always reverts), but one frame contributes at most
  `0.25 s`, so the frame after a debugger stop or hibernation no longer reverts
  the mode before the user can see it.
- Enhanced rendering now composes modern meshes before the final gameplay
  overlays on Vulkan and OpenGL, preserving meshes behind translucent menus
  and preventing UI/lens-flare pixels from entering the world lighting copy.
  Replaces the earlier forced 2D-depth workaround, which could obscure the
  world depending on view angle and make flare rectangles affect lighting.
  The explicit OT boundary also handles batches containing both 2D and 3D
  primitives; OpenGL restores texture bindings before drawing the overlay tail.
- Vulkan modern scene-depth copies now transition both depth/stencil aspects
  together and initialize the destination transfer layout before copying,
  correcting Khronos validation errors 03320 and 09600 in the enhanced path.
- The overhead map, the loading progress bar and the rest of the 2D UI were
  painted over by the 3D scene on the Vulkan backend. A replayed mid-frame VRAM
  write closes and reopens the main render pass, and the depth/stencil attachment
  used `VK_ATTACHMENT_STORE_OP_DONT_CARE`, so the reopened pass loaded undefined
  depth and nothing drawn after the split was depth-tested against what came
  before. The attachment now stores depth and stencil (still cleared each frame),
  so the 2D UI occludes the scene again and the fullscreen map matches OpenGL.
- Image-streaming UI (the overhead map above all) drew from the wrong data on the
  Vulkan backend. The backend records the whole frame's PSX draws at present
  time while the OpenGL renderer executes each `DrawSync` flush immediately, so
  (1) every `GR_UpdateVertexBuffer` wrote from offset 0 and overwrote the
  previous flush's vertices, and (2) every draw sampled the frame's final VRAM
  contents, making all sixteen recycled map tile slots hold the last batch.
  Vertex uploads now append (the buffer keeps four flushes, each draw is offset
  by its upload's base) and `GR_CopyVRAM` queues its rectangle with the pixels so
  the writes are replayed in flush order while the draws are recorded. The map
  screen now draws its tiles and labels; full parity with OpenGL is tracked in
  `knowledge/product/vulkan-ui-image-parity.md`.
- Vulkan untextured UI primitives now use the GL-equivalent decoded white
  texture instead of sampling VRAM. Game presentation prefers UNORM so police
  flashing, compass and HUD blending use display-space values; constant alpha
  matches OpenGL. Expanded PSX self-tests cover these paths and stencil.
- Vulkan stencil capability survives resource initialization, mask operations
  match OpenGL, and depth-disabled draws retain stencil protection. Resumed
  passes preserve stencil and use compatible dependencies, fixing the Khronos
  `VUID-vkCmdDraw-renderPass-02684` error and map shadow bleed. Depth writes
  require depth testing, correcting the previous interpretation of GL state.
- The loading screen was black on the Vulkan backend and its progress bar was
  lost. The PSX loading path draws the art once (`ShowLoadingScreen`) and then
  redraws only the bar (`ShowLoading`) while the level streams in; the OpenGL
  renderer keeps the art because it only clears when the draw environment has
  `isbg` set, but the Vulkan main pass always used `LOAD_OP_CLEAR` and the
  `clearRequested` flag written by `GR_Clear` was never read. The main pass
  colour attachment now uses `LOAD_OP_LOAD` with a `PRESENT_SRC` initial layout
  and clears with `vkCmdClearAttachments` only when the game asked for it (or
  when a swapchain image is used for the first time), so the loading art
  persists exactly as on OpenGL. `-vkpsxtest` still passes with unchanged
  pixel assertions.
- The HD-texture override preview in the developer graphics panel showed
  nothing on the Vulkan backend because the ImGui Vulkan backend needs a
  `VkDescriptorSet`, while the texture id is a slot index there. A new
  backend-aware accessor (`PsyX_GetOverlayTextureId`,
  `PsyX_GetRGBATextureSize`) returns a cached
  `ImGui_ImplVulkan_AddTexture` descriptor set on Vulkan and the `GLuint` on
  OpenGL, and the panel uses it with the texture's real aspect ratio.
- The Vulkan game path recreated only the swapchain handed to the compositor on a
  window resize; the framebuffers, swapchain image views and depth attachment
  built for the old extent were left alive and then shadowed by the new ones, so
  every resize leaked them and left the newly created framebuffers referencing
  stale depth/colour attachments. Resize and out-of-date acquire now run a single
  `RecreateSwapchain()` that waits idle, tears down the framebuffer/view/depth
  resources before the swapchain, and rebuilds at the new extent; the readback
  buffer and the new extent are logged (`swapchain recreated WxH images=N`). A
  live session resized through 1024x768, 800x600, 1600x900 and 1280x720 keeps
  presenting at ~30 FPS with zero validation errors.
- The Vulkan game path generated mip chains for PSX `GR_CreateRGBATextureMipmapped`
  textures with whole-image layout barriers. Uploading left mip 0 as
  `TRANSFER_DST`, then a single image-wide barrier forced every level to
  `TRANSFER_SRC` before the next level was written as `TRANSFER_DST`; the
  generated mips were also never moved to `SHADER_READ_ONLY`. Validation
  reported `VUID-VkImageMemoryBarrier-oldLayout-01197`,
  `VUID-vkCmdBlitImage-srcImageLayout-00221` and `VUID-vkCmdDraw-None-09600`
  (30 messages, capped by the duplicate limit) on every run that loaded such a
  texture. Barriers are now per mip level and the final transition covers every
  level; a Vulkan game run reports zero validation errors.
- The VRAM TGA export (`GR_SaveVRAM`, bound to F10 and used by the debug VRAM
  dump) wrote a 18-byte header-only file on the Vulkan build: the pixel loop was
  compiled out unless `USE_OPENGL`, and the Vulkan build reports that as 0. The
  writer only reads the CPU VRAM mirror, which both backends keep current, so it
  is now backend-agnostic (18 + 1024x512x2 = 1,048,594 bytes). The `-vkpsxtest`
  self-test exports the image and asserts the exact byte count so this cannot
  silently regress.
- The Vulkan game path used the PSX clip rectangle as-is for its scissor, but
  `GR_SetupClipMode` hands over GL's bottom-left convention. Every clipped HUD
  element therefore tested against a vertically mirrored region; the minimap
  (a 63x60 clip in the bottom-right corner) was dropped entirely. The scissor Y
  is now mapped to Vulkan's top-left origin like the viewport.
- The experimental Vulkan backend drew nothing: the main render pass was
  created before the swapchain negotiated its surface format, so its colour
  attachment format was `VK_FORMAT_UNDEFINED` and every recorded draw was
  silently discarded. The surface format is now negotiated before the render
  passes. `PsyX_Vk_ReadbackRgba` also flipped rows despite documenting a
  top-left origin, which made `-vkshot` screenshots upside down.
- Inspector model names are sanitized. Some name-table entries, notably
  sprite-only models, are not text; those characters used to reach inspector
  labels, catalog records and export file names and are now reported as an
  unnamed model.
- Car and pedestrian textures are now nameable in the inspector, so their
  textures export normally, through the batch export and in the catalog. The
  lookup required an exact texture-page/CLUT match against the level's named
  textures, but cars and pedestrians are drawn with runtime palettes that are
  never registered (measured: the car page holds 30 named textures and the
  player car's CLUT matched none of them). A named texture on the same page
  whose VRAM region contains the selection is now used as a fallback; the PNG
  still resolves from VRAM with the primitive's own CLUT, so the palette is
  preserved.
- Sprite scenery (trees and other billboards) now carries identity. It is drawn
  by `DrawSprites` as a subdivided mesh, which registered nothing, so a click
  resolved a single mesh triangle, reported an unsupported source and left the
  trunk unselectable. Sprites are registered as resource-backed objects
  (`sprite:` keys) with their model slot and placement, and the ground shadow
  stays outside the range.
- Buildings drawn from a streamed texture page now resolve a texture name. Only
  the level texture loaders registered page names, so a page assigned by the
  texture spool had none and every primitive from it reported an unregistered
  texture (measured: a facade on page 8 matched nothing at all, and the named
  texture count rose from 300 to 363 once the spool registered its pages).
- Clicking a source no producer registered no longer selects whatever is behind
  it. The registered-source preference that stops the atmospheric haze from
  shadowing the scene only outranks the nearest candidate when that candidate is
  a screen-space overlay, that is, carries no depth; real geometry keeps
  draw-order priority.
- The developer panel's Export section reports why a texture lookup succeeded or
  failed (`Explain texture lookup`), separating a page/CLUT mismatch, a region
  mismatch and the palette fallback.
- Debug-start snapshots now honour the saved `gameType` instead of forcing
  `GAME_TAKEADRIVE`, and a snapshot captured during a replay is not applied at
  startup (reproduce it with the generated `-replay` command).
- An absent `[render] textureOverrides` key no longer forces HD texture
  overrides on, so the developer panel toggle persists across launches.
- Timed screenshots are taken before the buffer swap, so `captureAfterSeconds`
  records the frame that was just rendered instead of the undefined back buffer.
- Inspector export detects a truncated output path and refuses to replace an
  existing but unreadable `manifest.json`; WIC initialisation no longer
  unbalances a pre-existing COM apartment.
- Fixed zero-byte PNG exports caused by rejecting the WIC encoder's BGRA
  pixel-format negotiation after opening the destination file. RGBA pixels
  are now explicitly converted to BGRA, retaining alpha and colour channels.
- Transparent PNG texture overrides no longer render black or incorrectly
  occlude geometry. Override draws now discard fragments below 0.5 alpha; the
  cutout is limited to active region overrides, so original PSX sampling,
  high-resolution fonts, and the fallback path keep their previous behaviour.
- Screenshots saved by `PsyX_TakeScreenshot` (F12 or `captureAfterSeconds`) are
  no longer vertically mirrored. `glReadPixels` rows are flipped before
  `SDL_SaveBMP`, matching the displayed orientation.
- The PsyCross `developer-overlay.patch` now checks out with LF line endings
  (`.gitattributes`), so a fresh Windows clone with `core.autocrlf=true`
  applies it instead of failing on the corrupted binary payload.
- `scripts/apply_psycross_patches.ps1` no longer hangs under Windows PowerShell
  5.1. With `$ErrorActionPreference='Stop'`, redirecting the stderr of a failing
  `git apply` to `$null` deadlocked; the combined output is captured instead.
- `windows_dev_prepare.ps1` skips dependencies that are already present,
  applies the PsyCross patch, and opens the solution at the path Premake
  actually generates (`src_rebuild/build/REDRIVER2.sln`).
- `linux_dev_prepare.sh` applies the PsyCross patch and changes into the
  directory Premake actually generates (`src_rebuild/build`), so a clean Linux
  checkout configures and builds. The Linux target (`release_dev_x64`,
  `debug_x64`) was verified to compile and to start and load a level with the
  WSLg D3D12/Mesa core context.

## [8.0] - 2026-07-02

Upstream baseline for this fork: OpenDriver2/REDRIVER2 commit `b2d8857`.

### Added

- High-resolution fonts and dynamic lighting from street lights.
- An additional car-model slot so all base, non-special car models can be
  visible at the same time.
- Restored lights for spooled objects and the birds previously present in
  Driver 1.
- 64-bit platform support and direct Linux execution on a 64-bit ABI.

### Fixed

- Cutscene car deviation in Car Bomb and Tanner's vertical position on Chicago
  lifting bridges.
- A film-director crash when changing the camera from a car to event objects.
- Civilian cars spawning outside roads, missing sirens, high-pitched horns,
  missing skid sounds, and selected texture artifacts.
- High CPU load in the main menu, plus minor bugs and regressions.

[Unreleased]: https://github.com/SimStm/REDRIVER2-Plus/compare/8.0...HEAD
[8.0]: https://github.com/OpenDriver2/REDRIVER2/releases/tag/8.0
