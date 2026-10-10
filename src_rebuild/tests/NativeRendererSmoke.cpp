// E03 native renderer smoke test.
//
// Part 1: creates a hidden Win32 window and drives the project-owned Diligent
// device/swapchain/present owner through creation, three presented frames, a
// resize and a clean shutdown.
//
// Part 2: loads E02 converted content (a GLB written and read back through the
// project-owned glTF writer/importer) and uploads its geometry into an
// immutable Diligent vertex buffer, then reads it back through a staging buffer
// and checks the bytes match, proving converted content reaches GPU resources.
//
// Part 3: loads converted content through NativeRenderer_LoadContent and draws
// it with the game camera into an offscreen RGBA8 target, then checks the
// readback pixels, camera response and the content release/reload transitions.
//
// It requires a Vulkan-capable GPU/driver and the Diligent libraries built by
// scripts/fetch_diligent.ps1; it needs no game assets. Build and run
// instructions are in tests/README.md.

#include "../native/NativeRenderer.h"
#include "../utils/GltfLoader.h"
#include "../utils/GltfWriter.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Buffer.h"
#include "Texture.h"
#include "Sampler.h"
#include "Shader.h"
#include "PipelineState.h"
#include "InputLayout.h"
#include "ShaderResourceBinding.h"
#include "ShaderResourceVariable.h"
#include "RefCntAutoPtr.hpp"
#include "GraphicsTypes.h"

using namespace Diligent;

static int g_checks = 0;
static int g_failures = 0;

static void Check(bool passed, const char* description)
{
	g_checks++;
	printf("%s: %s\n", passed ? "PASS" : "FAIL", description);
	if (!passed)
		g_failures++;
}

static LRESULT CALLBACK SmokeWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	return DefWindowProcA(window, message, wparam, lparam);
}

