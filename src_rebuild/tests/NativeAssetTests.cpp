#include "../utils/NativeAssetDecode.h"
#include "../utils/NativeTextureDecode.h"
#include "../utils/NativeAssetCache.h"
#include "../utils/NativeModernDecode.h"
#include "../utils/NativeGpuAssetCache.h"
#include "../utils/NativeBackdropDecode.h"
#include "../utils/NativeRegionCoverage.h"
#include "../PsyCross/src/render/PsyX_NativeScene.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <objbase.h>
#endif

using namespace NativeAsset;
static unsigned int checks = 0;
// Exercise the actual CPU ownership boundary; no Vulkan success is claimed by
// this suite. GPU uploads/readback are separately checked by -vknativetest.
static PsyXNativeScene gpuOwner;
static int failMeshCreate = 0;
static bool resetOnMaterialCreate = false;
extern "C" void PsyX_Log_Warning(const char*, ...) { std::fprintf(stderr, "FAIL: unexpected GPU cache lifetime warning\n"); std::exit(1); }
PsyXNativeResult PsyX_Native_CreateMesh(const PsyXNativeMeshDesc* desc, PsyXNativeMeshHandle* handle)
{
	if (failMeshCreate && !--failMeshCreate) return PSYX_NATIVE_OUT_OF_BUDGET;
	return gpuOwner.Create(desc, handle);
}
PsyXNativeResult PsyX_Native_DestroyMesh(PsyXNativeMeshHandle handle) { return gpuOwner.Destroy(handle); }
PsyXNativeResult PsyX_Native_CreateMaterial(const PsyXNativeMaterialDesc* desc, PsyXNativeMaterialHandle* handle)
{
	if (resetOnMaterialCreate) { gpuOwner.Reset(); resetOnMaterialCreate = false; }
	return gpuOwner.CreateMaterial(desc, handle);
}
PsyXNativeResult PsyX_Native_DestroyMaterial(PsyXNativeMaterialHandle handle) { return gpuOwner.DestroyMaterial(handle); }
uint64_t PsyX_Native_GetSceneGeneration() { return gpuOwner.generation; }
static void Check(bool condition, const char* reason)
{
	++checks;
	if (!condition) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}
