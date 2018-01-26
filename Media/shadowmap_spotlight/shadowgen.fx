
#include "../common.fx"


///////////////////////////////////////////////////////////////////
// Shadow map generation
///////////////////////////////////////////////////////////////////

float4 VS(float4 Pos : POSITION) : SV_Position
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
