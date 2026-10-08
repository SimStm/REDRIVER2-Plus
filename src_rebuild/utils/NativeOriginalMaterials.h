#ifndef NATIVE_ORIGINAL_MATERIALS_H
#define NATIVE_ORIGINAL_MATERIALS_H
#include "NativeGpuAssetCache.h"
#include "HdTextureOverrides.h"
namespace NativeAsset
{
class OriginalMaterialAdapter
{
public:
	void BeginFrame() { vramCurrent = false; }
	void Clear();
	Result Resolve(const CachedGeometry& geometry, std::vector<ResolvedMaterial>& output);
private:
	struct OverrideCopy
	{
		unsigned int page, texture;
		unsigned short clut;
		uint64_t revision;
		HdNativeImageResult result;
		HdNativeImage image;
		std::shared_ptr<const std::vector<uint8_t> > pixels;
	};
	std::vector<OverrideCopy> overrides;
	std::vector<unsigned short> vram;
	bool vramCurrent = false;
	const OverrideCopy* FindOverride(unsigned int page, unsigned int texture, unsigned short clut, uint64_t revision);
};
}
#endif
