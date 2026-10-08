---
type: Product
title: Native renderer bootstrap (R01)
status: implemented
updated: 2026-10-07
tags: [okf, rendering, vulkan, native]
---

# Native renderer bootstrap (R01)

R01 is implemented and accepted for the tested Windows desktop analytic slice.
R02 original imports are now accepted separately; R03-R12 remain pending.
The full renderer remains planned; original graphical categories are
not migrated by this analytic scene.

## Runtime boundary

`-nativeworld` or F11 Rendering's **Native analytic world (R01)** requests a
session-only fixture. The final single-player camera is sampled after game
camera construction. Six asymmetric triangles anchor after 30 simulation ticks;
**Rebuild analytic scene** anchors them again at the current camera. Original
gameplay, loading, collision, simulation timing and configuration persistence
are unchanged. Split-screen/unsupported backend/missing snapshot or boundary
fall back to the complete legacy frame, with an explicit effective reason.

`PsyX_native.h` is a C-compatible, render-thread-only boundary. Descriptors and
snapshots are copied, never retaining game pointers. Mesh handles contain slot
and generation; scene reset invalidates all handles. Slots do not wrap at
generation exhaustion. Caps:32 meshes,64 instances,8 MiB CPU vertex/index payload.
Creation validates finite attributes, opaque linear colour, bounds and indices;
publishing rejects stale identities and invalid matrices atomically.

Pending owned meshes upload only after the existing frame fence. Destruction
immediately invalidates handles; GPU buffers and CPU payload reclaim after the
last successful submission completes. Failed uploads produce explicit legacy
fallback. Ordinary CPU payloads remain separate from GPU-mapped storage.
Diagnostics report CPU payload bytes, not GPU allocation peaks.

## Camera and passes

The adapter converts original `(x,y,z)` to native `(x,-y,z)`, final inverse-camera
rotation to RH view looking along -Z, and source winding once at this boundary.
Matrices are column-major. Near10/far125000, Vulkan depth0..1, positive viewport
with one projection Y reflection, CCW front and `LESS_OR_EQUAL`. Logical display
height preserves NTSC240/PAL256; horizontal projection retains the game's
original aspect/FOV convention. No GTE-encoded native vertices or PGXP caches.

The unlit native pass uses actual position/normal/UV/linear-colour attributes,
an indexed persistent mesh and MVP/tint push constants. The existing Vulkan
owner provides the only SDL window, device, queue, swapchain, command buffer,
frame fence and presentation. Native D32 is recreated at the **new** swapchain
extent. Native colour replaces the old world prefix; old shadows/composites are
not applied to native pixels. Ordered VRAM/offscreen work remains available for
original overlays. HUD/minimap and ImGui compose afterwards with the separate
legacy depth, never attempting cross-projection depth occlusion.

F11 reports requested/effective mode, reason, actual native/legacy/suppressed
draws and ready/pending/retiring counts. R00's original-source native counters
remain zero because analytic geometry is not original-category migration.

## Executed validation and limits

Windows11/RTX3060Ti/driver610.88, Release_dev x64,1280x720, original NTSC Chicago
R00 spawn and texture mods. VS build passed. `run_native_scene_tests.ps1`:18
ownership/C ABI +8 camera/clip cases passed; MSVC host compilation of Android,
Emscripten and PSX excluded branches passed (not complete target builds).
`-vknativetest` passed actual GPU pixels at tolerance3 bytes:near occlusion,
front/back winding, clipping, camera movement/wide FOV, original UI, depth-state
independence,800x600 resize, minimize/restore, fence retirement,8 reloads leaving
zero payload/mesh ownership and explicit legacy fallback. Fresh legacy self-test
passed. Restore initially failed and exposed old-size native targets; corrected
new-extent allocation then passed.

Installed RenderDoc1.46 capture/replay:stable original-game frame430 has1 native
18-index D32 draw followed by10 original UI draws/318 indices in D24S8, zero
messages. Post-VS float4 clip positions, CCW/back culling, enabled depth/write,
LessEqual and disabled stencil inspected. Fixture captures2/6 have zero messages;
capture5 spanning resize/minimize records a legacy PRESENT-vs-GENERAL layout
message before the native pass. Application source uses no GENERAL layout;
plain VS validation of the corrected GPU test is clean. This is attributed to
the capture/layer combination by isolation, with the precise interacting layer
unidentified. See [executed evidence](../rendering/r01-validation.md) and primary
RenderDoc source links there; do not claim every capture is warning-free.
Native/legacy readback passed with and without capture instrumentation.

Matched PGXP-on/off city PNGs are SHA256-identical; focal128 wide-FOV capture
has the same1 native+10 UI draws with zero messages. Actual ImGui checkbox
restores legacy city; native rebuild leaves1 ready,0 pending/retiring/rejected.
Existing OpenGL3.3 launches city/HUD/ImGui with explicit unsupported-native
fallback. Five INIs and debugger settings restored byte-exactly after owned
sessions stopped. Source/text review and fork publication are recorded in the
evidence document. No Linux/macOS runtime, full target builds, GPU timing/
peak-memory or original-content migration claims. Next:R02.
