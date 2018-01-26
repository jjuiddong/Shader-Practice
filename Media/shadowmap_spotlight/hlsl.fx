
#include "../common.fx"

Texture2D<float> DepthTexture         : register(t0);
Texture2D<float4> ColorSpecIntTexture : register(t1);
Texture2D<float3> NormalTexture       : register(t2);
Texture2D<float4> SpecPowTexture      : register(t3);
Texture2D<float> SpotShadowMapTexture : register(t4);
static const float2 g_SpecPowerRange = { 10.0, 250.0 };
#define EyePosition (ViewInv[3].xyz)

SamplerComparisonState PCFSampler : register(s4)
{
	Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;

	AddressU = Border;
	AddressV = Border;
	AddressW = Border;
	BorderColor = float4(1, 1, 1, 1);
	ComparisonFunc = LESS_EQUAL;
};


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

cbuffer cbSpotLight : register(b8)
{
	float4 SpotLightPos;
	float4 SpotLightRangeRcp;
	float4 SpotDirToLight;
	float4 SpotCosOuterCone;
	float4 SpotColor;
	float4 SpotCosConeAttRange;

	matrix LightProjection;
	float4 SinAngle;
	float4 CosAngle;
	matrix ToShadowmap;
}

struct DS_OUTPUT
{
	float4 Position : SV_POSITION;
	float3 PositionXYW	: TEXCOORD0;
};



/////////////////////////////////////////////////////////////////////////////
// Vertex shader
/////////////////////////////////////////////////////////////////////////////
float4 VS() : SV_Position
{
	return float4(0.0, 0.0, 0.0, 1.0);
}

/////////////////////////////////////////////////////////////////////////////
// Hull shader
/////////////////////////////////////////////////////////////////////////////
struct HS_CONSTANT_DATA_OUTPUT
{
	float Edges[4] : SV_TessFactor;
	float Inside[2] : SV_InsideTessFactor;
};

HS_CONSTANT_DATA_OUTPUT SpotLightConstantHS()
{
	HS_CONSTANT_DATA_OUTPUT Output;

	float tessFactor = 18.0;
	Output.Edges[0] = Output.Edges[1] = Output.Edges[2] = Output.Edges[3] = tessFactor;
	Output.Inside[0] = Output.Inside[1] = tessFactor;

	return Output;
}

struct HS_OUTPUT
{
	float3 Position : POSITION;
};


[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("SpotLightConstantHS")]
HS_OUTPUT SpotLightHS(uint PatchID : SV_PrimitiveID)
{
	HS_OUTPUT Output;

	Output.Position = float3(0.0, 0.0, 0.0);

	return Output;
}

/////////////////////////////////////////////////////////////////////////////
// Domain Shader shader
/////////////////////////////////////////////////////////////////////////////
#define CylinderPortion 0.2f
#define ExpendAmount    (1.0f + CylinderPortion)

