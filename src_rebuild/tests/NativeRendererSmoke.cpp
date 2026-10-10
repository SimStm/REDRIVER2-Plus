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
// Part 4: registers a multi-primitive converted resource and draws it in
// instance mode, checking that every primitive and its base colour reaches the
// target while the instance still counts once.
//
// Part 5: a converted resource with one textured and one untextured primitive
// draws the textured primitive's own image (a red/green split, proving the raw
// texel UVs are scaled by 1/256) and the untextured primitive's base colour.
//
// Part 6: loads a stored-block textured GLB through the multi record (the
// renderer samples the primitive's own image) and rejects a PNG outside the
// stored-block subset with a named diagnostic, leaving the loaded content and
// the scene draw unaffected.
//
// Part 7: world opacity (supersedes the earlier straight-alpha blend checks).
// A near quad with a 4x1 image (opaque red, alpha-128 green, an alpha-0 hole
// and opaque blue) over a farther opaque quad renders through the real offscreen
// pipeline with NO blending: the opaque texels show themselves, the STP=128
// texel shows its full RGB, and the alpha-0 texel is a cutout hole that shows
// the farther quad through it (no depth written, no blend).
//
// It requires a Vulkan-capable GPU/driver and the Diligent libraries built by
// scripts/fetch_diligent.ps1; it needs no game assets. Build and run
// instructions are in tests/README.md.

#include "../native/NativeRenderer.h"
#include "../utils/GltfLoader.h"
#include "../utils/GltfWriter.h"
#include "../utils/NativeCameraView.h"
#include "../utils/NativeInstanceTransform.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

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

// The native camera API takes the live GTE screen distance h (the game's
// scr_z), not a FOV. This h reproduces the previous 60-degree vertical FOV
// under the legacy GTE focal length: 2h/NATIVE_PSX_DISPLAY_HEIGHT ==
// 1/tan(30 degrees) == 1.73205, so the existing pixel expectations stay valid.
static const float kSmokeScreenDistance = 120.0f / 0.57735027f; // 2h/240 = 1/tan(30 deg)

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
// The quad's world placement lives only in the GLB node transform (z = +5,
// ahead of the +Z-forward camera) and the camera comes through SetCamera, so a
// red centre pixel proves both the content and the camera reached the draw;
// moving the camera must move it off the centre. Content is then released and
// re-loaded to cover the ownership transitions.
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
	quad.transform[14] = 5.0f; // the only placement: translate to z = +5 (ahead)

	char error[256];
	Check(Gltf_WriteFile("content-scene.glb", quad, error, sizeof(error)) == 0,
		"scene fixture GLB written");

	NativeRenderer_SetCamera(identity, kSmokeScreenDistance); // 60 degrees

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
	NativeRenderer_SetCamera(movedView, kSmokeScreenDistance);

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

	NativeRenderer_SetCamera(identity, kSmokeScreenDistance);
	unsigned char reloadedImage[64 * 64 * 4] = {};
	const bool reloaded = NativeRenderer_RenderSceneToRGBA(reloadedImage, 64, 64, error, sizeof(error)) == 1;
	const unsigned char* reloadedCentre = &reloadedImage[((32 * 64) + 32) * 4];
	Check(reloaded && reloadedCentre[0] >= 239 && reloadedCentre[1] <= 16 &&
		reloadedCentre[2] <= 16 && reloadedCentre[3] == 255,
		"replaced content resources still draw the quad");
}

// E03 visual parity P1: the scene pipelines cull nothing (CULL_MODE_NONE),
// matching the legacy GPU (PsyX_Vk.cpp sets VK_CULL_MODE_NONE). The game
// pre-culls every primitive in software (gte_nclip / opz > 0 and the
// PLOT_NO_CULL / PLOT_INV_CULL overrides, draw.c:784/796/1156-1165), so the
// renderer must not cull again. A quad and its reversed-winding twin must
// therefore BOTH render under the same camera; culling either one would drop a
// face the game deliberately kept. Both quads sit at z = +5 (ahead) under an
// identity camera.
static void TestNoCullBothFaces()
{
	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identity, kSmokeScreenDistance); // 60 degrees
	NativeRenderer_SetClipPlanes(0.02f, 300.0f); // metre-scale fixtures

	GltfMeshData quad;
	// Stored order c0..c3 is the convex counter-clockwise boundary order; the
	// decoder stores (v0,v1,v3,v2), so c2 is v3 and c3 is v2.
	// c0=(-1,-1), c1=(1,-1), c2=(1,1), c3=(-1,1).
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
	quad.transform[14] = 5.0f;

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
		printf("    (front centre %u,%u,%u,%u, expected red)\n",
			frontCentre[0], frontCentre[1], frontCentre[2], frontCentre[3]);
	Check(frontVisible, "front-winding quad renders");

	// Back winding: reversed index order. With culling disabled it must still
	// render; under the old CULL_MODE_BACK policy this was the culled case.
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
	const bool backVisible = backCentre[0] >= 239 && backCentre[1] <= 16 &&
		backCentre[2] <= 16 && backCentre[3] == 255;
	if (!backVisible)
		printf("    (back centre %u,%u,%u,%u, expected red: no face is culled)\n",
			backCentre[0], backCentre[1], backCentre[2], backCentre[3]);
	Check(backVisible, "back-winding quad renders (the scene pipelines cull nothing)");

	NativeRenderer_ClearContent();
}

// ---------------------------------------------------------------------------
// E03 block 2 item 4: per-instance draw API, resource residency and the
// +Z-forward camera convention. The fixtures are raw-unit rectangles (4096
// units per metre) so the same unit policy as the game applies.
// ---------------------------------------------------------------------------

// Writes a horizontal (x-z plane) rectangle with the decoder's stored order
// (c0=v0, c1=v1, c2=v3, c3=v2), which for a convex quad is the counter-clockwise
// boundary order, and the (0,1,2)+(0,2,3) fan. `backWinding` reverses the index
// order; the scene pipelines cull nothing, so both windings render.
static bool WriteRectangleFixture(const char* path, float halfX, float halfZ,
	bool backWinding, const float rgba[4], char* error, int errorCapacity)
{
	GltfMeshData quad;
	quad.positions = { -halfX, 0, -halfZ,  halfX, 0, -halfZ,  halfX, 0, halfZ,  -halfX, 0, halfZ };
	quad.uvs = { 0, 0,  1, 0,  0, 1,  1, 1 };
	quad.vertexCount = 4;
	quad.triangleCount = 2;
	if (backWinding)
		quad.indices = { 0, 2, 1,  0, 3, 2 };
	else
		quad.indices = { 0, 1, 2,  0, 2, 3 };
	quad.baseColorFactor[0] = rgba[0];
	quad.baseColorFactor[1] = rgba[1];
	quad.baseColorFactor[2] = rgba[2];
	quad.baseColorFactor[3] = rgba[3];
	return Gltf_WriteFile(path, quad, error, errorCapacity) == 0;
}

// Writes a vertical (x-y plane) marker quad facing a camera that looks down +Z,
// with the same front winding as the decoder's world quads (c0=v0, c1=v1,
// c2=v3, c3=v2 and the (0,1,2)+(0,2,3) fan). Used by the vertical-convention
// pin so a marker's camera-space y is exactly its world y under an identity
// camera.
static bool WriteVerticalMarkerFixture(const char* path, float halfSize,
	const float rgba[4], char* error, int errorCapacity)
{
	GltfMeshData quad;
	quad.positions = { -halfSize, -halfSize, 0,  halfSize, -halfSize, 0,
		halfSize, halfSize, 0,  -halfSize, halfSize, 0 };
	quad.uvs = { 0, 0,  1, 0,  1, 1,  0, 1 };
	quad.vertexCount = 4;
	quad.triangleCount = 2;
	quad.indices = { 0, 1, 2,  0, 2, 3 };
	quad.baseColorFactor[0] = rgba[0];
	quad.baseColorFactor[1] = rgba[1];
	quad.baseColorFactor[2] = rgba[2];
	quad.baseColorFactor[3] = rgba[3];
	return Gltf_WriteFile(path, quad, error, errorCapacity) == 0;
}

static void MakeInstance(NativeInstance& instance, const char* key,
	const float pos[3], unsigned int yang, int level, int variant,
	int sourceObjectIndex, int modelSlot, int modelRevision)
{
	snprintf(instance.resourceKey, sizeof(instance.resourceKey), "%s", key);
	const int ipos[3] = { (int)pos[0], (int)pos[1], (int)pos[2] };
	NativeInstance_ComposeTransform(ipos, yang, instance.transform);
	instance.level = level;
	instance.variant = variant;
	instance.sourceObjectIndex = sourceObjectIndex;
	instance.modelSlot = modelSlot;
	instance.modelRevision = modelRevision;
}

