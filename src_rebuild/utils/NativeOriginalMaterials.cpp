#include "NativeOriginalMaterials.h"
#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "driver2.h"
#include "C/texture.h"
#include "PsyX/PsyX_render.h"
#include "PsyX/PsyX_public.h"
#include <new>
#include <utility>

namespace NativeAsset
{
void OriginalMaterialAdapter::Clear() { overrides.clear(); vram.clear(); vramCurrent = false; }
const OriginalMaterialAdapter::OverrideCopy* OriginalMaterialAdapter::FindOverride(
	unsigned int page, unsigned int texture, unsigned short clut, uint64_t revision)
{
	for (size_t i = 0; i < overrides.size();)
	{
		if (overrides[i].revision != revision) { overrides.erase(overrides.begin()+i); continue; }
		if (overrides[i].page == page && overrides[i].texture == texture && overrides[i].clut == clut) return &overrides[i];
		++i;
	}
	if (overrides.size() >= PSYX_NATIVE_MAX_MATERIALS) return NULL;
	OverrideCopy copy;
	copy.page = page; copy.texture = texture; copy.clut = clut; copy.revision = revision;
	copy.result = HdTextureOverrides_CopyNativeImage(page, texture, clut, copy.image);
	size_t bytes = copy.image.rgba.size();
	for (size_t i = 0; i < overrides.size(); ++i) if (overrides[i].pixels) bytes += overrides[i].pixels->size();
	if (bytes > PSYX_NATIVE_MAX_MATERIAL_BYTES) return NULL;
	if (copy.result == HdNativeImageResult::Ready)
		copy.pixels = std::make_shared<const std::vector<uint8_t> >(std::move(copy.image.rgba));
	overrides.push_back(std::move(copy));
	return &overrides.back();
}
Result OriginalMaterialAdapter::Resolve(const CachedGeometry& geometry, std::vector<ResolvedMaterial>& output)
{
	output.clear();
	if (geometry.failure.result != Result::Complete) return geometry.failure.result;
	const uint64_t revision = HdTextureOverrides_GetNativeRevision();
	if (!revision) return Result::OutOfBudget;
	try
	{
		if (!vramCurrent)
		{
			vram.resize(1024*512);
			GR_ReadVRAM(vram.data(), 0, 0, 1024, 512); // CPU mirror; no GPU mapping.
			vramCurrent = true;
		}
		std::vector<ResolvedMaterial> next;
		for (size_t i = 0; i < geometry.mesh.parts.size(); ++i)
		{
			const MaterialReference& ref = geometry.mesh.parts[i].material;
			if (ref.textureSet >= 128 || ref.textureIndex >= 32) return Result::Invalid;
			unsigned short u = 0, v = 0, width = 0, height = 0;
			if (!HdTextureOverrides_GetKnownTextureRegion(ref.textureSet, ref.textureIndex,
				NULL, NULL, &u, &v, &width, &height, NULL, 0)) return Result::Unsupported;
			ResolvedMaterial material;
			material.revision = revision;
			// Retain one full chain; the copied instance chooses runtime sampling.
			material.filter = PSYX_NATIVE_TRILINEAR;
			material.cull = geometry.mesh.shapeFlags & (SHAPE_FLAG_SPRITE | SHAPE_FLAG_ALPHA) ? PSYX_NATIVE_CULL_NONE : PSYX_NATIVE_CULL_BACK;
			const unsigned short tpage = texture_pages[ref.textureSet], clut = texture_cluts[ref.textureSet][ref.textureIndex];
			const OverrideCopy* hd = FindOverride(ref.textureSet, ref.textureIndex, clut, revision);
			if (!hd) return Result::OutOfBudget;
			if (hd->result == HdNativeImageResult::SourceChanged || hd->result == HdNativeImageResult::Failed) return Result::Invalid;
			if (hd->result == HdNativeImageResult::Ready)
			{
				material.image.tpage = tpage; material.image.clut = clut;
				material.sharedRgba = hd->pixels;
				material.image.width = hd->image.width; material.image.height = hd->image.height;
				material.uvPolicy = ResolvedMaterial::UvPolicy::ClampOverride;
			}
			else
			{
				// Original MODEL UVs can intentionally sample atlas texels outside
				// the named texture rectangle (Chicago1180 page0/id28: width24,
				// UV extent32). Preserve those texels; do not clamp original art
				// to inspector metadata. Active PNG overrides retain legacy clamp.
				const Result expanded = ExpandOriginalTextureRegion(geometry.mesh.parts[i], u, v, width, height);
				if (expanded != Result::Complete) return expanded;
				const Result decoded = DecodeTexture(Bytes(vram.data(), vram.size()*sizeof(unsigned short)),
					tpage, clut, u, v, width, height, false, material.image);
				if (decoded != Result::Complete) return decoded;
			}
			material.regionU = u; material.regionV = v; material.regionWidth = width; material.regionHeight = height;
			next.push_back(std::move(material));
		}
		output.swap(next);
		return Result::Complete;
	}
	catch (const std::bad_alloc&) { return Result::OutOfBudget; }
}
}
#endif