// Uploads `bytes` into an immutable buffer of `bindFlags` and reads it back.
static bool UploadAndReadBack(IRenderDevice* device, IDeviceContext* context, const void* bytes,
	Uint32 size, BIND_FLAGS bindFlags)
{
	BufferDesc sourceDesc;
	sourceDesc.Name = "content";
	sourceDesc.BindFlags = bindFlags;
	sourceDesc.Usage = USAGE_IMMUTABLE;
	sourceDesc.Size = size;
	BufferData sourceData;
	sourceData.pData = bytes;
	sourceData.DataSize = size;
	RefCntAutoPtr<IBuffer> source;
	device->CreateBuffer(sourceDesc, &sourceData, &source);
	if (!source)
	{
		printf("    (source buffer creation failed)\n");
		return false;
	}

	BufferDesc stagingDesc;
	stagingDesc.Name = "content-staging";
	stagingDesc.BindFlags = BIND_NONE;
	stagingDesc.Usage = USAGE_STAGING;
	stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
	stagingDesc.Size = size;
	RefCntAutoPtr<IBuffer> staging;
	device->CreateBuffer(stagingDesc, nullptr, &staging);
	if (!staging)
	{
		printf("    (staging buffer creation failed)\n");
		return false;
	}

	context->CopyBuffer(source, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
		staging, 0, size, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	context->Flush();
	context->WaitForIdle();

	void* mapped = nullptr;
	context->MapBuffer(staging, MAP_READ, MAP_FLAG_NONE, mapped);
	if (!mapped)
	{
		printf("    (staging buffer could not be mapped)\n");
		return false;
	}
	const bool equal = memcmp(mapped, bytes, size) == 0;
	if (!equal)
		printf("    (readback differs: first read byte %u, expected %u)\n",
			((const unsigned char*)mapped)[0], ((const unsigned char*)bytes)[0]);
	context->UnmapBuffer(staging, MAP_READ);
	return equal;
}

static void TestContentUpload()
{
	// Build, write and read back a converted mesh through the E02 record.
	GltfMeshData source;
	source.positions = { 0, 0, 0, 100, 0, 0, 0, 100, 0 };
	source.uvs = { 0, 0, 1, 0, 0, 1 };
	source.indices = { 0, 1, 2 };
	source.vertexCount = 3;
	source.triangleCount = 1;

	char error[256];
	if (Gltf_WriteFile("content.glb", source, error, sizeof(error)) != 0)
	{
		Check(false, "converted GLB written");
		return;
	}
	GltfMeshData loaded;
	Check(Gltf_LoadFile("content.glb", &loaded, error, sizeof(error)) == 0 && loaded.vertexCount == 3,
		"converted GLB loaded back");

	IRenderDevice* device = (IRenderDevice*)NativeRenderer_GetDevice();
	IDeviceContext* context = (IDeviceContext*)NativeRenderer_GetContext();
	Check(device != nullptr && context != nullptr, "device and context available");
	if (!device || !context)
		return;

	const Uint32 positionBytes = (Uint32)(loaded.positions.size() * sizeof(float));
	Check(UploadAndReadBack(device, context, loaded.positions.data(), positionBytes, BIND_VERTEX_BUFFER),
		"converted vertex positions uploaded and read back identical");

	const Uint32 indexBytes = (Uint32)(loaded.indices.size() * sizeof(unsigned int));
	Check(UploadAndReadBack(device, context, loaded.indices.data(), indexBytes, BIND_INDEX_BUFFER),
		"converted indices uploaded and read back identical");
}

// Renders two overlapping triangles (the nearer one drawn first) into an
// offscreen RGBA8 target with a depth buffer through a Diligent pipeline
// (runtime-compiled HLSL) and reads the centre pixel back. Depth testing must
// keep the nearer green triangle over the farther red one; per-vertex colour
// exercises a second input attribute.
static bool RenderTriangleToOffscreen(IRenderDevice* device, IDeviceContext* context, unsigned char outPixel[4])
{
	const char* vertexSource =
		"struct VSIn  { float3 Pos : ATTRIB0; float3 Col : ATTRIB1; };\n"
		"struct VSOut { float4 Pos : SV_POSITION; float3 Col : COLOR0; };\n"
		"void main(in VSIn input, out VSOut output)\n"
		"{ output.Pos = float4(input.Pos, 1.0); output.Col = input.Col; }\n";
	const char* pixelSource =
		"struct PSIn { float4 Pos : SV_POSITION; float3 Col : COLOR0; };\n"
		"float4 main(in PSIn input) : SV_TARGET { return float4(input.Col, 1.0); }\n";

	ShaderCreateInfo vsCI;
	vsCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	vsCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
	vsCI.EntryPoint = "main";
	vsCI.Source = vertexSource;
	RefCntAutoPtr<IShader> vertexShader;
	device->CreateShader(vsCI, &vertexShader);

	ShaderCreateInfo psCI;
	psCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	psCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
	psCI.EntryPoint = "main";
	psCI.Source = pixelSource;
	RefCntAutoPtr<IShader> pixelShader;
	device->CreateShader(psCI, &pixelShader);
	if (!vertexShader || !pixelShader)
	{
		printf("    (shader compilation failed)\n");
		return false;
	}

	TextureDesc targetDesc;
	targetDesc.Name = "offscreen";
	targetDesc.Type = RESOURCE_DIM_TEX_2D;
	targetDesc.Width = 64;
	targetDesc.Height = 64;
	targetDesc.Format = TEX_FORMAT_RGBA8_UNORM;
	targetDesc.BindFlags = BIND_RENDER_TARGET;
	targetDesc.Usage = USAGE_DEFAULT;
	RefCntAutoPtr<ITexture> target;
	device->CreateTexture(targetDesc, nullptr, &target);
	if (!target)
		return false;

	LayoutElement elements[2] = {};
	elements[0].InputIndex = 0;
	elements[0].BufferSlot = 0;
	elements[0].NumComponents = 3;
	elements[0].ValueType = VT_FLOAT32;
	elements[0].IsNormalized = False;
	elements[0].RelativeOffset = 0;
	elements[1].InputIndex = 1;
	elements[1].BufferSlot = 0;
	elements[1].NumComponents = 3;
	elements[1].ValueType = VT_FLOAT32;
	elements[1].IsNormalized = False;
	elements[1].RelativeOffset = 3 * sizeof(float);

	TextureDesc depthDesc;
	depthDesc.Name = "depth";
	depthDesc.Type = RESOURCE_DIM_TEX_2D;
	depthDesc.Width = 64;
	depthDesc.Height = 64;
	depthDesc.Format = TEX_FORMAT_D32_FLOAT;
	depthDesc.BindFlags = BIND_DEPTH_STENCIL;
	depthDesc.Usage = USAGE_DEFAULT;
	RefCntAutoPtr<ITexture> depth;
	device->CreateTexture(depthDesc, nullptr, &depth);
	if (!depth)
		return false;

	GraphicsPipelineStateCreateInfo psoCI;
	psoCI.PSODesc.Name = "smoke-depth";
	psoCI.GraphicsPipeline.NumRenderTargets = 1;
	psoCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
	psoCI.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
	psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
	psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
	psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
	psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;
	psoCI.GraphicsPipeline.InputLayout.LayoutElements = elements;
	psoCI.GraphicsPipeline.InputLayout.NumElements = 2;
	psoCI.pVS = vertexShader;
	psoCI.pPS = pixelShader;
	RefCntAutoPtr<IPipelineState> pipeline;
	device->CreateGraphicsPipelineState(psoCI, &pipeline);
	if (!pipeline)
	{
		printf("    (pipeline creation failed)\n");
		return false;
	}

	// Position (3) + colour (3). The green triangle is nearer (z = 0.2) and is
	// drawn first; the red one is farther (z = 0.5). Depth testing must keep the
	// green pixels even though red is drawn last.
	const float vertices[] = {
		-0.8f, 0.8f, 0.2f, 0.0f, 1.0f, 0.0f,
		 0.8f, 0.8f, 0.2f, 0.0f, 1.0f, 0.0f,
		 0.0f, -0.8f, 0.2f, 0.0f, 1.0f, 0.0f,
		-0.8f, -0.8f, 0.5f, 1.0f, 0.0f, 0.0f,
		 0.8f, -0.8f, 0.5f, 1.0f, 0.0f, 0.0f,
		 0.0f, 0.8f, 0.5f, 1.0f, 0.0f, 0.0f,
	};
	BufferDesc vertexDesc;
	vertexDesc.Name = "triangles";
	vertexDesc.BindFlags = BIND_VERTEX_BUFFER;
	vertexDesc.Usage = USAGE_IMMUTABLE;
	vertexDesc.Size = sizeof(vertices);
	BufferData vertexData;
	vertexData.pData = vertices;
	vertexData.DataSize = sizeof(vertices);
	RefCntAutoPtr<IBuffer> vertexBuffer;
	device->CreateBuffer(vertexDesc, &vertexData, &vertexBuffer);
	if (!vertexBuffer)
		return false;

	ITextureView* renderTarget = target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
	ITextureView* depthTarget = depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	context->SetRenderTargets(1, &renderTarget, depthTarget, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	context->ClearRenderTarget(renderTarget, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	context->ClearDepthStencil(depthTarget, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	context->SetPipelineState(pipeline);
	IBuffer* vertexBuffers[1] = { vertexBuffer };
	context->SetVertexBuffers(0, 1, vertexBuffers, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	DrawAttribs draw;
	draw.NumVertices = 6;
	draw.Flags = DRAW_FLAG_NONE;
	context->Draw(draw);

	TextureDesc stagingDesc = targetDesc;
	stagingDesc.Name = "offscreen-staging";
	stagingDesc.BindFlags = BIND_NONE;
	stagingDesc.Usage = USAGE_STAGING;
	stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
	RefCntAutoPtr<ITexture> staging;
	device->CreateTexture(stagingDesc, nullptr, &staging);
	if (!staging)
		return false;

	CopyTextureAttribs copy;
	copy.pSrcTexture = target;
	copy.pDstTexture = staging;
	copy.SrcMipLevel = 0;
	copy.DstMipLevel = 0;
	copy.SrcSlice = 0;
	copy.DstSlice = 0;
	context->CopyTexture(copy);
	context->Flush();
	context->WaitForIdle();

	MappedTextureSubresource mapped = {};
	context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
	if (!mapped.pData)
		return false;
	const unsigned char* center = (const unsigned char*)mapped.pData + (size_t)32 * mapped.Stride + (size_t)32 * 4;
	outPixel[0] = center[0];
	outPixel[1] = center[1];
	outPixel[2] = center[2];
	outPixel[3] = center[3];
	context->UnmapTextureSubresource(staging, 0, 0);
	return true;
}

// Renders a textured quad through a pipeline whose vertex shader applies a
// constant-buffer MVP (the world-camera seam) and whose pixel shader samples a
// base-colour texture, returning the centre pixel.
static bool RenderTexturedQuadToOffscreen(IRenderDevice* device, IDeviceContext* context,
	const float mvp[16], unsigned char outPixel[4])
{
	const char* vertexSource =
		"cbuffer Constants { float4x4 MVP; };\n"
		"struct VSIn  { float3 Pos : ATTRIB0; float2 UV : ATTRIB1; };\n"
		"struct VSOut { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };\n"
		"void main(in VSIn input, out VSOut output)\n"
		"{ output.Pos = mul(MVP, float4(input.Pos, 1.0)); output.UV = input.UV; }\n";
	const char* pixelSource =
		"Texture2D    gTexture;\n"
		"SamplerState gSampler;\n"
		"struct PSIn { float4 Pos : SV_POSITION; float2 UV : TEXCOORD0; };\n"
		"float4 main(in PSIn input) : SV_TARGET { return gTexture.Sample(gSampler, input.UV); }\n";

	ShaderCreateInfo vsCI;
	vsCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	vsCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
	vsCI.EntryPoint = "main";
	vsCI.Source = vertexSource;
	RefCntAutoPtr<IShader> vertexShader;
	device->CreateShader(vsCI, &vertexShader);

	ShaderCreateInfo psCI;
	psCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	psCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
	psCI.EntryPoint = "main";
	psCI.Source = pixelSource;
	RefCntAutoPtr<IShader> pixelShader;
	device->CreateShader(psCI, &pixelShader);
	if (!vertexShader || !pixelShader)
	{
		printf("    (textured shader compilation failed)\n");
		return false;
	}

	BufferDesc cbDesc;
	cbDesc.Name = "Constants";
	cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
	cbDesc.Usage = USAGE_DYNAMIC;
	cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
	cbDesc.Size = 16 * sizeof(float);
	RefCntAutoPtr<IBuffer> constants;
	device->CreateBuffer(cbDesc, nullptr, &constants);
	if (!constants)
		return false;
	context->UpdateBuffer(constants, 0, cbDesc.Size, mvp, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	const unsigned char texels[8] = { 255, 0, 0, 255, 0, 255, 0, 255 };
	TextureDesc textureDesc;
	textureDesc.Name = "baseColor";
	textureDesc.Type = RESOURCE_DIM_TEX_2D;
	textureDesc.Width = 2;
	textureDesc.Height = 1;
	textureDesc.Format = TEX_FORMAT_RGBA8_UNORM;
	textureDesc.BindFlags = BIND_SHADER_RESOURCE;
	textureDesc.Usage = USAGE_IMMUTABLE;
	TextureSubResData subResource;
	subResource.pData = texels;
	subResource.Stride = 2 * 4;
	TextureData textureData;
	textureData.pSubResources = &subResource;
	textureData.NumSubresources = 1;
	RefCntAutoPtr<ITexture> texture;
	device->CreateTexture(textureDesc, &textureData, &texture);
	if (!texture)
		return false;

	SamplerDesc samplerDesc;
	RefCntAutoPtr<ISampler> sampler;
	device->CreateSampler(samplerDesc, &sampler);
	if (!sampler)
		return false;

	TextureDesc targetDesc;
	targetDesc.Name = "textured-offscreen";
	targetDesc.Type = RESOURCE_DIM_TEX_2D;
	targetDesc.Width = 64;
	targetDesc.Height = 64;
	targetDesc.Format = TEX_FORMAT_RGBA8_UNORM;
	targetDesc.BindFlags = BIND_RENDER_TARGET;
	targetDesc.Usage = USAGE_DEFAULT;
	RefCntAutoPtr<ITexture> target;
	device->CreateTexture(targetDesc, nullptr, &target);
	if (!target)
		return false;

	LayoutElement elements[2] = {};
	elements[0].InputIndex = 0;
	elements[0].BufferSlot = 0;
	elements[0].NumComponents = 3;
	elements[0].ValueType = VT_FLOAT32;
	elements[0].IsNormalized = False;
	elements[0].RelativeOffset = 0;
	elements[1].InputIndex = 1;
	elements[1].BufferSlot = 0;
	elements[1].NumComponents = 2;
	elements[1].ValueType = VT_FLOAT32;
	elements[1].IsNormalized = False;
	elements[1].RelativeOffset = 3 * sizeof(float);

	ShaderResourceVariableDesc variables[3] = {};
	variables[0].Name = "Constants";
	variables[0].ShaderStages = SHADER_TYPE_VERTEX;
	variables[0].Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	variables[1].Name = "gTexture";
	variables[1].ShaderStages = SHADER_TYPE_PIXEL;
	variables[1].Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	variables[2].Name = "gSampler";
	variables[2].ShaderStages = SHADER_TYPE_PIXEL;
	variables[2].Type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

	GraphicsPipelineStateCreateInfo psoCI;
	psoCI.PSODesc.Name = "smoke-textured";
	psoCI.PSODesc.ResourceLayout.Variables = variables;
	psoCI.PSODesc.ResourceLayout.NumVariables = 3;
	psoCI.GraphicsPipeline.NumRenderTargets = 1;
	psoCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
	psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
	psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
	psoCI.GraphicsPipeline.InputLayout.LayoutElements = elements;
	psoCI.GraphicsPipeline.InputLayout.NumElements = 2;
	psoCI.pVS = vertexShader;
	psoCI.pPS = pixelShader;
	RefCntAutoPtr<IPipelineState> pipeline;
	device->CreateGraphicsPipelineState(psoCI, &pipeline);
	if (!pipeline)
	{
		printf("    (textured pipeline creation failed)\n");
		return false;
	}

	RefCntAutoPtr<IShaderResourceBinding> srb;
	pipeline->CreateShaderResourceBinding(&srb, true);
	srb->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(constants);
	srb->GetVariableByName(SHADER_TYPE_PIXEL, "gTexture")->Set(texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	srb->GetVariableByName(SHADER_TYPE_PIXEL, "gSampler")->Set(sampler);

	// Full-viewport quad; UVs stay in [0, 0.5) so every sample hits texel 0 (red).
	const float vertices[] = {
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f, 0.5f, 0.0f,
		 1.0f,  1.0f, 0.0f, 0.5f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 0.5f, 1.0f,
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
	};
	BufferDesc vertexDesc;
	vertexDesc.Name = "quad";
	vertexDesc.BindFlags = BIND_VERTEX_BUFFER;
	vertexDesc.Usage = USAGE_IMMUTABLE;
	vertexDesc.Size = sizeof(vertices);
	BufferData vertexData;
	vertexData.pData = vertices;
	vertexData.DataSize = sizeof(vertices);
	RefCntAutoPtr<IBuffer> vertexBuffer;
	device->CreateBuffer(vertexDesc, &vertexData, &vertexBuffer);
	if (!vertexBuffer)
		return false;

	ITextureView* renderTarget = target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	context->SetRenderTargets(1, &renderTarget, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	context->ClearRenderTarget(renderTarget, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	context->SetPipelineState(pipeline);
	context->CommitShaderResources(srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	IBuffer* vertexBuffers[1] = { vertexBuffer };
	context->SetVertexBuffers(0, 1, vertexBuffers, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	DrawAttribs draw;
	draw.NumVertices = 6;
	draw.Flags = DRAW_FLAG_NONE;
	context->Draw(draw);

	TextureDesc stagingDesc = targetDesc;
	stagingDesc.Name = "textured-staging";
	stagingDesc.BindFlags = BIND_NONE;
	stagingDesc.Usage = USAGE_STAGING;
	stagingDesc.CPUAccessFlags = CPU_ACCESS_READ;
	RefCntAutoPtr<ITexture> staging;
	device->CreateTexture(stagingDesc, nullptr, &staging);
	if (!staging)
		return false;

	CopyTextureAttribs copy;
	copy.pSrcTexture = target;
	copy.pDstTexture = staging;
	context->CopyTexture(copy);
	context->Flush();
	context->WaitForIdle();

	MappedTextureSubresource mapped = {};
	context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
	if (!mapped.pData)
		return false;
	const unsigned char* center = (const unsigned char*)mapped.pData + (size_t)32 * mapped.Stride + (size_t)32 * 4;
	outPixel[0] = center[0];
	outPixel[1] = center[1];
	outPixel[2] = center[2];
	outPixel[3] = center[3];
	context->UnmapTextureSubresource(staging, 0, 0);
	return true;
}

static void TestTexturedQuad()
{
	IRenderDevice* device = (IRenderDevice*)NativeRenderer_GetDevice();
	IDeviceContext* context = (IDeviceContext*)NativeRenderer_GetContext();
	if (!device || !context)
	{
		Check(false, "device and context available for textured draw");
		return;
	}

	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	unsigned char pixel[4] = {};
	const bool rendered = RenderTexturedQuadToOffscreen(device, context, identity, pixel);
	Check(rendered && pixel[0] >= 239 && pixel[1] <= 16 && pixel[2] <= 16 && pixel[3] == 255,
		"textured quad sampled the base-colour texture (red texel)");
	if (!rendered)
		printf("    (textured quad render failed)\n");

	// Moving the quad off-screen through the MVP must leave the centre clear,
	// proving the constant-buffer camera transform is actually applied.
	const float translated[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1 };
	unsigned char moved[4] = {};
	const bool renderedMoved = RenderTexturedQuadToOffscreen(device, context, translated, moved);
	Check(renderedMoved && moved[0] <= 16 && moved[1] <= 16 && moved[2] <= 16 && moved[3] == 255,
		"MVP camera transform moved the quad off the centre pixel");
}

// True when no pixel of an RGBA8 image looks like the fixture's red material.
static bool NoRedPixels(const unsigned char* rgba, int pixelCount)
{
	for (int i = 0; i < pixelCount; ++i)
	{
		const unsigned char* pixel = rgba + (size_t)i * 4;
		if (pixel[0] >= 200 && pixel[1] <= 64 && pixel[2] <= 64)
			return false;
	}
	return true;
}

// E03-A: drives converted E02 content through the renderer's own scene path.
// The quad's world placement lives only in the GLB node transform (z = -5) and
// the camera comes through SetCamera, so a red centre pixel proves both the
// content and the camera reached the draw; moving the camera must move it off
// the centre. Content is then released and re-loaded to cover the ownership
// transitions.
static void TestConvertedContentScene()
{
	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

	GltfMeshData quad;
	quad.positions = { -1, -1, 0,  1, -1, 0,  1, 1, 0,  -1, 1, 0 };
	quad.uvs = { 0, 0,  1, 0,  1, 1,  0, 1 };
	quad.indices = { 0, 1, 2,  0, 2, 3 };
	quad.vertexCount = 4;
	quad.triangleCount = 2;
	quad.baseColorFactor[0] = 1.0f;
	quad.baseColorFactor[1] = 0.0f;
	quad.baseColorFactor[2] = 0.0f;
	quad.baseColorFactor[3] = 1.0f;
	for (int i = 0; i < 16; ++i)
		quad.transform[i] = identity[i];
	quad.transform[14] = -5.0f; // the only placement: translate to z = -5

	char error[256];
	Check(Gltf_WriteFile("content-scene.glb", quad, error, sizeof(error)) == 0,
		"scene fixture GLB written");

	NativeRenderer_SetCamera(identity, 1.0472f); // 60 degrees

	Check(NativeRenderer_LoadContent("content-scene.glb", error, sizeof(error)) == 1,
		"converted content loaded for drawing");
	Check(NativeRenderer_HasContent() == 1, "renderer reports converted content loaded");
	int vertices = 0, triangles = 0;
	NativeRenderer_GetContentStats(&vertices, &triangles);
	Check(vertices == 4 && triangles == 2, "content stats report 4 vertices / 2 triangles");

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"converted content rendered offscreen");
	const unsigned char* centre = &image[((32 * 64) + 32) * 4];
	const bool red = centre[0] >= 239 && centre[1] <= 16 && centre[2] <= 16 && centre[3] == 255;
	if (!red)
		printf("    (centre pixel %u,%u,%u,%u, expected red from the base-colour factor)\n",
			centre[0], centre[1], centre[2], centre[3]);
	Check(red, "offscreen centre pixel is the converted quad's base colour");

	// Move the camera the way the game stores view[12..14] (full world->camera
	// translation, raw game units) and re-render: the quad must leave the centre.
	float movedView[16];
	for (int i = 0; i < 16; ++i)
		movedView[i] = identity[i];
	movedView[12] = -1000.0f;
	NativeRenderer_SetCamera(movedView, 1.0472f);

	unsigned char movedImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(movedImage, 64, 64, error, sizeof(error)) == 1,
		"converted content re-rendered with the moved camera");
	const unsigned char* movedCentre = &movedImage[((32 * 64) + 32) * 4];
	Check(!(movedCentre[0] >= 239 && movedCentre[1] <= 16 && movedCentre[2] <= 16 && movedCentre[3] == 255),
		"camera translation moved the converted quad off the centre pixel");
	Check(NoRedPixels(movedImage, 64 * 64), "no converted content pixels remain after the camera moved");

	// A failed load must leave the loaded content and its stats untouched.
	char loadError[256] = "";
	Check(NativeRenderer_LoadContent("missing-content.glb", loadError, sizeof(loadError)) == 0 &&
		loadError[0] != '\0', "missing content file reports an error");
	Check(NativeRenderer_HasContent() == 1, "failed load left the loaded content intact");
	vertices = 0;
	triangles = 0;
	NativeRenderer_GetContentStats(&vertices, &triangles);
	Check(vertices == 4 && triangles == 2, "failed load left the content stats intact");

	NativeRenderer_ClearContent();
	Check(NativeRenderer_HasContent() == 0, "content cleared");
	vertices = 7;
	triangles = 7;
	NativeRenderer_GetContentStats(&vertices, &triangles);
	Check(vertices == 0 && triangles == 0, "content stats report zero after clear");

	// With the content released the offscreen path must draw the placeholder;
	// the camera looks level above the ground and pillars, so the centre pixel
	// is the dark-blue scene clear colour (~13,18,31 from 0.05/0.07/0.12).
	unsigned char placeholderImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(placeholderImage, 64, 64, error, sizeof(error)) == 1,
		"placeholder rendered offscreen without converted content");
	const unsigned char* placeholderCentre = &placeholderImage[((32 * 64) + 32) * 4];
	const bool clearBlue = placeholderCentre[0] >= 9 && placeholderCentre[0] <= 17 &&
		placeholderCentre[1] >= 14 && placeholderCentre[1] <= 22 &&
		placeholderCentre[2] >= 27 && placeholderCentre[2] <= 35 &&
		placeholderCentre[3] == 255;
	if (!clearBlue)
		printf("    (centre pixel %u,%u,%u,%u, expected the scene clear colour ~13,18,31,255)\n",
			placeholderCentre[0], placeholderCentre[1], placeholderCentre[2], placeholderCentre[3]);
	Check(clearBlue, "placeholder scene renders without converted content");

	Check(NativeRenderer_RenderScene(error, sizeof(error)) == 1,
		"placeholder scene still renders after content release");

	// Re-loading the same content twice must replace the old resources cleanly.
	Check(NativeRenderer_LoadContent("content-scene.glb", error, sizeof(error)) == 1,
		"content loaded again after clear");
	Check(NativeRenderer_LoadContent("content-scene.glb", error, sizeof(error)) == 1,
		"content re-loaded twice in a row");
	vertices = 0;
	triangles = 0;
	NativeRenderer_GetContentStats(&vertices, &triangles);
	Check(vertices == 4 && triangles == 2, "stats stay correct after resource replacement");

	NativeRenderer_SetCamera(identity, 1.0472f);
	unsigned char reloadedImage[64 * 64 * 4] = {};
	const bool reloaded = NativeRenderer_RenderSceneToRGBA(reloadedImage, 64, 64, error, sizeof(error)) == 1;
	const unsigned char* reloadedCentre = &reloadedImage[((32 * 64) + 32) * 4];
	Check(reloaded && reloadedCentre[0] >= 239 && reloadedCentre[1] <= 16 &&
		reloadedCentre[2] <= 16 && reloadedCentre[3] == 255,
		"replaced content resources still draw the quad");
}

// E03-B2.5: pins the decoder's winding to the renderer's cull mode. The decoder
// emits a world quad as (v0,v1,v3,v2) and fans it (0,1,2)+(0,2,3); the first
// triangle (v0,v1,v3) has a positive signed area in the y-up NDC for a
// front-facing face. The scene pipelines cull back faces with CULL_MODE_BACK and
// FrontCounterClockwise = True (native/NativeRenderer.cpp), so a front-winding
// quad must be visible and a reversed back-winding quad must be culled. Both
// quads sit at z = -5 under an identity camera.
static void TestWindingCullPolicy()
{
	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identity, 1.0472f); // 60 degrees

	GltfMeshData quad;
	// Symmetric proxy quad stored in boundary order; the fan below is the same
	// (0,1,2)+(0,2,3) emission the decoder produces, and the pure winding
	// fixture pins the (v0,v1,v3,v2) vertex mapping.
	quad.positions = { -1, -1, 0,  1, -1, 0,  1, 1, 0,  -1, 1, 0 };
	quad.uvs = { 0, 0,  1, 0,  1, 1,  0, 1 };
	quad.vertexCount = 4;
	quad.triangleCount = 2;
	quad.baseColorFactor[0] = 1.0f;
	quad.baseColorFactor[1] = 0.0f;
	quad.baseColorFactor[2] = 0.0f;
	quad.baseColorFactor[3] = 1.0f;
	for (int i = 0; i < 16; ++i)
		quad.transform[i] = identity[i];
	quad.transform[14] = -5.0f;

	char error[256];

	// Front winding: the same (0,1,2)+(0,2,3) fan the decoder emits for a quad.
	GltfMeshData front = quad;
	front.indices = { 0, 1, 2,  0, 2, 3 };
	Check(Gltf_WriteFile("winding-front.glb", front, error, sizeof(error)) == 0,
		"front-winding GLB written");
	Check(NativeRenderer_LoadContent("winding-front.glb", error, sizeof(error)) == 1,
		"front-winding content loaded");
	unsigned char frontImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(frontImage, 64, 64, error, sizeof(error)) == 1,
		"front-winding quad rendered offscreen");
	const unsigned char* frontCentre = &frontImage[((32 * 64) + 32) * 4];
	const bool frontVisible = frontCentre[0] >= 239 && frontCentre[1] <= 16 &&
		frontCentre[2] <= 16 && frontCentre[3] == 255;
	if (!frontVisible)
		printf("    (front centre %u,%u,%u,%u, expected red: front face must survive the cull)\n",
			frontCentre[0], frontCentre[1], frontCentre[2], frontCentre[3]);
	Check(frontVisible, "front-winding quad is kept by the pinned back-face cull");

	// Back winding: reversed index order gives a negative NDC signed area and
	// must be culled, leaving the scene clear colour at the centre.
	GltfMeshData back = quad;
	back.indices = { 0, 2, 1,  0, 3, 2 };
	Check(Gltf_WriteFile("winding-back.glb", back, error, sizeof(error)) == 0,
		"back-winding GLB written");
	Check(NativeRenderer_LoadContent("winding-back.glb", error, sizeof(error)) == 1,
		"back-winding content loaded");
	unsigned char backImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(backImage, 64, 64, error, sizeof(error)) == 1,
		"back-winding quad rendered offscreen");
	const unsigned char* backCentre = &backImage[((32 * 64) + 32) * 4];
	const bool backCulled = backCentre[0] >= 9 && backCentre[0] <= 17 &&
		backCentre[1] >= 14 && backCentre[1] <= 22 &&
		backCentre[2] >= 27 && backCentre[2] <= 35 && backCentre[3] == 255;
	if (!backCulled)
		printf("    (back centre %u,%u,%u,%u, expected the clear colour ~13,18,31,255)\n",
			backCentre[0], backCentre[1], backCentre[2], backCentre[3]);
	Check(backCulled, "back-winding quad is culled by the pinned back-face cull");

	NativeRenderer_ClearContent();
}

static void TestTriangleDraw()
{
	IRenderDevice* device = (IRenderDevice*)NativeRenderer_GetDevice();
	IDeviceContext* context = (IDeviceContext*)NativeRenderer_GetContext();
	if (!device || !context)
	{
		Check(false, "device and context available for draw");
		return;
	}

	unsigned char pixel[4] = {};
	const bool rendered = RenderTriangleToOffscreen(device, context, pixel);
	Check(rendered, "converted-content pipeline rendered to an offscreen target");

	const bool nearExpected = rendered &&
		pixel[0] <= 16 &&
		pixel[1] >= 239 &&
		pixel[2] <= 16 &&
		pixel[3] == 255;
	if (!nearExpected)
		printf("    (centre pixel %u,%u,%u,%u, expected the nearer green ~0,255,0,255)\n", pixel[0], pixel[1], pixel[2], pixel[3]);
	Check(nearExpected, "depth test kept the nearer triangle at the centre pixel");
}

int main()
{
	WNDCLASSA windowClass = {};
	windowClass.lpfnWndProc = SmokeWindowProc;
	windowClass.hInstance = GetModuleHandleA(NULL);
	windowClass.lpszClassName = "REDRIVER2NativeRendererSmoke";
	Check(RegisterClassA(&windowClass) != 0, "window class registered");

	HWND window = CreateWindowExA(0, windowClass.lpszClassName, "smoke",
		WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, NULL, NULL, windowClass.hInstance, NULL);
	Check(window != NULL, "hidden window created");
	if (!window)
	{
		printf("%d checks, %d failures\n", g_checks, g_failures);
		return 1;
	}

	char error[256];
	Check(NativeRenderer_Init(window, 320, 240, error, sizeof(error)) == 1, "device and swap chain created");
	Check(NativeRenderer_IsActive() == 1, "renderer reports active");

	bool presented = true;
	for (int i = 0; i < 3 && presented; ++i)
		presented = NativeRenderer_RenderClearAndPresent(0.1f, 0.2f, 0.3f, 1.0f, error, sizeof(error)) == 1;
	Check(presented, "three frames cleared and presented");

	TestContentUpload();
	TestTriangleDraw();
	TestTexturedQuad();
	TestConvertedContentScene();
	TestWindingCullPolicy();

	NativeRenderer_Resize(640, 480);
	Check(NativeRenderer_RenderClearAndPresent(0.3f, 0.1f, 0.2f, 1.0f, error, sizeof(error)) == 1,
		"frame presented after resize");

	// The scene path must survive a swap chain resize (its depth target is
	// recreated) and the offscreen target must follow a size change.
	const float identityView[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identityView, 1.0472f);
	Check(NativeRenderer_RenderScene(error, sizeof(error)) == 1, "scene renders after swap chain resize");

	unsigned char resizedImage[96 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(resizedImage, 96, 64, error, sizeof(error)) == 1,
		"offscreen scene target recreated at a new size");

	NativeRenderer_Shutdown();
	Check(NativeRenderer_IsActive() == 0, "renderer shut down");

	DestroyWindow(window);
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}

#else
int main()
{
	printf("native renderer smoke is Windows-only\n");
	return 0;
}
#endif
