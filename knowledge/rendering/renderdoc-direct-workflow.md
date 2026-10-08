---
type: Playbook
title: Direct native build and RenderDoc1.41 analysis
created: 2026-10-08
tags: [okf, rendering, validation, tooling]
---

# Direct native build and RenderDoc1.41 analysis

Use direct MSBuild, rdc-cli capture and local RenderDoc1.41 replay for routine
native validation. Reserve Visual Studio MCP for breakpoints, stack/locals and
debugger-specific investigations. Retain configured documentation/desktop MCPs
where they supply needed capabilities. The user authorized this choice.

## Build and run

From repository root, with the existing generated projects/toolchain:

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' `
  src_rebuild/build/REDRIVER2.vcxproj /p:Configuration=Release_dev `
  /p:Platform=x64 /m /nologo /v:minimal /fl `
  /flp:logfile=src_rebuild/build/native-renderer-r03/direct-msbuild.log
```

Locate the actual MSBuild installation when the environment changes. This
machine uses the64-bit default executable; do not invent an `amd64/` subpath.
Compilation alone is not native validation. Launch self-exiting native GPU
fixtures with `Start-Process -WindowStyle Hidden -PassThru`, in the executable's
working directory, and verify a **fresh completed** `vk_fixture.log` with PASS
and no failure. The command-line handler now propagates fixture failure as
exit1; require both exit0 and a fresh completed PASS report for acceptance.

For controlled interactive scene capture, back up every changed INI/generated
debugger file first and verify SHA256 restoration afterward. Record actual
FOV/HD/window/native flags, build and source commit. Use absolute capture output:

```powershell
$captureOutput = Join-Path (Get-Location).Path 'src_rebuild/build/native-renderer-r03/native-west-road-direct141.rdc'
& 'C:/Python314/Scripts/rdc.exe' capture -o $captureOutput `
  --frame 500 --timeout 120 --api-validation --json -- `
  'G:/_Projetos/Pessoal/REDRIVER2-Plus/src_rebuild/bin/Release_dev/REDRIVER2_dev.exe' `
  -nointro -mission 50 -gametype 1 -level 0 -playercar 0 `
  -startpos -7000 -229300 -startdir 512 -players 1 -nativeregion
```

Capture has a bounded watchdog for a failed launch/capture; it is independent
of the daemon script response limit. Verify the returned capture filename,
Vulkan API/frame and owned process closure. Relative `-o` resolves relative to
the target working directory here. Preserve the game's actual `REDRIVER2.log`;
do not assume its filename matches `REDRIVER2_dev.exe`.

## Local replay without daemon RPC

`rdc doctor` verifies configured Python replay1.41. Program Files RenderDoc is
still1.46 here; never implicitly use that capture executable. The direct runner
uses the Python environment containing rdc-cli and its configured module:

```powershell
& 'C:/Python314/python.exe' scripts/run_renderdoc_analysis.py `
  src_rebuild/build/native-renderer-r03/native-west-road-direct141_frame500.rdc `
  --gpu 'RTX 3060 Ti' --script scripts/inspect_native_capture.py `
  --arg 'output=G:/_Projetos/Pessoal/REDRIVER2-Plus/src_rebuild/build/native-renderer-r03/direct-inspection141' `
  --summary src_rebuild/build/native-renderer-r03/direct-inspection-summary.json
```

Repeat `--script` for sequential trusted local scripts; each receives `rd`,
`controller` and a fresh `args` dictionary, with optional `result` recorded.
Repeat `--arg KEY=VALUE` for distinct keys. Version1.41, existing files, replay
support/status and a unique explicit GPU are required; errors exit nonzero.
Use a fresh summary path per run and check the current process exit/output;
a receipt from an earlier run must never establish success after a failure.
JSON results may contain failed acceptance flags: successful execution only
means the analysis ran. Interpret the actual pixel/resource/diagnostic gates.

The installed `rdc script` client itself imposes30s socket response timeout;
bypassing only MCP does not remove it. The direct runner has no daemon/RPC or
job-duration cutoff. Observe progress from the same process/session across tool
yields; a command observation limit does not mean the job failed or stopped.
Avoid duplicate replays/captures or arbitrary process kills. Diagnose a process
that makes no progress from its logs/state rather than blindly retrying.

The [executed west evidence](r03-west-road-validation.md) verifies32/33s direct
analyses, exact MCP/direct reports and a fresh independent pixel oracle. Preserve
initial/per-event replay warnings, native colour/ID/depth and bound resource
hashes. Capture assets stay in ignored build directories. R03 remains incomplete.

Subsequent [full backdrop replay checks](r03-full-backdrop-validation.md) use the
same local1.41 runner without MCP/daemon RPC: six attachments,28 materials/196
mips and80 cull/depth states per capture match. Final script phases1.425s/1.357s,
exit0; four initial HIGH messages are retained with a false clean flag. Source/
post-VS host pixel checks and copied corruption controls are separate gates.
These timings are analysis throughput, not GPU performance or stage completion.

[Complete-view audit](r03-complete-view-coverage-validation.md) subsequently
exports all1023 world ranges per existing capture in41.262s/32.849s through the
same local1.41 runner, beyond the daemon RPC cap. Observe the same process across
client yields. Source/post geometry and source-alpha/fragment-discard/attachment
controls remain separate from process success and complete scene composition.
No arbitrary response timeout or observed ID selects an expected native winner.

[Source-generated scene composition](r03-complete-composition-validation.md)
uses actual1.41 extended state exports14.406s/13.768s and independent host
whole-view oracles42.656s/31.531s, all exit0 without RPC timeout. Keep observing
the same process across client yields.4145830 decided pixels/40 controls pass;
1370 decisions remain pending. This workflow is the routine choice; keep VS MCP
for actual debugger state. No new build/capture/GPU fixture or GPU cost is claimed.

[All-candidate scene traces](r03-composition-trace-validation.md) subsequently
complete1902/971 source-selected candidates in163.973s/84.143s, exit0, same
local1.41 process across client yields. No response timeout or duplicate replay.
Host raw-artwork/composition checks pass4147200 pixels/0 exclusions and42 new
controls reject. Keep shader-input/output references distinct from exact offline
arithmetic or GPU cost; four HIGH diagnostics and remaining R03 gates stay visible.
