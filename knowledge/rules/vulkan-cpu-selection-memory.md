---
type: Rule
title: Keep Vulkan CPU selection reads in cached memory
description: Host-coherent mapped Vulkan allocations do not guarantee cheap CPU reads.
tags: [okf, rules, vulkan, performance, memory]
---

# Keep Vulkan CPU selection reads in cached memory

**When** CPU selection repeatedly reads vertices or constants uploaded to Vulkan,
**then** read an ordinary-RAM upload copy and a local constants copy rather than
repeatedly walking the mapped GPU allocation. `HOST_VISIBLE | HOST_COHERENT`
does not imply `HOST_CACHED`; measure actual submission cost before accepting
the mapped allocation as an efficient CPU data source.

- Keep copies bounded, owned by the renderer and matched to the current upload
  offsets/frame. Reject incomplete coverage instead of consuming stale data.
- Preserve the existing fence/submit lifetime for buffers reused by the GPU.
- Copy enabled geometry at upload time; do not add a disabled-path per-frame
  copy or read back GPU memory to reconstruct the original CPU source.
- Record measured results and their environment, separating native runs from
  instrumented RenderDoc runs and avoiding claims of isolated GPU timing.

Learned from the 2026-10-06 legacy shadow caster implementation: batching alone
left costly mapped reads; caching vertices and UBO constants retained the same
frame-430 colour/shadow exports while restoring the measured 30 Hz budget.
