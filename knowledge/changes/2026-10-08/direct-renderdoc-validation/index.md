---
type: Change
title: Direct native capture and RenderDoc1.41 analysis
created: 2026-10-08
tags: [okf, rendering, tooling, validation]
---

# Direct native capture and RenderDoc1.41 analysis

The user authorized bypassing VS/RDC MCPs for routine validation when faster or
more reliable. The installed rdc-cli script client itself has a30s daemon socket
response limit. Add `scripts/run_renderdoc_analysis.py` to use configured1.41
local SDK replay, explicit GPU/status checks, fresh per-script argument scopes,
phase progress/results and ordered exception-safe shutdown without daemon RPC.
Keep VS MCP for debugger-specific investigations and preserve the OpenGL backend.

Actual direct build48.48s, fresh native GPU PASS, fresh frame500 capture and33s
analysis pass. Public report/native attachments/resources exactly equal MCP;
fresh scoped road810425 pixel/1602 occlusion comparisons pass. Invalid GPU
and deliberate script exceptions are rejected with exit1/no success summary;
subsequent replay and per-script argument isolation pass. Two west views provide
1217385 colour/ID/depth samples with no scoped errors, but wide sky/auxiliary
counts/negative-w geometry and remaining region gates are unresolved.

The six settings/generated XML files restore byte/hash-exactly. No native
production renderer change is made, and R03 remains incomplete. Document the
absolute capture-output requirement, process observation versus execution limits,
initial replay warnings and fixture-log gate (test return code is discarded).

See [executed evidence](../../../rendering/r03-west-road-validation.md),
[repeatable workflow](../../../rendering/renderdoc-direct-workflow.md),
[tooling rule](../../../rules/mcp-agent-tooling.md) and compatibility VK028.
