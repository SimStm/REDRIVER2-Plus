#include "NativeRenderer.h"

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "EngineFactoryVk.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "NativeWindow.h"
#include "DebugOutput.h"
#include "RefCntAutoPtr.hpp"

#include "../utils/GltfLoader.h"
#include "../utils/NativeCameraView.h"

#include <chrono>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace Diligent;

namespace
{
// Diligent reports the precise reason a swap chain fails through this callback;
// route it to a file so an in-game probe can be diagnosed from the log.
void DiligentMessage(DEBUG_MESSAGE_SEVERITY severity, const char* message,
	const char* function, const char* file, int line)
{
	FILE* log = fopen("native_renderer.log", "a");
	if (log)
	{
		fprintf(log, "Diligent[%d] %s (%s:%d)\n", (int)severity, message ? message : "",
			file ? file : "?", line);
		fclose(log);
	}
}

IEngineFactoryVk* g_factory = nullptr;
RefCntAutoPtr<IRenderDevice> g_device;
RefCntAutoPtr<IDeviceContext> g_context;
RefCntAutoPtr<ISwapChain> g_swapChain;
int g_active = 0;
Uint32 g_syncInterval = 1;
int g_bilinearFiltering = 0;
NativeFrameStats g_frameStats = {};
NativeOverlayHandler g_overlayHandler = nullptr;
unsigned int g_frameDrawCalls = 0;
unsigned int g_frameCulledInstances = 0;

double FrameMilliseconds()
{
	return std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}


void SetError(char* error, int capacity, const char* message)
{
	if (error && capacity > 0)
		snprintf(error, capacity, "%s", message);
}

// Appends one dev-only diagnostic line to the same log the Diligent message
// callback uses. A diagnostic never changes the render result and is never
// fatal; it only records that a diagnosis toggle is active.
void LogInitDiagnostic(const char* message)
{
	FILE* log = fopen("native_renderer.log", "a");
	if (!log)
		return;
	fprintf(log, "native renderer: %s\n", message);
	fclose(log);
}

// Reads a dev-only boolean environment toggle: only the exact value "1" enables
// it, so an unset or any other value is the production default.
int ReadEnvToggle(const char* name)
{
	const char* value = getenv(name);
	return value && strcmp(value, "1") == 0;
}

// P7 diagnosis toggles, read exactly once on the first pipeline build. They are
// deliberately dev-only and process-wide:
//   * noCutout: the scene pixel shader keeps an alpha==0 texel instead of
//     discarding it (RED2_NATIVE_NO_CUTOUT=1). It discriminates "the cutout
//     removes texels the legacy renders" from missing geometry.
//   * noDepth: the scene pipelines disable the depth test AND write
//     (RED2_NATIVE_NO_DEPTH=1), so every primitive draws regardless of order.
//     It discriminates "depth hides geometry that should show" from missing
//     geometry.
// Both default off, so the scene pipeline is byte-for-byte the production one
// when the environment is unset.
struct SceneDiagnostics
{
	int loaded;
	int noCutout;
	int noDepth;
};

SceneDiagnostics g_sceneDiagnostics = { 0, 0, 0 };

const SceneDiagnostics& GetSceneDiagnostics()
{
	if (!g_sceneDiagnostics.loaded)
	{
		g_sceneDiagnostics.loaded = 1;
		g_sceneDiagnostics.noCutout = ReadEnvToggle("RED2_NATIVE_NO_CUTOUT");
		g_sceneDiagnostics.noDepth = ReadEnvToggle("RED2_NATIVE_NO_DEPTH");
	}
	return g_sceneDiagnostics;
}

// ---------------------------------------------------------------------------
// Scene: a camera-driven, depth-tested, textured draw. Column-major matrices
// throughout; the shader uses mul(MVP, float4(pos, 1)). Converted E02 content
// replaces the placeholder scene while it is loaded; both use the same
// pipeline layout and draw path.
// ---------------------------------------------------------------------------

struct SceneVertex
{
	float x, y, z, u, v;
	float r, g, b, a;
};

// 24 floats / 96 bytes: MVP, the material base-colour factor and the legacy
// vertex-colour scale. HLSL packs the float4x4 into the first 64 bytes and each
// float4 into the next 16.
struct SceneConstants
{
	float mvp[16];
	float baseColor[4];
	float colorScale[4];
};

// One drawable primitive: geometry, texture, bindings and its own base-colour
// factor. A converted multi-primitive GLB yields one of these per primitive; the
// built-in placeholder has exactly one. Each primitive owns its immutable GPU
// buffers and its bindings for both scene pipelines.
struct ScenePrimitive
{
	RefCntAutoPtr<IBuffer> vertices;
	RefCntAutoPtr<IBuffer> indices;	// null -> non-indexed draw
	RefCntAutoPtr<ITexture> texture;
	RefCntAutoPtr<ISampler> sampler;
	RefCntAutoPtr<IShaderResourceBinding> srb;			// swap-chain pipeline
	RefCntAutoPtr<IShaderResourceBinding> srbOffscreen;	// RGBA8 offscreen pipeline
	Uint32 vertexCount;
	Uint32 indexCount;
	int triangleCount;
	float baseColor[4];
	// Legacy vertex-colour scale: the `bright` multiplier the legacy GPU packed
	// per primitive (PsyX_GPU.cpp MakeTexcoordQuad etc.). Textured primitives use
	// 2; untextured use 1. Applied to the vertex colour in float in the vertex
	// shader, never baked into the attribute, so the modulation and its
	// framebuffer clamp match the legacy exactly.
	float colorScale;

	ScenePrimitive()
		: vertexCount(0), indexCount(0), triangleCount(0), colorScale(1.0f)
	{
		baseColor[0] = baseColor[1] = baseColor[2] = baseColor[3] = 1.0f;
	}
};

// One drawable: a set of primitives sharing one node transform. The placeholder
// and the loaded content are separate items, so releasing content leaves the
// placeholder intact. `vertexCount`/`triangleCount` are the aggregate counts
// across the primitives (the content stats and the log read them).
struct SceneDrawItem
{
	std::vector<ScenePrimitive> primitives;
	float transform[16];
	int vertexCount;
	int triangleCount;
	int ready;
	float minimum[3];
	float maximum[3];

	SceneDrawItem() : vertexCount(0), triangleCount(0), ready(0)
	{
		for (int i = 0; i < 16; ++i)
			transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			minimum[axis] = FLT_MAX;
			maximum[axis] = -FLT_MAX;
		}
	}
};

struct SceneResources
{
	RefCntAutoPtr<IShader> vertexShader;
	RefCntAutoPtr<IShader> pixelShader;
	RefCntAutoPtr<IPipelineState> pipeline;			// matches the swap-chain format
	RefCntAutoPtr<IPipelineState> offscreenPipeline;	// matches the RGBA8 offscreen target
	RefCntAutoPtr<IBuffer> constants;
	SceneDrawItem placeholder;
	SceneDrawItem content;
	RefCntAutoPtr<ITexture> depth;
	RefCntAutoPtr<ITextureView> depthView;
	int pipelinesReady;

	SceneResources() : pipelinesReady(0) {}
};

struct OffscreenTargets
{
	RefCntAutoPtr<ITexture> color;
	RefCntAutoPtr<ITextureView> colorView;
	RefCntAutoPtr<ITexture> depth;
	RefCntAutoPtr<ITextureView> depthView;
	RefCntAutoPtr<ITexture> staging;
	Uint32 width;
	Uint32 height;

	OffscreenTargets() : width(0), height(0) {}
};

