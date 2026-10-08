#include "../PsyCross/src/render/PsyX_NativeScene.h"
#include "../PsyCross/src/render/PsyX_NativePick.h"
#include "../PsyCross/src/render/PsyX_NativeTransform.h"
#include "../PsyCross/src/render/PsyX_NativePlane.h"
#include "../utils/DeveloperNativeCamera.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <cstdlib>
#include <memory>

extern "C" PsyXNativeMeshDesc NativeSceneCProbe(const PsyXNativeVertex*, const uint32_t*);
extern "C" PsyXNativeSampling NativeSamplingCProbe(void);
extern "C" PsyXNativePickResult NativePickCProbe(void);

static void Check(bool condition, const char* message)
{
	if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static float ProjectDepth(const PsyXNativeView& view, float distance)
{
	return (-distance * view.projection[10] + view.projection[14]) / distance;
}

struct Fixture
{
	std::unique_ptr<PsyXNativeScene> owner;
	PsyXNativeScene& scene;
	PsyXNativeVertex vertices[3];
	uint32_t indices[3];
	PsyXNativeMeshDesc desc;
	PsyXNativeMeshHandle handle;
	PsyXNativeInstance instance;
	PsyXNativeSnapshot snapshot;
	Fixture() : owner(new PsyXNativeScene()), scene(*owner)
	{
		memset(vertices, 0, sizeof(vertices));
		vertices[1].position[0] = vertices[2].position[1] = 1;
		for (unsigned int i = 0; i < 3; ++i)
		{
			vertices[i].normal[2] = 1;
			for (unsigned int j = 0; j < 4; ++j) vertices[i].color[j] = 1;
			indices[i] = i;
		}
		desc = NativeSceneCProbe(vertices, indices);
		memset(&handle, 0, sizeof(handle));
		memset(&instance, 0, sizeof(instance));
		for (unsigned int i = 0; i < 16; i += 5) instance.world[i] = 1;
		for (unsigned int i = 0; i < 4; ++i) instance.tint[i] = 1;
		instance.identity = 42;
		memset(&snapshot, 0, sizeof(snapshot));
		snapshot.size = sizeof(snapshot);
		snapshot.version = PSYX_NATIVE_VERSION;
		snapshot.sceneGeneration = scene.generation;
		snapshot.simulationTick = 60;
		for (unsigned int i = 0; i < 16; i += 5)
			snapshot.view.view[i] = snapshot.view.projection[i] = 1;
		snapshot.view.nearPlane = 10;
		snapshot.view.farPlane = 125000;
		snapshot.instances = &instance;
		snapshot.instanceCount = 1;
	}
	void Create()
	{
		Check(scene.Create(&desc, &handle) == PSYX_NATIVE_PENDING && handle.generation, "owned pending create/C ABI");
		instance.mesh = handle;
	}
};

static void TestMixedHeightRanges()
{
	Fixture f;
	PsyXNativeVertex vertices[8];
	const float positions[8][3]={{0,0,0},{1,0,0},{0,0,1},{1,-1,1},
		{0,2,0},{1,2,0},{0,2,1},{0,9,0}}; // Last vertex is intentionally unreferenced.
	for (unsigned int i=0;i<8;++i)
	{
		vertices[i]=f.vertices[0];
		memcpy(vertices[i].position,positions[i],sizeof(positions[i]));
	}
	const uint32_t indices[]={0,1,2, 1,0,2, 0,1,3, 2,3,0, 4,5,6, 0,1,2, 4,5,6};
	f.desc.vertices=vertices;f.desc.vertexCount=8;f.desc.indices=indices;f.desc.indexCount=21;
	f.desc.boundsMin[1]=-1;f.desc.boundsMax[1]=9;f.desc.boundsMax[2]=1;
	f.Create();
	const PsyXNativeScene::Mesh& mesh=f.scene.meshes[f.handle.slot];
	Check(mesh.drawRanges.size()==5,"Mixed top/side/height mesh splits only consecutive indexed runs");
	const uint32_t firsts[]={0,6,12,15,18},counts[]={6,6,3,3,3};
	const bool horizontal[]={true,false,true,true,true};
	const float heights[]={0,0,2,0,2};
	for(unsigned int i=0;i<5;++i)
		Check(mesh.drawRanges[i].firstIndex==firsts[i] && mesh.drawRanges[i].indexCount==counts[i] &&
			mesh.drawRanges[i].horizontalPlane==horizontal[i] && mesh.drawRanges[i].planeY==heights[i],
			"Exact range coverage/order/height, without reordering nonadjacent triangles");
	Check(!memcmp(mesh.vertices.data(),vertices,sizeof(vertices)) && !memcmp(mesh.indices.data(),indices,sizeof(indices)),
		"Derived draw ranges preserve every source vertex/index byte");
	vertices[0].position[1]=3;
	Check(mesh.drawRanges[0].planeY==0 && mesh.vertices[0].position[1]==0,"Mixed mesh owns its source and derived heights");
	const uint64_t rangeBytes=f.scene.ownedDrawRangeBytes;
	PsyXNativeMeshHandle rejected={};
	f.scene.ownedDrawRangeBytes=PSYX_NATIVE_MAX_BYTES;
	Check(f.scene.Create(&f.desc,&rejected)==PSYX_NATIVE_OUT_OF_BUDGET && !rejected.generation &&
		f.scene.ownedDrawRangeBytes==PSYX_NATIVE_MAX_BYTES,"CPU range budget rejects without partial publication");
	f.scene.ownedDrawRangeBytes=rangeBytes;
	std::vector<uint32_t> alternating;
	for(unsigned int i=0;i<PsyXNativeScene::MaxDrawRanges+1;++i)
	{
		const uint32_t* triangle=indices+(i%2 ? 6 : 12);
		alternating.insert(alternating.end(),triangle,triangle+3);
	}
	f.desc.indices=alternating.data();f.desc.indexCount=uint32_t(alternating.size());
	Check(f.scene.Create(&f.desc,&rejected)==PSYX_NATIVE_OUT_OF_BUDGET && !rejected.generation &&
		f.scene.ownedDrawRangeBytes==rangeBytes,"Alternating-plane mesh cannot exceed bounded draw range count");
	std::vector<PsyXNativeInstance> instances(PsyXNativeScene::MaxDrawRanges/5+1,f.instance);
	f.snapshot.instances=instances.data();f.snapshot.instanceCount=uint32_t(instances.size()-1);
	Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_OK,"Frame inside physical draw budget accepts mixed meshes");
	const uint32_t accepted=f.scene.snapshot.instanceCount;
	++f.snapshot.instanceCount;
	Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_OUT_OF_BUDGET && f.scene.snapshot.instanceCount==accepted,
		"Physical draw budget rejection preserves the previous native snapshot");
	for(size_t i=0;i<instances.size();++i) instances[i].layer=PSYX_NATIVE_BACKDROP;
	Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_OK,"Backdrop keeps one draw per logical mesh");
	f.scene.meshes[f.handle.slot].lastSubmission=7;
	f.scene.Destroy(f.handle);
	Check(!f.scene.CanReclaim(f.handle.slot,6) && mesh.drawRanges.size()==5 && f.scene.ownedDrawRangeBytes==rangeBytes,
		"Range metadata remains owned until its submission fence completes");
	Check(f.scene.CanReclaim(f.handle.slot,7),"Completed submission permits range retirement");
	f.scene.Reclaim(f.handle.slot);
	Check(mesh.drawRanges.empty() && !f.scene.ownedDrawRangeBytes && !f.scene.ownedBytes,
		"Retirement frees mixed range metadata and geometry budgets");
	std::puts("PASS: 17 mixed-height range/source/budget/publication/fence checks");
}