// A top-down game-style camera at (0, height, 0) looking down -Y: world +X maps
// to screen right and camera depth = height - world.y. World +Z maps to
// camera-space +y, which under the game's +Y-down convention (see
// NativeRenderer.cpp BuildPerspective) appears toward the BOTTOM of the screen.
static void BuildTopDownView(float height, float view[16])
{
	view[0] = 1.0f;  view[4] = 0.0f;  view[8] = 0.0f;   view[12] = 0.0f;
	view[1] = 0.0f;  view[5] = 0.0f;  view[9] = 1.0f;   view[13] = 0.0f;
	view[2] = 0.0f;  view[6] = -1.0f; view[10] = 0.0f;  view[14] = height;
	view[3] = 0.0f;  view[7] = 0.0f;  view[11] = 0.0f;  view[15] = 1.0f;
}

// An angled game-style camera at (0, height, zOffset), pitched down by
// `pitch` so it looks forward (+Z) and down at the ground; camera depth grows
// with world z, so "ahead" is +Z and "behind" is negative camera z.
static void BuildAngledView(float height, float zOffset, float pitch, float view[16])
{
	const float c = cosf(pitch);
	const float s = sinf(pitch);
	// R rows: (1,0,0), (0,c,s), (0,-s,c); t = -R * cam.
	view[0] = 1.0f;  view[4] = 0.0f;  view[8] = 0.0f;   view[12] = -s * zOffset;
	view[1] = 0.0f;  view[5] = c;     view[9] = s;      view[13] = -(c * height + s * zOffset);
	view[2] = 0.0f;  view[6] = -s;    view[10] = c;     view[14] = s * height - c * zOffset;
	view[3] = 0.0f;  view[7] = 0.0f;  view[11] = 0.0f;  view[15] = 1.0f;
}

static bool IsRed(const unsigned char* pixel)
{
	return pixel[0] >= 200 && pixel[1] <= 64 && pixel[2] <= 64;
}

static bool IsGreen(const unsigned char* pixel)
{
	return pixel[1] >= 200 && pixel[0] <= 64 && pixel[2] <= 64;
}

static bool IsBlue(const unsigned char* pixel)
{
	return pixel[2] >= 200 && pixel[0] <= 64 && pixel[1] <= 64;
}

static bool IsYellow(const unsigned char* pixel)
{
	return pixel[0] >= 200 && pixel[1] >= 200 && pixel[2] <= 64;
}

static bool IsClear(const unsigned char* pixel)
{
	return pixel[0] >= 9 && pixel[0] <= 17 &&
		pixel[1] >= 14 && pixel[1] <= 22 &&
		pixel[2] >= 27 && pixel[2] <= 35 && pixel[3] == 255;
}

static int CountRed(const unsigned char* image, int width, int height)
{
	int count = 0;
	for (int i = 0; i < width * height; ++i)
		if (IsRed(image + (size_t)i * 4))
			++count;
	return count;
}

static int CountGreen(const unsigned char* image, int width, int height)
{
	int count = 0;
	for (int i = 0; i < width * height; ++i)
		if (IsGreen(image + (size_t)i * 4))
			++count;
	return count;
}

// Mean row (0 = top) of the pixels matching `red` or green, or -1 when none.
// Used by the vertical-convention pin to place a marker in a screen half.
static float MeanRowOf(const unsigned char* image, int width, int height, bool red)
{
	long long sum = 0;
	int count = 0;
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const unsigned char* pixel = image + ((size_t)y * width + x) * 4;
			if (red ? IsRed(pixel) : IsGreen(pixel))
			{
				sum += y;
				++count;
			}
		}
	}
	return count > 0 ? (float)sum / (float)count : -1.0f;
}

static int CountNonClear(const unsigned char* image, int width, int height)
{
	int count = 0;
	for (int i = 0; i < width * height; ++i)
		if (!IsClear(image + (size_t)i * 4))
			++count;
	return count;
}

// Bounding box of the red pixels within columns [x0, x1). Returns false when
// there are none.
static bool RedBoundingBox(const unsigned char* image, int width, int height,
	int x0, int x1, int* minX, int* minY, int* maxX, int* maxY)
{
	bool found = false;
	for (int y = 0; y < height; ++y)
	{
		for (int x = x0; x < x1 && x < width; ++x)
		{
			if (!IsRed(image + ((size_t)y * width + x) * 4))
				continue;
			if (!found)
			{
				*minX = *maxX = x;
				*minY = *maxY = y;
				found = true;
			}
			else
			{
				if (x < *minX) *minX = x;
				if (x > *maxX) *maxX = x;
				if (y < *minY) *minY = y;
				if (y > *maxY) *maxY = y;
			}
		}
	}
	return found;
}

// Acceptance (a): three instances (including a yawed one) at known transforms
// appear in their expected screen regions, and the yaw is observable through
// the footprint aspect.
static void TestInstanceScreenRegions()
{
	char error[256];
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	Check(WriteRectangleFixture("instance-red.glb", 2500.0f, 1500.0f, false, red, error, sizeof(error)),
		"instance rectangle fixture written");
	Check(NativeRenderer_RegisterResource("quad-red", "instance-red.glb", error, sizeof(error)) == 1,
		"instance rectangle resource registered");

	float view[16];
	BuildTopDownView(20000.0f, view);
	NativeRenderer_SetCamera(view, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(100.0f, 125000.0f);

	NativeInstance instances[3];
	const float leftPos[3] = { -9000.0f, 0.0f, 8000.0f };
	const float centrePos[3] = { 0.0f, 0.0f, 8000.0f };
	const float rightPos[3] = { 9000.0f, 0.0f, 8000.0f };
	MakeInstance(instances[0], "quad-red", leftPos, 0, 1, 0, 10, 3, 7);
	MakeInstance(instances[1], "quad-red", centrePos, 0, 1, 0, 11, 3, 7);
	MakeInstance(instances[2], "quad-red", rightPos, 16, 1, 0, 12, 3, 7); // yawed

	Check(NativeRenderer_SetInstances(instances, 3, error, sizeof(error)) == 1,
		"three instances set");

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"three instances rendered offscreen");

	int drawn = 0, skipped = 0;
	NativeRenderer_GetInstanceStats(nullptr, &drawn, &skipped, nullptr);
	Check(drawn == 3 && skipped == 0, "all three instances drawn");

	int minX = 0, minY = 0, maxX = 0, maxY = 0;
	const bool left = RedBoundingBox(image, 64, 64, 0, 14, &minX, &minY, &maxX, &maxY);
	Check(left, "the left instance appears in the left screen region");
	const bool centre = RedBoundingBox(image, 64, 64, 25, 39, &minX, &minY, &maxX, &maxY);
	Check(centre, "the centre instance appears in the centre screen region");
	const int centreWidth = maxX - minX + 1;
	const int centreHeight = maxY - minY + 1;
	const bool right = RedBoundingBox(image, 64, 64, 50, 64, &minX, &minY, &maxX, &maxY);
	Check(right, "the yang=16 instance appears in the right screen region");
	const int rightWidth = maxX - minX + 1;
	const int rightHeight = maxY - minY + 1;

	if (!(centreWidth > centreHeight))
		printf("    (centre footprint %dx%d, expected wider than tall)\n", centreWidth, centreHeight);
	if (!(rightHeight > rightWidth))
		printf("    (right footprint %dx%d, expected taller than wide after the yaw)\n", rightWidth, rightHeight);
	Check(centreWidth > centreHeight, "the unyawed instance footprint is wider than tall");
	Check(rightHeight > rightWidth, "the yang=16 yaw swaps the footprint aspect");

	// A back-winding instance is NOT culled: the scene pipelines cull nothing
	// (matching the legacy GPU; the game software-culls), so both windings
	// render. Under the old CULL_MODE_BACK policy this was the culled case.
	const float backRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	Check(WriteRectangleFixture("instance-back.glb", 6000.0f, 4000.0f, true, backRed, error, sizeof(error)),
		"back-winding instance fixture written");
	Check(NativeRenderer_RegisterResource("quad-back", "instance-back.glb", error, sizeof(error)) == 1,
		"back-winding instance resource registered");
	NativeInstance backInstance;
	MakeInstance(backInstance, "quad-back", centrePos, 0, 1, 0, 13, 3, 7);
	Check(NativeRenderer_SetInstances(&backInstance, 1, error, sizeof(error)) == 1,
		"back-winding instance set");
	unsigned char backImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(backImage, 64, 64, error, sizeof(error)) == 1,
		"back-winding instance rendered offscreen");
	Check(CountRed(backImage, 64, 64) > 50, "the back-winding instance renders (no culling)");

	// Acceptance (b): the nearer instance occludes the farther one. Draw the
	// nearer green first and the farther red second so draw order cannot pass
	// the check; only the depth test can keep green.
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();
	const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	Check(WriteRectangleFixture("instance-green.glb", 2500.0f, 1500.0f, false, green, error, sizeof(error)),
		"instance green fixture written");
	Check(NativeRenderer_RegisterResource("quad-red", "instance-red.glb", error, sizeof(error)) == 1,
		"red resource re-registered for the depth test");
	Check(NativeRenderer_RegisterResource("quad-green", "instance-green.glb", error, sizeof(error)) == 1,
		"green resource registered");

	NativeInstance depth[2];
	// Both project to the same NDC y under the top-down camera (z / (H - y) is
	// equal: 5600 / 14000 == 8000 / 20000; the projection's y negation scales
	// both by the same -f, so they still share one screen row), so the nearer
	// green quad (camera depth 14000) is larger and fully covers the farther red
	// one (depth 20000).
	const float nearPos[3] = { 0.0f, 6000.0f, 5600.0f }; // nearer: camera depth 14000
	const float farPos[3] = { 0.0f, 0.0f, 8000.0f };    // farther: camera depth 20000
	MakeInstance(depth[0], "quad-green", nearPos, 0, 1, 0, 20, 4, 1);
	MakeInstance(depth[1], "quad-red", farPos, 0, 1, 0, 21, 4, 1);
	Check(NativeRenderer_SetInstances(depth, 2, error, sizeof(error)) == 1,
		"overlapping depth instances set");

	unsigned char depthImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(depthImage, 64, 64, error, sizeof(error)) == 1,
		"overlapping depth instances rendered offscreen");
	Check(CountGreen(depthImage, 64, 64) > 50,
		"the nearer instance wins the overlapping region");
	Check(CountRed(depthImage, 64, 64) == 0, "the farther instance is fully occluded");
}

