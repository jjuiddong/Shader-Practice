
#include "../common.fx"

cbuffer cbCapsuleLightPS : register(b6) // capsule light constants
{
	float4 gCapsuleLightLength;
	float4 gCapsuleLightRange;
}

struct VSOUT_SPOTLIGHT
{
	float4 Pos : SV_POSITION;
	float3 Normal : TEXCOORD0;
	float2 Tex : TEXCOORD1;
	float4 PosH : TEXCOORD2;
	float3 PosW : TEXCOORD3;
	float3 toEye : TEXCOORD4;
	float clip : SV_ClipDistance0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VSOUT_SPOTLIGHT VS(float4 Pos : POSITION
	, float3 Normal : NORMAL
	, float2 Tex : TEXCOORD0
	, uint instID : SV_InstanceID
	, uniform bool IsInstancing
)
{
	VSOUT_SPOTLIGHT output = (VSOUT_SPOTLIGHT)0;
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


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VSOUT_SPOTLIGHT In) : SV_Target
{
	// Find the shortest distance between the pixel and capsules segment
	float3 ToCapsuleStart = In.PosW - gLight_PosW;
	float DistOnLine = dot(ToCapsuleStart, gLight_Direction) / gCapsuleLightLength;
	DistOnLine = saturate(DistOnLine) * gCapsuleLightLength;
	float3 PointOnLine = gLight_PosW + gLight_Direction * DistOnLine;
	float3 ToLight = PointOnLine - In.PosW;
	float DistToLight = length(ToLight);

	ToLight /= DistToLight; // Normalize

	// Sample the texture and convert to linear space
	float3 DiffuseColor = txDiffuse.Sample(samLinear, In.Tex).rgb;
	DiffuseColor *= DiffuseColor;

	// Blinn Phong diffuse
	const float3 L = ToLight;
	const float3 H = normalize(L + normalize(In.toEye));
	const float3 N = normalize(In.Normal);
	const float lightV = saturate(dot(N, L));

	float3 finalColor = 
		  gLight_Diffuse.xyz * lightV
		+ gLight_Specular.xyz * pow(saturate(dot(N, H)), 0.1f);// gMtrl_Pow)
		;

	// Attenuation
	float range = gCapsuleLightRange.x;
	float DistToLightNorm = 1.0 - saturate(DistToLight * (1/range));
	float Attn = DistToLightNorm * DistToLightNorm;
	finalColor *= DiffuseColor * Attn;

	return float4(finalColor, 1.0);
}


float4 PS_Old(VSOUT_SPOTLIGHT In) : SV_Target
{
	float4 color = GetLightingColor(In.Normal, In.toEye, 1.f);
	float4 texColor = txDiffuse.Sample(samLinear, In.Tex);
	float4 Out = color * texColor;
	return float4(Out.xyz, gMtrl_Diffuse.a * texColor.a);
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


technique11 Unlit_Old
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS(NotInstancing)));
		SetGeometryShader(NULL);
		SetHullShader(NULL);
		SetDomainShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS_Old()));
	}
}
