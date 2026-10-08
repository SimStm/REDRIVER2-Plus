"""Inspect an open native region capture through RenderDoc 1.41 / rdc script.

Usage: rdc script scripts/inspect_native_capture.py --arg output=ABS_DIRECTORY
The daemon injects rd, controller and args. Outputs contain local game assets;
use an ignored build directory and never distribute the capture or exports.
This checks resources/passes; region visual acceptance needs separate oracles.
"""

import hashlib
import json
from pathlib import Path


def draw_actions(items):
    for action in items:
        if action.flags & rd.ActionFlags.Drawcall:
            yield action
        yield from draw_actions(action.children)


def texture_bytes(texture):
    digest = hashlib.sha256()
    size = 0
    for level in range(texture.mips):
        subresource = rd.Subresource()
        subresource.mip = level
        raw = bytes(controller.GetTextureData(texture.resourceId, subresource))
        digest.update(raw)
        size += len(raw)
    return size, digest.hexdigest()


def debug_messages():
    return [{"eid": m.eventId, "severity": str(m.severity), "message": m.description}
            for m in controller.GetDebugMessages()]


def retain_messages():
    for message in debug_messages():
        if message not in messages:
            messages.append(message)


def fragment_constants(pipeline):
    block = pipeline.GetConstantBlocks(rd.ShaderStage.Fragment)[0].descriptor
    variables = controller.GetCBufferVariableContents(
        pipeline.GetGraphicsPipelineObject(), pipeline.GetShader(rd.ShaderStage.Fragment),
        rd.ShaderStage.Fragment, pipeline.GetShaderEntryPoint(rd.ShaderStage.Fragment),
        0, block.resource, block.byteOffset, block.byteSize)
    return {variable.name: variable for variable in variables}


def region_count_contract(logical_counts, range_coverage_matches):
    """Separate the fixed native slice from the historical auxiliary baseline.

    Auxiliary batches vary with retained consumers and texture state. Neither
    count check establishes pixel correctness or completion of their migration.
    Keep the existing combined field unchanged for older report consumers.
    """
    expected = {"backdrop": 80, "logicalWorld": 876, "auxiliary": 10}
    static_match = (logical_counts["backdrop"] == expected["backdrop"] and
                    logical_counts["logicalWorld"] == expected["logicalWorld"] and
                    range_coverage_matches)
    auxiliary_match = logical_counts["auxiliary"] == expected["auxiliary"]
    return {"expectedRegionCounts": expected,
            "staticRegionCountsMatch": static_match,
            "auxiliaryBaselineCountsMatch": auxiliary_match,
            "regionCountsMatch": static_match and auxiliary_match,
            "regionCountsScope": "Historical combined count check; static inventory "
                                 "and auxiliary baseline are reported separately. "
                                 "No pixel, producer attribution or migration acceptance."}


version = rd.GetVersionString()
if version != "1.41":
    raise RuntimeError("Native capture inspection requires RenderDoc 1.41; got " + version)
output = Path(args["output"])
if not output.is_absolute():
    raise ValueError("output must be an absolute local artifact directory")
output.mkdir(parents=True, exist_ok=True)
messages = debug_messages()
textures = {str(t.resourceId): t for t in controller.GetTextures()}
passes = {"backdrop": [], "world": [], "auxiliary": []}
geometry = {}
materials = {}
draws = []
cutouts = []
world_ranges = {}

for action in draw_actions(controller.GetRootActions()):
    controller.SetFrameEvent(action.eventId, False)
    retain_messages()
    pipeline = controller.GetPipelineState()
    vulkan = controller.GetVulkanPipelineState()
    depth = textures.get(str(pipeline.GetDepthTarget().resource))
    if depth is None or not depth.format.Name().startswith("D32"):
        passes["auxiliary"].append(action.eventId)
        continue
    kind = "world" if vulkan.depthStencil.depthWriteEnable else "backdrop"
    passes[kind].append(action.eventId)
    constants = fragment_constants(pipeline) if kind == "world" else None
    pick_id = constants["pickId"].value.u32v[0] if constants else 0
    # Original opaque materials also use an alpha threshold (opaque art has
    # full alpha). Two-sided world parts identify this region's tree policy;
    # source/pixel checks remain independent of this pipeline inventory.
    if kind == "world" and vulkan.rasterizer.cullMode == rd.CullMode.NoCull:
        cutoff = constants["alphaCutoff"].value.f32v[0]
        if cutoff > 0.0:
            cutouts.append({"eid": action.eventId,
                            "pickId": constants["pickId"].value.u32v[0],
                            "alphaCutoff": cutoff,
                            "twoSided": vulkan.rasterizer.cullMode == rd.CullMode.NoCull,
                            "depthTest": vulkan.depthStencil.depthTestEnable})
    vertex = pipeline.GetVBuffers()[0].resourceId
    index = pipeline.GetIBuffer().resourceId
    for resource in (vertex, index):
        key = str(resource)
        if key not in geometry:
            raw = bytes(controller.GetBufferData(resource, 0, 0))
            geometry[key] = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
    binding = pipeline.GetReadOnlyResources(rd.ShaderStage.Fragment)[0]
    texture = textures[str(binding.descriptor.resource)]
    key = str(texture.resourceId)
    if key not in materials:
        size, digest = texture_bytes(texture)
        materials[key] = {"extent": [texture.width, texture.height],
                          "mips": texture.mips, "viewMips": binding.descriptor.numMips,
                          "maxLOD": binding.sampler.maxLOD, "bytes": size,
                          "sha256": digest}
    draw = {"kind": kind, "vb": geometry[str(vertex)]["sha256"],
                  "ib": geometry[str(index)]["sha256"],
                  "material": materials[key]["sha256"],
                  "bias": vulkan.rasterizer.depthBias,
                  "pickId": pick_id, "firstIndex": action.indexOffset,
                  "indexCount": action.numIndices}
    draws.append(draw)
    if kind == "world":
        ranges = world_ranges.setdefault(pick_id, [])
        ranges.append({"eid": action.eventId, "firstIndex": action.indexOffset,
                       "indexCount": action.numIndices,
                       "sourceIndexCount": geometry[str(index)]["bytes"] // 4,
                       "vb": draw["vb"], "ib": draw["ib"], "material": draw["material"],
                       "depthPlane": list(constants["depthPlane"].value.f32v)[:4]})
    retain_messages()

