#ifndef NATIVE_SKY_SHADER_H
#define NATIVE_SKY_SHADER_H
// The private converted sky carries model 0..3 in Shade, and canonical 128/64
// in Col. This shader never reads the building shade palette. RGB is the already
// evaluated original byte color; reflection division truncates before modulation.
// colorScale.w is the sky-only normalized bilinear texel phase (zero for point).
static const char* NativeSkyShader_Source()
{
	return
		"cbuffer Constants { float4x4 MVP; float4 baseColor; float4 colorScale; float4 worldShadePalette[32]; };\n"
		"struct VSIn { float3 Pos:ATTRIB0; float2 UV:ATTRIB1; float4 Col:ATTRIB2; float Shade:ATTRIB3; };\n"
		"struct VSOut { float4 Pos:SV_POSITION; float2 UV:TEXCOORD0; float4 Col:COLOR0; };\n"
		"void main(in VSIn input, out VSOut output) {\n"
		" output.Pos=mul(MVP,float4(input.Pos,1.0)); output.Pos.z=output.Pos.w;\n"
		" int model=(int)input.Shade; if(worldShadePalette[0][model]<0.5) output.Pos=float4(0,0,-1,1);\n"
		" float3 rgb=input.Col.r<0.4 ? floor(colorScale.rgb/2.0) : colorScale.rgb;\n"
		" output.Col=float4(rgb*(2.0/255.0),1.0); output.UV=input.UV+colorScale.ww; }\n";
}
#endif
