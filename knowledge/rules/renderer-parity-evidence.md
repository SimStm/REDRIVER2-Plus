---
type: Rule
title: Verify renderer parity with state and pixel evidence
description: Separate API semantics, test coverage and runtime acceptance.
tags: [okf, rendering, validation, vulkan]
---

# Verify renderer parity with state and pixel evidence

**When** translating graphics state between backends, **then** inspect the
reference backend's complete state and check API semantics before diagnosing a
visual defect. GL_DEPTH_TEST disables both testing and depth writes when off;
a comparison mask is not a stencil write mask. Preserve selected capabilities
through resource initialization and test the bridge used by actual game draws.

**When** reporting a visual fix, **then** distinguish a passing synthetic test,
a breakpoint hit and a captured rendered result. Fix the animation phase and
runtime state for GL/Vulkan comparison. Several presentations do not prove all
swapchain images were acquired. Read the validation layer's enable confirmation
and error output rather than inferring installation or correctness.

**When** capturing a modern-path frame, **then** pick the capture point that
includes it: the timed capture tick runs before `GR_EndScene` on OpenGL, which
is where `PsyX_ModernMesh_RenderFrame` draws, so GL tick captures contain only
the legacy scene. Use F12 (after present) for GL modern frames, or the timed
tick on Vulkan, and state which path produced each image.

Evidence: the September 2026 UI follow-up found missing white primitives,
stencil capability erased by memset, incorrect masks and incompatible resumed
passes after the original parity record had already claimed completion.