// Writes a multi-primitive GLB with two horizontal (x-z plane) rectangles: a red
// one centred at x = -9000 and a green one centred at x = +9000. The node
// transform stays identity, so the placement is baked into each primitive's
// positions. Front winding, the same (0,1,2)+(0,2,3) fan the decoder emits.
static bool WriteMultiFixture(const char* path, char* error, int errorCapacity)
{
	GltfMultiMeshData multi;

	const float halfX = 2500.0f;
	const float halfZ = 1500.0f;
	const float offsets[2] = { -9000.0f, 9000.0f };
	const float colors[2][4] = {
		{ 1.0f, 0.0f, 0.0f, 1.0f },	// red (left)
		{ 0.0f, 1.0f, 0.0f, 1.0f },	// green (right)
	};

	for (int p = 0; p < 2; ++p)
	{
		const float x = offsets[p];
		GltfPrimitiveData primitive;
		primitive.positions = {
			x - halfX, 0, -halfZ,  x + halfX, 0, -halfZ,  x + halfX, 0, halfZ,  x - halfX, 0, halfZ,
		};
		primitive.uvs = { 0, 0,  1, 0,  0, 1,  1, 1 };
		primitive.indices = { 0, 1, 2,  0, 2, 3 };
		primitive.vertexCount = 4;
		primitive.triangleCount = 2;
		for (int c = 0; c < 4; ++c)
			primitive.baseColorFactor[c] = colors[p][c];
		multi.primitives.push_back(primitive);
	}

	return Gltf_WriteFileMulti(path, multi, error, errorCapacity) == 0;
}

// E03 multi-primitive consumption: one registered resource with two primitives
// (distinct base colours) drawn as a single instance. Both colours must appear at
// their expected screen regions, proving every primitive of the resource is
// loaded and drawn, while the instance still counts once in the stats.
static void TestMultiPrimitiveResource()
{
	char error[256];
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	Check(WriteMultiFixture("multi-instance.glb", error, sizeof(error)),
		"multi-primitive fixture written");
	Check(NativeRenderer_RegisterResource("multi", "multi-instance.glb", error, sizeof(error)) == 1,
		"multi-primitive resource registered");

	float view[16];
	BuildTopDownView(20000.0f, view);
	NativeRenderer_SetCamera(view, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(100.0f, 125000.0f);

	NativeInstance instance;
	const float pos[3] = { 0.0f, 0.0f, 8000.0f };
	MakeInstance(instance, "multi", pos, 0, 1, 0, 60, 8, 1);
	Check(NativeRenderer_SetInstances(&instance, 1, error, sizeof(error)) == 1,
		"multi-primitive instance set");

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"multi-primitive instance rendered offscreen");

	int drawn = 0, skipped = 0;
	NativeRenderer_GetInstanceStats(nullptr, &drawn, &skipped, nullptr);
	Check(drawn == 1 && skipped == 0,
		"the multi-primitive instance counts as one drawn instance");

	bool redLeft = false, greenLeft = false, redRight = false, greenRight = false;
	for (int y = 0; y < 64; ++y)
	{
		for (int x = 0; x < 64; ++x)
		{
			const unsigned char* pixel = image + ((size_t)y * 64 + x) * 4;
			if (x < 32)
			{
				if (IsRed(pixel)) redLeft = true;
				if (IsGreen(pixel)) greenLeft = true;
			}
			else
			{
				if (IsRed(pixel)) redRight = true;
				if (IsGreen(pixel)) greenRight = true;
			}
		}
	}
	Check(redLeft, "the red primitive is visible in the left screen region");
	Check(greenRight, "the green primitive is visible in the right screen region");
	Check(!greenLeft, "the green primitive does not leak into the left region");
	Check(!redRight, "the red primitive does not leak into the right region");

	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();
}

// Writes a multi-primitive GLB with one textured and one untextured primitive.
// The left primitive carries a 2x1 base-colour image (texel 0 red, texel 1
// green) and raw page texel UVs 64 and 192; the renderer's 1/256 scale maps them
// to 0.25 and 0.75, so nearest sampling splits the quad at its midpoint (the
// texture's texel boundary is at u = 0.5): red on the screen-left half, green on
// the screen-right half. Without the scale the whole quad would clamp to the
// green texel. The right primitive has no image and a blue base-colour factor,
// so it samples the shared white texel and shows pure blue. Front winding, the
// same (0,1,2)+(0,2,3) fan the decoder emits.
static bool WriteTexturedMultiFixture(const char* path, char* error, int errorCapacity)
{
	GltfMultiMeshData multi;

	const float halfX = 3500.0f;
	const float halfZ = 3000.0f;

	GltfPrimitiveData left;
	left.positions = {
		-5000.0f - halfX, 0, -halfZ,  -5000.0f + halfX, 0, -halfZ,
		-5000.0f + halfX, 0, halfZ,  -5000.0f - halfX, 0, halfZ,
	};
	left.uvs = { 64, 0,  192, 0,  192, 128,  64, 128 };
	left.indices = { 0, 1, 2,  0, 2, 3 };
	left.vertexCount = 4;
	left.triangleCount = 2;
	left.imageRgba = { 255, 0, 0, 255,  0, 255, 0, 255 };
	left.imageWidth = 2;
	left.imageHeight = 1;
	multi.primitives.push_back(left);

	GltfPrimitiveData right;
	right.positions = {
		5000.0f - halfX, 0, -halfZ,  5000.0f + halfX, 0, -halfZ,
		5000.0f + halfX, 0, halfZ,   5000.0f - halfX, 0, halfZ,
	};
	right.uvs = { 0, 0,  1, 0,  1, 1,  0, 1 };
	right.indices = { 0, 1, 2,  0, 2, 3 };
	right.vertexCount = 4;
	right.triangleCount = 2;
	right.baseColorFactor[0] = 0.0f;
	right.baseColorFactor[1] = 0.0f;
	right.baseColorFactor[2] = 1.0f;
	right.baseColorFactor[3] = 1.0f;
	multi.primitives.push_back(right);

	return Gltf_WriteFileMulti(path, multi, error, errorCapacity) == 0;
}

