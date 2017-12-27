
#include "../common.fx"


cbuffer cbDirLightPS : register(b6) // Ambient light constants
{
	float4 AmbientDown;
	float4 AmbientRange;
}



//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VSOUT_TEX VS(float4 Pos : POSITION
	, float3 Normal : NORMAL
	, float2 Tex : TEXCOORD0
	, uint instID : SV_InstanceID
	, uniform bool IsInstancing
)
{
	VSOUT_TEX output = (VSOUT_TEX)0;
	const matrix mWorld = IsInstancing ? gWorldInst[instID] : gWorld;

	float4 PosW = mul(Pos, mWorld);
	output.Pos = mul(PosW, gView);
	output.Pos = mul(output.Pos, gProjection);
	output.Normal = normalize(mul(Normal, (float3x3)mWorld));
	output.Tex = Tex;
	output.PosH = output.Pos;
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
float4 PS(VSOUT_TEX In) : SV_Target
{
	// Sample the texture and convert to linear space
	float3 DiffuseColor = txDiffuse.Sample(samLinear, In.Tex).rgb;
	DiffuseColor *= DiffuseColor;
	// Calculate the ambient color
	float3 AmbientColor = CalcAmbient(In.Normal, DiffuseColor);
	// Return the ambient color
	return float4(AmbientColor, 1.0);
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

