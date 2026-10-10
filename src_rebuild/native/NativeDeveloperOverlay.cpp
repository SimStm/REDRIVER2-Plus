#include "NativeDeveloperOverlay.h"
#include "imgui.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "RefCntAutoPtr.hpp"
#include <algorithm>
#include <cstring>
#include <stdio.h>

using namespace Diligent;

namespace
{
RefCntAutoPtr<IRenderDevice> device;
RefCntAutoPtr<IPipelineState> pipeline;
RefCntAutoPtr<IShaderResourceBinding> bindings;
RefCntAutoPtr<IBuffer> vertices, indices, constants;
RefCntAutoPtr<ITexture> font;
Uint64 vertexCapacity = 0, indexCapacity = 0;
bool warnedLegacyTexture = false;

bool DynamicBuffer(RefCntAutoPtr<IBuffer>& buffer, Uint64 size, BIND_FLAGS flags, const char* name)
{
	BufferDesc desc;
	desc.Name = name;
	desc.Size = size;
	desc.Usage = USAGE_DYNAMIC;
	desc.BindFlags = flags;
	desc.CPUAccessFlags = CPU_ACCESS_WRITE;
	buffer.Release();
	device->CreateBuffer(desc, nullptr, &buffer);
	return buffer != nullptr;
}

void BindState(IDeviceContext* context, ITextureView* target, int width, int height)
{
	context->SetRenderTargets(1, &target, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	Viewport viewport;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	context->SetViewports(1, &viewport, width, height);
	context->SetPipelineState(pipeline);
	IBuffer* vb = vertices;
	Uint64 offset = 0;
	context->SetVertexBuffers(0, 1, &vb, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
	context->SetIndexBuffer(indices, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
	context->CommitShaderResources(bindings, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}
}

bool NativeDeveloperOverlay_Init(void* nativeDevice)
{
	NativeDeveloperOverlay_Shutdown();
	device = static_cast<IRenderDevice*>(nativeDevice);
	if (!device)
		return false;
	ShaderCreateInfo shader;
	shader.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
	shader.EntryPoint = "main";
	shader.Desc.ShaderType = SHADER_TYPE_VERTEX;
	shader.Desc.Name = "Native developer overlay vertex";
	shader.Source = R"(
cbuffer Constants { float4 transform; }
struct Output { float4 pos:SV_POSITION; float2 uv:TEXCOORD; float4 color:COLOR; };
Output main(float2 pos:ATTRIB0, float2 uv:ATTRIB1, float4 color:ATTRIB2) {
 Output o; o.pos=float4(pos*transform.xy+transform.zw,0,1); o.uv=uv; o.color=color; return o;
})";
	RefCntAutoPtr<IShader> vs, ps;
	device->CreateShader(shader, &vs);
	shader.Desc.ShaderType = SHADER_TYPE_PIXEL;
	shader.Desc.Name = "Native developer overlay pixel";
	shader.Source = R"(
Texture2D fontTexture; SamplerState fontSampler;
float4 main(float4 pos:SV_POSITION, float2 uv:TEXCOORD, float4 color:COLOR):SV_TARGET {
 return color*fontTexture.Sample(fontSampler,uv);
})";
	device->CreateShader(shader, &ps);
	if (!vs || !ps)
		return false;
	LayoutElement layout[] = {
		{0, 0, 2, VT_FLOAT32, False, (Uint32)offsetof(ImDrawVert, pos), sizeof(ImDrawVert)},
		{1, 0, 2, VT_FLOAT32, False, (Uint32)offsetof(ImDrawVert, uv), sizeof(ImDrawVert)},
		{2, 0, 4, VT_UINT8, True, (Uint32)offsetof(ImDrawVert, col), sizeof(ImDrawVert)}
	};
	GraphicsPipelineStateCreateInfo info;
	info.PSODesc.Name = "Native developer overlay";
	info.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
	info.pVS = vs;
	info.pPS = ps;
	auto& graphics = info.GraphicsPipeline;
	graphics.NumRenderTargets = 1;
	graphics.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
	graphics.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	graphics.RasterizerDesc.CullMode = CULL_MODE_NONE;
	graphics.RasterizerDesc.ScissorEnable = True;
	graphics.DepthStencilDesc.DepthEnable = False;
	graphics.DepthStencilDesc.DepthWriteEnable = False;
	graphics.InputLayout.LayoutElements = layout;
	graphics.InputLayout.NumElements = 3;
	auto& blend = graphics.BlendDesc.RenderTargets[0];
	blend.BlendEnable = True;
	blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
	blend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
	blend.SrcBlendAlpha = BLEND_FACTOR_ONE;
	blend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
	device->CreateGraphicsPipelineState(info, &pipeline);
	if (!pipeline || !DynamicBuffer(constants, 16, BIND_UNIFORM_BUFFER, "Native overlay transform"))
		return false;
	unsigned char* pixels;
	int width, height;
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	TextureDesc texture;
	texture.Name = "Native ImGui font atlas";
	texture.Type = RESOURCE_DIM_TEX_2D;
	texture.Width = width;
	texture.Height = height;
	texture.Format = TEX_FORMAT_RGBA8_UNORM;
	texture.Usage = USAGE_IMMUTABLE;
	texture.BindFlags = BIND_SHADER_RESOURCE;
	TextureSubResData subresource;
	subresource.pData = pixels;
	subresource.Stride = width * 4;
	TextureData data;
	data.pSubResources = &subresource;
	data.NumSubresources = 1;
	device->CreateTexture(texture, &data, &font);
	RefCntAutoPtr<ISampler> sampler;
	SamplerDesc sampling;
	sampling.MinFilter = sampling.MagFilter = sampling.MipFilter = FILTER_TYPE_LINEAR;
	sampling.AddressU = sampling.AddressV = sampling.AddressW = TEXTURE_ADDRESS_CLAMP;
	device->CreateSampler(sampling, &sampler);
	if (!font || !sampler)
		return false;
	pipeline->CreateShaderResourceBinding(&bindings, true);
	if (!bindings)
		return false;
	bindings->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(constants);
	bindings->GetVariableByName(SHADER_TYPE_PIXEL, "fontTexture")->Set(font->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	bindings->GetVariableByName(SHADER_TYPE_PIXEL, "fontSampler")->Set(sampler);
	ImGui::GetIO().Fonts->SetTexID((ImTextureID)font->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
	ImGui::GetIO().BackendRendererName = "redriver2_diligent";
	ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	return true;
}

bool NativeDeveloperOverlay_Draw(void* nativeContext, void* nativeTarget, ImDrawData* data)
{
	if (!data || data->TotalVtxCount == 0)
		return true;
	if (!pipeline || !bindings || !nativeContext || !nativeTarget)
		return false;
	auto* context = static_cast<IDeviceContext*>(nativeContext);
	auto* target = static_cast<ITextureView*>(nativeTarget);
	const int width = (int)(data->DisplaySize.x * data->FramebufferScale.x);
	const int height = (int)(data->DisplaySize.y * data->FramebufferScale.y);
	if (width <= 0 || height <= 0)
		return true;
	const Uint64 vertexBytes = (Uint64)data->TotalVtxCount * sizeof(ImDrawVert);
	const Uint64 indexBytes = (Uint64)data->TotalIdxCount * sizeof(ImDrawIdx);
	// A developer overlay must not cause unbounded allocations on corrupt data.
	if (vertexBytes > 32 * 1024 * 1024 || indexBytes > 16 * 1024 * 1024)
		return false;
	if (vertexBytes > vertexCapacity)
	{
		if (!DynamicBuffer(vertices, vertexBytes, BIND_VERTEX_BUFFER, "Native overlay vertices")) return false;
		vertexCapacity = vertexBytes;
	}
	if (indexBytes > indexCapacity)
	{
		if (!DynamicBuffer(indices, indexBytes, BIND_INDEX_BUFFER, "Native overlay indices")) return false;
		indexCapacity = indexBytes;
	}
	void* vertexMap = nullptr;
	void* indexMap = nullptr;
	context->MapBuffer(vertices, MAP_WRITE, MAP_FLAG_DISCARD, vertexMap);
	if (!vertexMap) return false;
	context->MapBuffer(indices, MAP_WRITE, MAP_FLAG_DISCARD, indexMap);
	if (!indexMap) { context->UnmapBuffer(vertices, MAP_WRITE); return false; }
	for (const ImDrawList* list : data->CmdLists)
	{
		memcpy(vertexMap, list->VtxBuffer.Data, list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(indexMap, list->IdxBuffer.Data, list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vertexMap = (char*)vertexMap + list->VtxBuffer.Size * sizeof(ImDrawVert);
		indexMap = (char*)indexMap + list->IdxBuffer.Size * sizeof(ImDrawIdx);
	}
	context->UnmapBuffer(vertices, MAP_WRITE);
	context->UnmapBuffer(indices, MAP_WRITE);
	const float transform[] = {2 / data->DisplaySize.x, -2 / data->DisplaySize.y,
		-1 - data->DisplayPos.x * 2 / data->DisplaySize.x, 1 + data->DisplayPos.y * 2 / data->DisplaySize.y};
	void* mapped = nullptr;
	context->MapBuffer(constants, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
	if (!mapped) return false;
	memcpy(mapped, transform, sizeof(transform));
	context->UnmapBuffer(constants, MAP_WRITE);
	BindState(context, target, width, height);
	Uint32 vertexOffset = 0, indexOffset = 0;
	for (const ImDrawList* list : data->CmdLists)
	{
		for (const ImDrawCmd& command : list->CmdBuffer)
		{
			if (command.UserCallback)
			{
				if (command.UserCallback == ImDrawCallback_ResetRenderState) BindState(context, target, width, height);
				else command.UserCallback(list, &command);
				continue;
			}
			// Legacy inspector texture handles are not Diligent resources. Never
			// cast them to GPU pointers; native inspector images need registration.
			if (command.GetTexID() != ImGui::GetIO().Fonts->TexID)
			{
				if (!warnedLegacyTexture)
				{
					FILE* log = fopen("native_renderer.log", "a");
					if (log) { fputs("native overlay: legacy inspector image omitted; native texture registration pending\n", log); fclose(log); }
					warnedLegacyTexture = true;
				}
				continue;
			}
			Rect clip;
			clip.left = std::max(0, (int)((command.ClipRect.x - data->DisplayPos.x) * data->FramebufferScale.x));
			clip.top = std::max(0, (int)((command.ClipRect.y - data->DisplayPos.y) * data->FramebufferScale.y));
			clip.right = std::min(width, (int)((command.ClipRect.z - data->DisplayPos.x) * data->FramebufferScale.x));
			clip.bottom = std::min(height, (int)((command.ClipRect.w - data->DisplayPos.y) * data->FramebufferScale.y));
			if (clip.right <= clip.left || clip.bottom <= clip.top) continue;
			context->SetScissorRects(1, &clip, width, height);
			DrawIndexedAttribs draw;
			draw.NumIndices = command.ElemCount;
			draw.IndexType = sizeof(ImDrawIdx) == 2 ? VT_UINT16 : VT_UINT32;
			draw.FirstIndexLocation = indexOffset + command.IdxOffset;
			draw.BaseVertex = vertexOffset + command.VtxOffset;
			context->DrawIndexed(draw);
		}
		vertexOffset += list->VtxBuffer.Size;
		indexOffset += list->IdxBuffer.Size;
	}
	return true;
}

void NativeDeveloperOverlay_Shutdown()
{
	if (ImGui::GetCurrentContext())
	{
		ImGui::GetIO().Fonts->SetTexID(0);
		ImGui::GetIO().BackendRendererName = nullptr;
		ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
	}
	bindings.Release(); pipeline.Release(); font.Release();
	vertices.Release(); indices.Release(); constants.Release(); device.Release();
	vertexCapacity = indexCapacity = 0;
}