SceneResources g_scene;
OffscreenTargets g_offscreen;
float g_view[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
// The live GTE screen distance h (the game's scr_z), set by the bridge each
// frame. The projection's focal length is 2h/NATIVE_PSX_DISPLAY_HEIGHT, so it
// follows the game's dynamic field of view instead of a fixed FOV. Defaults to
// the game's gCameraDefaultScrZ (256) until the bridge supplies a value.
float g_screenDistance = 256.0f;
// The live GTE screen offset (SetGeomOffset OFX/OFY) in PSX display pixels, set
// by the bridge each frame from the GTE control registers. The renderer is
// game-agnostic, so it defaults to the PSX display centre (160,120), which the
// projection turns into a zero NDC translation; the game's live (160,128)
// pushes the image down by 8 PSX lines. See NativeCameraView_BuildProjection.
float g_screenOffsetX = NATIVE_PSX_DISPLAY_WIDTH * 0.5f;
float g_screenOffsetY = NATIVE_PSX_DISPLAY_HEIGHT * 0.5f;
float g_clipNear = 0.02f;
float g_clipFar = 300.0f;

// Registered resources, keyed by the canonical conversion-cache text key. The
// item owns its immutable GPU buffers/texture/sampler and its shader bindings;
// the shared constant buffer is updated per instance at draw time.
struct ResourceEntry
{
	char key[NATIVE_RESOURCE_KEY_CAPACITY];
	SceneDrawItem item;

	ResourceEntry() { key[0] = '\0'; }
};

ResourceEntry g_resources[NATIVE_MAX_RESOURCES];
int g_resourceCount = 0;

NativeInstance g_instances[NATIVE_MAX_INSTANCES];
int g_instanceCount = 0;
int g_instanceDrawn = 0;
int g_instanceSkipped = 0;
char g_instanceDiagnostic[NATIVE_INSTANCE_DIAGNOSTIC_CAPACITY] = "";

ResourceEntry* FindResource(const char* key)
{
	if (!key || !key[0])
		return nullptr;
	for (int i = 0; i < g_resourceCount; ++i)
	{
		if (strcmp(g_resources[i].key, key) == 0)
			return &g_resources[i];
	}
	return nullptr;
}

void SetInstanceDiagnosticKey(const char* prefix, const char* key)
{
	snprintf(g_instanceDiagnostic, sizeof(g_instanceDiagnostic), "%s%s",
		prefix, key ? key : "");
}

// Resolves the current instance list against resident resources. Fills `items`
// with one pointer per submitted instance (NULL when unresolved) and counts the
// unresolved ones. `items` may be NULL when only the count is needed.
int ResolveInstances(const SceneDrawItem** items)
{
	int skipped = 0;
	for (int i = 0; i < g_instanceCount; ++i)
	{
		ResourceEntry* entry = FindResource(g_instances[i].resourceKey);
		if (items)
			items[i] = entry ? &entry->item : nullptr;
		if (!entry)
		{
			if (skipped == 0)
				SetInstanceDiagnosticKey("unresolved instance key: ",
					g_instances[i].resourceKey);
			++skipped;
		}
	}
	return skipped;
}

void MultiplyColumnMajor(const float a[16], const float b[16], float out[16])
{
	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k)
				sum += a[k * 4 + row] * b[col * 4 + k];
			out[col * 4 + row] = sum;
		}
	}
}

// Diligent/Vulkan left-handed perspective projection: the camera looks down
// +Z, so a point ahead has positive camera-space z and a positive clip w. The
// NDC z range is [0, 1] (Diligent's Vulkan D3D emulation; BasicMath.hpp
// SetNearFarClipPlanes with NegativeOneToOneZ = false), giving
//   out[10] = far / (far - near)
//   out[11] = +1
//   out[14] = -near * far / (far - near)
//
// Vertical convention: out[5] = -f negates camera-space y. The game camera
// space is +Y-down: GTE screen space is +Y-down (PsyX_GTE.cpp:395-396,
// SY = OFY + IR2*h/sz, so a camera-space +y point lands BELOW the screen
// centre), and the game's inv_camera_matrix row 1 is the camera Y axis
// (rndrasm.c). Diligent's Vulkan backend flips the viewport to Direct3D's
// +Y-up convention (DeviceContextVkImpl.cpp "Turn the viewport upside down to
// be consistent with Direct3D"). Mapping camera +y to +NDC.y (out[5] = +f)
// would therefore render the world vertically mirrored against the game.
// Negating it maps camera +y to -NDC.y (the lower half), matching the game and
// the project's modern-mesh precedent (PsyX_ModernMesh.cpp:105,
// `-view.y * u_xyScale.y`). out[11] stays +1, so the projection is still
// +Z-forward (clip.w = +z) and the game's inv_camera_matrix is fed directly
// with no reflection. The negation is a y reflection: it flips the NDC
// signed-area sign of every triangle, which the scene cull pin compensates for
// (see CreateScenePipeline). Column-major, mul(MVP, float4(pos, 1)).
// The focal length comes from the live GTE screen distance h (the game's
// scr_z), not a fixed FOV: the legacy GTE projects x_screen = ofx + x*h/z, so
// the vertical focal is 2h/NATIVE_PSX_DISPLAY_HEIGHT and the horizontal scale
// follows the render target aspect (the legacy window mapping keeps the PSX
// display square). See NativeCameraView_BuildProjection for the full mapping.
void BuildPerspective(float screenDistanceH, float aspect, float nearPlane, float farPlane, float out[16])
{
	NativeCameraView_BuildProjection(screenDistanceH,
		NATIVE_PSX_DISPLAY_WIDTH, NATIVE_PSX_DISPLAY_HEIGHT,
		g_screenOffsetX, g_screenOffsetY,
		aspect, nearPlane, farPlane, out);
}

void AddQuad(std::vector<SceneVertex>& out,
	const float a[3], const float b[3], const float c[3], const float d[3])
{
	out.push_back({ a[0], a[1], a[2], 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f });
	out.push_back({ b[0], b[1], b[2], 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f });
	out.push_back({ c[0], c[1], c[2], 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f });
	out.push_back({ a[0], a[1], a[2], 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f });
	out.push_back({ c[0], c[1], c[2], 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f });
	out.push_back({ d[0], d[1], d[2], 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f });
}

void AddCube(std::vector<SceneVertex>& out, float cx, float cz, float size)
{
	const float h = size * 0.5f;
	const float x0 = cx - h, x1 = cx + h, z0 = cz - h, z1 = cz + h, y0 = 0.0f, y1 = size;

	const float side0[3] = { x0, y0, z0 }, side1[3] = { x1, y0, z0 };
	const float side2[3] = { x1, y1, z0 }, side3[3] = { x0, y1, z0 };
	AddQuad(out, side0, side1, side2, side3); // -Z face

	const float east0[3] = { x1, y0, z0 }, east1[3] = { x1, y0, z1 };
	const float east2[3] = { x1, y1, z1 }, east3[3] = { x1, y1, z0 };
	AddQuad(out, east0, east1, east2, east3); // +X face

	const float south0[3] = { x1, y0, z1 }, south1[3] = { x0, y0, z1 };
	const float south2[3] = { x0, y1, z1 }, south3[3] = { x1, y1, z1 };
	AddQuad(out, south0, south1, south2, south3); // +Z face

	const float west0[3] = { x0, y0, z1 }, west1[3] = { x0, y0, z0 };
	const float west2[3] = { x0, y1, z0 }, west3[3] = { x0, y1, z1 };
	AddQuad(out, west0, west1, west2, west3); // -X face

	const float top0[3] = { x0, y1, z0 }, top1[3] = { x1, y1, z0 };
	const float top2[3] = { x1, y1, z1 }, top3[3] = { x0, y1, z1 };
	AddQuad(out, top0, top1, top2, top3); // +Y face
}

// Converted-content UVs are raw 8-bit texel coordinates (the E03 builder keeps
// the decoder's texel UVs; see WorldMeshBuilder.h). A PSX texture page is
// 256x256 texels, so sampling a full page needs the raw UV divided by 256. The
// scale is applied once at upload time, so the shaders stay unchanged and the
// built-in placeholder (which uses normalised 0..1 UVs) is unaffected.
const float kContentTexelUvScale = 1.0f / 256.0f;

// Uploads an RGBA8 row-major image (`width * height * 4` bytes, `width * 4` per
// row) as an immutable 2D texture.
int CreateRgbaTexture(const char* name, const unsigned char* rgba, int width, int height,
	ITexture** out)
{
	TextureDesc desc;
	desc.Name = name;
	desc.Type = RESOURCE_DIM_TEX_2D;
	desc.Width = (Uint32)width;
	desc.Height = (Uint32)height;
	desc.Format = TEX_FORMAT_RGBA8_UNORM;
	desc.BindFlags = BIND_SHADER_RESOURCE;
	desc.Usage = USAGE_IMMUTABLE;
	TextureSubResData subResource;
	subResource.pData = rgba;
	subResource.Stride = (Uint64)width * 4u;
	TextureData textureData;
	textureData.pSubResources = &subResource;
	textureData.NumSubresources = 1;
	g_device->CreateTexture(desc, &textureData, out);
	return *out != nullptr;
}