if not passes["world"]:
    raise RuntimeError("No native D32 world draws; auxiliary pixels do not prove native rendering")
controller.SetFrameEvent(passes["world"][-1], False)
pipeline = controller.GetPipelineState()
targets = pipeline.GetOutputTargets()
outputs = {"colour": targets[0].resource, "ids": targets[1].resource,
           "depth": pipeline.GetDepthTarget().resource}
formats = {name: textures[str(resource)].format.Name() for name, resource in outputs.items()}
if not formats["ids"].startswith("R32_UINT"):
    raise RuntimeError("Native logical ID attachment missing: " + formats["ids"])
hashes = {}
for name, resource in outputs.items():
    raw = bytes(controller.GetTextureData(resource, rd.Subresource()))
    (output / (name + ".bin")).write_bytes(raw)
    hashes[name] = hashlib.sha256(raw).hexdigest()
save = rd.TextureSave()
save.resourceId = outputs["colour"]
save.destType = rd.FileType.PNG
controller.SaveTexture(save, str(output / "world.png"))
# SetFrameEvent resets current-event messages. Retain all observed diagnostics;
# a clean final draw cannot erase an earlier warning.
retain_messages()
counts = {name: len(events) for name, events in passes.items()}
logical_counts = {"backdrop": counts["backdrop"], "logicalWorld": len(world_ranges),
                  "auxiliary": counts["auxiliary"]}
range_coverage_matches = set(world_ranges) == set(range(1, 877))
for ranges in world_ranges.values():
    next_index = 0
    binding = [(ranges[0][key]) for key in ("vb", "ib", "material", "sourceIndexCount")]
    for draw in ranges:
        range_coverage_matches &= draw["firstIndex"] == next_index and draw["indexCount"] > 0
        range_coverage_matches &= [draw[key] for key in ("vb", "ib", "material", "sourceIndexCount")] == binding
        next_index += draw["indexCount"]
    range_coverage_matches &= next_index == ranges[0]["sourceIndexCount"]
count_contract = region_count_contract(logical_counts, range_coverage_matches)
cutout_policy_matches = len(cutouts) == 44 and all(
    draw["twoSided"] and draw["depthTest"] and draw["alphaCutoff"] == 0.5
    for draw in cutouts)
report = {"replayVersion": version, "passes": passes, "counts": counts,
          "logicalCounts": logical_counts, "worldRanges": world_ranges,
          "rangeCoverageMatch": range_coverage_matches,
          "cutouts": cutouts, "expectedRegionCutoutDraws": 44,
          "regionCutoutPolicyMatch": cutout_policy_matches,
          "geometry": geometry, "geometryBytes": sum(g["bytes"] for g in geometry.values()),
          "materials": materials, "materialBytes": sum(m["bytes"] for m in materials.values()),
          "draws": draws, "formats": formats, "hashes": hashes, "messages": messages,
          "validationClean": not messages,
          "note": "Resource/pass evidence only; diagnostic messages are retained, never filtered."}
report.update(count_contract)
(output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
result = {"replayVersion": version, "counts": counts, "logicalCounts": logical_counts,
          "rangeCoverageMatch": range_coverage_matches,
          "cutoutDraws": len(cutouts), "regionCutoutPolicyMatch": cutout_policy_matches,
          "geometryBytes": report["geometryBytes"], "materialImages": len(materials),
          "materialBytes": report["materialBytes"], "formats": formats,
          "hashes": hashes, "messageCount": len(messages), "validationClean": not messages,
          "report": str(output / "report.json")}
result.update(count_contract)