// E03 texture phase 3: a multi-primitive resource with one textured and one
// untextured primitive. The textured primitive samples its own image (a
// red/green split reached only when the raw texel UVs are scaled by 1/256); the
// untextured primitive samples the shared white texel times its blue base
// colour. Both must land in their expected screen regions through the real
// offscreen pipeline, while the instance still counts once.
static void TestTexturedMultiPrimitiveResource()
{
	char error[256];
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	Check(WriteTexturedMultiFixture("textured-multi.glb", error, sizeof(error)),
		"textured multi-primitive fixture written");
	Check(NativeRenderer_RegisterResource("textured-multi", "textured-multi.glb", error, sizeof(error)) == 1,
		"textured multi-primitive resource registered");

	float view[16];
	BuildTopDownView(20000.0f, view);
	NativeRenderer_SetCamera(view, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(100.0f, 125000.0f);

	NativeInstance instance;
	const float pos[3] = { 0.0f, 0.0f, 0.0f };
	MakeInstance(instance, "textured-multi", pos, 0, 1, 0, 70, 9, 1);
	Check(NativeRenderer_SetInstances(&instance, 1, error, sizeof(error)) == 1,
		"textured multi-primitive instance set");

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"textured multi-primitive instance rendered offscreen");

	int drawn = 0, skipped = 0;
	NativeRenderer_GetInstanceStats(nullptr, &drawn, &skipped, nullptr);
	Check(drawn == 1 && skipped == 0,
		"the textured multi-primitive instance counts as one drawn instance");

	int redLeft = 0, greenLeft = 0, blueLeft = 0;
	long long redX = 0, greenX = 0;
	int redRight = 0, greenRight = 0, blueRight = 0;
	for (int y = 0; y < 64; ++y)
	{
		for (int x = 0; x < 64; ++x)
		{
			const unsigned char* pixel = image + ((size_t)y * 64 + x) * 4;
			if (x < 32)
			{
				if (IsRed(pixel)) { ++redLeft; redX += x; }
				if (IsGreen(pixel)) { ++greenLeft; greenX += x; }
				if (IsBlue(pixel)) ++blueLeft;
			}
			else
			{
				if (IsRed(pixel)) ++redRight;
				if (IsGreen(pixel)) ++greenRight;
				if (IsBlue(pixel)) ++blueRight;
			}
		}
	}

	if (!(redLeft > 10 && greenLeft > 10))
		printf("    (left region red %d, green %d; expected both image texels)\n",
			redLeft, greenLeft);
	Check(redLeft > 10 && greenLeft > 10,
		"the textured primitive shows both image texels in the left region");
	Check(redLeft > greenLeft / 2 && greenLeft > redLeft / 2,
		"the red and green halves are balanced (raw texel UVs were scaled by 1/256)");
	Check(redLeft > 0 && greenLeft > 0 &&
		(float)redX / redLeft < (float)greenX / greenLeft,
		"red samples appear left of green samples within the textured primitive");
	Check(blueRight > 10,
		"the untextured primitive shows its blue base colour in the right region");
	Check(redRight == 0 && greenRight == 0,
		"no textured colour leaks into the right region");
	Check(blueLeft == 0,
		"the untextured base colour does not leak into the left region");
	Check(NativeRenderer_SetBilinearFiltering(1, error, sizeof(error)) == 1,
		"bilinear filtering updates resident resources");
	unsigned char linearImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(linearImage, 64, 64, error, sizeof(error)) == 1,
		"bilinear material renders through the native offscreen pipeline");
	int mixed = 0;
	for (int y = 0; y < 64; ++y)
		for (int x = 0; x < 32; ++x)
		{
			const unsigned char* pixel = linearImage + (y * 64 + x) * 4;
			if (pixel[0] > 20 && pixel[1] > 20 && pixel[2] < 10) ++mixed;
		}
	Check(mixed > 10, "linear filtering blends adjacent red and green texels on the GPU");
	NativeRenderer_ClearResources();
	Check(NativeRenderer_RegisterResource("textured-multi", "textured-multi.glb", error, sizeof(error)) == 1,
		"resource can be registered while linear filtering is selected");
	unsigned char newResourceImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(newResourceImage, 64, 64, error, sizeof(error)) == 1 &&
		memcmp(linearImage, newResourceImage, sizeof(linearImage)) == 0,
		"new resources inherit the same linear sampler policy");
	Check(NativeRenderer_SetBilinearFiltering(0, error, sizeof(error)) == 1,
		"nearest filtering can be restored at runtime");
	unsigned char restoredImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(restoredImage, 64, 64, error, sizeof(error)) == 1 &&
		memcmp(image, restoredImage, sizeof(image)) == 0,
		"restoring nearest reproduces the original framebuffer bytes");

	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();
}

// Writes the world-opacity fixture: two horizontal (x-z plane) quads in one
// multi record, drawn in primitive order through the real scene pipeline.
//
//   * near quad (world y = 0, camera depth 20000) carries a 4x1 base-colour
//     image encoding the PSX alpha convention (knowledge/rules/
//     texture-alpha-semantics.md), left to right: opaque red (255,0,0,255),
//     STP green (0,255,0,128), a 0000h hole (0,0,255,0) and opaque blue
//     (0,0,255,255). Raw texel UVs 0..256 map to normalized 0..1, so each texel
//     owns a 16-pixel screen quarter.
//   * far quad (world y = -4000, camera depth 24000, so BEHIND the near one)
//     is untextured and opaque yellow (base colour 1,1,0,1 over the shared white
//     texel). It covers the whole view and shows through the near quad's hole.
//
// The world half-extent that exactly fills the 64x64 view: with the camera at
// y = 20000 (depth 20000) and a 60-degree fov, NDC.x = 1 at
// x = depth * tan(30) = 11547. All near-quad vertices share world y = 0, so its
// camera depth is constant and the quarter boundaries fall at screen x =
// 16/32/48. The far quad is larger (half 20000) so it still covers the view at
// depth 24000.
static bool WriteWorldOpacityFixture(const char* path, char* error, int errorCapacity)
{
	GltfMultiMeshData multi;

	GltfPrimitiveData nearPrimitive;
	const float half = 11547.0f;
	nearPrimitive.positions = {
		-half, 0, -half,  half, 0, -half,  half, 0, half,  -half, 0, half,
	};
	nearPrimitive.uvs = { 0, 0,  256, 0,  256, 256,  0, 256 };
	nearPrimitive.indices = { 0, 1, 2,  0, 2, 3 };
	nearPrimitive.vertexCount = 4;
	nearPrimitive.triangleCount = 2;
	nearPrimitive.imageRgba = {
		255, 0, 0, 255,	// opaque red
		0, 255, 0, 128,	// STP green (alpha 128)
		0, 0, 255, 0,	// 0000h hole (alpha 0)
		0, 0, 255, 255,	// opaque blue
	};
	nearPrimitive.imageWidth = 4;
	nearPrimitive.imageHeight = 1;
	multi.primitives.push_back(nearPrimitive);

	GltfPrimitiveData farPrimitive;
	const float farHalf = 20000.0f;
	const float farY = -4000.0f; // camera depth 24000: behind the near quad
	farPrimitive.positions = {
		-farHalf, farY, -farHalf,  farHalf, farY, -farHalf,
		farHalf, farY, farHalf,  -farHalf, farY, farHalf,
	};
	farPrimitive.uvs = { 0, 0,  1, 0,  1, 1,  0, 1 };
	farPrimitive.indices = { 0, 1, 2,  0, 2, 3 };
	farPrimitive.vertexCount = 4;
	farPrimitive.triangleCount = 2;
	farPrimitive.baseColorFactor[0] = 1.0f;
	farPrimitive.baseColorFactor[1] = 1.0f;
	farPrimitive.baseColorFactor[2] = 0.0f;
	farPrimitive.baseColorFactor[3] = 1.0f;
	multi.primitives.push_back(farPrimitive);

	return Gltf_WriteFileMulti(path, multi, error, errorCapacity) == 0;
}

