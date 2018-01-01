
#include "../common.fx"

static const float2 g_SpecPowerRange = { 10.0, 250.0 };

cbuffer cbPerObjectPS : register(b0) // Model pixel shader constants
{
	float specExp : packoffset(c0);
	float specIntensity : packoffset(c0.y);
}


struct VSOUT_DIRLIGHT
{
	float4 Pos : SV_POSITION;
	float3 Normal : TEXCOORD0;
	float2 Tex : TEXCOORD1;
	float4 PosH : TEXCOORD2;
	float3 PosW : TEXCOORD3;
	float3 toEye : TEXCOORD4;
	float clip : SV_ClipDistance0;
};



/////////////////////////////////////////////////////////////////////////////
// Vertex shader
/////////////////////////////////////////////////////////////////////////////

VSOUT_DIRLIGHT VS(float4 Pos : POSITION
	, float3 Normal : NORMAL
	, float2 Tex : TEXCOORD0
	, uint instID : SV_InstanceID
	, uniform bool IsInstancing
)
{
	VSOUT_DIRLIGHT output = (VSOUT_DIRLIGHT)0;
	const matrix mWorld = IsInstancing ? gWorldInst[instID] : gWorld;

	float4 PosW = mul(Pos, mWorld);
	output.Pos = mul(PosW, gView);
	output.Pos = mul(output.Pos, gProjection);
	output.Normal = normalize(mul(Normal, (float3x3)mWorld));
	output.Tex = Tex;
	output.PosH = output.Pos;
	output.PosW = PosW.xyz;
	output.toEye = normalize(float4(gEyePosW, 1) - PosW).xyz;
	output.clip = dot(PosW, gClipPlane);

	return output;
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shader
/////////////////////////////////////////////////////////////////////////////

struct PS_GBUFFER_OUT
{
	float4 ColorSpecInt : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 SpecPow : SV_TARGET2;
};

PS_GBUFFER_OUT PackGBuffer(float3 BaseColor, float3 Normal, float SpecIntensity, float SpecPower)
{
	PS_GBUFFER_OUT Out;

	// Normalize the specular power
	float SpecPowerNorm = max(0.0001, (SpecPower - g_SpecPowerRange.x) / g_SpecPowerRange.y);

	// Pack all the data into the GBuffer structure
	Out.ColorSpecInt = float4(BaseColor.rgb, SpecIntensity);
	Out.Normal = float4(Normal * 0.5 + 0.5, 0.0);
	Out.SpecPow = float4(SpecPowerNorm, 0.0, 0.0, 0.0);

	return Out;
}

PS_GBUFFER_OUT PS(VSOUT_DIRLIGHT In)
{
	// Lookup mesh texture and modulate it with diffuse
	float3 DiffuseColor = txDiffuse.Sample(samLinear, In.Tex);
	DiffuseColor *= DiffuseColor;

	return PackGBuffer(DiffuseColor, normalize(In.Normal), specIntensity, specExp);
}



technique11 Unlit
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS(NotInstancing)));
		SetGeometryShader(NULL);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
}