// Configured nearest/bilinear sampler with clamp-to-edge addressing, so a raw
// texel UV never wraps. Only
// one mip level is uploaded, so the mip filter is never exercised.
int CreateContentSampler(ISampler** out)
{
	SamplerDesc desc;
	desc.MinFilter = g_bilinearFiltering ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
	desc.MagFilter = g_bilinearFiltering ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
	desc.MipFilter = FILTER_TYPE_POINT;
	desc.AddressU = TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = TEXTURE_ADDRESS_CLAMP;
	g_device->CreateSampler(desc, out);
	return *out != nullptr;
}

// Creates the per-primitive shader resource bindings for both scene pipelines
// and binds the shared constant buffer, the primitive texture and its sampler.
int CreatePrimitiveShaderBindings(ScenePrimitive& primitive)
{
	primitive.srb.Release();
	primitive.srbOffscreen.Release();

	g_scene.pipeline->CreateShaderResourceBinding(&primitive.srb, true);
	if (!primitive.srb)
		return 0;
	primitive.srb->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(g_scene.constants);
	primitive.srb->GetVariableByName(SHADER_TYPE_PIXEL, "gTexture")->Set(primitive.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	primitive.srb->GetVariableByName(SHADER_TYPE_PIXEL, "gSampler")->Set(primitive.sampler);

	if (g_scene.offscreenPipeline.RawPtr() == g_scene.pipeline.RawPtr())
	{
		primitive.srbOffscreen = primitive.srb; // same pipeline serves both targets
		return 1;
	}

	g_scene.offscreenPipeline->CreateShaderResourceBinding(&primitive.srbOffscreen, true);
	if (!primitive.srbOffscreen)
		return 0;
	primitive.srbOffscreen->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(g_scene.constants);
	primitive.srbOffscreen->GetVariableByName(SHADER_TYPE_PIXEL, "gTexture")->Set(primitive.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	primitive.srbOffscreen->GetVariableByName(SHADER_TYPE_PIXEL, "gSampler")->Set(primitive.sampler);
	return 1;
}

int CreateScenePipeline(TEXTURE_FORMAT colorFormat, const char* name, IPipelineState** out)
{
	LayoutElement elements[3] = {};
	elements[0].InputIndex = 0;
	elements[0].BufferSlot = 0;
	elements[0].NumComponents = 3;
	elements[0].ValueType = VT_FLOAT32;
	elements[0].RelativeOffset = 0;
	elements[1].InputIndex = 1;
	elements[1].BufferSlot = 0;
	elements[1].NumComponents = 2;
	elements[1].ValueType = VT_FLOAT32;
	elements[1].RelativeOffset = 3 * sizeof(float);
	// Per-vertex colour: the legacy PSX primitive colour byte normalised to
	// [0,1] (psx.vert `a_color` is R8G8B8A8_UNORM; psx.frag multiplies the
	// sampled texel by it). Kept as four floats here so the existing scene
	// geometry can default it to opaque white.
	elements[2].InputIndex = 2;
	elements[2].BufferSlot = 0;
	elements[2].NumComponents = 4;
	elements[2].ValueType = VT_FLOAT32;
	elements[2].RelativeOffset = 5 * sizeof(float);

	ShaderResourceVariableDesc variables[3] = {};
	variables[0].Name = "Constants";
	variables[0].ShaderStages = SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL;
	variables[0].Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	variables[1].Name = "gTexture";
	variables[1].ShaderStages = SHADER_TYPE_PIXEL;
	variables[1].Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	variables[2].Name = "gSampler";
	variables[2].ShaderStages = SHADER_TYPE_PIXEL;
	variables[2].Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

	GraphicsPipelineStateCreateInfo psoCI;
	psoCI.PSODesc.Name = name;
	psoCI.PSODesc.ResourceLayout.Variables = variables;
	psoCI.PSODesc.ResourceLayout.NumVariables = 3;
	psoCI.GraphicsPipeline.NumRenderTargets = 1;
	psoCI.GraphicsPipeline.RTVFormats[0] = colorFormat;
	psoCI.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
	psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	// Culling: none. The legacy GPU never culls (PsyX_Vk.cpp sets
	// VK_CULL_MODE_NONE); the game pre-culls every primitive in software with
	// gte_nclip / opz > 0 and the PLOT_NO_CULL / PLOT_INV_CULL overrides
	// (draw.c:784/796/1156-1165). The scene pipelines must not cull again, or
	// they remove the faces the game deliberately kept. The vertical convention
	// (BuildPerspective out[5] = -f) still reflects the NDC, but with culling
	// disabled the winding sign no longer selects a face; the decoder's stored
	// order only needs to produce the correct raster coverage.
	psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;

	// Depth: test and write on with LESS_OR_EQUAL, matching the legacy opaque
	// primitive state (test/write on, COMPARISON_FUNC_LESS_EQUAL; blending is
	// what disables the depth test there, and world draws never blend). The
	// dev-only P7 toggle RED2_NATIVE_NO_DEPTH=1 turns both off so a capture can
	// tell a depth-occlusion problem from missing geometry.
	const SceneDiagnostics& diagnostics = GetSceneDiagnostics();
	psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = diagnostics.noDepth ? False : True;
	psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = diagnostics.noDepth ? False : True;
	psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;

	// Opacity: world models are always opaque. PlotBuildingModel,
	// PlotModelSubdivNxN and drawMesh issue setPolyFT4 with the semi-transparency
	// bit clear, and the legacy fragment shader ignores STP for opaque draws
	// (psx.frag). So the scene pipeline blends nothing: the pixel shader discards
	// a 0000h texel (the cutout, psx.frag:111) and renders every other texel
	// fully opaque, including an STP=128 texel. This matches the baked texture's
	// 0/128/255 convention (knowledge/rules/texture-alpha-semantics.md) with no
	// blend. True semi-transparent content needs a separate blended,
	// depth-test-disabled, ordered pass; that is a later block, not this opaque
	// world slice.
	RenderTargetBlendDesc& blend = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
	blend.BlendEnable = False;

	psoCI.GraphicsPipeline.InputLayout.LayoutElements = elements;
	psoCI.GraphicsPipeline.InputLayout.NumElements = 3;
	psoCI.pVS = g_scene.vertexShader;
	psoCI.pPS = g_scene.pixelShader;
	g_device->CreateGraphicsPipelineState(psoCI, out);
	return *out != nullptr;
}

// Shaders, constant buffer and both pipelines. The swap chain uses an sRGB
// format, while the offscreen readback target is RGBA8_UNORM, so the two draw
// targets need pipelines whose RTV formats match them exactly.
int EnsureScenePipelines()
{
	if (g_scene.pipelinesReady)
		return 1;

	const char* vertexSource =
		"cbuffer Constants { float4x4 MVP; float4 baseColor; float4 colorScale; };\n"
		"struct VSIn  { float3 Pos : ATTRIB0; float2 UV : ATTRIB1; float4 Col : ATTRIB2; };\n"
		"struct VSOut { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; float4 Col : COLOR0; };\n"
		// The legacy vertex shader scales the normalised vertex colour by the
		// per-primitive `bright` multiplier before interpolation (psx.vert:33-34,
		// PsyX_render.cpp:787-788): v_color.xyz = a_color.xyz * bright. The scene
		// reproduces that here in float (never in the attribute), so the
		// modulation and the framebuffer clamp match the legacy.
		"void main(in VSIn input, out VSOut output)\n"
		"{ output.Pos = mul(MVP, float4(input.Pos, 1.0)); output.UV = input.UV;"
		" output.Col = float4(input.Col.rgb * colorScale.rgb, input.Col.a); }\n";
	// World primitives are opaque: a 0000h texel (alpha 0) is a cutout and is
	// discarded (psx.frag:111), every other texel renders fully opaque (the
	// STP bit is ignored for opaque draws, so a 128 texel shows its full RGB),
	// and the framebuffer alpha stays opaque. The shared white texel (alpha
	// 255) keeps the untextured/placeholder draws opaque with no discard.
	// The per-vertex colour reproduces the legacy `texColor * v_color`
	// modulation (psx.frag:147); it is opaque white for geometry that carries
	// none, so the base-colour factor still applies unchanged.
	//
	// The dev-only P7 toggle RED2_NATIVE_NO_CUTOUT=1 omits the discard, so an
	// alpha==0 texel renders with its own colour (alpha still ignored/opaque);
	// unset, the shader text is exactly the production one.
	const SceneDiagnostics& diagnostics = GetSceneDiagnostics();
	std::string pixelSource =
		"cbuffer Constants { float4x4 MVP; float4 baseColor; float4 colorScale; };\n"
		"Texture2D    gTexture;\n"
		"SamplerState gSampler;\n"
		"struct PSIn { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; float4 Col : COLOR0; };\n"
		"float4 main(in PSIn input) : SV_TARGET\n"
		"{\n"
		"    float4 texel = gTexture.Sample(gSampler, input.UV);\n";
	if (!diagnostics.noCutout)
		pixelSource += "    if (texel.a == 0.0) discard;\n";
	pixelSource +=
		"    return float4(texel.rgb * baseColor.rgb * input.Col.rgb, 1.0);\n"
		"}\n";

	ShaderCreateInfo shaderCI;
	shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	shaderCI.EntryPoint = "main";
	shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
	shaderCI.Source = vertexSource;
	g_device->CreateShader(shaderCI, &g_scene.vertexShader);

	shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
	shaderCI.Source = pixelSource.c_str();
	g_device->CreateShader(shaderCI, &g_scene.pixelShader);
	if (!g_scene.vertexShader || !g_scene.pixelShader)
		return 0;

	BufferDesc constantDesc;
	constantDesc.Name = "SceneConstants";
	constantDesc.BindFlags = BIND_UNIFORM_BUFFER;
	constantDesc.Usage = USAGE_DYNAMIC;
	constantDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	constantDesc.Size = sizeof(SceneConstants);
	g_device->CreateBuffer(constantDesc, nullptr, &g_scene.constants);
	if (!g_scene.constants)
		return 0;

	const TEXTURE_FORMAT swapColorFormat = g_swapChain
		? g_swapChain->GetDesc().ColorBufferFormat
		: TEX_FORMAT_RGBA8_UNORM;
	g_scene.pipeline.Release();
	if (!CreateScenePipeline(swapColorFormat, "NativeScene", &g_scene.pipeline))
		return 0;

	g_scene.offscreenPipeline.Release();
	if (swapColorFormat == TEX_FORMAT_RGBA8_UNORM)
		g_scene.offscreenPipeline = g_scene.pipeline;
	else if (!CreateScenePipeline(TEX_FORMAT_RGBA8_UNORM, "NativeSceneOffscreen", &g_scene.offscreenPipeline))
		return 0;

	// Warn-only, emitted once when the pipelines are first built: name each
	// active diagnosis toggle so a capture can be attributed to its env var.
	if (diagnostics.noCutout)
		LogInitDiagnostic("cutout disabled by RED2_NATIVE_NO_CUTOUT");
	if (diagnostics.noDepth)
		LogInitDiagnostic("depth test/write disabled by RED2_NATIVE_NO_DEPTH");

	g_scene.pipelinesReady = 1;
	return 1;
}

// The built-in placeholder: textured ground and four pillars. It is created
// once and stays available after converted content is cleared.
int CreatePlaceholderItem()
{
	if (g_scene.placeholder.ready)
		return 1;

	// Raw game world units: the view rotation is scaled by 1/4096 (see the
	// game's CameraRotation), so a scene metre is 4096 raw units.
	std::vector<SceneVertex> vertices;
	const float ground0[3] = { -20000.0f, 0.0f, -20000.0f };
	const float ground1[3] = { 20000.0f, 0.0f, -20000.0f };
	const float ground2[3] = { 20000.0f, 0.0f, 20000.0f };
	const float ground3[3] = { -20000.0f, 0.0f, 20000.0f };
	AddQuad(vertices, ground0, ground1, ground2, ground3);
	AddCube(vertices, -8000.0f, -8000.0f, 1600.0f);
	AddCube(vertices, 8000.0f, -8000.0f, 1600.0f);
	AddCube(vertices, -8000.0f, 8000.0f, 1600.0f);
	AddCube(vertices, 8000.0f, 8000.0f, 1600.0f);

	SceneDrawItem item;
	item.primitives.push_back(ScenePrimitive());
	ScenePrimitive& primitive = item.primitives.back();

	BufferDesc vertexDesc;
	vertexDesc.Name = "PlaceholderVertices";
	vertexDesc.BindFlags = BIND_VERTEX_BUFFER;
	vertexDesc.Usage = USAGE_IMMUTABLE;
	vertexDesc.Size = (Uint32)(vertices.size() * sizeof(SceneVertex));
	BufferData vertexData;
	vertexData.pData = vertices.data();
	vertexData.DataSize = vertexDesc.Size;
	g_device->CreateBuffer(vertexDesc, &vertexData, &primitive.vertices);
	if (!primitive.vertices)
		return 0;
	primitive.vertexCount = (Uint32)vertices.size();

	// Procedural checkerboard so the scene is visibly textured.
	const int texSize = 64;
	unsigned char texels[texSize * texSize * 4];
	for (int y = 0; y < texSize; ++y)
	{
		for (int x = 0; x < texSize; ++x)
		{
			const int checker = ((x / 8) + (y / 8)) & 1;
			unsigned char* texel = &texels[(y * texSize + x) * 4];
			texel[0] = checker ? 200 : 60;
			texel[1] = checker ? 170 : 60;
			texel[2] = checker ? 90 : 60;
			texel[3] = 255;
		}
	}

	TextureDesc textureDesc;
	textureDesc.Name = "PlaceholderTexture";
	textureDesc.Type = RESOURCE_DIM_TEX_2D;
	textureDesc.Width = texSize;
	textureDesc.Height = texSize;
	textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
	textureDesc.BindFlags = BIND_SHADER_RESOURCE;
	textureDesc.Usage = USAGE_IMMUTABLE;
	TextureSubResData subResource;
	subResource.pData = texels;
	subResource.Stride = texSize * 4;
	TextureData textureData;
	textureData.pSubResources = &subResource;
	textureData.NumSubresources = 1;
	g_device->CreateTexture(textureDesc, &textureData, &primitive.texture);
	if (!primitive.texture)
		return 0;

	SamplerDesc samplerDesc;
	g_device->CreateSampler(samplerDesc, &primitive.sampler);
	if (!primitive.sampler)
		return 0;

	if (!CreatePrimitiveShaderBindings(primitive))
		return 0;

	item.vertexCount = (int)vertices.size();
	item.triangleCount = (int)(vertices.size() / 3);
	item.ready = 1;
	g_scene.placeholder = item;
	return 1;
}

// Builds a draw item from a converted multi-primitive GLB record: one
// ScenePrimitive per glTF primitive, each with its own immutable vertex/index
// buffers, its own base-colour factor and its own shader bindings for both scene
// pipelines. A primitive whose record carries an embedded base-colour image gets
// its own RGBA8 texture and samples it with the shared PSX-look sampler;
// primitives without an image sample a shared 1x1 white texel so their
// base-colour factor is what is visible. UVs are raw texel coordinates and are
// scaled by 1/256 once here (kContentTexelUvScale). Everything is created into a
// local item so a failure releases it without touching the live content.
int CreateContentItem(const GltfMultiMeshData& mesh, SceneDrawItem& out,
	char* error, int errorCapacity)
{
	if (mesh.primitives.empty())
	{
		SetError(error, errorCapacity, "the converted mesh has no primitives");
		return 0;
	}

	SceneDrawItem item;

	// One shared white texel for untextured primitives and one shared PSX-look
	// sampler for every primitive of the item.
	RefCntAutoPtr<ITexture> whiteTexture;
	RefCntAutoPtr<ISampler> contentSampler;
	{
		const unsigned char white[4] = { 255, 255, 255, 255 };
		if (!CreateRgbaTexture("ContentWhite", white, 1, 1, &whiteTexture))
		{
			SetError(error, errorCapacity, "the converted base-colour texture could not be created");
			return 0;
		}
		if (!CreateContentSampler(&contentSampler))
		{
			SetError(error, errorCapacity, "the converted content sampler could not be created");
			return 0;
		}
	}

	item.primitives.reserve(mesh.primitives.size());
	for (size_t p = 0; p < mesh.primitives.size(); ++p)
	{
		const GltfPrimitiveData& source = mesh.primitives[p];
		const size_t vertexCount = source.positions.size() / 3;
		if (vertexCount == 0)
		{
			SetError(error, errorCapacity, "a converted primitive has no vertices");
			return 0;
		}

		std::vector<SceneVertex> vertices(vertexCount);
		for (size_t i = 0; i < vertexCount; ++i)
		{
			SceneVertex& vertex = vertices[i];
			vertex.x = source.positions[i * 3 + 0];
			vertex.y = source.positions[i * 3 + 1];
			vertex.z = source.positions[i * 3 + 2];
			for (int axis = 0; axis < 3; ++axis)
			{
				const float position = source.positions[i * 3 + axis];
				if (position < item.minimum[axis]) item.minimum[axis] = position;
				if (position > item.maximum[axis]) item.maximum[axis] = position;
			}
			// Normals are deliberately not fetched yet: this E03 slice is unlit
			// and the vertex format stays position + UV; lighting/PBR is a later
			// block.
			if (source.uvs.size() >= (i + 1) * 2)
			{
				vertex.u = source.uvs[i * 2 + 0] * kContentTexelUvScale;
				vertex.v = source.uvs[i * 2 + 1] * kContentTexelUvScale;
			}
			else
			{
				vertex.u = 0.0f;
				vertex.v = 0.0f;
			}

			// Per-vertex colour, matching psx.frag's `texColor * v_color`. The
			// writer stores it as normalised ubyte RGBA; here it becomes four
			// floats. Geometry with no colour attribute defaults to opaque
			// white, so its base-colour factor is what is visible.
			if (source.colors.size() >= (i + 1) * 4)
			{
				vertex.r = (float)source.colors[i * 4 + 0] / 255.0f;
				vertex.g = (float)source.colors[i * 4 + 1] / 255.0f;
				vertex.b = (float)source.colors[i * 4 + 2] / 255.0f;
				vertex.a = (float)source.colors[i * 4 + 3] / 255.0f;
			}
			else
			{
				vertex.r = 1.0f;
				vertex.g = 1.0f;
				vertex.b = 1.0f;
				vertex.a = 1.0f;
			}
		}

		item.primitives.push_back(ScenePrimitive());
		ScenePrimitive& primitive = item.primitives.back();
		primitive.texture = whiteTexture;
		primitive.sampler = contentSampler;

		// A primitive with an embedded base-colour image gets its own immutable
		// RGBA8 texture. The loader guarantees the image is exactly
		// width*height*4 bytes, but the size is re-checked here so a malformed
		// record fails with a named error instead of reading out of bounds.
		if (!source.imageRgba.empty())
		{
			const size_t expected =
				(size_t)source.imageWidth * (size_t)source.imageHeight * 4u;
			if (source.imageWidth <= 0 || source.imageHeight <= 0 ||
				source.imageRgba.size() != expected)
			{
				SetError(error, errorCapacity,
					"a converted primitive image does not match its dimensions");
				return 0;
			}
			if (!CreateRgbaTexture("ContentBaseColor", source.imageRgba.data(),
					source.imageWidth, source.imageHeight, &primitive.texture))
			{
				SetError(error, errorCapacity,
					"a converted base-colour image could not be created");
				return 0;
			}
		}

		BufferDesc vertexDesc;
		vertexDesc.Name = "ContentVertices";
		vertexDesc.BindFlags = BIND_VERTEX_BUFFER;
		vertexDesc.Usage = USAGE_IMMUTABLE;
		vertexDesc.Size = (Uint32)(vertices.size() * sizeof(SceneVertex));
		BufferData vertexData;
		vertexData.pData = vertices.data();
		vertexData.DataSize = vertexDesc.Size;
		g_device->CreateBuffer(vertexDesc, &vertexData, &primitive.vertices);
		if (!primitive.vertices)
		{
			SetError(error, errorCapacity, "the converted vertex buffer could not be created");
			return 0;
		}
		primitive.vertexCount = (Uint32)vertices.size();

		if (!source.indices.empty())
		{
			BufferDesc indexDesc;
			indexDesc.Name = "ContentIndices";
			indexDesc.BindFlags = BIND_INDEX_BUFFER;
			indexDesc.Usage = USAGE_IMMUTABLE;
			indexDesc.Size = (Uint32)(source.indices.size() * sizeof(unsigned int));
			BufferData indexData;
			indexData.pData = source.indices.data();
			indexData.DataSize = indexDesc.Size;
			g_device->CreateBuffer(indexDesc, &indexData, &primitive.indices);
			if (!primitive.indices)
			{
				SetError(error, errorCapacity, "the converted index buffer could not be created");
				return 0;
			}
			primitive.indexCount = (Uint32)source.indices.size();
		}

		for (int c = 0; c < 4; ++c)
			primitive.baseColor[c] = source.baseColorFactor[c];

		// E03 visual parity P6: every converted primitive descends from a legacy
		// *textured* world primitive (SourcePrimitiveClass is always Textured*),
		// which the legacy GPU packs with bright = 2 (PsyX_GPU.cpp:790,844,893
		// MakeTexcoordQuad/Triangle/Rect). The legacy vertex shader therefore
		// modulates by (colourByte/255) * 2 (psx.vert:33-34), i.e. colourByte/128:
		// a day ambient of 128 becomes ~1.0 instead of 0.502. The built-in
		// placeholder is a native-only fixture with white vertex colours, so its
		// scale stays 1 and is irrelevant to legacy fidelity.
		primitive.colorScale = 2.0f;

		primitive.triangleCount = source.triangleCount > 0
			? source.triangleCount
			: (int)(source.indices.size() / 3);

		if (!CreatePrimitiveShaderBindings(primitive))
		{
			SetError(error, errorCapacity, "the converted content shader bindings could not be created");
			return 0;
		}

		item.vertexCount += (int)primitive.vertexCount;
		item.triangleCount += primitive.triangleCount;
	}

	for (int i = 0; i < 16; ++i)
		item.transform[i] = mesh.transform[i];

	item.ready = 1;
	out = item;
	return 1;
}

void ReleaseSceneResources()
{
	g_scene = SceneResources();
	g_offscreen = OffscreenTargets();
}

int EnsureDepthTarget()
{
	const SwapChainDesc& swapDesc = g_swapChain->GetDesc();
	if (g_scene.depth && g_scene.depth->GetDesc().Width == swapDesc.Width &&
		g_scene.depth->GetDesc().Height == swapDesc.Height)
		return 1;

	g_scene.depthView.Release();
	g_scene.depth.Release();

	TextureDesc depthDesc;
	depthDesc.Name = "SceneDepth";
	depthDesc.Type = RESOURCE_DIM_TEX_2D;
	depthDesc.Width = swapDesc.Width;
	depthDesc.Height = swapDesc.Height;
	depthDesc.Format = TEX_FORMAT_D32_FLOAT;
	depthDesc.BindFlags = BIND_DEPTH_STENCIL;
	depthDesc.Usage = USAGE_DEFAULT;
	g_device->CreateTexture(depthDesc, nullptr, &g_scene.depth);
	if (!g_scene.depth)
		return 0;
	g_scene.depthView = g_scene.depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
	return g_scene.depthView != nullptr;
}

// Pipelines and placeholder must exist before any scene draw.
int EnsureSceneReady()
{
	return EnsureScenePipelines() && CreatePlaceholderItem();
}

int EnsureOffscreenTargets(Uint32 width, Uint32 height)
{
	if (g_offscreen.color && g_offscreen.width == width && g_offscreen.height == height)
		return 1;

	g_offscreen = OffscreenTargets();

	TextureDesc colorDesc;
	colorDesc.Name = "SceneOffscreenColor";
	colorDesc.Type = RESOURCE_DIM_TEX_2D;
	colorDesc.Width = width;
	colorDesc.Height = height;
	colorDesc.Format = TEX_FORMAT_RGBA8_UNORM;
	colorDesc.BindFlags = BIND_RENDER_TARGET;
	colorDesc.Usage = USAGE_DEFAULT;
	g_device->CreateTexture(colorDesc, nullptr, &g_offscreen.color);
	if (!g_offscreen.color)
		return 0;
	g_offscreen.colorView = g_offscreen.color->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
	if (!g_offscreen.colorView)
		return 0;

	TextureDesc depthDesc;
	depthDesc.Name = "SceneOffscreenDepth";
	depthDesc.Type = RESOURCE_DIM_TEX_2D;
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.Format = TEX_FORMAT_D32_FLOAT;
	depthDesc.BindFlags = BIND_DEPTH_STENCIL;
	depthDesc.Usage = USAGE_DEFAULT;
	g_device->CreateTexture(depthDesc, nullptr, &g_offscreen.depth);
	if (!g_offscreen.depth)
		return 0;
	g_offscreen.depthView = g_offscreen.depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
	if (!g_offscreen.depthView)
		return 0;

	TextureDesc stagingDesc = colorDesc;
	stagingDesc.Name = "SceneOffscreenStaging";
	stagingDesc.BindFlags = BIND_NONE;
	stagingDesc.Usage = USAGE_STAGING;
	stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
	g_device->CreateTexture(stagingDesc, nullptr, &g_offscreen.staging);
	if (!g_offscreen.staging)
		return 0;

	g_offscreen.width = width;
	g_offscreen.height = height;
	return 1;
}

// Uploads the per-draw constants and issues one draw for a primitive. The shared
// constant buffer is written per call, so the caller must commit and draw
// before the next Map/Discard (the per-primitive loops rely on this).
int DrawPrimitive(const ScenePrimitive& primitive, IPipelineState* pipeline,
	IShaderResourceBinding* srb, const float mvp[16])
{
	++g_frameDrawCalls;
	SceneConstants constants;
	for (int i = 0; i < 16; ++i)
		constants.mvp[i] = mvp[i];
	for (int i = 0; i < 4; ++i)
		constants.baseColor[i] = primitive.baseColor[i];
	for (int i = 0; i < 4; ++i)
		constants.colorScale[i] = primitive.colorScale;
	// DISCARD allocates a fresh dynamic range. Previous draws keep their own
	// constants without transfer commands or ending the Vulkan render pass.
	void* mapped = nullptr;
	g_context->MapBuffer(g_scene.constants, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
	if (!mapped)
		return 0;
	memcpy(mapped, &constants, sizeof(constants));
	g_context->UnmapBuffer(g_scene.constants, MAP_WRITE);

	g_context->SetPipelineState(pipeline);
	g_context->CommitShaderResources(srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	IBuffer* vertexBuffers[1] = { primitive.vertices.RawPtr() };
	g_context->SetVertexBuffers(0, 1, vertexBuffers, nullptr,
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	if (primitive.indices)
	{
		g_context->SetIndexBuffer(primitive.indices, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		DrawIndexedAttribs draw;
		draw.NumIndices = primitive.indexCount;
		draw.IndexType = VT_UINT32;
		draw.Flags = DRAW_FLAG_NONE;
		g_context->DrawIndexed(draw);
	}
	else
	{
		DrawAttribs draw;
		draw.NumVertices = primitive.vertexCount;
		draw.Flags = DRAW_FLAG_NONE;
		g_context->Draw(draw);
	}
	return 1;
}

// Draws every ready primitive of an item with the given MVP and pipeline,
// selecting the binding that matches the target. Returns the number of
// primitives actually issued, so the caller can tell a fully empty item from a
// drawn one.
int DrawItemPrimitives(const SceneDrawItem& item, IPipelineState* pipeline,
	int offscreenTarget, const float mvp[16])
{
	int drawn = 0;
	for (size_t p = 0; p < item.primitives.size(); ++p)
	{
		const ScenePrimitive& primitive = item.primitives[p];
		IShaderResourceBinding* srb = offscreenTarget
			? primitive.srbOffscreen.RawPtr()
			: primitive.srb.RawPtr();
		if (!primitive.vertices || !srb)
			continue;
		if (!DrawPrimitive(primitive, pipeline, srb, mvp))
			return -1;
		++drawn;
	}
	return drawn;
}

// Shared scene draw used by RenderScene (swap chain) and RenderSceneToRGBA
// (offscreen). The mode is explicit: a non-empty instance list draws ONLY those
// instances (each with its own world transform, no instancing), otherwise the
// loaded content, otherwise the placeholder. The pixel-level tests stay
// deterministic because each mode draws only its own geometry.
int RenderSceneInto(ITextureView* renderTarget, ITextureView* depthView,
	Uint32 width, Uint32 height, int offscreenTarget)
{
	IPipelineState* pipeline = offscreenTarget
		? g_scene.offscreenPipeline.RawPtr()
		: g_scene.pipeline.RawPtr();
	if (!pipeline)
		return 0;

	float projection[16];
	const float aspect = height > 0 ? (float)width / (float)height : 1.0f;
	BuildPerspective(g_screenDistance, aspect, g_clipNear, g_clipFar, projection);

	// Dev diagnostic: RED2_NATIVE_CLEAR_MAGENTA=1 clears with magenta instead of
	// the normal dark background, so an undrawn (clear colour) region can be
	// told apart from a drawn texel that happens to have the normal colour.
	float clearColor[4] = { 0.05f, 0.07f, 0.12f, 1.0f };
	if (getenv("RED2_NATIVE_CLEAR_MAGENTA") != nullptr)
	{
		clearColor[0] = 1.0f;
		clearColor[1] = 0.0f;
		clearColor[2] = 1.0f;
	}
	g_context->SetRenderTargets(1, &renderTarget, depthView, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	g_context->ClearRenderTarget(renderTarget, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	g_context->ClearDepthStencil(depthView, CLEAR_DEPTH_FLAG, 1.0f, 0,
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	if (g_instanceCount > 0)
	{
		// Instances mode: no placeholder fallback. Unresolved keys are skipped
		// with a counter and a first-key diagnostic; an all-unresolved
		// submission leaves the clear colour and reports the diagnostic.
		const SceneDrawItem* items[NATIVE_MAX_INSTANCES];
		g_instanceSkipped = ResolveInstances(items);
		g_instanceDrawn = 0;
		for (int i = 0; i < g_instanceCount; ++i)
		{
			const SceneDrawItem* item = items[i];
			if (!item || !item->ready || item->primitives.empty())
				continue;

			// MVP = projection * view * instance transform; raw game units. The
			// instance transform is shared by all of the item's primitives.
			float viewModel[16];
			MultiplyColumnMajor(g_view, g_instances[i].transform, viewModel);
			float mvp[16];
			MultiplyColumnMajor(projection, viewModel, mvp);
			if (!NativeCameraView_BoundsVisible(mvp, item->minimum, item->maximum))
			{
				++g_frameCulledInstances;
				continue;
			}

			// `drawn` counts instances, not primitives: an instance that issues
			// at least one primitive counts once, however many it has.
			const int primitiveCount = DrawItemPrimitives(*item, pipeline, offscreenTarget, mvp);
			if (primitiveCount < 0)
				return 0;
			if (primitiveCount > 0)
				++g_instanceDrawn;
		}
		if (g_instanceDrawn == 0 && g_instanceSkipped > 0 && g_instanceDiagnostic[0] == '\0')
			SetInstanceDiagnosticKey("no instance could be drawn; first key: ",
				g_instances[0].resourceKey);
		return 1;
	}

	// Content mode when a converted GLB is loaded, placeholder mode otherwise.
	const SceneDrawItem& item = g_scene.content.ready ? g_scene.content : g_scene.placeholder;
	if (!item.ready || item.primitives.empty())
		return 0;

	// MVP = projection * view * model. The model is the content node transform
	// (identity for the placeholder); world units remain raw game units.
	float viewModel[16];
	MultiplyColumnMajor(g_view, item.transform, viewModel);
	float mvp[16];
	MultiplyColumnMajor(projection, viewModel, mvp);
	return DrawItemPrimitives(item, pipeline, offscreenTarget, mvp) > 0 ? 1 : 0;
}
} // namespace

int NativeRenderer_Init(void* nativeWindowHandle, int width, int height, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!nativeWindowHandle || width <= 0 || height <= 0)
	{
		SetError(error, errorCapacity, "a native window handle and a positive size are required");
		return 0;
	}
	if (g_active)
	{
		SetError(error, errorCapacity, "the native renderer is already active");
		return 0;
	}

	try
	{
		SetDebugMessageCallback(DiligentMessage);
		g_factory = GetEngineFactoryVk();
		if (!g_factory)
		{
			SetError(error, errorCapacity, "the Diligent Vulkan engine factory is unavailable");
			return 0;
		}

		EngineVkCreateInfo createInfo;
		g_factory->CreateDeviceAndContextsVk(createInfo, &g_device, &g_context);
		if (!g_device || !g_context)
		{
			SetError(error, errorCapacity, "the Diligent device or immediate context could not be created");
			NativeRenderer_Shutdown();
			return 0;
		}

		SwapChainDesc swapChainDesc;
		// Original texels/prelit colours are display-encoded UNORM values.
		// Match the offscreen path; an sRGB RTV would encode them a second time.
		swapChainDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
		// MAILBOX with two images can leave acquisition waiting for scanout:
		// one image is displayed and one is pending. Keep a third image available
		// for the next frame. Vulkan surface limits may clamp this request.
		swapChainDesc.BufferCount = 3;
		swapChainDesc.Width = (Uint32)width;
		swapChainDesc.Height = (Uint32)height;

		NativeWindow window;
		window.hWnd = nativeWindowHandle;

		g_factory->CreateSwapChainVk(g_device, g_context, swapChainDesc, window, &g_swapChain);
		if (!g_swapChain)
		{
			SetError(error, errorCapacity, "the Diligent swap chain could not be created for the window");
			NativeRenderer_Shutdown();
			return 0;
		}
	}
	catch (...)
	{
		SetError(error, errorCapacity, "the Diligent renderer threw during initialization");
		NativeRenderer_Shutdown();
		return 0;
	}

	g_active = 1;
	return 1;
}

int NativeRenderer_IsActive(void)
{
	return g_active;
}

void NativeRenderer_Resize(int width, int height)
{
	if (!g_active || !g_swapChain || width <= 0 || height <= 0)
		return;
	g_swapChain->Resize((Uint32)width, (Uint32)height);
}

int NativeRenderer_RenderClearAndPresent(float red, float green, float blue, float alpha,
	char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!g_active || !g_swapChain || !g_context)
	{
		SetError(error, errorCapacity, "the native renderer is not active");
		return 0;
	}

	ITextureView* renderTarget = g_swapChain->GetCurrentBackBufferRTV();
	if (!renderTarget)
	{
		SetError(error, errorCapacity, "the swap chain has no current back buffer");
		return 0;
	}

	const float clearColor[4] = { red, green, blue, alpha };
	const double drawStart = FrameMilliseconds();
	g_frameDrawCalls = 0;
	g_frameCulledInstances = 0;
	g_context->SetRenderTargets(1, &renderTarget, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	g_context->ClearRenderTarget(renderTarget, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	const double drawEnd = FrameMilliseconds();
	if (g_overlayHandler && !g_overlayHandler(g_context.RawPtr(), renderTarget))
	{
		SetError(error, errorCapacity, "the native developer overlay could not be drawn");
		return 0;
	}
	const double overlayEnd = FrameMilliseconds();
	g_context->Flush();
	const double flushEnd = FrameMilliseconds();
	g_swapChain->Present(g_syncInterval);
	const double presentEnd = FrameMilliseconds();
	g_frameStats.drawMilliseconds = drawEnd - drawStart;
	g_frameStats.overlayMilliseconds = overlayEnd - drawEnd;
	g_frameStats.flushMilliseconds = flushEnd - overlayEnd;
	g_frameStats.presentMilliseconds = presentEnd - flushEnd;
	g_frameStats.drawCalls = 0;
	g_frameStats.culledInstances = 0;
	g_frameStats.swapChainBuffers = g_swapChain->GetDesc().BufferCount;
	return 1;
}

void NativeRenderer_SetCamera(const float view[16], float screenDistanceH)
{
	if (!view)
		return;
	for (int i = 0; i < 16; ++i)
		g_view[i] = view[i];
	if (screenDistanceH > 0.0f)
		g_screenDistance = screenDistanceH;
}

void NativeRenderer_SetScreenOffset(float offsetX, float offsetY)
{
	g_screenOffsetX = offsetX;
	g_screenOffsetY = offsetY;
}

int NativeRenderer_RenderScene(char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!g_active || !g_swapChain || !g_context)
	{
		SetError(error, errorCapacity, "the native renderer is not active");
		return 0;
	}
	if (!EnsureSceneReady())
	{
		SetError(error, errorCapacity, "the native scene pipeline could not be created");
		return 0;
	}
	if (!EnsureDepthTarget())
	{
		SetError(error, errorCapacity, "the native scene depth target could not be created");
		return 0;
	}

	ITextureView* renderTarget = g_swapChain->GetCurrentBackBufferRTV();
	if (!renderTarget)
	{
		SetError(error, errorCapacity, "the swap chain has no current back buffer");
		return 0;
	}

	const double drawStart = FrameMilliseconds();
	g_frameDrawCalls = 0;
	g_frameCulledInstances = 0;
	const SwapChainDesc& swapDesc = g_swapChain->GetDesc();
	g_frameStats.swapChainBuffers = swapDesc.BufferCount;
	if (!RenderSceneInto(renderTarget, g_scene.depthView, swapDesc.Width, swapDesc.Height, 0))
	{
		SetError(error, errorCapacity, "the native scene could not be drawn");
		return 0;
	}
	const double drawEnd = FrameMilliseconds();
	if (g_overlayHandler && !g_overlayHandler(g_context.RawPtr(), renderTarget))
	{
		SetError(error, errorCapacity, "the native developer overlay could not be drawn");
		return 0;
	}
	const double overlayEnd = FrameMilliseconds();
	g_context->Flush();
	const double flushEnd = FrameMilliseconds();
	g_swapChain->Present(g_syncInterval);
	const double presentEnd = FrameMilliseconds();
	g_frameStats.drawMilliseconds = drawEnd - drawStart;
	g_frameStats.overlayMilliseconds = overlayEnd - drawEnd;
	g_frameStats.flushMilliseconds = flushEnd - overlayEnd;
	g_frameStats.presentMilliseconds = presentEnd - flushEnd;
	g_frameStats.drawCalls = g_frameDrawCalls;
	g_frameStats.culledInstances = g_frameCulledInstances;
	return 1;
}

void NativeRenderer_SetVSync(int enabled)
{
	g_syncInterval = enabled ? 1 : 0;
}

int NativeRenderer_SetBilinearFiltering(int enabled, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0) error[0] = '\0';
	enabled = enabled != 0;
	if (enabled == g_bilinearFiltering) return 1;
	if (!g_device)
	{
		SetError(error, errorCapacity, "material filtering requires an active native device");
		return 0;
	}
	SamplerDesc desc;
	desc.MinFilter = desc.MagFilter = enabled ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
	desc.MipFilter = FILTER_TYPE_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = TEXTURE_ADDRESS_CLAMP;
	RefCntAutoPtr<ISampler> sampler;
	g_device->CreateSampler(desc, &sampler);
	if (!sampler)
	{
		SetError(error, errorCapacity, "native material sampler creation failed");
		return 0;
	}
	// ALLOW_OVERWRITE permits a mutable binding change but does not protect
	// an in-flight Vulkan descriptor set. This rare settings change waits once;
	// unchanged frame settings return above and never stall the GPU here.
	g_context->WaitForIdle();
	auto update = [&sampler](SceneDrawItem& item)
	{
		for (ScenePrimitive& primitive : item.primitives)
		{
			primitive.sampler = sampler;
			if (primitive.srb)
				primitive.srb->GetVariableByName(SHADER_TYPE_PIXEL, "gSampler")->Set(sampler, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
			if (primitive.srbOffscreen && primitive.srbOffscreen != primitive.srb)
				primitive.srbOffscreen->GetVariableByName(SHADER_TYPE_PIXEL, "gSampler")->Set(sampler, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
		}
	};
	update(g_scene.placeholder);
	update(g_scene.content);
	for (int i = 0; i < g_resourceCount; ++i) update(g_resources[i].item);
	g_bilinearFiltering = enabled;
	return 1;
}

void NativeRenderer_SetOverlayHandler(NativeOverlayHandler handler)
{
	g_overlayHandler = handler;
}

void NativeRenderer_GetFrameStats(NativeFrameStats* stats)
{
	if (stats)
		*stats = g_frameStats;
}

int NativeRenderer_LoadContent(const char* glbPath, char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!glbPath || !glbPath[0])
	{
		SetError(error, errorCapacity, "a converted content path is required");
		return 0;
	}
	if (!g_active || !g_device || !g_context)
	{
		SetError(error, errorCapacity, "the native renderer is not active");
		return 0;
	}

	GltfMultiMeshData mesh;
	if (Gltf_LoadFileMulti(glbPath, &mesh, error, errorCapacity) != 0)
		return 0; // the previously loaded content stays untouched

	if (!EnsureScenePipelines())
	{
		SetError(error, errorCapacity, "the native scene pipeline could not be created");
		return 0;
	}

	SceneDrawItem item;
	if (!CreateContentItem(mesh, item, error, errorCapacity))
		return 0; // the partial item releases itself; live content stays untouched

	g_scene.content = item; // releases the previously loaded content resources
	return 1;
}

void NativeRenderer_ClearContent(void)
{
	g_scene.content = SceneDrawItem();
}

int NativeRenderer_HasContent(void)
{
	return g_scene.content.ready;
}

void NativeRenderer_GetContentStats(int* vertexCount, int* triangleCount)
{
	if (vertexCount)
		*vertexCount = g_scene.content.ready ? (int)g_scene.content.vertexCount : 0;
	if (triangleCount)
		*triangleCount = g_scene.content.ready ? g_scene.content.triangleCount : 0;
}

int NativeRenderer_RegisterResource(const char* key, const char* glbPath,
	char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!key || !key[0])
	{
		SetError(error, errorCapacity, "a resource key is required");
		return 0;
	}
	if (!glbPath || !glbPath[0])
	{
		SetError(error, errorCapacity, "a converted content path is required");
		return 0;
	}
	if (!g_active || !g_device || !g_context)
	{
		SetError(error, errorCapacity, "the native renderer is not active");
		return 0;
	}
	if (strlen(key) >= NATIVE_RESOURCE_KEY_CAPACITY)
	{
		SetError(error, errorCapacity, "the resource key is too long");
		return 0;
	}

	// Idempotent: a resident key is left untouched.
	if (FindResource(key))
		return 1;

	if (g_resourceCount >= NATIVE_MAX_RESOURCES)
	{
		SetError(error, errorCapacity,
			"the native resource capacity is full; no resource was evicted");
		return 0;
	}

	GltfMultiMeshData mesh;
	if (Gltf_LoadFileMulti(glbPath, &mesh, error, errorCapacity) != 0)
		return 0;

	if (!EnsureScenePipelines())
	{
		SetError(error, errorCapacity, "the native scene pipeline could not be created");
		return 0;
	}

	SceneDrawItem item;
	if (!CreateContentItem(mesh, item, error, errorCapacity))
		return 0; // the partial item releases itself; resident resources stay untouched

	ResourceEntry& entry = g_resources[g_resourceCount];
	snprintf(entry.key, sizeof(entry.key), "%s", key);
	entry.item = item;
	++g_resourceCount;
	return 1;
}

void NativeRenderer_UnregisterResource(const char* key)
{
	if (!key || !key[0])
		return;
	for (int i = 0; i < g_resourceCount; ++i)
	{
		if (strcmp(g_resources[i].key, key) != 0)
			continue;
		// Shift the tail down; the last slot is cleared so its GPU resources
		// are released promptly.
		for (int j = i; j + 1 < g_resourceCount; ++j)
			g_resources[j] = g_resources[j + 1];
		--g_resourceCount;
		g_resources[g_resourceCount] = ResourceEntry();
		return;
	}
}

void NativeRenderer_ClearResources(void)
{
	for (int i = 0; i < g_resourceCount; ++i)
		g_resources[i] = ResourceEntry();
	g_resourceCount = 0;
}

int NativeRenderer_ResourceResident(const char* key)
{
	return FindResource(key) != nullptr;
}

int NativeRenderer_SetInstances(const NativeInstance* instances, int count,
	char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (count < 0)
	{
		SetError(error, errorCapacity, "the instance count cannot be negative");
		return 0;
	}
	if (count > NATIVE_MAX_INSTANCES)
	{
		SetError(error, errorCapacity,
			"the instance count exceeds NATIVE_MAX_INSTANCES");
		return 0;
	}
	if (count > 0 && !instances)
	{
		SetError(error, errorCapacity, "an instance array is required for a positive count");
		return 0;
	}

	for (int i = 0; i < count; ++i)
		g_instances[i] = instances[i];
	g_instanceCount = count;

	// Resolve now so the stats and the first-key diagnostic are available
	// before the next draw. Unresolved keys are counted, not fatal.
	g_instanceDiagnostic[0] = '\0';
	g_instanceSkipped = ResolveInstances(nullptr);
	g_instanceDrawn = 0;
	return 1;
}

void NativeRenderer_ClearInstances(void)
{
	g_instanceCount = 0;
	g_instanceDrawn = 0;
	g_instanceSkipped = 0;
	g_instanceDiagnostic[0] = '\0';
}

void NativeRenderer_SetClipPlanes(float nearPlane, float farPlane)
{
	if (!(nearPlane > 0.0f) || !(farPlane > nearPlane))
		return;
	g_clipNear = nearPlane;
	g_clipFar = farPlane;
}

void NativeRenderer_GetInstanceStats(int* submitted, int* drawn,
	int* skippedUnresolved, int* residentResources)
{
	if (submitted)
		*submitted = g_instanceCount;
	if (drawn)
		*drawn = g_instanceDrawn;
	if (skippedUnresolved)
		*skippedUnresolved = g_instanceSkipped;
	if (residentResources)
		*residentResources = g_resourceCount;
}

int NativeRenderer_GetFirstInstanceDiagnostic(char* out, int capacity)
{
	if (!out || capacity <= 0)
		return 0;
	if (g_instanceDiagnostic[0] == '\0')
	{
		out[0] = '\0';
		return 0;
	}
	snprintf(out, capacity, "%s", g_instanceDiagnostic);
	return 1;
}

int NativeRenderer_RenderSceneToRGBA(unsigned char* rgba, int width, int height,
	char* error, int errorCapacity)
{
	if (error && errorCapacity > 0)
		error[0] = '\0';
	if (!rgba || width <= 0 || height <= 0)
	{
		SetError(error, errorCapacity, "a readback buffer and a positive size are required");
		return 0;
	}
	if (!g_active || !g_context || !g_device)
	{
		SetError(error, errorCapacity, "the native renderer is not active");
		return 0;
	}
	if (!EnsureSceneReady())
	{
		SetError(error, errorCapacity, "the native scene pipeline could not be created");
		return 0;
	}
	if (!EnsureOffscreenTargets((Uint32)width, (Uint32)height))
	{
		SetError(error, errorCapacity, "the offscreen scene target could not be created");
		return 0;
	}

	if (!RenderSceneInto(g_offscreen.colorView, g_offscreen.depthView,
		(Uint32)width, (Uint32)height, 1))
	{
		SetError(error, errorCapacity, "the native scene could not be drawn into the offscreen target");
		return 0;
	}

	CopyTextureAttribs copy;
	copy.pSrcTexture = g_offscreen.color;
	copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
	copy.pDstTexture = g_offscreen.staging;
	copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
	g_context->CopyTexture(copy);
	g_context->Flush();
	g_context->WaitForIdle();

	MappedTextureSubresource mapped = {};
	g_context->MapTextureSubresource(g_offscreen.staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
	if (!mapped.pData)
	{
		SetError(error, errorCapacity, "the offscreen readback buffer could not be mapped");
		return 0;
	}
	for (int row = 0; row < height; ++row)
	{
		memcpy(rgba + (size_t)row * (size_t)width * 4,
			(const unsigned char*)mapped.pData + (size_t)row * mapped.Stride,
			(size_t)width * 4);
	}
	g_context->UnmapTextureSubresource(g_offscreen.staging, 0, 0);
	return 1;
}

void* NativeRenderer_GetDevice(void)
{
	return g_active ? g_device.RawPtr() : nullptr;
}

void* NativeRenderer_GetContext(void)
{
	return g_active ? g_context.RawPtr() : nullptr;
}

void NativeRenderer_Shutdown(void)
{
	g_overlayHandler = nullptr;
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();
	ReleaseSceneResources();
	g_swapChain.Release();
	g_context.Release();
	g_device.Release();
	g_factory = nullptr;
	g_active = 0;
	g_bilinearFiltering = 0;
}

#endif // _WIN32