int main()
{
	TestMixedHeightRanges();
	{
		Fixture f; f.Create();
		const PsyXNativeSnapshot before=f.scene.snapshot;
		memset(f.snapshot.view.projection,0,sizeof(f.snapshot.view.projection));
		Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_INVALID &&
			!memcmp(&before,&f.scene.snapshot,sizeof(before)),"Singular native view rejects transactionally");
		Check(!f.scene.meshes[f.handle.slot].drawRanges[0].horizontalPlane,"Non-horizontal mesh retains raster depth");
		f.vertices[2].position[1]=0; f.vertices[2].position[2]=1; f.desc.boundsMax[2]=1; f.Create();
		Check(f.scene.meshes[f.handle.slot].drawRanges[0].horizontalPlane && f.scene.meshes[f.handle.slot].drawRanges[0].planeY==0,
			"Owned mesh derives an exact horizontal plane");
		f.vertices[0].position[1]=1;
		Check(f.scene.meshes[f.handle.slot].drawRanges[0].planeY==0,"Plane classification does not retain caller storage");
		f.scene.Destroy(f.handle); f.scene.Reclaim(f.handle.slot);
		Check(f.scene.meshes[f.handle.slot].drawRanges.empty(),
			"Retirement clears derived plane state");
		float view[16]={},projection[16]={};
		view[0]=view[15]=1; view[5]=view[10]=.8660254f; view[6]=.5f;view[9]=-.5f;
		view[13]=-6.9282032f; view[14]=-4;
		projection[0]=1;projection[5]=-1;projection[10]=-100.f/99;projection[11]=-1;projection[14]=-100.f/99;
		double inverse[16]; float plane[4],other[4];
		Check(PsyXNativePlane::Inverse(inverse,projection,view),"Invertible native clip transform");
		Check(PsyXNativePlane::Horizontal(plane,inverse,f.instance.world,0,1920,1080,0)==PsyXNativePlane::PlanarDepth,
			"Horizontal plane produces canonical window depth");
		f.instance.world[12]=7424;f.instance.world[14]=-219904;
		Check(PsyXNativePlane::Horizontal(other,inverse,f.instance.world,0,1920,1080,0)==PsyXNativePlane::PlanarDepth &&
			!memcmp(plane,other,sizeof(plane)),"Equal planes ignore distinct horizontal mesh origins");
		f.instance.world[13]=-2;
		Check(PsyXNativePlane::Horizontal(other,inverse,f.instance.world,2,1920,1080,0)==PsyXNativePlane::PlanarDepth &&
			!memcmp(plane,other,sizeof(plane)),"Different local heights with equal world height share exact coefficients");
		f.instance.world[13]=0;
		// Independent double projection of a known plane point checks the equation.
		double point[]={0,0,-16,1},eye[4]={},clip[4]={};
		for(unsigned int row=0;row<4;++row) for(unsigned int k=0;k<4;++k) eye[row]+=double(view[k*4+row])*point[k];
		for(unsigned int row=0;row<4;++row) for(unsigned int k=0;k<4;++k) clip[row]+=double(projection[k*4+row])*eye[k];
		const double x=(clip[0]/clip[3]+1)*960,y=(clip[1]/clip[3]+1)*540;
		Check(std::fabs(plane[0]*x+plane[1]*y+plane[2]-clip[2]/clip[3])<1e-7,"Canonical depth matches independent physical projection");
		f.instance.world[1]=.1f;
		Check(PsyXNativePlane::Horizontal(other,inverse,f.instance.world,0,1920,1080,1)==PsyXNativePlane::RasterDepth && other[3]<0,
			"Pitched instance retains ordinary native raster depth");
		f.instance.world[1]=0;
		Check(PsyXNativePlane::Horizontal(other,inverse,f.instance.world,0,0,1080,0)==PsyXNativePlane::Invalid,"Zero extent is an explicit invalid result");
		memset(view,0,sizeof(view));
		Check(!PsyXNativePlane::Inverse(inverse,projection,view),"Singular inverse cannot be consumed");
		memset(projection,0,sizeof(projection));
		for(unsigned int i=0;i<16;i+=5) view[i]=projection[i]=1;
		Check(PsyXNativePlane::Inverse(inverse,projection,view) &&
			PsyXNativePlane::Horizontal(other,inverse,f.instance.world,0,1920,1080,0)==PsyXNativePlane::RasterDepth,
			"Edge-on plane uses ordinary native raster depth");
		view[0]=std::numeric_limits<float>::quiet_NaN();
		Check(!PsyXNativePlane::Inverse(inverse,projection,view),"Nonfinite inverse cannot be consumed");
		std::puts("PASS: 15 canonical plane ownership/projection/error checks");
	}
	{
		// Actual Chicago coplanar grass placements, quantized view basis and
		// large camera translation. The three local points name one world point.
		const float view[]={.677246094f,-.0280761719f,.735351562f,0, 0,.999267578f,.0383300781f,0,
			.735839844f,.0261230469f,-.676513672f,0,156440.5f,5635.44922f,-154299.219f,1};
		float projection[16]={}; projection[0]=1.19999993f; projection[5]=-2.13333344f;
		projection[10]=-1.00007999f; projection[11]=-1; projection[14]=-10.0008001f;
		const float origins[][3]={{7313,17,-219336},{7424,17,-219904},{8337,17,-219336}};
		const float local[][4]={{-164,0,-512,1},{404,0,-401,1},{512,0,164,1}};
		const double sharedWorld[]={7825,17,-219500,1};
		double eye[4]={},expected[4]={};
		for (unsigned int row=0;row<4;++row) for(unsigned int k=0;k<4;++k) eye[row]+=double(view[k*4+row])*sharedWorld[k];
		for (unsigned int row=0;row<4;++row) for(unsigned int k=0;k<4;++k) expected[row]+=double(projection[k*4+row])*eye[k];
		for (unsigned int instance=0;instance<3;++instance)
		{
			float world[16]={},mvp[16]; world[5]=world[15]=1;
			if(instance<2) { world[2]=1;world[8]=-1; } else world[0]=world[10]=-1;
			for(unsigned int axis=0;axis<3;++axis) world[12+axis]=origins[instance][axis];
			PsyXNativeTransform::Compose(mvp,projection,view,world);
			for (unsigned int row=0;row<4;++row)
			{
				double clip=0; for(unsigned int k=0;k<4;++k) clip+=double(mvp[k*4+row])*local[instance][k];
				Check(std::fabs(clip-expected[row])<.0001,"Coplanar original placements agree with shared world point after projection");
			}
		}
		std::puts("PASS: 12 large-city native transform coordinates");
	}
	unsigned int cases = 0;
	{
		Fixture f; f.desc.depthLayer=PSYX_NATIVE_MAX_DEPTH_LAYER; f.Create();
		Check(f.scene.meshes[f.handle.slot].depthLayer==PSYX_NATIVE_MAX_DEPTH_LAYER,
			"C descriptor copies bounded native depth layer"); ++cases;
		f.desc.depthLayer=PSYX_NATIVE_MAX_DEPTH_LAYER+1;
		const uint64_t before=f.scene.ownedBytes;
		Check(f.scene.Create(&f.desc,&f.handle)==PSYX_NATIVE_INVALID && !f.handle.generation &&
			f.scene.ownedBytes==before,"Invalid depth layer cannot publish a handle or change ownership"); ++cases;
	}
	{
		Fixture f; f.Create(); ++cases;
		f.vertices[0].position[0] = 999; f.indices[0] = 999;
		Check(f.scene.meshes[f.handle.slot].vertices[0].position[0] == 0 &&
			f.scene.meshes[f.handle.slot].indices[0] == 0, "mesh source lifetime"); ++cases;
		Check(f.scene.Publish(&f.snapshot) == PSYX_NATIVE_OK, "valid pending resource snapshot");
		f.instance.world[12] = 999; f.snapshot.view.view[12] = 999;
		Check(f.scene.instances[0].world[12] == 0 && f.scene.snapshot.view.view[12] == 0 &&
			f.scene.snapshot.instances == f.scene.instances, "snapshot has no caller pointers"); ++cases;
		f.scene.meshes[f.handle.slot].state = PsyXNativeScene::Ready;
		f.scene.meshes[f.handle.slot].lastSubmission = 8;
		Check(f.scene.Destroy(f.handle) == PSYX_NATIVE_OK && !f.scene.IsLive(f.handle) &&
			!f.scene.CanReclaim(f.handle.slot, 7) && f.scene.CanReclaim(f.handle.slot, 8), "fence retirement and immediate invalidation"); ++cases;
		Check(f.scene.Publish(&f.snapshot) == PSYX_NATIVE_STALE && f.scene.snapshotPending,
			"rejected publish is atomic; recorder must revalidate destroyed handles"); ++cases;
		f.scene.Reclaim(f.handle.slot);
		Check(f.scene.ownedBytes == 0, "reclaim releases CPU budget");
		f.desc = NativeSceneCProbe(f.vertices, f.indices); f.vertices[0].position[0] = 0; f.indices[0] = 0;
		PsyXNativeMeshHandle replacement;
		Check(f.scene.Create(&f.desc, &replacement) == PSYX_NATIVE_PENDING && replacement.slot == f.handle.slot &&
			replacement.generation != f.handle.generation && !f.scene.IsLive(f.handle), "reused slot rejects stale generation"); ++cases;
	}
	{
		Fixture f; f.Create(); f.scene.Publish(&f.snapshot);
		uint64_t generation = f.scene.Reset();
		Check(generation != f.snapshot.sceneGeneration && !f.scene.snapshotPending && !f.scene.IsLive(f.handle) &&
			f.scene.Publish(&f.snapshot) == PSYX_NATIVE_STALE, "scene reset invalidates resources and snapshot"); ++cases;
	}
	{
		Fixture f; f.Create(); f.scene.Publish(&f.snapshot); f.scene.SetRequested(0);
		Check(!f.scene.snapshotPending, "disable cannot replay an earlier world in frontend"); ++cases;
	}
	{
		Fixture f; f.desc.size--; Check(f.scene.Create(&f.desc, &f.handle) == PSYX_NATIVE_INVALID &&
			!f.handle.generation && f.scene.ownedBytes == 0, "invalid descriptor does not publish handle");
		f.desc.size++; f.indices[2] = 3;
		Check(f.scene.Create(&f.desc, &f.handle) == PSYX_NATIVE_INVALID, "index out of vertex range"); ++cases;
		f.indices[2] = 2; f.vertices[0].uv[0] = std::numeric_limits<float>::quiet_NaN();
		Check(f.scene.Create(&f.desc, &f.handle) == PSYX_NATIVE_INVALID, "nonfinite attribute rejected"); ++cases;
		f.vertices[0].uv[0] = 0; f.desc.boundsMax[0] = 0;
		Check(f.scene.Create(&f.desc, &f.handle) == PSYX_NATIVE_INVALID, "bounds must contain every vertex"); ++cases;
	}
	{
		Fixture f; f.desc.vertexCount = std::numeric_limits<uint32_t>::max();
		Check(f.scene.Create(&f.desc, &f.handle) == PSYX_NATIVE_OUT_OF_BUDGET && f.scene.ownedBytes == 0,
			"large spans rejected before dereference; no arithmetic overflow"); ++cases;
	}
	{
		Fixture f; f.Create(); f.instance.world[3] = 1;
		Check(f.scene.Publish(&f.snapshot) == PSYX_NATIVE_INVALID && !f.scene.snapshotPending,
			"nonaffine transform rejected"); f.instance.world[3] = 0; f.instance.tint[3] = 0.5f;
		Check(f.scene.Publish(&f.snapshot) == PSYX_NATIVE_INVALID, "unsupported transparency is explicit"); ++cases;
		f.instance.tint[3] = 1; f.snapshot.view.nearPlane = f.snapshot.view.farPlane;
		Check(f.scene.Publish(&f.snapshot) == PSYX_NATIVE_INVALID, "invalid clip interval rejected"); ++cases;
	}
	{
		Fixture f; f.Create(); f.instance.layer = PSYX_NATIVE_BACKDROP;
		Check(f.scene.Publish(&f.snapshot) == PSYX_NATIVE_OK && f.scene.instances[0].layer == PSYX_NATIVE_BACKDROP,
			"copied backdrop policy independent of scene batching"); ++cases;
		f.instance.layer = static_cast<PsyXNativeLayer>(2);
		Check(f.scene.Publish(&f.snapshot) == PSYX_NATIVE_INVALID && f.scene.instances[0].layer == PSYX_NATIVE_BACKDROP,
			"unknown draw layer rejects publication atomically"); ++cases;
	}
	{
		Fixture f; for (uint32_t i = 0; i < PSYX_NATIVE_MAX_MESHES; ++i) f.Create();
		Check(f.scene.Create(&f.desc, &f.handle) == PSYX_NATIVE_OUT_OF_BUDGET && !f.handle.generation,
			"slot limit cannot overwrite live resource"); ++cases;
	}
	{
		Fixture f; f.Create(); f.scene.meshes[f.handle.slot].generation = std::numeric_limits<uint32_t>::max();
		f.handle.generation = std::numeric_limits<uint32_t>::max(); f.scene.Destroy(f.handle); f.scene.Reclaim(f.handle.slot);
		f.Create(); Check(f.handle.slot != 0 && f.scene.meshes[0].state == PsyXNativeScene::Exhausted,
			"mesh generation exhaustion never wraps"); ++cases;
		f.scene.generation = std::numeric_limits<uint64_t>::max(); f.scene.Reset();
		Check(f.scene.Create(&f.desc, &f.handle) == PSYX_NATIVE_OUT_OF_BUDGET,
			"scene generation exhaustion never wraps"); ++cases;
	}
	{
		Fixture f; f.Create(); f.scene.meshes[f.handle.slot].state = PsyXNativeScene::Ready;
		PsyXNativeStats stats; f.scene.GetStats(&stats);
		Check(stats.residentMeshes == 1 && stats.pendingMeshes == 0 && stats.ownedBytes == 3 * sizeof(PsyXNativeVertex) + 12 &&
			stats.nativeDraws == 0 && stats.effective == 0, "CPU ownership is not claimed as GPU submission"); ++cases;
	}
	std::printf("PASS: %u native ownership/C ABI cases\n", cases);
	unsigned int cameraCases = 0;
	const float identity[] = {1,0,0, 0,1,0, 0,0,1};
	const float origin[] = {0,0,0};
	PsyXNativeView view = {};
	Check(DeveloperNativeCamera_Build(&view, identity, origin, 256, 320, 240, 1280, 720), "valid final camera adapter");
	Check(view.view[0] == 1 && view.view[5] == 1 && view.view[10] == -1 &&
		view.projection[5] < 0 && view.projection[11] == -1, "Y-up basis, -Z and one viewport Y flip"); ++cameraCases;
	Check(std::fabs(ProjectDepth(view, 10)) < 0.00001f && std::fabs(ProjectDepth(view, 125000) - 1) < 0.000001f &&
		ProjectDepth(view, 5) < 0 && ProjectDepth(view, 200000) > 1, "Vulkan clip depth near/far and outside planes"); ++cameraCases;
	PsyXNativeView wide = {};
	Check(DeveloperNativeCamera_Build(&wide, identity, origin, 128, 320, 240, 1280, 720) &&
		wide.projection[0] == view.projection[0] * 0.5f && wide.projection[5] == view.projection[5] * 0.5f,
		"focal length is not degrees; wider FOV preserves depth"); ++cameraCases;
	PsyXNativeView resized = {};
	Check(DeveloperNativeCamera_Build(&resized, identity, origin, 256, 320, 240, 800, 600) &&
		std::fabs(resized.projection[0] - 1.6f) < 0.000001f && resized.projection[5] == view.projection[5],
		"window aspect correction is independent of logical vertical focal"); ++cameraCases;
	PsyXNativeView pal = {};
	Check(DeveloperNativeCamera_Build(&pal, identity, origin, 256, 320, 256, 1280, 720) &&
		pal.projection[0] == view.projection[0] && pal.projection[5] == -2,
		"PAL logical height follows source display while fixed pixel aspect is retained"); ++cameraCases;
	const float translated[] = {200,-50,400};
	PsyXNativeView moved = {};
	Check(DeveloperNativeCamera_Build(&moved, identity, translated, 256, 320, 240, 1280, 720), "translated camera build");
	Check(moved.cameraPosition[1] == 50 && moved.view[12] == -200 && moved.view[13] == -50 && moved.view[14] == 400,
		"camera translation and world reflection use the same basis"); ++cameraCases;
	const float yaw[] = {0,0,-1, 0,1,0, 1,0,0};
	Check(DeveloperNativeCamera_Build(&moved, yaw, translated, 256, 320, 240, 1280, 720) &&
		moved.view[8] == -1 && moved.view[2] == -1 && moved.view[12] == 400 && moved.view[14] == 200,
		"ninety-degree camera rotates native world without Euler reinterpretation"); ++cameraCases;
	float empty[9] = {};
	Check(!DeveloperNativeCamera_Build(&moved, empty, origin, 256, 320, 240, 1280, 720) &&
		!DeveloperNativeCamera_Build(&moved, identity, origin, 256, 320, 240, 0, 0) &&
		!DeveloperNativeCamera_Build(&moved, identity, origin, std::numeric_limits<float>::max(), 320, 240, 1280, 720),
		"invalid/minimized/overflow camera inputs rejected"); ++cameraCases;
	float backdrop[16]={};
	Check(DeveloperNativeCamera_BuildBackdropTransform(backdrop,moved,-17),"Rotated backdrop transform builds");
	for (unsigned int row=0; row<3; ++row)
	{
		float projected=0;
		for (unsigned int column=0; column<3; ++column) projected+=moved.view[column*4+row]*backdrop[12+column];
		Check(std::fabs(projected-(row==1 ? 17.0f : 0.0f))<.001f,"Backdrop retains exact view-space sky offset");
	} ++cameraCases;
	float savedBackdrop[16]; memcpy(savedBackdrop,backdrop,sizeof(backdrop));
	moved.view[12]=100000; moved.view[13]=-9000; moved.view[14]=800000;
	Check(DeveloperNativeCamera_BuildBackdropTransform(backdrop,moved,-17) && !memcmp(savedBackdrop,backdrop,sizeof(backdrop)),
		"Backdrop offset is independent of camera world position"); ++cameraCases;
	moved.view[0]=.9999f; moved.view[4]=.0002f; // Quantized, slightly nonorthogonal basis.
	Check(DeveloperNativeCamera_BuildBackdropTransform(backdrop,moved,-17),"Quantized backdrop basis uses inverse rather than transpose");
	for (unsigned int row=0; row<3; ++row)
	{
		float projected=0;
		for (unsigned int column=0; column<3; ++column) projected+=moved.view[column*4+row]*backdrop[12+column];
		Check(std::fabs(projected-(row==1 ? 17.0f : 0.0f))<.001f,"Quantized sky offset round trip");
	} ++cameraCases;
	PsyXNativeView singular={};
	Check(!DeveloperNativeCamera_BuildBackdropTransform(backdrop,singular,-17) &&
		!DeveloperNativeCamera_BuildBackdropTransform(backdrop,view,std::numeric_limits<float>::infinity()),
		"Singular or nonfinite backdrop inputs reject"); ++cameraCases;
	std::printf("PASS: %u native camera/clip cases\n", cameraCases);
	unsigned int materialCases = 0;
	{
		Fixture f;
		uint8_t pixels[] = {255,0,0,128, 0,0,0,255};
		PsyXNativeMaterialDesc desc = {};
		desc.size=sizeof(desc); desc.version=PSYX_NATIVE_VERSION;
		desc.width=2; desc.height=1; desc.byteCount=sizeof(pixels);
		desc.rgba=pixels; desc.filter=PSYX_NATIVE_LINEAR; desc.alphaCutoff=0.5f;
		PsyXNativeMaterialHandle material = {};
		Check(f.scene.CreateMaterial(&desc,&material)==PSYX_NATIVE_PENDING && material.generation,
			"owned material create is explicitly pending"); ++materialCases;
		pixels[0]=0;
		Check(f.scene.materials[material.slot].rgba[0]==255 && f.scene.ownedMaterialBytes==8,
			"material pixels copied and charged once"); ++materialCases;
		f.desc.material=material; f.Create();
		Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_OK, "mesh snapshot references owned pending material");
		f.scene.materials[material.slot].lastSubmission=7;
		Check(f.scene.DestroyMaterial(material)==PSYX_NATIVE_OK && !f.scene.IsMaterialLive(material) &&
			!f.scene.CanReclaimMaterial(material.slot,6) && f.scene.CanReclaimMaterial(material.slot,7),
			"material invalidates immediately and retires after last GPU use"); ++materialCases;
		Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_STALE, "destroyed material rejects the entire mesh snapshot"); ++materialCases;
		f.scene.ReclaimMaterial(material.slot);
		PsyXNativeMaterialHandle replacement = {};
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_PENDING && replacement.generation!=material.generation &&
			!f.scene.IsMaterialLive(material) && f.scene.Publish(&f.snapshot)==PSYX_NATIVE_STALE,
			"material slot reuse cannot redirect an old mesh"); ++materialCases;
		f.scene.Reset();
		Check(!f.scene.IsMaterialLive(replacement) && f.scene.CanReclaimMaterial(replacement.slot,0),
			"scene reset invalidates pending material"); ++materialCases;
		f.scene.ReclaimMaterial(replacement.slot);
		Check(f.scene.ownedMaterialBytes==0, "retirement returns material byte budget");
		desc.byteCount=7;
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_INVALID && !replacement.generation,
			"actual material byte span must match widened dimensions"); ++materialCases;
		desc.byteCount=8; desc.filter=(PsyXNativeFilter)3;
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_INVALID, "unknown sampler is explicit");
		desc.filter=PSYX_NATIVE_NEAREST; desc.cull=(PsyXNativeCull)2;
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_INVALID, "unknown culling policy is explicit"); ++materialCases;
		desc.cull=PSYX_NATIVE_CULL_NONE;
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_PENDING && f.scene.materials[replacement.slot].cull==PSYX_NATIVE_CULL_NONE,
			"two-sided material policy copied independently of texture alpha"); ++materialCases;
		f.scene.DestroyMaterial(replacement); f.scene.ReclaimMaterial(replacement.slot);
		desc.cull=PSYX_NATIVE_CULL_BACK;
		desc.filter=PSYX_NATIVE_NEAREST; desc.alphaCutoff=std::numeric_limits<float>::quiet_NaN();
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_INVALID, "nonfinite cutoff rejected"); ++materialCases;
		desc.alphaCutoff=0.5f;
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_PENDING, "small material occupies aggregate budget");
		const PsyXNativeMaterialHandle budgetOccupant = replacement;
		desc.width=4096; desc.height=4096; desc.byteCount=UINT64_C(4096)*4096*4;
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_OUT_OF_BUDGET && f.scene.ownedMaterialBytes==8,
			"oversize material budget checked before pixel traversal"); ++materialCases;
		f.scene.DestroyMaterial(budgetOccupant); f.scene.ReclaimMaterial(budgetOccupant.slot);
		desc.width=2; desc.height=1; desc.byteCount=8;
		for (unsigned int i=0;i<PSYX_NATIVE_MAX_MATERIALS;++i)
			Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_PENDING, "bounded material slot creation");
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_OUT_OF_BUDGET && !replacement.generation,
			"full material table cannot overwrite live resources"); ++materialCases;
		f.scene.Reset();
		for (unsigned int i=0;i<PSYX_NATIVE_MAX_MATERIALS;++i) f.scene.ReclaimMaterial(i);
		f.scene.materials[0].generation=std::numeric_limits<uint32_t>::max();
		f.scene.materials[0].state=PsyXNativeScene::Retiring;
		f.scene.ReclaimMaterial(0);
		Check(f.scene.CreateMaterial(&desc,&replacement)==PSYX_NATIVE_PENDING && replacement.slot!=0 &&
			f.scene.materials[0].state==PsyXNativeScene::Exhausted, "material generation never wraps"); ++materialCases;
	}
	std::printf("PASS: %u native material ownership cases\n",materialCases);
	unsigned int mipCases=0;
	{
		std::vector<PsyXNativeMip::Level> levels;
		std::vector<uint8_t> pixels;
		uint64_t bytes=PsyXNativeMip::Layout(2,2,true,levels);
		const uint8_t checker[]={0,0,0,255,255,255,255,255,255,255,255,255,0,0,0,255};
		PsyXNativeMip::Build(checker,levels,bytes,.5f,pixels);
		Check(bytes==20 && levels.size()==2 && levels[1].offset==16 && !memcmp(pixels.data(),checker,16),
			"mip ownership includes byte-exact base and lower level"); ++mipCases;
		Check(pixels[16]==188 && pixels[17]==188 && pixels[18]==188 && pixels[19]==255,
			"mip averaging happens in linear light, not sRGB bytes"); ++mipCases;
		const uint8_t halo[]={255,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255};
		PsyXNativeMip::Build(halo,levels,bytes,.5f,pixels);
		Check(pixels[16]==255 && pixels[17]==255 && pixels[18]==255,
			"transparent source color cannot bleed into opaque mip edges"); ++mipCases;
		const uint8_t empty[]={255,0,0,0,255,0,0,0,255,0,0,0,255,0,0,0};
		PsyXNativeMip::Build(empty,levels,bytes,.5f,pixels);
		Check(pixels[16]==0 && pixels[17]==0 && pixels[18]==0 && pixels[19]==0,
			"fully transparent mip remains a hole with zero RGB"); ++mipCases;
		bytes=PsyXNativeMip::Layout(4,4,true,levels);
		std::vector<uint8_t> cutout(64,255);
		const unsigned int counts[]={3,2,1,0};
		for (unsigned int block=0;block<4;++block)
		for (unsigned int texel=0;texel<4;++texel)
		{
			const unsigned int x=(block%2)*2+texel%2, y=(block/2)*2+texel/2;
			cutout[(y*4+x)*4+3]=texel<counts[block] ? 255 : 0;
		}
		PsyXNativeMip::Build(cutout.data(),levels,bytes,.8f,pixels);
		unsigned int mipCoverage=0;
		for (unsigned int i=0;i<4;++i) if (pixels[64+i*4+3]/255.0f>=.8f) ++mipCoverage;
		Check(mipCoverage==1 && pixels[67]==204 && pixels[79]==0,
			"cutout scaling uses shader cutoff and closest attainable coverage without filling holes"); ++mipCases;
		bytes=PsyXNativeMip::Layout(5,3,true,levels);
		std::vector<uint8_t> odd(60,0);
		for (size_t i=0;i<15;++i) odd[i*4+3]=255;
		odd[56]=odd[57]=odd[58]=255;
		PsyXNativeMip::Build(odd.data(),levels,bytes,.5f,pixels);
		Check(bytes==72 && levels.size()==3 && levels[1].width==2 && levels[1].height==1 &&
			pixels[60]==0 && pixels[64]==102 && pixels[68]==73,
			"NPOT area footprints include the last row and column"); ++mipCases;
		for (unsigned int orientation=0;orientation<2;++orientation)
		{
			bytes=PsyXNativeMip::Layout(orientation ? 4096 : 1,orientation ? 1 : 4096,true,levels);
			std::vector<uint8_t> thin(4096*4,255);
			PsyXNativeMip::Build(thin.data(),levels,bytes,1.0f,pixels);
			Check(bytes==32764 && levels.size()==13 && levels.back().width==1 && levels.back().height==1 &&
				pixels.back()==255,"thin texture chain never has a zero dimension"); ++mipCases;
		}
		bytes=PsyXNativeMip::Layout(1,1,true,levels);
		Check(bytes==4 && levels.size()==1,"one-texel mip chain has one level"); ++mipCases;
		Check(!PsyXNativeMip::Layout(0,1,true,levels) && levels.empty() &&
			!PsyXNativeMip::Layout(4097,1,true,levels),"invalid layout cannot traverse caller pixels"); ++mipCases;
		Fixture f;
		PsyXNativeMaterialDesc desc={};
		desc.size=sizeof(desc); desc.version=PSYX_NATIVE_VERSION; desc.filter=PSYX_NATIVE_TRILINEAR;
		desc.width=desc.height=2; desc.rgba=checker; desc.byteCount=16; desc.alphaCutoff=.5f;
		PsyXNativeMaterialHandle material={};
		Check(f.scene.CreateMaterial(&desc,&material)==PSYX_NATIVE_PENDING &&
			f.scene.ownedMaterialBytes==20 && f.scene.materials[material.slot].levels.size()==2,
			"trilinear material charges all mip bytes before GPU upload"); ++mipCases;
		f.scene.DestroyMaterial(material); f.scene.ReclaimMaterial(material.slot);
		Check(!f.scene.ownedMaterialBytes && f.scene.materials[material.slot].levels.empty(),
			"retirement releases the full chain and its level metadata"); ++mipCases;
		desc.width=desc.height=4096; desc.byteCount=UINT64_C(4096)*4096*4;
		Check(f.scene.CreateMaterial(&desc,&material)==PSYX_NATIVE_OUT_OF_BUDGET && !material.generation && !f.scene.ownedMaterialBytes,
			"derived mip bytes reject a base that alone fits the cap before reading it"); ++mipCases;
	}
	std::printf("PASS: %u native mip ownership/filter cases\n",mipCases);
	unsigned int samplingCases=0;
	{
		Fixture f;
		const uint8_t pixels[]={255,255,255,255,0,0,0,255};
		PsyXNativeMaterialDesc material={};
		material.size=sizeof(material); material.version=PSYX_NATIVE_VERSION;
		material.width=2; material.height=1; material.byteCount=8; material.rgba=pixels;
		material.filter=PSYX_NATIVE_TRILINEAR;
		Check(f.scene.CreateMaterial(&material,&f.desc.material)==PSYX_NATIVE_PENDING,"Sampling fixture owns full chain");
		f.Create();
		PsyXNativeInstance pair[]={f.instance,f.instance};
		pair[0].sampling=PSYX_NATIVE_SAMPLE_NEAREST; pair[1].sampling=NativeSamplingCProbe();
		f.snapshot.instances=pair; f.snapshot.instanceCount=2;
		Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_OK && f.scene.instances[0].sampling==PSYX_NATIVE_SAMPLE_NEAREST &&
			f.scene.instances[1].sampling==PSYX_NATIVE_SAMPLE_TRILINEAR && f.scene.ownedMaterialBytes==12,
			"Same material supports independent per-instance sampling/C enum ABI"); ++samplingCases;
		pair[0].sampling=PSYX_NATIVE_SAMPLE_LINEAR;
		Check(f.scene.instances[0].sampling==PSYX_NATIVE_SAMPLE_NEAREST,
			"Sampling is copied with snapshot rather than borrowed producer state"); ++samplingCases;
		for (unsigned int mode=0;mode<=3;++mode)
		{
			pair[0].sampling=PsyXNativeSampling(mode);
			Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_OK && f.scene.IsLive(f.handle) &&
				f.scene.ownedMaterialBytes==12,"Every sampler choice retains owned mesh/image identity"); ++samplingCases;
		}
		pair[1].sampling=PsyXNativeSampling(4);
		Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_INVALID && f.scene.snapshot.instanceCount==2 &&
			f.scene.instances[1].sampling==PSYX_NATIVE_SAMPLE_TRILINEAR,
			"Unknown sampling rejects atomically without corrupting the valid snapshot"); ++samplingCases;
		Fixture base;
		material.filter=PSYX_NATIVE_NEAREST;
		Check(base.scene.CreateMaterial(&material,&base.desc.material)==PSYX_NATIVE_PENDING,"Base-only sampling fixture");
		base.Create();
		Check(base.scene.Publish(&base.snapshot)==PSYX_NATIVE_OK,"Default material sampling remains valid");
		base.instance.sampling=PSYX_NATIVE_SAMPLE_TRILINEAR;
		Check(base.scene.Publish(&base.snapshot)==PSYX_NATIVE_UNSUPPORTED && base.scene.instances[0].sampling==PSYX_NATIVE_USE_MATERIAL_FILTER,
			"Trilinear override cannot silently sample an incomplete base-only chain"); ++samplingCases;
	}
	std::printf("PASS: %u native sampling snapshot cases\n",samplingCases);
	unsigned int pickCases=0;
	{
		const PsyXNativePickResult abi=NativePickCProbe();
		Check(abi.size==sizeof(abi) && abi.version==PSYX_NATIVE_VERSION && abi.identity==UINT64_C(0x100000065) && abi.reason==PSYX_NATIVE_PENDING,
			"Native pick result crosses C/C++ ABI with full 64-bit identity"); ++pickCases;
	}
	{
		Fixture f; f.Create();
		PsyXNativePick pick;
		PsyXNativePickResult result={}; uint32_t x=0,y=0;
		Check(pick.Get(f.scene,&result)==PSYX_NATIVE_NO_SNAPSHOT && result.version==PSYX_NATIVE_VERSION && !result.hit,
			"No invented selection before a pick"); ++pickCases;
		Check(pick.Request(-1,0,640,480)==PSYX_NATIVE_INVALID && !pick.Pending(),"Reject outside window atomically"); ++pickCases;
		Check(pick.Request(320,240,640,480)==PSYX_NATIVE_OK && pick.Get(f.scene,&result)==PSYX_NATIVE_PENDING,
			"Asynchronous pick request"); ++pickCases;
		Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_OK && pick.Capture(f.scene,640,480,1280,960,x,y) && x==640 && y==480,
			"Window to drawable scaling keeps top origin"); ++pickCases;
		pick.Submitted(17);
		f.instance.identity=999; f.snapshot.simulationTick=100;
		Check(f.scene.Publish(&f.snapshot)==PSYX_NATIVE_OK,"Later snapshot publish");
		pick.Complete(1,f.scene);
		Check(pick.Get(f.scene,&result)==PSYX_NATIVE_OK && result.hit && result.identity==42 &&
			result.simulationTick==60 && result.submittedSerial==17 && result.mesh.generation==f.handle.generation,
			"Result comes from submitted frame, not newer snapshot"); ++pickCases;
		Check(pick.Request(639,479,640,480)==PSYX_NATIVE_OK && pick.Capture(f.scene,640,480,1280,960,x,y) && x==1278 && y==958,
			"Last window pixel scales without crossing the drawable boundary"); ++pickCases;
		pick.Submitted(18); pick.Complete(0,f.scene);
		Check(pick.Get(f.scene,&result)==PSYX_NATIVE_OK && !result.hit && !result.identity,"Zero ID is an explicit miss"); ++pickCases;
		pick.Request(0,0,640,480); pick.Capture(f.scene,640,480,640,480,x,y); pick.Submitted(19);
		pick.Request(1,1,640,480); pick.Complete(1,f.scene);
		Check(pick.Get(f.scene,&result)==PSYX_NATIVE_PENDING && pick.Pending(),"New request supersedes in-flight result"); ++pickCases;
		pick.Capture(f.scene,640,480,640,480,x,y); pick.Submitted(20); pick.Cancel(); pick.Complete(1,f.scene);
		Check(pick.Get(f.scene,&result)==PSYX_NATIVE_NO_SNAPSHOT,"Cancel cannot be undone by fence completion"); ++pickCases;
		pick.Request(0,0,640,480);
		Check(!pick.Capture(f.scene,800,600,800,600,x,y) && pick.Get(f.scene,&result)==PSYX_NATIVE_STALE,
			"Resize rejects the old window request"); ++pickCases;
		pick.Request(0,0,640,480); pick.Capture(f.scene,640,480,640,480,x,y); pick.Submitted(21); pick.Complete(2,f.scene);
		Check(pick.Get(f.scene,&result)==PSYX_NATIVE_INVALID && !result.hit,"Unknown GPU ID cannot index beyond bounded lookup"); ++pickCases;
		f.instance.layer=PSYX_NATIVE_BACKDROP; f.scene.Publish(&f.snapshot);
		pick.Request(0,0,640,480); pick.Capture(f.scene,640,480,640,480,x,y); pick.Submitted(22); pick.Complete(1,f.scene);
		Check(pick.Get(f.scene,&result)==PSYX_NATIVE_INVALID,"Backdrop cannot become a world selection"); ++pickCases;
		f.instance.layer=PSYX_NATIVE_WORLD; f.scene.Publish(&f.snapshot);
		pick.Request(0,0,640,480); pick.Capture(f.scene,640,480,640,480,x,y); pick.Submitted(23);
		f.scene.Destroy(f.handle); pick.Complete(1,f.scene);
		Check(pick.Get(f.scene,&result)==PSYX_NATIVE_STALE && !result.hit,"Retired handle invalidates in-flight pick"); ++pickCases;
		Fixture retained; retained.Create(); retained.scene.Publish(&retained.snapshot);
		pick.Request(0,0,640,480); pick.Capture(retained.scene,640,480,640,480,x,y); pick.Submitted(24); pick.Complete(1,retained.scene);
		retained.scene.Destroy(retained.handle);
		Check(pick.Get(retained.scene,&result)==PSYX_NATIVE_STALE && !result.hit,"Completed selection detects later resource retirement"); ++pickCases;
		retained.scene.Reclaim(retained.handle.slot); retained.Create(); retained.scene.Publish(&retained.snapshot);
		pick.Request(0,0,640,480); pick.Capture(retained.scene,640,480,640,480,x,y); pick.Submitted(25);
		retained.scene.Reset(); pick.Complete(1,retained.scene);
		Check(pick.Get(retained.scene,&result)==PSYX_NATIVE_STALE,"Scene reset rejects the captured generation"); ++pickCases;
		pick.Request(0,0,640,480); pick.Fail(PSYX_NATIVE_UPLOAD_FAILED);
		Check(pick.Get(retained.scene,&result)==PSYX_NATIVE_UPLOAD_FAILED && !pick.Pending(),"Failed submission never becomes a valid pick"); ++pickCases;
		Fixture textured;
		const uint8_t rgba[]={255,255,255,255};
		PsyXNativeMaterialDesc mat={}; mat.size=sizeof(mat); mat.version=PSYX_NATIVE_VERSION;
		mat.rgba=rgba; mat.width=mat.height=1; mat.byteCount=4;
		Check(textured.scene.CreateMaterial(&mat,&textured.desc.material)==PSYX_NATIVE_PENDING,"Pick material fixture");
		textured.Create(); textured.scene.Publish(&textured.snapshot);
		pick.Request(0,0,640,480); pick.Capture(textured.scene,640,480,640,480,x,y); pick.Submitted(26); pick.Complete(1,textured.scene);
		Check(pick.Get(textured.scene,&result)==PSYX_NATIVE_OK && result.hit && result.material.generation==textured.desc.material.generation,
			"Picking retains the exact material generation"); ++pickCases;
		textured.scene.DestroyMaterial(textured.desc.material);
		Check(pick.Get(textured.scene,&result)==PSYX_NATIVE_STALE && !result.hit,"Retired artwork cannot silently retarget a completed pick"); ++pickCases;
	}
	std::printf("PASS: %u native picking lifetime/coordinate cases\n",pickCases);
	unsigned int frameCases=0;
	{
		Fixture f;
		Check(f.scene.FrameState(PSYX_NATIVE_OK,false)==PSYX_NATIVE_FRAME_OFF,"Native off is explicit"); ++frameCases;
		f.scene.SetRequested(1);
		Check(f.scene.FrameState(PSYX_NATIVE_OK,false)==PSYX_NATIVE_FRAME_AUXILIARY,"Remaining frontend consumer is not a ready world"); ++frameCases;
		Check(f.scene.FrameState(PSYX_NATIVE_NO_SNAPSHOT,true)==PSYX_NATIVE_FRAME_FAILED,"World boundary without publication is an error"); ++frameCases;
		f.scene.SetFrameStatus(PSYX_NATIVE_PENDING);
		Check(f.scene.worldExpected && f.scene.producerReason==PSYX_NATIVE_PENDING &&
			f.scene.FrameState(PSYX_NATIVE_PENDING,false)==PSYX_NATIVE_FRAME_LOADING,"Explicit source loading requires no legacy boundary"); ++frameCases;
		f.Create(); f.scene.Publish(&f.snapshot);
		Check(f.scene.producerReason==PSYX_NATIVE_OK && f.scene.FrameState(PSYX_NATIVE_OK,true)==PSYX_NATIVE_FRAME_READY,
			"Successful native publication replaces loading"); ++frameCases;
		const PsyXNativeResult failures[]={PSYX_NATIVE_INVALID,PSYX_NATIVE_STALE,PSYX_NATIVE_UNSUPPORTED,
			PSYX_NATIVE_OUT_OF_BUDGET,PSYX_NATIVE_UPLOAD_FAILED,PSYX_NATIVE_NO_SNAPSHOT,PSYX_NATIVE_NO_BOUNDARY};
		for (unsigned int i=0;i<sizeof(failures)/sizeof(failures[0]);++i)
		{
			f.scene.Publish(&f.snapshot); f.scene.SetFrameStatus(failures[i]);
			Check(!f.scene.snapshotPending && !f.scene.snapshot.instanceCount &&
				f.scene.FrameState(failures[i],false)==PSYX_NATIVE_FRAME_FAILED,"Failure discards older publication, never selects legacy recovery"); ++frameCases;
		}
		f.scene.SetFrameStatus(PSYX_NATIVE_PENDING); f.scene.ConsumeSnapshot();
		Check(!f.scene.worldExpected && f.scene.producerReason==PSYX_NATIVE_NO_SNAPSHOT &&
			f.scene.FrameState(PSYX_NATIVE_NO_SNAPSHOT,true)==PSYX_NATIVE_FRAME_FAILED,"Loading cannot leak to an undeclared later world frame"); ++frameCases;
	}
	std::printf("PASS: %u native frame state/failure cases\n",frameCases);
	return 0;
}