// E03 visual parity P1: world geometry is always opaque, so the scene pipeline
// blends nothing and the pixel shader discards only a 0000h texel. This test
// supersedes the earlier straight-alpha blend semantics (TestAlphaBlendSemantics
// and its 50% blend fixture are gone). Over a farther opaque yellow quad the
// scene clears to (0.05, 0.07, 0.12) = (13, 18, 31), and:
//   * opaque red (a = 255)  -> 255,0,0,255 (unchanged)
//   * STP green (a = 128)   -> 0,255,0,255: FULLY opaque, NOT a 50% blend
//   * 0000h hole (a = 0)    -> the texel is discarded (no colour, no depth), so
//                              the farther yellow quad shows through it
//   * opaque blue (a = 255) -> 0,0,255,255 (unchanged)
// The full-green STP texel proves there is no blend; the yellow hole proves the
// discard writes no depth (a depth-writing hole would occlude the far quad).
static void TestWorldOpacity()
{
	char error[256];
	NativeRenderer_ClearContent();
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	Check(WriteWorldOpacityFixture("world-opacity.glb", error, sizeof(error)),
		"world-opacity fixture written");
	Check(NativeRenderer_LoadContent("world-opacity.glb", error, sizeof(error)) == 1,
		"world-opacity fixture loaded");

	float view[16];
	BuildTopDownView(20000.0f, view);
	NativeRenderer_SetCamera(view, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(100.0f, 125000.0f);

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"world-opacity fixture rendered offscreen");

	// Sample the centre row of each 16-pixel quarter (x = 8 / 24 / 40 / 56),
	// safely inside the texel regions.
	const unsigned char* opaqueRed = &image[((32 * 64) + 8) * 4];
	const unsigned char* stpGreen = &image[((32 * 64) + 24) * 4];
	const unsigned char* hole = &image[((32 * 64) + 40) * 4];
	const unsigned char* opaqueBlue = &image[((32 * 64) + 56) * 4];

	const bool redOpaque = IsRed(opaqueRed) && opaqueRed[3] == 255;
	if (!redOpaque)
		printf("    (alpha-255 texel %u,%u,%u,%u, expected opaque red 255,0,0,255)\n",
			opaqueRed[0], opaqueRed[1], opaqueRed[2], opaqueRed[3]);
	Check(redOpaque, "opaque red texel is unchanged");

	// STP=128 must render its full RGB, not blend: exactly (0,255,0,255).
	const bool greenFull = stpGreen[0] <= 16 && stpGreen[1] >= 239 &&
		stpGreen[2] <= 16 && stpGreen[3] == 255;
	if (!greenFull)
		printf("    (alpha-128 texel %u,%u,%u,%u, expected FULL opaque green 0,255,0,255)\n",
			stpGreen[0], stpGreen[1], stpGreen[2], stpGreen[3]);
	Check(greenFull, "alpha 128 (STP) renders fully opaque (no 50% blend)");

	// The 0000h texel is discarded; the farther yellow quad shows through it,
	// proving the hole wrote no colour and no depth (no blend, no occlusion).
	const bool holeShowsBehind = IsYellow(hole) && hole[3] == 255;
	if (!holeShowsBehind)
		printf("    (alpha-0 texel %u,%u,%u,%u, expected the farther yellow quad 255,255,0,255)\n",
			hole[0], hole[1], hole[2], hole[3]);
	Check(holeShowsBehind, "alpha 0 is a cutout hole: the farther quad shows through");
	Check(!IsClear(hole) && !IsBlue(hole),
		"the hole is neither the clear colour nor the near quad's blue (no blend, no depth)");

	const bool blueOpaque = IsBlue(opaqueBlue) && opaqueBlue[3] == 255;
	if (!blueOpaque)
		printf("    (opaque blue texel %u,%u,%u,%u, expected 0,0,255,255)\n",
			opaqueBlue[0], opaqueBlue[1], opaqueBlue[2], opaqueBlue[3]);
	Check(blueOpaque, "the neighbouring opaque texel is still fully opaque");

	// The near quad occludes the far one where it is opaque (LESS_OR_EQUAL depth):
	// the opaque red quarter must not show the far yellow quad.
	Check(!IsYellow(opaqueRed) && !IsYellow(opaqueBlue),
		"the near opaque texels occlude the farther quad (depth test)");

	NativeRenderer_ClearContent();
}

// ---------------------------------------------------------------------------
// E03 texture phase 3: converted content with an embedded base-colour image.
// The multi record now holds the writer's stored-block PNG, so a stored-block
// textured GLB must load and render through the primitive's own texture. A PNG
// outside that subset (here a fixed-Huffman variant) is still a named error, and
// the renderer must stay in its previous state.
// ---------------------------------------------------------------------------

static void AppendU32(std::vector<unsigned char>& out, unsigned int value)
{
	out.push_back((unsigned char)(value & 0xff));
	out.push_back((unsigned char)((value >> 8) & 0xff));
	out.push_back((unsigned char)((value >> 16) & 0xff));
	out.push_back((unsigned char)((value >> 24) & 0xff));
}

// Writes a minimal single-record GLB whose one primitive references a material
// with a base-colour texture. A real 1x1 PNG is embedded as images[0] (a
// bufferView in the shared BIN chunk) and textures[0] points at it. The PNG uses
// fixed Huffman coding, which the multi loader's stored-block-only decoder
// rejects as an unsupported PNG variant; this fixture is the negative case.
static bool WriteTexturedGlb(const char* path, char* error, int errorCapacity)
{
	// A valid 1x1 red PNG (120 bytes).
	static const unsigned char png[120] = {
		0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
		0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
		0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
		0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
		0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC,
		0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
		0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7, 0x6F, 0xA8, 0x64, 0x00,
		0x00, 0x00, 0x0D, 0x49, 0x44, 0x41, 0x54, 0x18, 0x57, 0x63, 0xF8, 0xCF,
		0xC0, 0xF0, 0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0xA6, 0x5C, 0x9B, 0x5D,
		0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
	};

	const unsigned int positionBytes = 36; // 3 * VEC3 float32, all zero
	std::vector<unsigned char> bin(positionBytes, 0);
	bin.insert(bin.end(), png, png + sizeof(png)); // 36 + 120 = 156 (4-byte aligned)

	const char* json =
		"{\"asset\":{\"version\":\"2.0\"},"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}],"
		"\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}],"
		"\"textures\":[{\"source\":0}],"
		"\"images\":[{\"bufferView\":1,\"mimeType\":\"image/png\"}],"
		"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
		"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
		"{\"buffer\":0,\"byteOffset\":36,\"byteLength\":120}],"
		"\"buffers\":[{\"byteLength\":156}]}";

	std::string jsonText = json;
	while (jsonText.size() % 4 != 0)
		jsonText += " ";

	const unsigned int jsonLength = (unsigned int)jsonText.size();
	const unsigned int binLength = (unsigned int)bin.size();
	const unsigned int totalLength = 12 + 8 + jsonLength + 8 + binLength;

	std::vector<unsigned char> glb;
	AppendU32(glb, 0x46546C67); // "glTF"
	AppendU32(glb, 2);
	AppendU32(glb, totalLength);
	AppendU32(glb, jsonLength);
	AppendU32(glb, 0x4E4F534A); // "JSON"
	glb.insert(glb.end(), jsonText.begin(), jsonText.end());
	AppendU32(glb, binLength);
	AppendU32(glb, 0x004E4942); // "BIN\0"
	glb.insert(glb.end(), bin.begin(), bin.end());

	FILE* file = fopen(path, "wb");
	if (!file)
	{
		snprintf(error, errorCapacity, "cannot create %s", path);
		return false;
	}
	const bool wrote = fwrite(glb.data(), 1, glb.size(), file) == glb.size();
	fclose(file);
	if (!wrote)
	{
		remove(path);
		snprintf(error, errorCapacity, "cannot write %s", path);
		return false;
	}
	return true;
}

// E03 texture phase 3: a stored-block textured GLB (the writer's own PNG
// subset) must now load and render through the multi record, sampling the
// primitive's own image. A PNG outside that subset is still refused with a
// named diagnostic, and the loaded content and the scene draw must stay intact.
static void TestTexturedContentRejected()
{
	char error[256];
	NativeRenderer_ClearContent();
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identity, kSmokeScreenDistance);
	// The fixtures are metre-scale (z = +5), so use metre-scale clip planes:
	// an earlier test leaves the raw-unit pair (100 / 125000) in place.
	NativeRenderer_SetClipPlanes(0.02f, 300.0f);

	// A stored-block textured GLB must load and render: the 1x1 blue image
	// proves the primitive sampled its own texture rather than the white texel.
	GltfMultiMeshData textured;
	GltfPrimitiveData texturedPrimitive;
	texturedPrimitive.positions = { -1, -1, 0,  1, -1, 0,  1, 1, 0,  -1, 1, 0 };
	texturedPrimitive.uvs = { 0, 0,  1, 0,  1, 1,  0, 1 };
	texturedPrimitive.indices = { 0, 1, 2,  0, 2, 3 };
	texturedPrimitive.vertexCount = 4;
	texturedPrimitive.triangleCount = 2;
	texturedPrimitive.imageRgba = { 0, 0, 255, 255 }; // 1x1 opaque blue
	texturedPrimitive.imageWidth = 1;
	texturedPrimitive.imageHeight = 1;
	textured.primitives.push_back(texturedPrimitive);
	textured.transform[14] = 5.0f; // ahead of the identity camera (+Z forward)
	Check(Gltf_WriteFileMulti("textured-stored.glb", textured, error, sizeof(error)) == 0,
		"stored-block textured GLB written");
	Check(NativeRenderer_LoadContent("textured-stored.glb", error, sizeof(error)) == 1,
		"LoadContent loads a stored-block textured GLB");

	int vertices = 0, triangles = 0;
	NativeRenderer_GetContentStats(&vertices, &triangles);
	Check(vertices == 4 && triangles == 2, "textured content stats report 4 vertices / 2 triangles");

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"stored-block textured content rendered offscreen");
	const unsigned char* centre = &image[((32 * 64) + 32) * 4];
	const bool blue = centre[2] >= 239 && centre[0] <= 16 && centre[1] <= 16 && centre[3] == 255;
	if (!blue)
		printf("    (centre pixel %u,%u,%u,%u, expected the 1x1 blue texture)\n",
			centre[0], centre[1], centre[2], centre[3]);
	Check(blue, "the stored-block textured primitive sampled its own image");

	// A PNG outside the stored-block subset is still rejected with a named
	// diagnostic, and the loaded textured content stays intact.
	Check(WriteTexturedGlb("textured-content.glb", error, sizeof(error)),
		"unsupported-variant GLB written with an embedded base-colour image");
	char loadError[256] = "";
	Check(NativeRenderer_LoadContent("textured-content.glb", loadError, sizeof(loadError)) == 0,
		"LoadContent rejects an unsupported PNG variant");
	Check(loadError[0] != '\0' && strstr(loadError, "deflate block type") != nullptr,
		"the rejection names the unsupported PNG deflate variant");

	// State stays clean: the loaded content and its stats are untouched, and
	// the scene still draws it.
	Check(NativeRenderer_HasContent() == 1, "the rejected load left the textured content loaded");
	vertices = 0;
	triangles = 0;
	NativeRenderer_GetContentStats(&vertices, &triangles);
	Check(vertices == 4 && triangles == 2, "the rejected load left the content stats intact");

	unsigned char guard[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(guard, 64, 64, error, sizeof(error)) == 1,
		"the scene still renders after the unsupported-variant rejection");
	const unsigned char* guardCentre = &guard[((32 * 64) + 32) * 4];
	Check(guardCentre[2] >= 239 && guardCentre[0] <= 16 && guardCentre[1] <= 16,
		"the textured guard content still draws after the rejection");

	// RegisterResource goes through the same loader and must reject it too,
	// leaving no resident resource.
	char registerError[256] = "";
	Check(NativeRenderer_RegisterResource("textured", "textured-content.glb",
		registerError, sizeof(registerError)) == 0, "RegisterResource rejects an unsupported PNG variant");
	Check(registerError[0] != '\0' && strstr(registerError, "deflate block type") != nullptr,
		"the register rejection names the unsupported PNG deflate variant");
	Check(NativeRenderer_ResourceResident("textured") == 0, "the rejected resource is not resident");

	NativeRenderer_ClearContent();
	Check(NativeRenderer_HasContent() == 0, "textured content cleared after the checks");
}

