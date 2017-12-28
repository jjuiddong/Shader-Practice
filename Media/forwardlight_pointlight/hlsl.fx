
#include "../common.fx"

cbuffer cbPointLightPS : register(b6) // point light constants
{
	float4 AmbientDown;
	float4 AmbientRange;
}


struct VSOUT_POINTLIGHT
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
VSOUT_POINTLIGHT VS(float4 Pos : POSITION
	, float3 Normal : NORMAL
	, float2 Tex : TEXCOORD0
	, uint instID : SV_InstanceID
	, uniform bool IsInstancing
)
{
	VSOUT_POINTLIGHT output = (VSOUT_POINTLIGHT)0;
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


// Ambient calculation helper function
float3 CalcAmbient(float3 normal, float3 color)
{
	// Convert from [-1, 1] to [0, 1]
	float up = normal.y * 0.5 + 0.5;
	// Calculate the ambient value
	float3 ambient = AmbientDown.xyz + up * AmbientRange.xyz;
	// Apply the ambient value to the color
	return ambient * color;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS(VSOUT_POINTLIGHT In) : SV_Target
{
	float3 ToLight = gLight_PosW - In.PosW;
	float DistToLight = length(ToLight);

	// Phong diffuse
	ToLight /= DistToLight; // Normalize

	// Sample the texture and convert to linear space
	float3 DiffuseColor = txDiffuse.Sample(samLinear, In.Tex).rgb;
	DiffuseColor *= DiffuseColor;
	// Calculate the ambient color
	//float3 AmbientColor = CalcAmbient(In.Normal, DiffuseColor);
	// Return the ambient color

	const float3 L = ToLight;
	const float3 H = normalize(L + normalize(In.toEye));
	const float3 N = normalize(In.Normal);
	const float lightV = saturate(dot(N, L));

	float3 finalColor = 
		  gLight_Diffuse.xyz * lightV
		+ gLight_Specular.xyz * pow(saturate(dot(N, H)), 0.1f);// gMtrl_Pow);

	float range = 30.f;
	float DistToLightNorm = 1.0 - saturate(DistToLight * (1/range));
	float Attn = DistToLightNorm * DistToLightNorm;
	finalColor *= DiffuseColor * Attn;

	return float4(finalColor, 1.0);
}


float4 PS_Old(VSOUT_POINTLIGHT In) : SV_Target
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
