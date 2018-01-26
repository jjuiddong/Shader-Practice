
#include "../common.fx"


///////////////////////////////////////////////////////////////////
// Shadow map generation
///////////////////////////////////////////////////////////////////
cbuffer cbShadowGenVS : register(b0)
{
	float4x4 ShadowMat : packoffset(c0);
}

float4 VS(float4 Pos : POSITION
	, float3 Normal : NORMAL
	, float2 Tex : TEXCOORD0
	) : SV_Position
{
	float4 PosW = mul(Pos, gWorld);
	PosW = mul(PosW, gView);
	PosW = mul(PosW, gProjection);
	return PosW;
}


technique11 Unlit
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetGeometryShader(NULL);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(NULL);
	}
}