// Acceptance (c)/(d): residency, set/clear/re-register transitions, unresolved
// keys, the named diagnostic, the no-fallback rule and the resource capacity.
static void TestInstanceResidencyAndDiagnostics()
{
	char error[256];
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	Check(WriteRectangleFixture("instance-red.glb", 2500.0f, 1500.0f, false, red, error, sizeof(error)),
		"residency fixture written");

	Check(NativeRenderer_RegisterResource("quad-red", "instance-red.glb", error, sizeof(error)) == 1,
		"residency resource registered");
	Check(NativeRenderer_ResourceResident("quad-red") == 1, "registered resource reports resident");
	Check(NativeRenderer_RegisterResource("quad-red", "instance-red.glb", error, sizeof(error)) == 1,
		"re-registering a resident key is an idempotent no-op");
	Check(NativeRenderer_ResourceResident("quad-missing") == 0, "an unknown key is not resident");

	float view[16];
	BuildTopDownView(20000.0f, view);
	NativeRenderer_SetCamera(view, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(100.0f, 125000.0f);

	NativeInstance instance;
	const float pos[3] = { 0.0f, 0.0f, 8000.0f };
	MakeInstance(instance, "quad-red", pos, 0, 1, 0, 30, 5, 2);

	int submitted = 0, drawn = 0, skipped = 0, resident = 0;
	Check(NativeRenderer_SetInstances(&instance, 1, error, sizeof(error)) == 1, "one instance set");
	NativeRenderer_GetInstanceStats(&submitted, &drawn, &skipped, &resident);
	Check(submitted == 1 && skipped == 0 && resident == 1,
		"one submitted, none unresolved, one resident");

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"instance rendered offscreen");
	NativeRenderer_GetInstanceStats(&submitted, &drawn, &skipped, &resident);
	Check(drawn == 1, "the resolved instance was drawn");
	Check(CountRed(image, 64, 64) > 50, "the instance is visible");

	// set -> render -> clear -> absent
	NativeRenderer_ClearInstances();
	NativeRenderer_GetInstanceStats(&submitted, &drawn, &skipped, &resident);
	Check(submitted == 0 && drawn == 0, "clearing instances resets the submission stats");
	unsigned char cleared[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(cleared, 64, 64, error, sizeof(error)) == 1,
		"rendered after clearing instances");
	Check(CountRed(cleared, 64, 64) == 0, "the cleared instance is absent");

	// re-register / set -> present
	NativeRenderer_UnregisterResource("quad-red");
	Check(NativeRenderer_ResourceResident("quad-red") == 0, "unregistered resource is gone");
	Check(NativeRenderer_RegisterResource("quad-red", "instance-red.glb", error, sizeof(error)) == 1,
		"resource re-registered after unregister");
	Check(NativeRenderer_SetInstances(&instance, 1, error, sizeof(error)) == 1, "instance set again");
	unsigned char reloaded[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(reloaded, 64, 64, error, sizeof(error)) == 1,
		"re-registered instance rendered");
	Check(CountRed(reloaded, 64, 64) > 50, "the re-registered instance is present again");

	// Adding a distant resolved object must preserve every framebuffer byte,
	// while removing its draw from the instance count (not an unresolved error).
	NativeInstance visibleAndOutside[2] = { instance, instance };
	const float outsidePosition[3] = { 10000000.0f, 0.0f, 0.0f };
	MakeInstance(visibleAndOutside[1], "quad-red", outsidePosition, 0, 1, 0, 32, 5, 2);
	Check(NativeRenderer_SetInstances(visibleAndOutside, 2, error, sizeof(error)) == 1,
		"visible and distant resolved instances submitted");
	unsigned char culledFrame[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(culledFrame, 64, 64, error, sizeof(error)) == 1,
		"frustum-culled frame rendered");
	Check(memcmp(culledFrame, reloaded, sizeof(culledFrame)) == 0,
		"culling the outside object preserves the entire visible frame");
	NativeRenderer_GetInstanceStats(&submitted, &drawn, &skipped, &resident);
	Check(submitted == 2 && drawn == 1 && skipped == 0,
		"frustum culling excludes a draw without reporting a missing resource");

	// A dense streamed sector must retain objects beyond the old 8192 bound.
	// Only the final instance is visible, so clipping/truncating the tail fails
	// an actual pixel comparison rather than just checking an advertised limit.
	std::vector<NativeInstance> dense(NATIVE_MAX_INSTANCES, visibleAndOutside[1]);
	dense.back() = instance;
	Check(NativeRenderer_SetInstances(dense.data(), (int)dense.size(), error, sizeof(error)) == 1,
		"a full dense sector is submitted without truncation");
	Check(NativeRenderer_RenderSceneToRGBA(culledFrame, 64, 64, error, sizeof(error)) == 1,
		"dense sector rendered with its final instance visible");
	Check(memcmp(culledFrame, reloaded, sizeof(culledFrame)) == 0,
		"the final dense-sector instance preserves the expected visible pixels");
	NativeRenderer_GetInstanceStats(&submitted, &drawn, &skipped, &resident);
	Check(submitted == NATIVE_MAX_INSTANCES && drawn == 1 && skipped == 0,
		"the entire dense sector is retained with only its visible object drawn");
	Check(NativeRenderer_SetInstances(dense.data(), NATIVE_MAX_INSTANCES + 1, error, sizeof(error)) == 0 && error[0],
		"submission beyond the dense-sector bound is explicitly rejected");

	// Acceptance (d): an unregistered key is skipped with a named diagnostic and
	// the placeholder is NOT drawn.
	NativeInstance missing;
	MakeInstance(missing, "unregistered-key", pos, 0, 1, 0, 31, 5, 2);
	Check(NativeRenderer_SetInstances(&missing, 1, error, sizeof(error)) == 1,
		"an unresolved key is not a fatal submission error");
	NativeRenderer_GetInstanceStats(&submitted, &drawn, &skipped, &resident);
	Check(submitted == 1 && skipped == 1 && drawn == 0, "the unresolved key is counted, not drawn");

	char diagnostic[NATIVE_INSTANCE_DIAGNOSTIC_CAPACITY] = "";
	Check(NativeRenderer_GetFirstInstanceDiagnostic(diagnostic, sizeof(diagnostic)) == 1 &&
		strstr(diagnostic, "unregistered-key") != nullptr,
		"the first unresolved key is named in the diagnostic");

	unsigned char noFallback[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(noFallback, 64, 64, error, sizeof(error)) == 1,
		"an all-unresolved submission still renders");
	Check(CountRed(noFallback, 64, 64) == 0 && CountNonClear(noFallback, 64, 64) == 0,
		"an all-unresolved submission draws neither the instance nor the placeholder");
	NativeRenderer_GetInstanceStats(&submitted, &drawn, &skipped, &resident);
	Check(drawn == 0, "no instance is drawn for an all-unresolved submission");

	// Capacity: the (NATIVE_MAX_RESOURCES+1)th distinct resource is a named
	// error and never evicts a resident one.
	NativeRenderer_ClearResources();
	for (int i = 0; i < NATIVE_MAX_RESOURCES; ++i)
	{
		char key[32];
		snprintf(key, sizeof(key), "cap-%d", i);
		Check(NativeRenderer_RegisterResource(key, "instance-red.glb", error, sizeof(error)) == 1,
			"capacity resource registered");
	}
	char capacityError[256] = "";
	Check(NativeRenderer_RegisterResource("cap-overflow", "instance-red.glb", capacityError, sizeof(capacityError)) == 0 &&
		capacityError[0] != '\0', "registering past the capacity fails with a diagnostic");
	NativeRenderer_GetInstanceStats(nullptr, nullptr, nullptr, &resident);
	Check(resident == NATIVE_MAX_RESOURCES, "a full table keeps every resident resource");

	NativeRenderer_ClearResources();
	NativeRenderer_ClearInstances();
}

