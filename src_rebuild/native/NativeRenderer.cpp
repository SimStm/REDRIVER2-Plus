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

#include <math.h>
#include <stdio.h>
#include <string.h>
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

void SetError(char* error, int capacity, const char* message)
{
	if (error && capacity > 0)
		snprintf(error, capacity, "%s", message);
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
};

// 20 floats / 80 bytes: MVP plus the material base-colour factor. HLSL packs
// the float4x4 into the first 64 bytes and the float4 into the next 16.
struct SceneConstants
{
	float mvp[16];
	float baseColor[4];
};

// One drawable: geometry, texture, bindings and placement. The placeholder and
// the loaded content are separate instances, so releasing content leaves the
// placeholder intact.
struct SceneDrawItem
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
	float transform[16];
	float baseColor[4];
	int ready;

	SceneDrawItem()
		: vertexCount(0), indexCount(0), triangleCount(0), ready(0)
	{
		for (int i = 0; i < 16; ++i)
			transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
		baseColor[0] = baseColor[1] = baseColor[2] = baseColor[3] = 1.0f;
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
float g_fovY = 1.0472f; // 60 degrees

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

void BuildPerspective(float fovY, float aspect, float nearPlane, float farPlane, float out[16])
{
	for (int i = 0; i < 16; ++i)
		out[i] = 0.0f;
	const float f = 1.0f / tanf(fovY * 0.5f);
	out[0] = f / aspect;
	out[5] = f;
	out[10] = farPlane / (nearPlane - farPlane);
	out[11] = -1.0f;
	out[14] = nearPlane * farPlane / (nearPlane - farPlane);
}

void AddQuad(std::vector<SceneVertex>& out,
	const float a[3], const float b[3], const float c[3], const float d[3])
{
	out.push_back({ a[0], a[1], a[2], 0.0f, 0.0f });
	out.push_back({ b[0], b[1], b[2], 1.0f, 0.0f });
	out.push_back({ c[0], c[1], c[2], 1.0f, 1.0f });
	out.push_back({ a[0], a[1], a[2], 0.0f, 0.0f });
	out.push_back({ c[0], c[1], c[2], 1.0f, 1.0f });
	out.push_back({ d[0], d[1], d[2], 0.0f, 1.0f });
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

// Creates the per-item shader resource bindings for both scene pipelines and
// binds the shared constant buffer, the item texture and its sampler.
int CreateItemShaderBindings(SceneDrawItem& item)
{
	item.srb.Release();
	item.srbOffscreen.Release();

	g_scene.pipeline->CreateShaderResourceBinding(&item.srb, true);
	if (!item.srb)
		return 0;
	item.srb->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(g_scene.constants);
	item.srb->GetVariableByName(SHADER_TYPE_PIXEL, "gTexture")->Set(item.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	item.srb->GetVariableByName(SHADER_TYPE_PIXEL, "gSampler")->Set(item.sampler);

	if (g_scene.offscreenPipeline.RawPtr() == g_scene.pipeline.RawPtr())
	{
		item.srbOffscreen = item.srb; // same pipeline serves both targets
		return 1;
	}

	g_scene.offscreenPipeline->CreateShaderResourceBinding(&item.srbOffscreen, true);
	if (!item.srbOffscreen)
		return 0;
	item.srbOffscreen->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(g_scene.constants);
	item.srbOffscreen->GetVariableByName(SHADER_TYPE_PIXEL, "gTexture")->Set(item.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	item.srbOffscreen->GetVariableByName(SHADER_TYPE_PIXEL, "gSampler")->Set(item.sampler);
	return 1;
}

int CreateScenePipeline(TEXTURE_FORMAT colorFormat, const char* name, IPipelineState** out)
{
	LayoutElement elements[2] = {};
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
	// Winding policy (pinned by tests/SourceModelDecoderTests.cpp and asserted by
	// tests/NativeRendererSmoke.cpp): the decoder emits world quads as
	// (v0,v1,v3,v2) and fans them (0,1,2)+(0,2,3); for a front-facing face the
	// first triangle (v0,v1,v3) has a positive signed area in the y-up NDC.
	// Under Diligent's Vulkan D3D emulation (negative viewport height),
	// FrontCounterClockwise = True is the setting that treats those positive-area
	// (counter-clockwise in the y-up NDC) triangles as front-facing; CULL_MODE_BACK
	// then removes the reversed back faces. CULL_MODE_NONE is not an accepted
	// policy.
	psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
	psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
	psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
	psoCI.GraphicsPipeline.InputLayout.LayoutElements = elements;
	psoCI.GraphicsPipeline.InputLayout.NumElements = 2;
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
		"cbuffer Constants { float4x4 MVP; float4 baseColor; };\n"
		"struct VSIn  { float3 Pos : ATTRIB0; float2 UV : ATTRIB1; };\n"
		"struct VSOut { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };\n"
		"void main(in VSIn input, out VSOut output)\n"
		"{ output.Pos = mul(MVP, float4(input.Pos, 1.0)); output.UV = input.UV; }\n";
	const char* pixelSource =
		"cbuffer Constants { float4x4 MVP; float4 baseColor; };\n"
		"Texture2D    gTexture;\n"
		"SamplerState gSampler;\n"
		"struct PSIn { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };\n"
		"float4 main(in PSIn input) : SV_TARGET { return gTexture.Sample(gSampler, input.UV) * baseColor; }\n";

	ShaderCreateInfo shaderCI;
	shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	shaderCI.EntryPoint = "main";
	shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
	shaderCI.Source = vertexSource;
	g_device->CreateShader(shaderCI, &g_scene.vertexShader);

	shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
	shaderCI.Source = pixelSource;
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

	BufferDesc vertexDesc;
	vertexDesc.Name = "PlaceholderVertices";
	vertexDesc.BindFlags = BIND_VERTEX_BUFFER;
	vertexDesc.Usage = USAGE_IMMUTABLE;
	vertexDesc.Size = (Uint32)(vertices.size() * sizeof(SceneVertex));
	BufferData vertexData;
	vertexData.pData = vertices.data();
	vertexData.DataSize = vertexDesc.Size;
	g_device->CreateBuffer(vertexDesc, &vertexData, &item.vertices);
	if (!item.vertices)
		return 0;
	item.vertexCount = (Uint32)vertices.size();

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
	g_device->CreateTexture(textureDesc, &textureData, &item.texture);
	if (!item.texture)
		return 0;

	SamplerDesc samplerDesc;
	g_device->CreateSampler(samplerDesc, &item.sampler);
	if (!item.sampler)
		return 0;

	if (!CreateItemShaderBindings(item))
		return 0;

	item.ready = 1;
	g_scene.placeholder = item;
	return 1;
}

// Builds a draw item from a converted GLB record. Everything is created into a
// local item so a failure releases it without touching the live content.
int CreateContentItem(const GltfMeshData& mesh, SceneDrawItem& out,
	char* error, int errorCapacity)
{
	const size_t vertexCount = mesh.positions.size() / 3;
	if (vertexCount == 0)
	{
		SetError(error, errorCapacity, "the converted mesh has no vertices");
		return 0;
	}

	std::vector<SceneVertex> vertices(vertexCount);
	for (size_t i = 0; i < vertexCount; ++i)
	{
		SceneVertex& vertex = vertices[i];
		vertex.x = mesh.positions[i * 3 + 0];
		vertex.y = mesh.positions[i * 3 + 1];
		vertex.z = mesh.positions[i * 3 + 2];
		// Normals are deliberately not fetched yet: this E03 slice is unlit and
		// the vertex format stays position + UV; lighting/PBR is a later block.
		if (mesh.uvs.size() >= (i + 1) * 2)
		{
			vertex.u = mesh.uvs[i * 2 + 0];
			vertex.v = mesh.uvs[i * 2 + 1];
		}
		else
		{
			vertex.u = 0.0f;
			vertex.v = 0.0f;
		}
	}

	SceneDrawItem item;

	BufferDesc vertexDesc;
	vertexDesc.Name = "ContentVertices";
	vertexDesc.BindFlags = BIND_VERTEX_BUFFER;
	vertexDesc.Usage = USAGE_IMMUTABLE;
	vertexDesc.Size = (Uint32)(vertices.size() * sizeof(SceneVertex));
	BufferData vertexData;
	vertexData.pData = vertices.data();
	vertexData.DataSize = vertexDesc.Size;
	g_device->CreateBuffer(vertexDesc, &vertexData, &item.vertices);
	if (!item.vertices)
	{
		SetError(error, errorCapacity, "the converted vertex buffer could not be created");
		return 0;
	}
	item.vertexCount = (Uint32)vertices.size();

	if (!mesh.indices.empty())
	{
		BufferDesc indexDesc;
		indexDesc.Name = "ContentIndices";
		indexDesc.BindFlags = BIND_INDEX_BUFFER;
		indexDesc.Usage = USAGE_IMMUTABLE;
		indexDesc.Size = (Uint32)(mesh.indices.size() * sizeof(unsigned int));
		BufferData indexData;
		indexData.pData = mesh.indices.data();
		indexData.DataSize = indexDesc.Size;
		g_device->CreateBuffer(indexDesc, &indexData, &item.indices);
		if (!item.indices)
		{
			SetError(error, errorCapacity, "the converted index buffer could not be created");
			return 0;
		}
		item.indexCount = (Uint32)mesh.indices.size();
	}

	if (!mesh.baseColorRGBA.empty() && mesh.baseColorWidth > 0 && mesh.baseColorHeight > 0)
	{
		TextureDesc textureDesc;
		textureDesc.Name = "ContentBaseColor";
		textureDesc.Type = RESOURCE_DIM_TEX_2D;
		textureDesc.Width = (Uint32)mesh.baseColorWidth;
		textureDesc.Height = (Uint32)mesh.baseColorHeight;
		textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
		textureDesc.BindFlags = BIND_SHADER_RESOURCE;
		textureDesc.Usage = USAGE_IMMUTABLE;
		TextureSubResData subResource;
		subResource.pData = mesh.baseColorRGBA.data();
		subResource.Stride = (Uint32)mesh.baseColorWidth * 4;
		TextureData textureData;
		textureData.pSubResources = &subResource;
		textureData.NumSubresources = 1;
		g_device->CreateTexture(textureDesc, &textureData, &item.texture);
	}
	else
	{
		// No embedded image: a white texel keeps the base-colour factor visible.
		const unsigned char white[4] = { 255, 255, 255, 255 };
		TextureDesc textureDesc;
		textureDesc.Name = "ContentWhite";
		textureDesc.Type = RESOURCE_DIM_TEX_2D;
		textureDesc.Width = 1;
		textureDesc.Height = 1;
		textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
		textureDesc.BindFlags = BIND_SHADER_RESOURCE;
		textureDesc.Usage = USAGE_IMMUTABLE;
		TextureSubResData subResource;
		subResource.pData = white;
		subResource.Stride = 4;
		TextureData textureData;
		textureData.pSubResources = &subResource;
		textureData.NumSubresources = 1;
		g_device->CreateTexture(textureDesc, &textureData, &item.texture);
	}
	if (!item.texture)
	{
		SetError(error, errorCapacity, "the converted base-colour texture could not be created");
		return 0;
	}

	SamplerDesc samplerDesc;
	g_device->CreateSampler(samplerDesc, &item.sampler);
	if (!item.sampler)
	{
		SetError(error, errorCapacity, "the converted content sampler could not be created");
		return 0;
	}

	if (!CreateItemShaderBindings(item))
	{
		SetError(error, errorCapacity, "the converted content shader bindings could not be created");
		return 0;
	}

	for (int i = 0; i < 16; ++i)
		item.transform[i] = mesh.transform[i];
	for (int i = 0; i < 4; ++i)
		item.baseColor[i] = mesh.baseColorFactor[i];
	item.triangleCount = mesh.triangleCount;

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

// Shared scene draw used by RenderScene (swap chain) and RenderSceneToRGBA
// (offscreen). Draws only the loaded content when one is present so the
// pixel-level tests stay deterministic; the placeholder is the fallback.
int RenderSceneInto(ITextureView* renderTarget, ITextureView* depthView,
	Uint32 width, Uint32 height, int offscreenTarget)
{
	const SceneDrawItem& item = g_scene.content.ready ? g_scene.content : g_scene.placeholder;
	IPipelineState* pipeline = offscreenTarget
		? g_scene.offscreenPipeline.RawPtr()
		: g_scene.pipeline.RawPtr();
	IShaderResourceBinding* srb = offscreenTarget
		? item.srbOffscreen.RawPtr()
		: item.srb.RawPtr();
	if (!item.ready || !pipeline || !srb || !item.vertices)
		return 0;

	// MVP = projection * view * model. The model is the content node transform
	// (identity for the placeholder); world units remain raw game units.
	float viewModel[16];
	MultiplyColumnMajor(g_view, item.transform, viewModel);
	float projection[16];
	const float aspect = height > 0 ? (float)width / (float)height : 1.0f;
	BuildPerspective(g_fovY, aspect, 0.02f, 300.0f, projection);
	float mvp[16];
	MultiplyColumnMajor(projection, viewModel, mvp);

	SceneConstants constants;
	for (int i = 0; i < 16; ++i)
		constants.mvp[i] = mvp[i];
	for (int i = 0; i < 4; ++i)
		constants.baseColor[i] = item.baseColor[i];
	g_context->UpdateBuffer(g_scene.constants, 0, sizeof(SceneConstants), &constants,
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	const float clearColor[4] = { 0.05f, 0.07f, 0.12f, 1.0f };
	g_context->SetRenderTargets(1, &renderTarget, depthView, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	g_context->ClearRenderTarget(renderTarget, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	g_context->ClearDepthStencil(depthView, CLEAR_DEPTH_FLAG, 1.0f, 0,
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	g_context->SetPipelineState(pipeline);
	g_context->CommitShaderResources(srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	IBuffer* vertexBuffers[1] = { item.vertices.RawPtr() };
	g_context->SetVertexBuffers(0, 1, vertexBuffers, nullptr,
		RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	if (item.indices)
	{
		g_context->SetIndexBuffer(item.indices, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		DrawIndexedAttribs draw;
		draw.NumIndices = item.indexCount;
		draw.IndexType = VT_UINT32;
		draw.Flags = DRAW_FLAG_NONE;
		g_context->DrawIndexed(draw);
	}
	else
	{
		DrawAttribs draw;
		draw.NumVertices = item.vertexCount;
		draw.Flags = DRAW_FLAG_NONE;
		g_context->Draw(draw);
	}
	return 1;
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
	g_context->SetRenderTargets(1, &renderTarget, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	g_context->ClearRenderTarget(renderTarget, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	g_context->Flush();
	g_swapChain->Present();
	return 1;
}

void NativeRenderer_SetCamera(const float view[16], float fovYRadians)
{
	if (!view)
		return;
	for (int i = 0; i < 16; ++i)
		g_view[i] = view[i];
	if (fovYRadians > 0.01f)
		g_fovY = fovYRadians;
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

	const SwapChainDesc& swapDesc = g_swapChain->GetDesc();
	if (!RenderSceneInto(renderTarget, g_scene.depthView, swapDesc.Width, swapDesc.Height, 0))
	{
		SetError(error, errorCapacity, "the native scene could not be drawn");
		return 0;
	}
	g_context->Flush();
	g_swapChain->Present();
	return 1;
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

	GltfMeshData mesh;
	if (Gltf_LoadFile(glbPath, &mesh, error, errorCapacity) != 0)
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
	ReleaseSceneResources();
	g_swapChain.Release();
	g_context.Release();
	g_device.Release();
	g_factory = nullptr;
	g_active = 0;
}

#endif // _WIN32
