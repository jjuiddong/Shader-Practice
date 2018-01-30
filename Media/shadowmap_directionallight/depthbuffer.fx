
#include "../common.fx"

Texture2DArray<float> DepthTexture	: register(t0);

SamplerState samPoint : register(s4)
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Clamp;
	AddressV = Clamp;
	BorderColor = float4(1, 1, 1, 1);
};


static float2 arrOffsets[6] = {
	float2(-0.833, -0.75),
	float2(-0.500, -0.75),
	float2(-0.166, -0.75),
	float2(0.166, -0.75),
	float2(0.500, -0.75),
	float2(0.833, -0.75),
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

static const float4 arrMask1[6] = {
	float4(1.0, 0.0, 0.0, 0.0),
	float4(0.0, 1.0, 0.0, 0.0),
	float4(0.0, 0.0, 1.0, 0.0),
	float4(0.0, 0.0, 0.0, 1.0),
	float4(0.0, 0.0, 0.0, 0.0),
	float4(0.0, 0.0, 0.0, 0.0),
};

static const float4 arrMask2[6] = {
	float4(0.0, 0.0, 0.0, 0.0),
	float4(0.0, 0.0, 0.0, 0.0),
	float4(0.0, 0.0, 0.0, 0.0),
	float4(0.0, 0.0, 0.0, 0.0),
	float4(1.0, 0.0, 0.0, 0.0),
	float4(0.0, 1.0, 0.0, 0.0),
};


struct VS_OUTPUT
{
	float4 Position		: SV_Position; // vertex position 
	float2 UV			: TEXCOORD0;   // vertex texture coords
	float4 sampMask1	: TEXCOORD1;
	float4 sampMask2	: TEXCOORD2;
};



VS_OUTPUT VS(uint VertexID : SV_VertexID)
{
	VS_OUTPUT Output;

	Output.Position = float4(arrBasePos[VertexID % 4].xy * 0.15 + arrOffsets[VertexID / 4], 0.0, 1.0);
	Output.UV = arrUV[VertexID % 4].xy;
	Output.sampMask1 = arrMask1[VertexID / 4].xyzw;
	Output.sampMask2 = arrMask2[VertexID / 4].xyzw;

	return Output;
}


float4 PS(VS_OUTPUT In) : SV_TARGET
{
	float4 finalColor = float4(0.0, 0.0, 0.0, 1.0);

	float depth1 = DepthTexture.Sample(samPoint, float3(In.UV.xy, 0.0)).x;
	float depth2 = DepthTexture.Sample(samPoint, float3(In.UV.xy, 1.0)).x;
	float depth3 = DepthTexture.Sample(samPoint, float3(In.UV.xy, 2.0)).x;
	float depth4 = DepthTexture.Sample(samPoint, float3(In.UV.xy, 3.0)).x;
	float depth5 = DepthTexture.Sample(samPoint, float3(In.UV.xy, 4.0)).x;
	float depth6 = DepthTexture.Sample(samPoint, float3(In.UV.xy, 5.0)).x;

	finalColor += float4(depth1, depth1, depth1, 1) * In.sampMask1.xxxx;
	finalColor += float4(depth2, depth2, depth2, 1) * In.sampMask1.yyyy;
	finalColor += float4(depth3, depth3, depth3, 1) * In.sampMask1.zzzz;
	finalColor += float4(depth4, depth4, depth4, 1) * In.sampMask1.wwww;
	finalColor += float4(depth5, depth5, depth5, 1) * In.sampMask2.xxxx;
	finalColor += float4(depth6, depth6, depth6, 1) * In.sampMask2.yyyy;

	return finalColor;
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