// Acceptance (e): the +Z-forward camera convention. With a real-style angled
// camera an object ahead is visible and one behind is not, and the placeholder
// is still visible in the no-instances mode. This check is depth-sensitive
// (camera-space z), not vertical: the projection's y negation mirrors the scene
// vertically but does not change which camera-space z is in front, so "ahead
// visible / behind clipped" is unaffected. The placeholder check counts
// non-clear pixels, so it survives the vertical mirror as long as the ground
// still fills the view (it does).
static void TestCameraConventionAndPlaceholder()
{
	char error[256];
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	Check(WriteRectangleFixture("instance-red.glb", 6000.0f, 4000.0f, false, red, error, sizeof(error)),
		"camera-convention fixture written");
	Check(NativeRenderer_RegisterResource("quad-red", "instance-red.glb", error, sizeof(error)) == 1,
		"camera-convention resource registered");

	float view[16];
	BuildAngledView(8000.0f, -20000.0f, 0.349f /* 20 degrees */, view);
	NativeRenderer_SetCamera(view, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(100.0f, 125000.0f);

	// No instances: the placeholder (ground and pillars) is visible.
	unsigned char placeholder[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(placeholder, 64, 64, error, sizeof(error)) == 1,
		"placeholder rendered with the angled camera");
	const int placeholderPixels = CountNonClear(placeholder, 64, 64);
	if (placeholderPixels < 64)
		printf("    (placeholder only %d non-clear pixels, expected the ground to fill the view)\n",
			placeholderPixels);
	Check(placeholderPixels > 64, "the placeholder is visible in the no-instances mode");

	// An instance ahead (+Z) is visible.
	NativeInstance ahead;
	const float aheadPos[3] = { 0.0f, 0.0f, 8000.0f };
	MakeInstance(ahead, "quad-red", aheadPos, 0, 1, 0, 40, 6, 1);
	Check(NativeRenderer_SetInstances(&ahead, 1, error, sizeof(error)) == 1, "ahead instance set");
	unsigned char aheadImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(aheadImage, 64, 64, error, sizeof(error)) == 1,
		"ahead instance rendered");
	Check(CountRed(aheadImage, 64, 64) > 50, "the object ahead of the camera is visible");

	// An instance behind the camera is not visible, and the instances mode never
	// falls back to the placeholder.
	NativeInstance behind;
	const float behindPos[3] = { 0.0f, 0.0f, -40000.0f };
	MakeInstance(behind, "quad-red", behindPos, 0, 1, 0, 41, 6, 1);
	Check(NativeRenderer_SetInstances(&behind, 1, error, sizeof(error)) == 1, "behind instance set");
	unsigned char behindImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(behindImage, 64, 64, error, sizeof(error)) == 1,
		"behind instance rendered");
	Check(CountRed(behindImage, 64, 64) == 0 && CountNonClear(behindImage, 64, 64) == 0,
		"the object behind the camera is clipped and no placeholder is drawn");

	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();
}

// E03 vertical-convention pin. The game camera space is +Y-down
// (PsyX_GTE.cpp:395-396, SY = OFY + IR2*h/sz, so a camera-space +y point lands
// below the screen centre), while Diligent's Vulkan backend flips the viewport
// to Direct3D's +Y-up convention (DeviceContextVkImpl.cpp "Turn the viewport
// upside down"). BuildPerspective negates out[5] so a camera-space +y point
// lands in the LOWER half, matching the game and the modern-mesh precedent
// (PsyX_ModernMesh.cpp:105, `-view.y`). Two vertical markers sit at
// camera-space +y (green) and -y (red) under an identity camera; the game
// convention puts green LOW and red HIGH. Under the old +f projection the two
// swap, so this test fails under the old convention and passes under the new.
static void TestVerticalConvention()
{
	char error[256];
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	const float red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	Check(WriteVerticalMarkerFixture("marker-green.glb", 600.0f, green, error, sizeof(error)),
		"vertical marker green fixture written");
	Check(WriteVerticalMarkerFixture("marker-red.glb", 600.0f, red, error, sizeof(error)),
		"vertical marker red fixture written");
	Check(NativeRenderer_RegisterResource("marker-green", "marker-green.glb", error, sizeof(error)) == 1,
		"vertical marker green resource registered");
	Check(NativeRenderer_RegisterResource("marker-red", "marker-red.glb", error, sizeof(error)) == 1,
		"vertical marker red resource registered");

	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identity, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(100.0f, 125000.0f);

	// Camera-space +y marker (green) and -y marker (red), both ahead (+z).
	NativeInstance markers[2];
	const float upPos[3] = { 0.0f, 3000.0f, 8000.0f };
	const float downPos[3] = { 0.0f, -3000.0f, 8000.0f };
	MakeInstance(markers[0], "marker-green", upPos, 0, 1, 0, 50, 7, 1);
	MakeInstance(markers[1], "marker-red", downPos, 0, 1, 0, 51, 7, 1);
	Check(NativeRenderer_SetInstances(markers, 2, error, sizeof(error)) == 1,
		"vertical markers set");

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"vertical markers rendered offscreen");

	const float greenRow = MeanRowOf(image, 64, 64, false);
	const float redRow = MeanRowOf(image, 64, 64, true);
	Check(greenRow >= 0.0f && redRow >= 0.0f, "both vertical markers are visible");
	if (!(greenRow > 32.0f && redRow < 32.0f))
		printf("    (green mean row %.1f, red mean row %.1f; game +y must be lower)\n",
			greenRow, redRow);
	Check(greenRow > 32.0f, "camera-space +y (game down) appears in the lower half");
	Check(redRow < 32.0f, "camera-space -y appears in the upper half");
	Check(greenRow > redRow, "the +y marker is below the -y marker");

	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();
}

