
#include "../common.fx"

cbuffer cbuffercbShadowMapGS : register(b6)
{
	matrix ShadowViewProj[3];
};

struct GS_OUTPUT
{
	float4 Pos		: SV_POSITION;
	uint RTIndex	: SV_RenderTargetArrayIndex;
};


///////////////////////////////////////////////////////////////////
// Shadow map generation
///////////////////////////////////////////////////////////////////
float4 VS(float4 Pos : POSITION) : SV_Position
{
	float4 PosW = mul(Pos, gWorld);
	return PosW;
}


[maxvertexcount(9)]
void GS(triangle float4 InPos[3] : SV_Position
	, inout TriangleStream<GS_OUTPUT> OutStream)
{
	for (int iFace = 0; iFace < 3; iFace++)
	{
		GS_OUTPUT output;

		output.RTIndex = iFace;

		for (int v = 0; v < 3; v++)
		{
			output.Pos = mul(InPos[v], ShadowViewProj[iFace]);
			OutStream.Append(output);
		}
		OutStream.RestartStrip();
	}
}


technique11 Unlit
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetGeometryShader(CompileShader(gs_5_0, GS()));
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(NULL);
	}
}
