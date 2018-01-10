
#include "../common.fx"

Texture2D<float> DepthTexture         : register(t0);
Texture2D<float4> ColorSpecIntTexture : register(t1);
Texture2D<float3> NormalTexture       : register(t2);
Texture2D<float4> SpecPowTexture      : register(t3);
static const float2 g_SpecPowerRange = { 10.0, 250.0 };
#define EyePosition (ViewInv[3].xyz)


cbuffer cbDirLight : register(b6)
{
	float4 AmbientDown;
	float4 AmbientRange;
}


cbuffer cbGBufferUnpack : register(b7)
{
	float4 PerspectiveValues;
	matrix ViewInv;
}


struct VS_OUTPUT
{
	float4 Position : SV_Position; // vertex position 
	float2 cpPos	: TEXCOORD0;
};

static const float2 arrBasePos[4] = {
	float2(-1.0, 1.0),
	float2(1.0, 1.0),
	float2(-1.0, -1.0),
	float2(1.0, -1.0),
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(uint VertexID : SV_VertexID)
{
	VS_OUTPUT Output;
	Output.Position = float4(arrBasePos[VertexID].xy, 0.0, 1.0);
	Output.cpPos = Output.Position.xy;
	return Output;
}



//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

struct SURFACE_DATA
{
	float LinearDepth;
	float3 Color;
	float3 Normal;
	float SpecPow;
	float SpecIntensity;
};


struct Material
{
	float3 normal;
	float4 diffuseColor;
	float specPow;
	float specIntensity;
};

void MaterialFromGBuffer(SURFACE_DATA gbd, inout Material mat)
{
	mat.normal = gbd.Normal;
	mat.diffuseColor.xyz = gbd.Color;
	mat.diffuseColor.w = 1.0; // Fully opaque
	mat.specPow = g_SpecPowerRange.x + g_SpecPowerRange.y * gbd.SpecPow;
	mat.specIntensity = gbd.SpecIntensity;
}



// Ambient light calculation helper function
float3 CalcAmbient(float3 normal, float3 color)
{
	// Convert from [-1, 1] to [0, 1]
	float up = normal.y * 0.5 + 0.5;

	// Calculate the ambient value
	float3 ambient = AmbientDown + up * AmbientRange;

	// Apply the ambient value to the color
	return ambient * color;
}

// Directional light calculation helper function
float3 CalcDirectional(float3 position, Material material)
{
	// Phong diffuse
	float3 DirToLight = -gLight_Direction;
	float3 DirLightColor = gLight_Diffuse.xyz;

	float NDotL = dot(DirToLight, material.normal);
	float3 finalColor = DirLightColor.rgb * saturate(NDotL);

	// Blinn specular
	float3 ToEye = EyePosition - position;
	ToEye = normalize(ToEye);
	float3 HalfWay = normalize(ToEye + DirToLight);
	float NDotH = saturate(dot(HalfWay, material.normal));
	finalColor += DirLightColor.rgb * pow(NDotH, material.specPow) * material.specIntensity;

	return finalColor * material.diffuseColor.rgb;
}


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

SURFACE_DATA UnpackGBuffer_Loc(int2 location)
{
	SURFACE_DATA Out;
	int3 location3 = int3(location, 0);

	float depth = DepthTexture.Load(location3).x;
	Out.LinearDepth = ConvertZToLinearDepth(depth);
	float4 baseColorSpecInt = ColorSpecIntTexture.Load(location3);
	Out.Color = baseColorSpecInt.xyz;
	Out.SpecIntensity = baseColorSpecInt.w;
	Out.Normal = NormalTexture.Load(location3).xyz;
	Out.Normal = normalize(Out.Normal * 2.0 - 1.0);
	Out.SpecPow = SpecPowTexture.Load(location3).x;

	return Out;
}


float4 PS(VS_OUTPUT In) : SV_Target
{
	// Unpack the GBuffer
	SURFACE_DATA gbd = UnpackGBuffer_Loc(In.Position.xy);

	// Convert the data into the material structure
	Material mat;
	MaterialFromGBuffer(gbd, mat);

	// Reconstruct the world position
	float3 position = CalcWorldPos(In.cpPos, gbd.LinearDepth);

	// Calculate the ambient color
	float3 finalColor = CalcAmbient(mat.normal, mat.diffuseColor.rgb);

	// Calculate the directional light
	finalColor += CalcDirectional(position, mat);

	// Return the final color
	return float4(finalColor, 1.0);
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