// E03 visual parity P5b: the GTE screen offset (SetGeomOffset) is applied as a
// constant NDC translation by the projection. A thin red horizontal band in
// camera space sits at the vertical centre under the centred offset (160,120);
// raising ofy by 60 PSX lines (120 -> 180) must move the band down by
// 60/240 = 0.25 of the target height (16px at 64px). The renderer is
// game-agnostic and defaults to the centred offset, so this test also pins that
// default.
static void TestScreenOffset()
{
	char error[256];
	NativeRenderer_ClearContent();
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identity, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(0.02f, 300.0f);
	NativeRenderer_SetScreenOffset(NATIVE_PSX_DISPLAY_WIDTH * 0.5f,
		NATIVE_PSX_DISPLAY_HEIGHT * 0.5f); // centred, the renderer default

	// A thin horizontal red band (x-y plane) at z = +5, y in [-0.4, 0.4]: under
	// the 60-degree-equivalent focal it projects to a ~9px band at the centre.
	GltfMeshData band;
	band.positions = { -1.0f, -0.4f, 0.0f,  1.0f, -0.4f, 0.0f,
		1.0f, 0.4f, 0.0f,  -1.0f, 0.4f, 0.0f };
	band.uvs = { 0, 0,  1, 0,  1, 1,  0, 1 };
	band.indices = { 0, 1, 2,  0, 2, 3 };
	band.vertexCount = 4;
	band.triangleCount = 2;
	band.baseColorFactor[0] = 1.0f;
	band.baseColorFactor[1] = 0.0f;
	band.baseColorFactor[2] = 0.0f;
	band.baseColorFactor[3] = 1.0f;
	for (int i = 0; i < 16; ++i)
		band.transform[i] = identity[i];
	band.transform[14] = 5.0f; // ahead of the identity camera (+Z forward)

	Check(Gltf_WriteFile("screen-offset-band.glb", band, error, sizeof(error)) == 0,
		"screen-offset band fixture written");
	Check(NativeRenderer_LoadContent("screen-offset-band.glb", error, sizeof(error)) == 1,
		"screen-offset band loaded");

	unsigned char centredImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(centredImage, 64, 64, error, sizeof(error)) == 1,
		"band rendered with the centred screen offset");
	const float centredRow = MeanRowOf(centredImage, 64, 64, true);
	Check(centredRow >= 0.0f, "the band is visible with the centred screen offset");

	// Move the offset 60 PSX lines down (ofy 120 -> 180): 60/240 = 0.25 of the
	// target height, i.e. 16px at 64px.
	NativeRenderer_SetScreenOffset(NATIVE_PSX_DISPLAY_WIDTH * 0.5f,
		NATIVE_PSX_DISPLAY_HEIGHT * 0.5f + 60.0f);
	unsigned char shiftedImage[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(shiftedImage, 64, 64, error, sizeof(error)) == 1,
		"band rendered with the lowered screen offset");
	const float shiftedRow = MeanRowOf(shiftedImage, 64, 64, true);
	Check(shiftedRow >= 0.0f, "the band is visible with the lowered screen offset");
	if (!(centredRow >= 0.0f && shiftedRow > centredRow + 5.0f))
		printf("    (band mean row %.1f centred -> %.1f lowered; expected a downward shift)\n",
			centredRow, shiftedRow);
	Check(centredRow >= 0.0f && shiftedRow > centredRow + 5.0f,
		"the screen offset shifts the projected geometry down");

	NativeRenderer_SetScreenOffset(NATIVE_PSX_DISPLAY_WIDTH * 0.5f,
		NATIVE_PSX_DISPLAY_HEIGHT * 0.5f); // restore the centred default
	NativeRenderer_ClearContent();
}

// E03 visual parity P2 + P6: the pixel shader multiplies the sampled texel by
// the per-vertex COLOR_0, matching psx.frag's `texColor * v_color`. The legacy
// vertex shader also scales that colour by the per-primitive `bright` multiplier
// before interpolation (psx.vert:33-34), and every textured primitive uses
// bright = 2 (PsyX_GPU.cpp:790 MakeTexcoordQuad). The effective textured
// modulation is therefore (colourByte/255) * 2, clamped at the framebuffer
// write. The native applies the same factor in float in the vertex shader, so a
// 1x1 opaque red texel renders:
//   * colourByte 128 -> (128/255)*2 ~= 1.004 -> full red 255 (the day ambient's
//     legacy ~1.0, i.e. the texel is essentially unchanged);
//   * colourByte 64  -> (64/255)*2  ~= 0.502 -> red ~128 (the factor is real:
//     without it the red would be 64);
//   * colourByte 200 -> (200/255)*2 ~= 1.569 -> full red 255 (clamps like the
//     legacy once the product exceeds 1.0).
// Writes the fixture, loads it and returns the centre pixel; false on any step
// failing so the caller can report it.
static bool RenderVertexColorCase(unsigned char colourByte, const char* path,
	unsigned char outPixel[4])
{
	NativeRenderer_ClearContent();

	GltfMultiMeshData multi;
	GltfPrimitiveData primitive;
	primitive.positions = { -1, -1, 0,  1, -1, 0,  1, 1, 0,  -1, 1, 0 };
	primitive.uvs = { 0, 0,  1, 0,  1, 1,  0, 1 };
	primitive.indices = { 0, 1, 2,  0, 2, 3 };
	primitive.vertexCount = 4;
	primitive.triangleCount = 2;
	primitive.imageRgba = { 255, 0, 0, 255 }; // 1x1 opaque red
	primitive.imageWidth = 1;
	primitive.imageHeight = 1;
	for (int c = 0; c < 4; ++c)
		primitive.baseColorFactor[c] = 1.0f;
	// Uniform opaque grey vertex colour on every corner.
	primitive.colors = { colourByte, colourByte, colourByte, 255,
		colourByte, colourByte, colourByte, 255,
		colourByte, colourByte, colourByte, 255,
		colourByte, colourByte, colourByte, 255 };
	multi.primitives.push_back(primitive);
	multi.transform[14] = 5.0f; // ahead of the identity camera (+Z forward)

	char error[256];
	if (Gltf_WriteFileMulti(path, multi, error, sizeof(error)) != 0)
		return false;
	if (NativeRenderer_LoadContent(path, error, sizeof(error)) != 1)
		return false;

	unsigned char image[64 * 64 * 4] = {};
	if (NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) != 1)
		return false;

	const unsigned char* centre = &image[((32 * 64) + 32) * 4];
	outPixel[0] = centre[0];
	outPixel[1] = centre[1];
	outPixel[2] = centre[2];
	outPixel[3] = centre[3];
	return true;
}

static void TestVertexColorModulation()
{
	NativeRenderer_ClearContent();
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identity, kSmokeScreenDistance);
	NativeRenderer_SetClipPlanes(0.02f, 300.0f);

	unsigned char pixel[4] = {};

	// colourByte 128: (128/255)*2 ~= 1.004, so the red texel stays ~unchanged.
	const bool unityRendered = RenderVertexColorCase(128, "vertex-color-128.glb", pixel);
	Check(unityRendered, "vertex-colour 128 fixture rendered");
	const bool unity = pixel[0] >= 239 && pixel[1] <= 16 && pixel[2] <= 16 && pixel[3] == 255;
	if (!unity)
		printf("    (centre pixel %u,%u,%u,%u, expected full red: (128/255)*2 ~= 1.0)\n",
			pixel[0], pixel[1], pixel[2], pixel[3]);
	Check(unity, "colour 128 * the legacy bright=2 factor leaves the red texel ~unchanged");

	// colourByte 64: (64/255)*2 ~= 0.502 -> red ~128, proving the factor applies
	// (without it the red would be 64).
	const bool halfRendered = RenderVertexColorCase(64, "vertex-color-064.glb", pixel);
	Check(halfRendered, "vertex-colour 64 fixture rendered");
	const bool half = pixel[0] >= 110 && pixel[0] <= 145 &&
		pixel[1] <= 16 && pixel[2] <= 16 && pixel[3] == 255;
	if (!half)
		printf("    (centre pixel %u,%u,%u,%u, expected red ~128: (64/255)*2 ~= 0.5)\n",
			pixel[0], pixel[1], pixel[2], pixel[3]);
	Check(half, "colour 64 * 2 halves the red texel (the legacy factor is applied)");

	// colourByte 200: (200/255)*2 ~= 1.569 -> clamps to full red, matching the
	// legacy framebuffer saturation once the product exceeds 1.0.
	const bool saturatedRendered = RenderVertexColorCase(200, "vertex-color-200.glb", pixel);
	Check(saturatedRendered, "vertex-colour 200 fixture rendered");
	const bool saturated = pixel[0] >= 239 && pixel[1] <= 16 && pixel[2] <= 16 && pixel[3] == 255;
	if (!saturated)
		printf("    (centre pixel %u,%u,%u,%u, expected full red: (200/255)*2 clamps to 1.0)\n",
			pixel[0], pixel[1], pixel[2], pixel[3]);
	Check(saturated, "colour 200 * 2 exceeds 1.0 and clamps like the legacy");

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
	NativeFrameStats loadingStats = {};
	NativeRenderer_GetFrameStats(&loadingStats);
	Check(loadingStats.drawCalls == 0 && loadingStats.culledInstances == 0 && loadingStats.swapChainBuffers > 0,
		"clear-only loading frames report zero scene draws and actual buffers");

	TestContentUpload();
	TestTriangleDraw();
	TestTexturedQuad();
	TestConvertedContentScene();
	TestNoCullBothFaces();
	TestInstanceScreenRegions();
	TestMultiPrimitiveResource();
	TestTexturedMultiPrimitiveResource();
	TestWorldOpacity();
	TestTexturedContentRejected();
	TestInstanceResidencyAndDiagnostics();
	TestCameraConventionAndPlaceholder();
	TestVerticalConvention();
	TestScreenOffset();
	TestVertexColorModulation();

	NativeRenderer_Resize(640, 480);
	Check(NativeRenderer_RenderClearAndPresent(0.3f, 0.1f, 0.2f, 1.0f, error, sizeof(error)) == 1,
		"frame presented after resize");

	// The scene path must survive a swap chain resize (its depth target is
	// recreated) and the offscreen target must follow a size change.
	const float identityView[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identityView, kSmokeScreenDistance);
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
