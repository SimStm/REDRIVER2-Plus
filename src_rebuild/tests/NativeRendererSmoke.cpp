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
// Part 5: rejects a single-record GLB with an embedded base-colour image (the
// multi-primitive record has no image), checking the named diagnostic and that
// the previously loaded content and the scene draw are unaffected.
//
// It requires a Vulkan-capable GPU/driver and the Diligent libraries built by
// scripts/fetch_diligent.ps1; it needs no game assets. Build and run
// instructions are in tests/README.md.

#include "../native/NativeRenderer.h"
#include "../utils/GltfLoader.h"
#include "../utils/GltfWriter.h"
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

// E03-B2.5/item4: pins the decoder's winding to the renderer's cull mode under
// the +Z-forward projection. The decoder emits a world quad as (v0,v1,v3,v2)
// and fans it (0,1,2)+(0,2,3); the first triangle (v0,v1,v3) is the game's
// gte_nclip triangle (draw.c:780-796). BuildPerspective negates camera-space y
// (out[5] = -f) to match the game's +Y-down screen convention, which reflects
// the NDC and makes that game-front triangle NEGATIVE-area. The scene pipelines
// cull back faces with CULL_MODE_BACK and FrontCounterClockwise = False
// (native/NativeRenderer.cpp): the reflection and the flipped pin cancel, so a
// front-winding quad is still visible and a reversed back-winding quad is still
// culled. Both quads sit at z = +5 (ahead) under an identity camera.
static void TestWindingCullPolicy()
{
	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identity, 1.0472f); // 60 degrees

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
		printf("    (front centre %u,%u,%u,%u, expected red: front face must survive the cull)\n",
			frontCentre[0], frontCentre[1], frontCentre[2], frontCentre[3]);
	Check(frontVisible, "front-winding quad is kept by the pinned back-face cull");

	// Back winding: reversed index order gives a positive NDC signed area under
	// the y-negated projection and must be culled, leaving the scene clear
	// colour at the centre.
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

// ---------------------------------------------------------------------------
// E03 block 2 item 4: per-instance draw API, resource residency and the
// +Z-forward camera convention. The fixtures are raw-unit rectangles (4096
// units per metre) so the same unit policy as the game applies.
// ---------------------------------------------------------------------------

// Writes a horizontal (x-z plane) rectangle with the decoder's stored order
// (c0=v0, c1=v1, c2=v3, c3=v2), which for a convex quad is the counter-clockwise
// boundary order, and the (0,1,2)+(0,2,3) fan. `backWinding` reverses the index
// order so the same geometry is culled.
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
	NativeRenderer_SetCamera(view, 1.0472f);
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

	// A back-winding instance is culled by the same pinned policy.
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
	Check(CountRed(backImage, 64, 64) == 0, "the back-winding instance is culled");

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
	NativeRenderer_SetCamera(view, 1.0472f);
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

// ---------------------------------------------------------------------------
// E03 gate follow-up (capacity-change boundary): the renderer's LoadContent and
// RegisterResource both go through Gltf_LoadFileMulti, which rejects a texture
// reference the multi-primitive record cannot hold. A single-record GLB with an
// embedded base-colour image must be refused with a named diagnostic, and the
// renderer must stay in its previous state.
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
// bufferView in the shared BIN chunk) and textures[0] points at it, so this is a
// genuine textured GLB, not just a texture reference. The multi-primitive record
// cannot hold that image, so Gltf_LoadFileMulti must reject it.
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

static void TestTexturedContentRejected()
{
	char error[256];
	NativeRenderer_ClearContent();
	NativeRenderer_ClearInstances();
	NativeRenderer_ClearResources();

	// A valid, untextured content record first: the rejected textured load must
	// leave it intact (the strongest form of "renderer state stays clean").
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
	quad.transform[14] = 5.0f; // the only placement: ahead of the identity camera (+Z forward)
	Check(Gltf_WriteFile("textured-guard.glb", quad, error, sizeof(error)) == 0,
		"untextured guard content written");
	Check(NativeRenderer_LoadContent("textured-guard.glb", error, sizeof(error)) == 1,
		"untextured guard content loaded");

	int vertices = 0, triangles = 0;
	NativeRenderer_GetContentStats(&vertices, &triangles);
	Check(vertices == 4 && triangles == 2, "guard content stats report 4 vertices / 2 triangles");

	const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	NativeRenderer_SetCamera(identity, 1.0472f);
	// The guard quad is metre-scale (z = +5), so use metre-scale clip planes:
	// an earlier test leaves the raw-unit pair (100 / 125000) in place.
	NativeRenderer_SetClipPlanes(0.02f, 300.0f);

	// A single-record GLB with an embedded base-colour image is outside the
	// multi-primitive record the renderer loads through.
	Check(WriteTexturedGlb("textured-content.glb", error, sizeof(error)),
		"textured GLB written with an embedded base-colour image");
	char loadError[256] = "";
	Check(NativeRenderer_LoadContent("textured-content.glb", loadError, sizeof(loadError)) == 0,
		"LoadContent rejects a textured GLB");
	Check(loadError[0] != '\0', "the textured-GLB rejection has a diagnostic");
	Check(strstr(loadError, "texture") != nullptr, "the diagnostic names the texture reference");

	// State stays clean: the previous content and its stats are untouched, and
	// the scene still draws it.
	Check(NativeRenderer_HasContent() == 1, "the rejected load left the guard content loaded");
	vertices = 0;
	triangles = 0;
	NativeRenderer_GetContentStats(&vertices, &triangles);
	Check(vertices == 4 && triangles == 2, "the rejected load left the content stats intact");

	unsigned char image[64 * 64 * 4] = {};
	Check(NativeRenderer_RenderSceneToRGBA(image, 64, 64, error, sizeof(error)) == 1,
		"the scene still renders after the textured rejection");
	const unsigned char* centre = &image[((32 * 64) + 32) * 4];
	Check(centre[0] >= 239 && centre[1] <= 16 && centre[2] <= 16 && centre[3] == 255,
		"the guard content still draws after the textured rejection");

	// RegisterResource goes through the same loader and must reject it too,
	// leaving no resident resource.
	char registerError[256] = "";
	Check(NativeRenderer_RegisterResource("textured", "textured-content.glb",
		registerError, sizeof(registerError)) == 0, "RegisterResource rejects a textured GLB");
	Check(registerError[0] != '\0' && strstr(registerError, "texture") != nullptr,
		"the register rejection names the texture reference");
	Check(NativeRenderer_ResourceResident("textured") == 0, "the rejected resource is not resident");

	NativeRenderer_ClearContent();
	Check(NativeRenderer_HasContent() == 0, "guard content cleared after the textured checks");
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
	NativeRenderer_SetCamera(view, 1.0472f);
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

	// Capacity: the 17th distinct resource is a named error and never evicts a
	// resident one.
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
	NativeRenderer_SetCamera(view, 1.0472f);
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
	NativeRenderer_SetCamera(identity, 1.0472f);
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
	TestInstanceScreenRegions();
	TestMultiPrimitiveResource();
	TestTexturedContentRejected();
	TestInstanceResidencyAndDiagnostics();
	TestCameraConventionAndPlaceholder();
	TestVerticalConvention();

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
