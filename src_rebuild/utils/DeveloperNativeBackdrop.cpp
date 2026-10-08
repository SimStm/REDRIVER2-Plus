#include "DeveloperNativeBackdrop.h"
#if !defined(PSX) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
#include "DeveloperNativeCamera.h"
#include "NativeBackdropDecode.h"
#include "NativeGpuAssetCache.h"
#include "driver2.h"
#include "C/models.h"
#include "PsyX/PsyX_render.h"
#include "PsyX/PsyX_public.h"
#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

namespace
{
struct BackdropModel
{
	NativeAsset::CachedGeometry geometry;
	std::vector<NativeAsset::BackdropFace> faces;
	std::vector<unsigned short> pages, cluts;
	int color[3], verticalOffset;
	bool observed;
	BackdropModel() : color(), verticalOffset(0), observed(false) {}
};
BackdropModel s_models[4];
NativeAsset::GpuAssetCache s_gpu;
bool s_enabled = false;
PsyXNativeResult s_failure = PSYX_NATIVE_OK;

bool SameBindings(const BackdropModel& cached, const std::vector<NativeAsset::BackdropFace>& faces)
{
	if (cached.faces.size()!=faces.size()) return false;
	for (size_t i=0; i<faces.size(); ++i)
		if (cached.faces[i].texture!=faces[i].texture || memcmp(cached.faces[i].uv,faces[i].uv,8)) return false;
	return true;
}
struct DecodedArt
{
	NativeAsset::BackdropFace face;
	NativeAsset::ResolvedMaterial material;
};
PsyXNativeResult ResolveMaterials(const BackdropModel& model, NativeAsset::Bytes vram,
	std::vector<DecodedArt>& artwork, std::vector<NativeAsset::ResolvedMaterial>& materials)
{
	for (size_t part=0; part<model.faces.size(); ++part)
	{
		const NativeAsset::BackdropFace& face = model.faces[part];
		size_t existing=0;
		while (existing<artwork.size())
		{
			const DecodedArt& cached=artwork[existing];
			if (cached.face.texture==face.texture && !memcmp(cached.face.uv,face.uv,8) &&
				cached.material.image.tpage==model.pages[part] && cached.material.image.clut==model.cluts[part]) break;
			++existing;
		}
		if (existing<artwork.size())
		{
			NativeAsset::ResolvedMaterial material=artwork[existing].material;
			material.revision=model.geometry.key.model.revision;
			materials.push_back(std::move(material)); continue;
		}
		unsigned int minU=255,minV=255,maxU=0,maxV=0;
		for (unsigned int corner=0; corner<4; ++corner)
		{
			minU=std::min(minU,unsigned(face.uv[corner*2])); maxU=std::max(maxU,unsigned(face.uv[corner*2]));
			minV=std::min(minV,unsigned(face.uv[corner*2+1])); maxV=std::max(maxV,unsigned(face.uv[corner*2+1]));
		}
		NativeAsset::ResolvedMaterial material;
		material.regionU=minU; material.regionV=minV; material.regionWidth=maxU-minU+1; material.regionHeight=maxV-minV+1;
		material.cull=PSYX_NATIVE_CULL_NONE;
		material.revision=model.geometry.key.model.revision;
		material.filter=PSYX_NATIVE_TRILINEAR;
		if (NativeAsset::DecodeTexture(vram,model.pages[part],model.cluts[part],minU,minV,
			material.regionWidth,material.regionHeight,false,material.image)!=NativeAsset::Result::Complete)
			return PSYX_NATIVE_UNSUPPORTED;
		// Share immutable owned decoded bytes across the four horizon models.
		// The cache lives for this frame only: VRAM is still read/decoded anew,
		// and GPU reuse still verifies exact content rather than a revision alone.
		material.sharedRgba=std::make_shared<const std::vector<uint8_t> >(std::move(material.image.rgba));
		DecodedArt decoded; decoded.face=face; decoded.material=material;
		artwork.push_back(std::move(decoded));
		materials.push_back(std::move(material));
	}
	return PSYX_NATIVE_OK;
}
}
void DeveloperNativeBackdrop_SetEnabled(int enabled) { s_enabled=enabled!=0; }
void DeveloperNativeBackdrop_BeginFrame()
{
	s_failure=PSYX_NATIVE_OK;
	for (unsigned int i=0; i<4; ++i) s_models[i].observed=false;
}
void DeveloperNativeBackdrop_Clear()
{
	s_gpu.Clear();
	for (unsigned int i=0; i<4; ++i) s_models[i]=BackdropModel();
	DeveloperNativeBackdrop_BeginFrame();
}
void DeveloperNativeBackdrop_Observe(const DeveloperNativeBackdropSource* source)
{
	if (!s_enabled || s_failure!=PSYX_NATIVE_OK) return;
	if (!source || !source->model || !source->textures || !source->uv || !source->pages || !source->cluts)
	{ s_failure=PSYX_NATIVE_INVALID; return; }
	unsigned int slot=0;
	while (slot<4 && modelpointers[slot]!=source->model) ++slot;
	if (slot==4 || source->uvCount!=28) { s_failure=PSYX_NATIVE_UNSUPPORTED; return; }
	try
	{
		const MODEL* model=static_cast<const MODEL*>(source->model);
		const unsigned int sourceSize=Models_GetSourceSize(slot);
		if (sourceSize<36) { s_failure=PSYX_NATIVE_STALE; return; }
		if (model->num_polys>source->textureCount || model->num_polys>40)
		{ s_failure=PSYX_NATIVE_INVALID; return; }
		NativeAsset::GeometryKey key;
		key.model=key.vertices=AssetCatalog_GetModelHandle(AssetCatalog_FindModel(slot));
		if (!AssetCatalog_HandleValid(key.model)) { s_failure=PSYX_NATIVE_STALE; return; }
		std::vector<NativeAsset::BackdropFace> faces(model->num_polys);
		std::vector<unsigned short> pages(model->num_polys), cluts(model->num_polys);
		for (unsigned int part=0; part<model->num_polys; ++part)
		{
			const unsigned int texture=source->textures[part];
			if (texture>=source->uvCount) { s_failure=PSYX_NATIVE_INVALID; return; }
			faces[part].texture=(unsigned char)texture;
			memcpy(faces[part].uv,source->uv+texture*8,8);
			pages[part]=source->pages[texture]; cluts[part]=source->cluts[texture];
		}
		BackdropModel& cached=s_models[slot];
		if (!(cached.geometry.key==key) || !SameBindings(cached,faces))
		{
			NativeAsset::Mesh decoded;
			NativeAsset::Failure failure;
			const NativeAsset::Result result=NativeAsset::DecodeBackdrop(
				NativeAsset::Bytes(model,sourceSize),faces.data(),faces.size(),decoded,failure);
			if (result!=NativeAsset::Result::Complete)
			{
				PsyX_Log_Warning("Native backdrop: model%u polygon%u %s\n",slot,failure.polygon,failure.reason.c_str());
				s_failure=result==NativeAsset::Result::OutOfBudget ? PSYX_NATIVE_OUT_OF_BUDGET : PSYX_NATIVE_UNSUPPORTED; return;
			}
			s_gpu.Clear(); // A changed art layout changes the immutable mesh UVs.
			cached.geometry=NativeAsset::CachedGeometry(); cached.geometry.key=key;
			cached.geometry.mesh=std::move(decoded); cached.faces=std::move(faces);
		}
		cached.pages=std::move(pages); cached.cluts=std::move(cluts);
		memcpy(cached.color,source->color,sizeof(cached.color)); cached.verticalOffset=source->verticalOffset;
		cached.observed=true;
	}
	catch (const std::bad_alloc&) { s_failure=PSYX_NATIVE_OUT_OF_BUDGET; }
}
PsyXNativeResult DeveloperNativeBackdrop_Append(const PsyXNativeView& view, std::vector<PsyXNativeInstance>& instances)
{
	if (!s_enabled) return PSYX_NATIVE_OK;
	if (s_failure!=PSYX_NATIVE_OK) return s_failure;
	for (unsigned int slot=0; slot<4; ++slot) if (!s_models[slot].observed) return PSYX_NATIVE_STALE;
	s_gpu.Prune();
	std::vector<unsigned short> vram(1024*512);
	GR_ReadVRAM(vram.data(),0,0,1024,512);
	std::vector<DecodedArt> artwork;
	artwork.reserve(28);
	for (unsigned int slot=0; slot<4; ++slot)
	{
		const BackdropModel& model=s_models[slot];
		float world[16];
		if (!DeveloperNativeCamera_BuildBackdropTransform(world,view,float(model.verticalOffset))) return PSYX_NATIVE_INVALID;
		std::vector<NativeAsset::ResolvedMaterial> materials;
		const PsyXNativeResult resolved=ResolveMaterials(model,NativeAsset::Bytes(vram.data(),vram.size()*2),artwork,materials);
		if (resolved!=PSYX_NATIVE_OK) return resolved;
		const NativeAsset::GpuGeometry* gpu=NULL;
		const PsyXNativeResult uploaded=s_gpu.Ensure(model.geometry,materials,&gpu);
		if (uploaded!=PSYX_NATIVE_OK && uploaded!=PSYX_NATIVE_PENDING) return uploaded;
		if (gpu->meshes.size()>PSYX_NATIVE_MAX_INSTANCES-instances.size()) return PSYX_NATIVE_OUT_OF_BUDGET;
		for (size_t part=0; part<gpu->meshes.size(); ++part)
		{
			PsyXNativeInstance instance={};
			instance.sampling=g_cfg_bilinearFiltering ? PSYX_NATIVE_SAMPLE_TRILINEAR : PSYX_NATIVE_SAMPLE_NEAREST;
			instance.mesh=gpu->meshes[part]; instance.layer=PSYX_NATIVE_BACKDROP;
			memcpy(instance.world,world,sizeof(world));
			for (unsigned int channel=0; channel<3; ++channel)
				instance.tint[channel]=float(part+8>=gpu->meshes.size() ? model.color[channel]/2 : model.color[channel])/128;
			instance.tint[3]=1;
			instances.push_back(instance);
		}
	}
	return PSYX_NATIVE_OK;
}
#else
void DeveloperNativeBackdrop_Observe(const DeveloperNativeBackdropSource*) {}
#endif
