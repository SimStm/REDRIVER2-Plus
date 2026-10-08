---
type: Change
title: Native tree exposure validation and terrain findings
created: 2026-10-08
tags: [okf, rendering, vulkan, validation]
---

# Native tree exposure validation and terrain findings

Four subsequent1.41 pixel histories identify grass569/sidewalk681: the latter
fails one D32 unit behind the former beside a passing tie. Its horizontal top
shares a material part with a vertical side, disabling mesh-level canonical
horizontal depth. Confirm copied transforms before implementing a scoped fix.

Read-only copied-source inspection locates five previously unexposed tree parts.
Six additional configured RenderDoc1.41 frame500 captures pass original and
tightened independent pixel oracles. Eleven-view totals are3079701 eligible
colours/87152 neighbour-band comparisons; all44 original-art parts have opaque
colour evidence,36 HD. Small part751 and excluded edges/remaining HD views are
not full silhouette acceptance. Full resource records and ordered bindings match
after excluding capture-local resource IDs only; the initial audit failure stays
recorded. Four HIGH replay warnings per new capture remain.

Visual inspection of all six PNGs finds another grass/sidewalk stippled overlap
in eastnorth and ground/backdrop coverage requiring camera/source diagnosis in
west. These are pending R03 gates, not accepted by the tree oracle. No production
source/shader/build/self-test change or rerun is claimed; fork426c9a3 is unchanged.
Five INIs/generated `.user` restored byte/hash-exactly and reverified after IDE
reopen; all10 VS pairs/Release_dev x64/Design/no breakpoints confirmed. Owned
targets and replay sessions closed. First combined export exceeds30s CLI response
timeout but completes; retained files and passing offline oracles corroborate it.
Subsequent inspection/export phases stay below the timeout without replay restart.

The official upstream8.0/b2d8857 release is rechecked before amending Unreleased.
R03 remains incomplete; R04 unstarted. See [evidence](../../../rendering/r03-tree-view-validation.md),
[product](../../../product/native-static-region.md) and compatibility VK026.
