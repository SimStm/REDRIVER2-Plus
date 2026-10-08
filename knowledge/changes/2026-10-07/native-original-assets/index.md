---
type: Change
title: Native original asset imports R02
date: 2026-10-07
tags: [okf, rendering, vulkan, native, assets]
---

# Native original asset imports R02

Implemented bounded owned original MODEL/placement/texture adapters and copied
PNG resolution through existing mod precedence, with catalog generation/revision
keys. The native material API adds sRGB GPU images, independent sampling and
fence-protected retirement inside the existing Vulkan owner. Submitted upload
failures retain their live resources until completion is confirmed.

The [product](../../../product/native-original-assets.md) describes the implemented
surface; [executed evidence](../../../rendering/r02-validation.md) records the
accepted Windows import slice. CPU checks total1,202. Actual city capture matches
four complete vertex/index/artwork hashes and fourteen transforms; live filtering,
PGXP/mod changes, actual slot/context reload and interrupted-reset recovery pass.
This incomplete road/facade fixture does not satisfy R03 region coverage.

An actual reset during creation exposed orphan resources. Rollback now retires
handles created after the reset as well as earlier stale handles; the repeated
GPU fault and CPU regression recover without budget growth. The
[lifetime/UV rule](../../../rules/native-upload-and-original-uv-lifetimes.md)
records this invariant and original UV overscan versus PNG clamp policy.

PsyCross `9ff48afee0384f92157568f6a2a3400b06b55fbd` is committed/pushed to the
project fork origin/master; parent gitlink staged. Five INIs/debugger settings
restored byte-exactly. Original data/artwork remain unchanged and are not included
in the evidence. The [VK-009 backlog](../../../roadmap/planned/port-vulkan-to-opengl-compatibility.md#vk-009---r02-original-decoders-and-native-materials-2026-10-07)
records the deferred GL work. Next: complete static region R03 under the active goal.