[domain("quad")]
DS_OUTPUT SpotLightDS(HS_CONSTANT_DATA_OUTPUT input, float2 UV : SV_DomainLocation
	, const OutputPatch<HS_OUTPUT, 4> quad)
{
	// Transform the UV's into clip-space
	float2 posClipSpace = UV.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

	// Find the vertex offsets based on the UV
	float2 posClipSpaceAbs = abs(posClipSpace.xy);
	float maxLen = max(posClipSpaceAbs.x, posClipSpaceAbs.y);

	// Force the cone vertices to the mesh edge
	float2 posClipSpaceNoCylAbs = saturate(posClipSpaceAbs * ExpendAmount);
	float maxLenNoCapsule = max(posClipSpaceNoCylAbs.x, posClipSpaceNoCylAbs.y);
	float2 posClipSpaceNoCyl = sign(posClipSpace.xy) * posClipSpaceNoCylAbs;

	// Convert the positions to half sphere with the cone vertices on the edge
	float3 halfSpherePos = normalize(float3(posClipSpaceNoCyl.xy, 1.0f - maxLenNoCapsule));

	// Scale the sphere to the size of the cones rounded base
	halfSpherePos = normalize(float3(halfSpherePos.xy * SinAngle.x, CosAngle.x));

	// Find the offsets for the cone vertices (0 for cone base)
	float cylinderOffsetZ = saturate((maxLen * ExpendAmount - 1.0f) / CylinderPortion);

	// Offset the cone vertices to thier final position
	float4 posLS = float4(halfSpherePos.xy * (1.0f - cylinderOffsetZ), halfSpherePos.z - cylinderOffsetZ * CosAngle.x, 1.0f);

	// Transform all the way to projected space and generate the UV coordinates
	DS_OUTPUT Output;
	Output.Position = mul(posLS, LightProjection);
	Output.PositionXYW = Output.Position.xyw;

	return Output;
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shader
/////////////////////////////////////////////////////////////////////////////

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


// Shadow PCF calculation helper function
float SpotShadowPCF(float3 position)
{
	// Transform the world position to shadow projected space
	float4 posShadowMap = mul(float4(position, 1.0), ToShadowmap);

	// Transform the position to shadow clip space
	float3 UVD = posShadowMap.xyz / posShadowMap.w;

	// Convert to shadow map UV values
	//UVD.xy = 0.5 * UVD.xy + 0.5;
	//UVD.y = 1.0 - UVD.y;

	// Compute the hardware PCF value
	const float bias = 0;// 0.001f;
	return SpotShadowMapTexture.SampleCmpLevelZero(PCFSampler, UVD.xy, UVD.z - bias);
}


float3 CalcSpot(float3 position, Material material, bool bUseShadow)
{
	float3 ToLight = SpotLightPos.xyz - position;
	float3 ToEye = EyePosition - position;
	float DistToLight = length(ToLight);

	// Phong diffuse
	ToLight /= DistToLight; // Normalize
	float NDotL = saturate(dot(ToLight, material.normal));
	float3 finalColor = material.diffuseColor.rgb * NDotL;

	// Blinn specular
	ToEye = normalize(ToEye);
	float3 HalfWay = normalize(ToEye + ToLight);
	float NDotH = saturate(dot(HalfWay, material.normal));
	finalColor += pow(NDotH, material.specPow) * material.specIntensity;

	// Cone attenuation
	float cosAng = dot(SpotDirToLight.xyz, ToLight);
	float conAtt = saturate((cosAng - SpotCosOuterCone.x) / SpotCosConeAttRange.x);
	conAtt *= conAtt;

	float shadowAtt = SpotShadowPCF(position);

	// Attenuation
	float DistToLightNorm = 1.0 - saturate(DistToLight * SpotLightRangeRcp.x);
	float Attn = DistToLightNorm * DistToLightNorm;
	finalColor *= SpotColor.rgb * Attn * conAtt * shadowAtt;

	return finalColor;
}


float4 SpotLightCommonPS(DS_OUTPUT In, bool bUseShadow) : SV_TARGET
{
	// Unpack the GBuffer
	SURFACE_DATA gbd = UnpackGBuffer_Loc(In.Position.xy);

	// Convert the data into the material structure
	Material mat;
	MaterialFromGBuffer(gbd, mat);

	// Reconstruct the world position
	//float3 position = CalcWorldPos(In.cpPos, gbd.LinearDepth);
	float3 position = CalcWorldPos(In.PositionXYW.xy / In.PositionXYW.z, gbd.LinearDepth);

	// Calculate the light contribution
	float3 finalColor = CalcSpot(position, mat, bUseShadow);

	// return the final color
	return float4(finalColor, 1.0);
}

float4 PS(DS_OUTPUT In) : SV_TARGET
{
	return SpotLightCommonPS(In, false);
}

float4 SpotLightShadowPS(DS_OUTPUT In) : SV_TARGET
{
	return SpotLightCommonPS(In, true);
}


technique11 Unlit
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetGeometryShader(NULL);
		SetHullShader(CompileShader(hs_5_0, SpotLightHS()));
		SetDomainShader(CompileShader(ds_5_0, SpotLightDS()));
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
}
