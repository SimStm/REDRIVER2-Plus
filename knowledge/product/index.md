---
type: Catalog
title: Product knowledge
description: Implemented surfaces and configuration, separate from planned behaviour.
tags: [okf, product]
---

# Product knowledge

Product notes describe implemented behaviour; verify the current source.
The current renderer baseline is summarized in
[knowledge/rendering/current-state.md](../rendering/current-state.md).
The replacement renderer is still planned; its stage documents are not products.
Detailed older graphics history is excluded from routine analysis.

## Shared asset and mod contracts

- [Asset catalog identity](asset-catalog-identity.md)
- [Whole-object inspection](inspector-selection-coverage.md)
- [Batch texture export](object-texture-batch-export.md)
- [Texture alpha semantics](texture-alpha-semantics.md)
- [Proportional semi-transparent overrides](semi-transparent-texture-overrides.md)
- [Texture export alpha fidelity](texture-export-alpha-fidelity.md)
- [Palette variants](texture-palette-variants.md)
- [Filtering/flicker diagnostics](texture-flicker-diagnostics.md)
- [Manifest merging](texture-manifest-merge.md)
- [Mod-system readiness](mod-system-pr-readiness.md)

## Runtime tools and fixtures

- [Runtime settings GUI](runtime-settings-gui.md)
- [Native renderer baseline diagnostics (R00)](native-renderer-baseline.md)
- [Native renderer bootstrap (R01; accepted Windows analytic slice)](native-renderer-bootstrap.md)
- [Native original asset imports (R02; accepted Windows import slice)](native-original-assets.md)
- [Native static region (R03; partial, acceptance pending)](native-static-region.md)
- [Playable testing playground](playable-testing-playground.md)

These current file/identity/tool contracts remain active during graphics migration.
They do not require native meshes to use PSX projection or primitive limits.