static void Put16(std::vector<unsigned char>& bytes, size_t offset, int value)
{
	bytes[offset] = (unsigned char)value; bytes[offset+1] = (unsigned char)((unsigned int)value >> 8);
}
static void Put32(std::vector<unsigned char>& bytes, size_t offset, unsigned int value)
{
	Put16(bytes, offset, value & 65535); Put16(bytes, offset + 2, value >> 16);
}
static std::vector<unsigned char> Fixture(unsigned int type)
{
	const unsigned int stride = type < 20 ? 16 : type < 22 ? 20 : 24;
	std::vector<unsigned char> bytes(36 + 32 + stride, 0);
	Put16(bytes,4,-1); Put16(bytes,12,4); Put16(bytes,14,1);
	Put32(bytes,16,36); Put32(bytes,20,68);
	const int positions[4][3] = { {-100,-30,5}, {100,-30,5}, {100,70,5}, {-100,70,5} };
	for (int i = 0; i < 4; ++i) for (int a = 0; a < 3; ++a) Put16(bytes, 36+i*8+a*2, positions[i][a]);
	bytes[68] = (unsigned char)type; bytes[69]=7; bytes[70]=3; bytes[71]=0x84;
	for (int i = 0; i < 4; ++i) { bytes[72+i]=(unsigned char)i; bytes[76+i*2]=(unsigned char)(i*40); bytes[77+i*2]=(unsigned char)(i*20); }
	if (type >= 20) { bytes[68+stride-4]=40; bytes[68+stride-3]=100; bytes[68+stride-2]=220; bytes[68+stride-1]=0x2c; }
	return bytes;
}
static Bytes Span(const std::vector<unsigned char>& bytes) { return Bytes(bytes.data(), bytes.size()); }
static std::vector<unsigned char> TerrainFixture(unsigned int faces=2)
{
	std::vector<unsigned char> bytes(36+faces*32+faces*20,0);
	Put16(bytes,0,0x400); Put16(bytes,4,-1); Put16(bytes,12,faces*4); Put16(bytes,14,faces);
	Put32(bytes,16,36); Put32(bytes,20,36+faces*32);
	for (unsigned int face=0; face<faces; ++face)
	{
		const int extent=100-int(face)*5;
		const int signs[][2]={{-1,-1},{-1,1},{1,1},{1,-1}};
		const size_t poly=36+faces*32+face*20; bytes[poly]=21;
		for (unsigned int i=0;i<4;++i)
		{
			Put16(bytes,36+(face*4+i)*8,extent*signs[i][0]);
			Put16(bytes,36+(face*4+i)*8+4,extent*signs[i][1]);
			bytes[poly+4+i]=(unsigned char)(face*4+i);
		}
	}
	return bytes;
}
static void TestTerrainLayers()
{
	Mesh mesh; Failure failure; std::vector<unsigned char> bytes=TerrainFixture(), before=bytes;
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::Complete && mesh.parts.size()==2 &&
		mesh.parts[0].depthLayer==0 && mesh.parts[1].depthLayer==1 && mesh.triangles==4,
		"Authored smaller coplanar terrain paint has a separate native layer even with the same material");
	Check(bytes==before && mesh.parts[1].vertices[0].position[1]==0 && mesh.boundsMax[1]==0,
		"Terrain layer classification preserves source bytes, vertices and bounds exactly");
	Put16(bytes,0,0);
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::Complete && mesh.parts.size()==1 &&
		mesh.parts[0].depthLayer==0,"Non-tile models never inherit a terrain layer");
	bytes=before;
	for(unsigned int i=0;i<4;++i) Put16(bytes,36+(4+i)*8+2,1);
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::Complete && mesh.parts.size()==1,
		"A physically separate parallel surface has ordinary depth");
	bytes=before; Put16(bytes,36+4*8,200);
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::Complete && mesh.parts.size()==1,
		"A face extending beyond its candidate base is not a coplanar overlay");
	bytes=before;
	std::memcpy(bytes.data()+68,bytes.data()+36,32);
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::Complete && mesh.parts.size()==1,
		"Equal-area duplicate source faces are not silently treated as overlays");
	bytes=before; std::swap(bytes[36+64+20+5],bytes[36+64+20+7]);
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::Complete && mesh.parts.size()==1,
		"Opposite-facing coplanar geometry is not an overlay");
	bytes=before; bytes[36+64+20+7]=bytes[36+64+20+6];
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::Complete && mesh.parts.size()==2 &&
		mesh.parts[1].depthLayer==1 && mesh.triangles==3,"A contained source triangle retains its authored overlay layer");
	bytes=TerrainFixture(10);
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::OutOfBudget && mesh.parts.empty() &&
		failure.polygon==9,"Excessive nested coplanar layers fail the complete resource atomically");
	// Repeated base faces exercise the independent one-time comparison bound.
	bytes=TerrainFixture(1); bytes.resize(68+1025*20);
	for(unsigned int face=1;face<1025;++face) std::memcpy(bytes.data()+68+face*20,bytes.data()+68,20);
	Put16(bytes,14,1025);
	Check(DecodeStatic(Span(bytes),Span(bytes),mesh,failure)==Result::OutOfBudget && mesh.parts.empty() &&
		failure.polygon==1024,"Terrain comparison capacity rejects excess faces without truncating geometry");
}
static void TestBackdrop()
{
	BackdropFace art={}; art.texture=27;
	const unsigned char uv[]={7,11,134,11,7,94,134,94};
	memcpy(art.uv,uv,sizeof(uv));
	Mesh mesh; Failure failure;
	for (unsigned int family=0; family<3; ++family)
	{
		const unsigned int type=family==0 ? 5 : family==1 ? 11 : 21;
		std::vector<unsigned char> bytes=Fixture(type==5 ? 21 : type); bytes[68]=(unsigned char)type;
		const std::vector<unsigned char> before=bytes;
		Check(DecodeBackdrop(Span(bytes),&art,1,mesh,failure)==Result::Complete &&
			mesh.source==Source::OriginalBackdrop && mesh.triangles==2 && mesh.parts.size()==1,
			"Horizon quad families decode owned source geometry");
		Check(bytes==before && mesh.parts[0].vertices[0].position[0]==-100 &&
			mesh.parts[0].vertices[0].position[1]==30 && mesh.parts[0].vertices[0].normal[0]==0,
			"Horizon positions reflect once without projected or invented normal data");
		const unsigned int corners[]={2,3,1,0};
		for (unsigned int corner=0; corner<4; ++corner)
			Check(mesh.parts[0].vertices[corner].uv[0]==(uv[corners[corner]*2]+.5f)/256 &&
				mesh.parts[0].vertices[corner].uv[1]==(uv[corners[corner]*2+1]+.5f)/256,
				"Horizon geometry/art correspondence matches PlotSkyPoly");
		for (size_t size=0; size<bytes.size(); ++size)
			Check(DecodeBackdrop(Bytes(bytes.data(),size),&art,1,mesh,failure)==Result::Invalid && mesh.parts.empty(),
				"Every truncated horizon byte span rejects atomically");
		bytes[75]=4;
		Check(DecodeBackdrop(Span(bytes),&art,1,mesh,failure)==Result::Invalid && mesh.parts.empty(),
			"Horizon bad index cannot publish partial geometry");
	}
	std::vector<unsigned char> bytes=Fixture(21);
	Check(DecodeBackdrop(Span(bytes),&art,0,mesh,failure)==Result::Invalid,"Horizon art count must match source polygons");
	art.texture=28;
	Check(DecodeBackdrop(Span(bytes),&art,1,mesh,failure)==Result::Invalid,"Horizon texture index is bounded");
	art.texture=0; bytes[68]=10;
	Check(DecodeBackdrop(Span(bytes),&art,1,mesh,failure)==Result::Unsupported && mesh.parts.empty(),"Horizon unsupported family is explicit");
}
static void CompleteRetirement()
{
	for (uint32_t i = 0; i < PSYX_NATIVE_MAX_MESHES; ++i)
		if (gpuOwner.CanReclaim(i, UINT64_MAX)) gpuOwner.Reclaim(i);
	for (uint32_t i = 0; i < PSYX_NATIVE_MAX_MATERIALS; ++i)
		if (gpuOwner.CanReclaimMaterial(i, UINT64_MAX)) gpuOwner.ReclaimMaterial(i);
}
static void TestSharedGpuMaterials()
{
	AssetCatalog_BeginContext(0, 0, 0);
	const int a = AssetCatalog_RegisterModel(41, "shared A", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	const int b = AssetCatalog_RegisterModel(42, "shared B", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	const int c = AssetCatalog_RegisterModel(43, "rollback C", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	CachedGeometry first;
	first.key.model = first.key.vertices = AssetCatalog_GetModelHandle(a);
	const std::vector<unsigned char> bytes = Fixture(11);
	Check(DecodeStatic(Span(bytes), Span(bytes), first.mesh, first.failure) == Result::Complete, "Shared material source decoded");
	CachedGeometry second = first, failing = first;
	second.key.model = AssetCatalog_GetModelHandle(b);
	failing.key.model = AssetCatalog_GetModelHandle(c);
	failing.mesh.parts.push_back(first.mesh.parts[0]);
	ResolvedMaterial material;
	material.regionWidth = 128; material.regionHeight = 64; material.revision = 1;
	material.image.width = material.image.height = 2; material.image.rgba.assign(16, 255);
	std::vector<ResolvedMaterial> materials(1, material);
	GpuAssetCache cache;
	const GpuGeometry* gpu = NULL;
	Check(cache.Ensure(first, materials, &gpu) == PSYX_NATIVE_PENDING && gpu, "First shared material created");
	const PsyXNativeMaterialHandle shared = gpu->materials[0];
	const PsyXNativeMeshHandle firstMesh = gpu->meshes[0];
	Check(cache.Ensure(second, materials, &gpu) == PSYX_NATIVE_PENDING && gpu &&
		gpu->materials[0].slot == shared.slot && gpu->materials[0].generation == shared.generation,
		"Distinct logical geometry shares identical owned material");
	PsyXNativeStats stats = {};
	gpuOwner.GetStats(&stats);
	Check(stats.pendingMeshes == 2 && stats.pendingMaterials == 1 && stats.ownedMaterialBytes == 16,
		"Material ownership budget counts one copy for two resources");
	failMeshCreate = 2;
	const std::vector<ResolvedMaterial> failingMaterials(2, material);
	Check(cache.Ensure(failing, failingMaterials, &gpu) == PSYX_NATIVE_OUT_OF_BUDGET && !gpu && cache.Count() == 2,
		"Late multi-part failure rolls back borrowed material references");
	CompleteRetirement();
	Check(gpuOwner.IsLive(firstMesh) && gpuOwner.IsMaterialLive(shared), "Rollback preserves material referenced by earlier geometry");
	materials[0].image.rgba[0] = 100;
	Check(cache.Ensure(first, materials, &gpu) == PSYX_NATIVE_PENDING && gpuOwner.IsMaterialLive(shared),
		"One resource content replacement cannot destroy another resource's material");
	CompleteRetirement();
	const PsyXNativeMaterialHandle replacement = gpu->materials[0];
	AssetCatalog_InvalidateModel(42); cache.Prune(); CompleteRetirement();
	Check(!gpuOwner.IsMaterialLive(shared) && gpuOwner.IsMaterialLive(replacement) && cache.Count() == 1,
		"Last shared reference retirement preserves unrelated replacement");
	gpuOwner.Reset(); CompleteRetirement(); cache.Prune();
	gpuOwner.GetStats(&stats);
	Check(cache.Count() == 0 && !stats.ownedMaterialBytes && !stats.ownedBytes,
		"Reset invalidation clears shared pool without destroying reused generations");
	Check(cache.Ensure(first, materials, &gpu) == PSYX_NATIVE_PENDING && gpu, "Shared pool recovers after scene reset");
	cache.Clear(); CompleteRetirement(); gpuOwner.GetStats(&stats);
	Check(!stats.ownedMaterialBytes && !stats.ownedBytes, "Shared material cache clears to zero ownership");
	materials[0].sharedRgba = std::make_shared<const std::vector<uint8_t> >(materials[0].image.rgba);
	std::weak_ptr<const std::vector<uint8_t> > artwork = materials[0].sharedRgba;
	materials[0].image.rgba.clear();
	Check(cache.Ensure(first, materials, &gpu) == PSYX_NATIVE_PENDING && gpu &&
		gpuOwner.materials[gpu->materials[0].slot].rgba[0] == 100,
		"Immutable shared PNG bytes reach copied GPU owner without temporary image vector");
	const PsyXNativeMeshHandle immutableMesh = gpu->meshes[0];
	Check(cache.Ensure(first, materials, &gpu) == PSYX_NATIVE_OK && gpu->meshes[0].generation == immutableMesh.generation,
		"Same immutable artwork owner reuses GPU resource");
	materials[0].sharedRgba = std::make_shared<const std::vector<uint8_t> >(16, 100);
	Check(cache.Ensure(first, materials, &gpu) == PSYX_NATIVE_PENDING && gpu && !gpuOwner.IsLive(immutableMesh),
		"Distinct immutable bytes replace GPU content without a revision change");
	Check(artwork.expired(), "Retired material releases its immutable artwork owner");
	artwork = materials[0].sharedRgba;
	materials.clear();
	Check(!artwork.expired(), "GPU cache retains immutable artwork after adapter releases it");
	cache.Clear(); CompleteRetirement();
	Check(artwork.expired() && !gpuOwner.ownedMaterialBytes && !gpuOwner.ownedBytes,
		"Last cache owner releases immutable artwork and native payloads");
}
static void TestGpuCache()
{
	AssetCatalog_BeginContext(0, 0, 0);
	const int record = AssetCatalog_RegisterModel(44, "gpu fixture", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	CachedGeometry geometry;
	geometry.key.model = geometry.key.vertices = AssetCatalog_GetModelHandle(record);
	const std::vector<unsigned char> original = Fixture(11);
	Check(DecodeStatic(Span(original), Span(original), geometry.mesh, geometry.failure) == Result::Complete, "GPU cache source fixture decoded");
	geometry.mesh.parts[0].depthLayer=1;
	ResolvedMaterial material;
	material.regionWidth = 128; material.regionHeight = 64; material.revision = 1;
	material.image.width = material.image.height = 2;
	material.image.rgba.assign(16, 255);
	std::vector<ResolvedMaterial> materials(1, material);
	GpuAssetCache cache;
	const GpuGeometry* gpu = NULL;
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_PENDING && gpu && cache.Count() == 1, "Whole GPU resource accepts every copied part");
	const PsyXNativeMeshHandle first = gpu->meshes[0];
	Check(gpuOwner.meshes[first.slot].depthLayer==1,"GPU cache copies terrain depth policy without changing source positions");
	geometry.mesh.parts[0].depthLayer=PSYX_NATIVE_MAX_DEPTH_LAYER+1;
	Check(cache.Ensure(geometry,materials,&gpu)==PSYX_NATIVE_INVALID && !gpu && gpuOwner.IsLive(first),
		"Invalid terrain layer rejects cache reuse without retiring the valid resource");
	geometry.mesh.parts[0].depthLayer=1;
	Check(cache.Ensure(geometry,materials,&gpu)==PSYX_NATIVE_OK && gpu,
		"Valid terrain policy recovers the same owned native resource after rejection");
	const PsyXNativeMaterialHandle firstMaterial = gpu->materials[0];
	Check(gpuOwner.meshes[first.slot].indices == geometry.mesh.parts[0].indices &&
		gpuOwner.meshes[first.slot].vertices[0].position[0] == geometry.mesh.parts[0].vertices[0].position[0], "GPU boundary preserves source indices and positions");
	Check(std::fabs(gpuOwner.meshes[first.slot].vertices[0].uv[0]-.5f/128) < .000001f &&
		std::fabs(gpuOwner.meshes[first.slot].vertices[3].uv[1]-60.5f/64) < .000001f, "Original byte UVs remap into cropped texture region");
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_OK && gpu->meshes[0].generation == first.generation &&
		gpu->meshes[0].slot == first.slot, "Unchanged content reuses immutable GPU handle");
	materials[0].image.rgba[0] = 123;
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_PENDING && !gpuOwner.IsLive(first) &&
		!gpuOwner.IsMaterialLive(firstMaterial), "Original palette/content changes retire old handles");
	CompleteRetirement();
	const PsyXNativeMeshHandle paletteChanged = gpu->meshes[0];
	materials[0].filter = PSYX_NATIVE_LINEAR;
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_PENDING && !gpuOwner.IsLive(paletteChanged) &&
		gpuOwner.materials[gpu->materials[0].slot].filter == PSYX_NATIVE_LINEAR, "Filtering changes rebuild material and copied geometry atomically");
	CompleteRetirement();
	materials[0].filter=PSYX_NATIVE_TRILINEAR;
	Check(cache.Ensure(geometry,materials,&gpu)==PSYX_NATIVE_PENDING &&
		gpuOwner.materials[gpu->materials[0].slot].levels.size()==2 &&
		gpuOwner.materials[gpu->materials[0].slot].rgba.size()==20,
		"Original material adapter accepts a complete owned trilinear chain");
	CompleteRetirement();
	materials[0].filter=PSYX_NATIVE_LINEAR;
	Check(cache.Ensure(geometry,materials,&gpu)==PSYX_NATIVE_PENDING,
		"Original material adapter returns to base-only filtering");
	CompleteRetirement();
	const PsyXNativeMeshHandle filtered = gpu->meshes[0];
	++materials[0].revision;
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_PENDING && !gpuOwner.IsLive(filtered), "Mod precedence/reload revision participates in GPU identity");
	CompleteRetirement();
	unsigned short atlasU = 208, atlasV = 128, atlasWidth = 24, atlasHeight = 62;
	Part atlasPart = geometry.mesh.parts[0];
	for (size_t i = 0; i < atlasPart.vertices.size(); ++i)
	{
		atlasPart.vertices[i].uv[0] = (i & 1 ? 239.5f : 208.5f)/256;
		atlasPart.vertices[i].uv[1] = (i & 2 ? 189.5f : 128.5f)/256;
	}
	Check(ExpandOriginalTextureRegion(atlasPart, atlasU, atlasV, atlasWidth, atlasHeight) == Result::Complete &&
		atlasU == 208 && atlasV == 128 && atlasWidth == 32 && atlasHeight == 62, "Original atlas overscan preserves source texels outside named width24");
	const float atlasFirstU = atlasPart.vertices[0].uv[0];
	atlasPart.vertices[3].uv[0] = std::numeric_limits<float>::quiet_NaN();
	Check(ExpandOriginalTextureRegion(atlasPart, atlasU, atlasV, atlasWidth, atlasHeight) == Result::Invalid && atlasWidth == 32,
		"Invalid source UV cannot mutate expanded region or cast NaN to integer");
	atlasPart.vertices[3].uv[0] = 239.5f/256;
	CachedGeometry atlasGeometry = geometry;
	atlasGeometry.key.lowerDetail = 17;
	atlasGeometry.mesh.parts[0] = atlasPart;
	ResolvedMaterial atlasMaterial = material;
	atlasMaterial.regionU = 208; atlasMaterial.regionV = 128; atlasMaterial.regionWidth = 24; atlasMaterial.regionHeight = 62;
	atlasMaterial.uvPolicy = ResolvedMaterial::UvPolicy::ClampOverride;
	std::vector<ResolvedMaterial> atlasMaterials(1, atlasMaterial);
	Check(cache.Ensure(atlasGeometry, atlasMaterials, &gpu) == PSYX_NATIVE_PENDING && gpu &&
		gpuOwner.meshes[gpu->meshes[0].slot].vertices[1].uv[0] > 1 && atlasPart.vertices[0].uv[0] == atlasFirstU,
		"Active PNG preserves overscan UVs for explicit clamp sampling without mutating source");
	AssetCatalog_InvalidateModel(44); cache.Prune(); CompleteRetirement();
	AssetCatalog_RegisterModel(44, "atlas reused", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	geometry.key.model = geometry.key.vertices = AssetCatalog_GetModelHandle(record);
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_PENDING, "Original cache recreated after atlas fixture eviction");
	const PsyXNativeMeshHandle stableAfterAtlas = gpu->meshes[0];
	materials[0].image.rgba.pop_back();
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_INVALID && !gpu && gpuOwner.IsLive(stableAfterAtlas), "Truncated artwork creates no partial replacement");
	materials[0].image.rgba.push_back(255);
	++materials[0].revision;
	failMeshCreate = 1;
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_OUT_OF_BUDGET && !gpu && gpuOwner.IsLive(stableAfterAtlas), "Failed mesh create rolls back its already created material");
	CompleteRetirement();
	Check(gpuOwner.ownedMaterialBytes == 16, "Rollback leaves only previous valid artwork owned");
	geometry.mesh.parts[0].vertices[3].uv[0] = 1;
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_INVALID && !gpu, "UV outside registered region rejects whole resource");
	geometry.mesh.parts[0].vertices[3].uv[0] = 120.5f/256;
	AssetCatalog_InvalidateModel(44); cache.Prune();
	Check(!cache.Count() && !gpuOwner.IsLive(stableAfterAtlas), "Region eviction retires GPU handles before slot reuse");
	CompleteRetirement();
	Check(!gpuOwner.ownedBytes && !gpuOwner.ownedMaterialBytes, "Eviction reclaims both payload budgets");
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_STALE && !gpu, "Reused source memory cannot revive invalid catalog handle");
	AssetCatalog_RegisterModel(44, "reused", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	geometry.key.model = geometry.key.vertices = AssetCatalog_GetModelHandle(record);
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_PENDING && gpu && !gpuOwner.IsLive(stableAfterAtlas), "New catalog revision creates a different generational resource");
	const PsyXNativeMeshHandle beforeReset = gpu->meshes[0];
	gpuOwner.Reset(); CompleteRetirement(); cache.Prune();
	Check(!cache.Count() && !gpuOwner.IsLive(beforeReset), "External scene reset invalidates cache without destroying reused slot");
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_PENDING, "Fresh scene can recreate immutable cached source");
	cache.Clear(); CompleteRetirement();
	Check(!gpuOwner.ownedBytes && !gpuOwner.ownedMaterialBytes && !gpuOwner.rejected, "Clear returns all owned resources without stale destroy calls");
	resetOnMaterialCreate = true;
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_STALE && !gpu && !cache.Count(),
		"Scene reset during creation cannot publish resources under the previous generation");
	CompleteRetirement();
	Check(!gpuOwner.ownedBytes && !gpuOwner.ownedMaterialBytes,
		"Interrupted creation retires resources created after reset instead of leaking them");
	Check(cache.Ensure(geometry, materials, &gpu) == PSYX_NATIVE_PENDING && gpu,
		"After interrupted rollback the same owned source can be recreated");
	cache.Clear(); CompleteRetirement();
	Check(!gpuOwner.ownedBytes && !gpuOwner.ownedMaterialBytes, "Interrupted-create recovery clears every payload");
}
static void TestCache()
{
	AssetCatalog_BeginContext(0, 0, 0);
	const int model = AssetCatalog_RegisterModel(12, "building", ASSET_CATALOG_SOURCE_DECLARED, 37, 19);
	const int parent = AssetCatalog_RegisterModel(37, "vertices", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	GeometryKey key;
	key.model = AssetCatalog_GetModelHandle(model); key.vertices = AssetCatalog_GetModelHandle(parent);
	key.lodParent = 37; key.highDetail = 19; key.lowerDetail = 5;
	std::vector<unsigned char> child = Fixture(21), source = Fixture(11);
	Put16(child, 4, 37);
	GeometryCache cache;
	const CachedGeometry* entry = NULL;
	Check(cache.Capture(key, Span(child), Span(source), &entry) == Result::Complete && entry, "Cache captures full owned original resource");
	Check(entry->stableId == "model:0:0:12" && entry->key.lowerDetail == 5 && entry->key.highDetail == 19, "Stable catalog identity and both LOD links retained");
	Check(cache.Count() == 1 && cache.OwnedBytes() >= entry->mesh.parts[0].vertices.size() * sizeof(PsyXNativeVertex), "Cache accounts owned geometry payload");
	const uint64_t sourceHash = entry->sourceHash;
	const float originalX = entry->mesh.parts[0].vertices[0].position[0];
	Put16(source, 36, 999); child[68] = 8;
	Check(cache.Capture(key, Span(child), Span(source), &entry) == Result::Complete && entry->sourceHash == sourceHash &&
		entry->mesh.parts[0].vertices[0].position[0] == originalX, "GTE/source reuse cannot mutate immutable cached geometry");
	GeometryKey changedLod = key; changedLod.lowerDetail = 6;
	Check(!cache.Find(changedLod), "LOD identity changes do not return another derived resource");
	Mesh retained = entry->mesh;
	AssetCatalog_InvalidateModel(37);
	Check(!cache.Find(key), "Parent vertex invalidation blocks cached instance immediately");
	cache.PruneStale();
	Check(!cache.Count() && !cache.OwnedBytes() && retained.parts[0].vertices[0].position[0] == originalX, "Prune frees only owner data; independently owned snapshot survives");
	AssetCatalog_RegisterModel(37, "reused", ASSET_CATALOG_SOURCE_DECLARED, -1, -1);
	Check(cache.Capture(key, Span(child), Span(source), &entry) == Result::Invalid && !entry, "Recycled parent slot cannot alias retained handle");
	key.vertices = AssetCatalog_GetModelHandle(parent);
	Check(cache.Capture(key, Span(child), Span(source), &entry) == Result::Unsupported && entry && entry->mesh.parts.empty() &&
		!entry->failure.reason.empty(), "Unsupported whole model is an explicit cached failure");
	AssetCatalog_RegisterModel(12, "replacement", ASSET_CATALOG_SOURCE_DECLARED, 37, 19);
	Check(!cache.Find(key), "Recycled model catalog revision blocks old resource");
	key.model = AssetCatalog_GetModelHandle(model); child = Fixture(21); Put16(child, 4, 37);
	Check(cache.Capture(key, Span(child), Span(source), &entry) == Result::Complete && cache.Count() == 1 &&
		entry->mesh.parts[0].vertices[0].position[0] == 999, "Re-registered slot creates new geometry instead of stale alias");
	GeometryCache noBytes(128, 1), noSlots(0, 32 * 1024 * 1024);
	Check(noBytes.Capture(key, Span(child), Span(source), &entry) == Result::OutOfBudget && !entry && !noBytes.Count(), "Cache byte pressure leaves no partial entry");
	Check(noSlots.Capture(key, Span(child), Span(source), &entry) == Result::OutOfBudget && !entry, "Cache slot pressure is explicit");
	GeometryKey oldDecoder = key; oldDecoder.decoderVersion = DecoderVersion + 1;
	Check(!cache.Find(oldDecoder) && cache.Capture(oldDecoder, Span(child), Span(source), &entry) == Result::Invalid, "Derived decoder version cannot alias original cache");
	AssetCatalog_BeginContext(1, 0, 0);
	Check(!cache.Find(key), "City/context generation invalidates geometry");
	cache.PruneStale();
	Check(!cache.Count() && !cache.OwnedBytes(), "Context reset releases derived cache payload");
}
static void TestModern(const char* path)
{
	GltfMeshData source;
	char error[512] = {};
	Check(Gltf_LoadFile(path, &source, error, sizeof(error)) == 0, "Existing owned GLB fixture loads through bounded existing importer");
	ModernImport imported; Failure failure;
	Check(DecodeModernPrimitive(source, imported, failure) == Result::Complete, "Owned GLB primitive uses shared native mesh contract");
	Check(imported.mesh.source == Source::ModernStatic && imported.mesh.sourceVertices == 2220 &&
		imported.mesh.triangles == 1479 && imported.mesh.parts[0].indices.size() == 4437, "Owned bollard verified source/index counts");
	Check(std::fabs(imported.mesh.boundsMin[0] - -.307116985f) < .000001f &&
		std::fabs(imported.mesh.boundsMax[1] - .948455989f) < .000001f, "Owned GLB source units/Y-up bounds unchanged");
	Check(imported.mesh.parts[0].indices == source.indices && imported.mesh.parts[0].vertices[0].uv[0] == source.uvs[0], "Modern source winding/UVs retained without original Y reflection");
	Check(!imported.baseColor.rgba.empty() && imported.baseColor.rgba == source.baseColorRGBA &&
		imported.baseColor.width == unsigned(source.baseColorWidth), "Owned embedded PNG feeds native material artwork contract");
	const float position = imported.mesh.parts[0].vertices[0].position[0];
	source.positions[0] += 100; source.baseColorRGBA[0] ^= 255;
	Check(imported.mesh.parts[0].vertices[0].position[0] == position && imported.baseColor.rgba[0] != source.baseColorRGBA[0], "Recycling importer buffers cannot alter native geometry or artwork");
	source.indices.back() = unsigned(source.vertexCount);
	Check(DecodeModernPrimitive(source, imported, failure) == Result::Invalid && imported.mesh.parts.empty() &&
		imported.baseColor.rgba.empty(), "Late invalid index rejects whole modern resource/material");
	source.indices.back() = 0; source.uvs.pop_back();
	Check(DecodeModernPrimitive(source, imported, failure) == Result::Invalid && imported.mesh.parts.empty(), "Truncated modern UV array is explicit whole-resource failure");
	source.uvs.push_back(0); source.normals[0] = std::numeric_limits<float>::quiet_NaN();
	Check(DecodeModernPrimitive(source, imported, failure) == Result::Invalid, "Non-finite modern normal rejected");
	source.normals[0] = 1; source.baseColorFactor[3] = .5f;
	Check(DecodeModernPrimitive(source, imported, failure) == Result::Unsupported, "Later transparent modern adapter is explicit, never silently opaque");
	source.baseColorFactor[3] = 1; source.baseColorRGBA.pop_back();
	Check(DecodeModernPrimitive(source, imported, failure) == Result::Invalid && imported.mesh.parts.empty(), "Incomplete modern image bytes reject complete import");
}
static void TestRegionCoverage()
{
	RegionCoverage coverage;
	coverage.BeginFrame();
	Check(coverage.Select(0) && coverage.Select(16383), "Region accounting accepts both 14-bit slot boundaries");
	Check(!coverage.Select(0) && coverage.Counts().selected == 2, "Straddling cell references do not duplicate source objects");
	Check(coverage.Copy(0, 2), "Source material parts copied once");
	coverage.ObserveView(0); coverage.ObserveView(0); coverage.ObserveView(16383); coverage.ObserveView(42);
	Check(coverage.Counts().viewInside == 2 && coverage.Counts().viewOutside == 1 &&
		coverage.Counts().duplicateView == 1 && coverage.Counts().viewUncopied == 1,
		"View candidates separate inside, outside, missing copy and repeat without changing selection");
	coverage.BeginPublication();
	Check(coverage.Publish(0, 2) && !coverage.CompletePublication(), "Uncopied selected source prevents complete publication");
	Check(coverage.Copy(16383, 3) && coverage.Publish(16383, 3) && coverage.CompletePublication(),
		"All selected sources and their exact part counts reconcile");
	Check(coverage.Counts().published == 2 && coverage.Counts().publishedParts == 5 &&
		coverage.Counts().copiedParts == 5, "Publication objects and material parts remain distinct");
	coverage.BeginPublication();
	Check(!coverage.CompletePublication() && coverage.Counts().published == 0 &&
		coverage.Counts().viewOutside == 1, "Rejected publication clears progress but preserves source/view diagnostics");
	Check(coverage.Publish(16383, 3) && coverage.Publish(0, 2) && coverage.CompletePublication(),
		"Publication order does not affect coverage");
	Check(!coverage.Publish(0, 2) && !coverage.CompletePublication(), "Duplicate object publication invalidates acceptance");
	coverage.BeginFrame();
	Check(coverage.Counts().selected == 0 && coverage.Counts().viewOutside == 0 &&
		coverage.Counts().published == 0 && coverage.Counts().invalid == 0, "New frame clears all slot history and counters");
	coverage.Select(42); coverage.Copy(42, 1); coverage.ObserveView(42);
	Check(coverage.Counts().viewInside == 1 && coverage.Counts().viewOutside == 0,
		"A recycled slot previously outside is classified from the new frame");
	Check(!coverage.Publish(42, 2) && coverage.Counts().published == 0, "Wrong material-part count cannot masquerade as coverage");
	coverage.BeginFrame(); coverage.Select(0);
	Check(!coverage.Copy(0, 0) && !coverage.Copy(0, 65536), "Empty and overflowing source part counts rejected");
	coverage.BeginFrame();
	Check(!coverage.Copy(1, 1) && !coverage.Publish(1, 1), "Unselected or uncopied slots cannot become native publication");
	coverage.BeginFrame(); coverage.Select(1); coverage.Copy(1, 1);
	Check(!coverage.Copy(1, 1) && coverage.Counts().copied == 1, "Duplicate copy cannot inflate source coverage");
	coverage.BeginFrame();
	Check(!coverage.Select(16384) && !coverage.Copy(UINT32_MAX, 1) && !coverage.Publish(16384, 1),
		"Out-of-bounds slots rejected without bitset access");
	coverage.ObserveView(UINT32_MAX);
	Check(coverage.Counts().invalid == 4 && coverage.Counts().viewOutside == 0,
		"Invalid pointer-derived slots are errors, not legitimate outside coverage");
}
int main(int argc, char** argv)
{
#ifdef _WIN32
	const HRESULT com = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	Check(SUCCEEDED(com), "Owned GLB image fixture has WIC COM context");
#endif
	Check(argc == 2, "Owned GLB fixture path is explicit");
	TestModern(argv[1]);
	TestCache();
	TestGpuCache();
	TestSharedGpuMaterials();
	TestBackdrop();
	TestRegionCoverage();
	TestTerrainLayers();
	Mesh mesh; Failure failure;
	for (unsigned int type = 10; type <= 23; ++type)
	{
		if (type > 11 && type < 20) continue;
		std::vector<unsigned char> source = Fixture(type), before = source;
		Check(DecodeStatic(Span(source),Span(source),mesh,failure)==Result::Complete, "Supported static family decodes");
		Check(source==before, "Original source remains byte-exact");
		Check(mesh.sourceVertices==4 && mesh.sourcePolygons==1 && mesh.triangles==((type&1)?2u:1u), "Original counts preserved");
		const Part& part=mesh.parts[0];
		Check(part.indices.size()==mesh.triangles*3 && part.vertices.size()==part.indices.size(), "Owned indexed triangle counts");
		Check(part.material.textureSet==7 && part.material.textureIndex==3 && part.material.normalDirection==0x84, "Texture and plane identity preserved");
		Check(part.material.hasSourceColor==(type>=20), "Original face colour presence explicit");
		if (type>=20) Check(part.material.sourceColor[0]==40 && part.material.sourceColor[2]==220, "Stored face colour preserved");
		Check(part.vertices[0].position[1]==30 && mesh.boundsMin[1]==-70 && mesh.boundsMax[1]==30, "Y reflection and bounds");
		Check(part.vertices[0].normal[2]==1 && part.vertices[0].uv[0]==0.5f/256, "Winding/source normal and texel centre");
		Check(part.vertices[1].uv[0]==((type&1)?120.5f:80.5f)/256, "Original diagonal and UV corner association");
		for (size_t length=0; length<source.size(); ++length)
		{
			Check(DecodeStatic(Bytes(source.data(),length),Span(source),mesh,failure)==Result::Invalid, "Every truncated polygon span rejected");
			Check(mesh.parts.empty() && mesh.triangles==0, "Failed decode never publishes partial resource");
		}
	}
	std::vector<unsigned char> child=Fixture(21), parent=Fixture(11);
	Put16(child,4,-2);
	Check(DecodeStatic(Span(child),Span(parent),mesh,failure)==Result::Invalid && mesh.parts.empty(), "Invalid negative parent index rejects complete resource");
	Put16(child,4,37); Put32(child,16,0xffffffffu);
	Check(DecodeStatic(Span(child),Span(parent),mesh,failure)==Result::Complete && mesh.vertexSource==37, "Instance consumes bounded parent vertices");
	const float ownedX=mesh.parts[0].vertices[0].position[0];
	Put16(parent,36,999);
	Check(mesh.parts[0].vertices[0].position[0]==ownedX, "Recycling source memory cannot alter owned mesh");
	Put16(parent,4,2);
	Check(DecodeStatic(Span(child),Span(parent),mesh,failure)==Result::Unsupported && mesh.parts.empty(), "Nested instances explicitly rejected");
	child=Fixture(21); child[72]=4;
	Check(DecodeStatic(Span(child),Span(child),mesh,failure)==Result::Invalid, "Invalid vertex index rejected");
	child=Fixture(21); Put32(child,20,0xfffffff0u);
	Check(DecodeStatic(Span(child),Span(child),mesh,failure)==Result::Invalid, "Overflowing source offset rejected");
	child=Fixture(21); Put32(child,16,0xfffffff0u);
	Check(DecodeStatic(Span(child),Span(child),mesh,failure)==Result::Invalid, "Overflowing vertex offset rejected");
	child=Fixture(21); child[68]=4;
	Check(DecodeStatic(Span(child),Span(child),mesh,failure)==Result::Unsupported && mesh.parts.empty(), "Unsupported complete family reported");
	child=Fixture(21); Put16(child,14,2); child.resize(child.size()+20,0); child[88]=8;
	Check(DecodeStatic(Span(child),Span(child),mesh,failure)==Result::Unsupported && failure.polygon==1 && mesh.parts.empty(), "Supported prefix cannot hide missing face");
	child=Fixture(21); child[75]=child[74]; child[82]=child[80]; child[83]=child[81];
	Check(DecodeStatic(Span(child),Span(child),mesh,failure)==Result::Complete && mesh.triangles==1, "Legacy converted triangle counted once");
	child=Fixture(21); Put16(child,14,65535); child.resize(88+65534*20);
	for (unsigned int i=1;i<65535;++i) std::memcpy(child.data()+68+i*20,child.data()+68,20);
	Check(DecodeStatic(Span(child),Span(child),mesh,failure)==Result::OutOfBudget && mesh.parts.empty(), "Output budget rejects atomic complete resource");
	std::vector<unsigned char> packed(8); Put16(packed,0,40000); Put16(packed,2,-61); Put16(packed,4,20000); Put16(packed,6,37*64+16);
	Placement placement;
	Check(DecodePlacement(Span(packed),40010,19980,placement)==Result::Complete, "Packed placement decodes");
	Check(placement.modelIndex==1061 && placement.yaw==16 && placement.position[0]==40000 && placement.position[1]==-31 && placement.position[2]==20000, "Packed model high bit/Y and cell wrap match original");
	Check(std::fabs(placement.world[8]-1)<0.000001f && placement.world[13]==31 && placement.world[12]==40000, "Placement native yaw/translation");
	Put16(packed,0,65530); Put16(packed,2,0);
	Check(DecodePlacement(Span(packed),-10,20000,placement)==Result::Complete && placement.position[0]==-6, "Negative cell-centre wrap");
	Put16(packed,0,0);
	Check(DecodePlacement(Span(packed),INT32_MAX,20000,placement)==Result::Invalid, "Cell-centre overflow rejected");
	Put16(packed,2,1); Put16(packed,6,65535);
	Check(DecodePlacement(Span(packed),0,0,placement)==Result::Unsupported, "Cell sentinel explicit");
	Check(DecodePlacement(Bytes(packed.data(),7),0,0,placement)==Result::Invalid, "Truncated placement rejected");
	std::vector<unsigned char> vram(1024*512*2,0);
	// Four palette indices, transparent zero / STP black / red / blue.
	Put16(vram,0,0x3210);
	Put16(vram,1024*2+0,0); Put16(vram,1024*2+2,0x8000);
	Put16(vram,1024*2+4,31); Put16(vram,1024*2+6,0x7c00);
	Texture texture;
	Check(DecodeTexture(Span(vram),0,64,0,0,4,1,false,texture)==Result::Complete, "4-bit original palette decoding");
	Check(texture.rgba[3]==0 && texture.rgba[7]==255 && texture.rgba[4]==0 && texture.rgba[8]==255 && texture.rgba[14]==255, "Zero and opaque STP-black colour semantics");
	Check(texture.hasCutout && texture.hasStp && texture.width==4 && texture.height==1, "Texture shape/cutout/STP diagnostics");
	Check(DecodeTexture(Span(vram),0,64,0,0,4,1,true,texture)==Result::Complete && texture.rgba[7]==128, "STP honoured only in semi-transparent context");
	Check(DecodeTexture(Span(vram),0,64,2,0,2,1,false,texture)==Result::Complete && texture.rgba[0]==255 && texture.rgba[6]==255, "Cropped source region preserves palette texels");
	Put16(vram,0,0x0201);
	Check(DecodeTexture(Span(vram),128,64,0,0,2,1,false,texture)==Result::Complete && texture.rgba[3]==255 && texture.rgba[4]==255, "8-bit CLUT decoding");
	Put16(vram,0,0x83e0);
	Check(DecodeTexture(Span(vram),256,65535,0,0,1,1,false,texture)==Result::Complete && texture.rgba[1]==255 && texture.rgba[3]==255, "16-bit direct colour ignores CLUT");
	Check(DecodeTexture(Span(vram),384,64,0,0,1,1,false,texture)==Result::Unsupported && texture.rgba.empty(), "Reserved texture format explicit");
	Check(DecodeTexture(Bytes(vram.data(),vram.size()-1),0,64,0,0,1,1,false,texture)==Result::Invalid, "Truncated VRAM mirror rejected");
	Check(DecodeTexture(Span(vram),0,64,255,0,2,1,false,texture)==Result::Invalid, "Region overrun rejected");
	Check(DecodeTexture(Span(vram),128,127,0,0,2,1,false,texture)==Result::Invalid, "8-bit palette row overrun rejected");
	Check(DecodeTexture(Span(vram),15|256,64,255,0,1,1,false,texture)==Result::Invalid, "Direct page overrun rejected");
	Check(DecodeTexture(Span(vram),0x800,64,0,0,1,1,false,texture)==Result::Invalid, "Extended page outside original VRAM rejected");
	std::printf("PASS: %u native original decode integrity checks\n",checks);
#ifdef _WIN32
	CoUninitialize();
#endif
	return 0;
}
