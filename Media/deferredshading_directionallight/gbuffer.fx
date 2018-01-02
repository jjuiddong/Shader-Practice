
#include "../common.fx"

Texture2D<float> DepthTexture         : register(t0);
Texture2D<float4> ColorSpecIntTexture : register(t1);
Texture2D<float3> NormalTexture       : register(t2);
Texture2D<float4> SpecPowTexture      : register(t3);

SamplerState samPoint : register(s4)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
	BorderColor = float4(1, 1, 1, 1);
};


static float2 arrOffsets[4] = {
	float2(-0.75, -0.75),
	float2(-0.25, -0.75),
	float2(0.25, -0.75),
	float2(0.75, -0.75),
};

static const float2 arrBasePos[4] = {
	float2(1.0, 1.0),
	float2(1.0, -1.0),
	float2(-1.0, 1.0),
	float2(-1.0, -1.0),
};

static const float2 arrUV[4] = {
	float2(1.0, 0.0),
	float2(1.0, 1.0),
	float2(0.0, 0.0),
	float2(0.0, 1.0),
};

static const float4 arrMask[4] = {
	float4(1.0, 0.0, 0.0, 0.0),
	float4(0.0, 1.0, 0.0, 0.0),
	float4(0.0, 0.0, 1.0, 0.0),
	float4(0.0, 0.0, 0.0, 1.0),
};


cbuffer cbGBufferUnpack : register(b7)
{
	float4 PerspectiveValues;
	matrix ViewInv;
}


struct VS_OUTPUT
{
	float4 Position	: SV_Position; // vertex position 
	float2 UV		: TEXCOORD0;   // vertex texture coords
	float4 sampMask	: TEXCOORD1;
};


float ConvertZToLinearDepth(float depth)
{
	float linearDepth = PerspectiveValues.z / (depth + PerspectiveValues.w);
	return linearDepth;
}

float3 CalcWorldPos(float2 csPos, float depth)
{
	float4 position;

	position.xy = csPos.xy * PerspectiveValues.xy * depth;
	position.z = depth;
	position.w = 1.0;

	return mul(position, ViewInv).xyz;
}

struct SURFACE_DATA
{
	float LinearDepth;
	float3 Color;
	float3 Normal;
	float SpecPow;
	float SpecIntensity;
};

SURFACE_DATA UnpackGBuffer(float2 UV)
{
	SURFACE_DATA Out;

	float depth = DepthTexture.Sample(samPoint, UV.xy).x;
	Out.LinearDepth = ConvertZToLinearDepth(depth);
	float4 baseColorSpecInt = ColorSpecIntTexture.Sample(samPoint, UV.xy);
	Out.Color = baseColorSpecInt.xyz;
	Out.SpecIntensity = baseColorSpecInt.w;
	Out.Normal = NormalTexture.Sample(samPoint, UV.xy).xyz;
	Out.Normal = normalize(Out.Normal * 2.0 - 1.0);
	Out.SpecPow = SpecPowTexture.Sample(samPoint, UV.xy).x;

	return Out;
}


VS_OUTPUT VS(uint VertexID : SV_VertexID)
{
	VS_OUTPUT Output;

	Output.Position = float4(arrBasePos[VertexID % 4].xy * 0.2 + arrOffsets[VertexID / 4], 0.0, 1.0);
	Output.UV = arrUV[VertexID % 4].xy;
	Output.sampMask = arrMask[VertexID / 4].xyzw;

	return Output;
}


float4 PS(VS_OUTPUT In) : SV_TARGET
{
	SURFACE_DATA gbd = UnpackGBuffer(In.UV.xy);
	float4 finalColor = float4(0.0, 0.0, 0.0, 1.0);
	finalColor += float4(1.0 - saturate(gbd.LinearDepth / 75.0), 1.0 - saturate(gbd.LinearDepth / 125.0), 1.0 - saturate(gbd.LinearDepth / 200.0), 0.0) * In.sampMask.xxxx;
	finalColor += float4(gbd.Color.xyz, 0.0) * In.sampMask.yyyy;
	finalColor += float4(gbd.Normal.xyz * 0.5 + 0.5, 0.0) * In.sampMask.zzzz;
	finalColor += float4(gbd.SpecIntensity, gbd.SpecPow, 0.0, 0.0) * In.sampMask.wwww;

	return finalColor;
}


VS_OUTPUT TextureVisVS(uint VertexID : SV_VertexID)
{
	VS_OUTPUT Output;

	Output.Position = float4(arrBasePos[VertexID].xy, 0.0, 1.0);
	Output.UV = arrUV[VertexID].xy;
	Output.sampMask = 0;

	return Output;
}


float4 TextureVisPS(VS_OUTPUT In) : SV_TARGET
{
	return float4(DepthTexture.Sample(samPoint, In.UV.xy).x, 0.0, 0.0, 1.0);
}


technique11 Unlit
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetGeometryShader(NULL);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
}
