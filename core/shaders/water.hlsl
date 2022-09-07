#include "shared/hlslCppShared.hlsli"
#include "common.hlsli"
#include "pbr.hlsli"
#include "tonemaping.hlsli"
#include "noise.inl"
#include "samplers.hlsli"

struct VsOut {
   float3 posW : POS_W;
   float3 normalW : NORMAL_W;
   float4 posH : SV_POSITION;
   uint instanceID : SV_InstanceID;
};

StructuredBuffer<SLight> gLights;

Texture2D<float> gShadowMap;

VsOut waterVS(uint instanceID : SV_InstanceID, uint vertexID : SV_VertexID) {
   VsOut output = (VsOut)0;

   float halfSize = gScene.waterPatchSize;
   float height = 2;

   float3 corners[] = {
      float3(-halfSize, height, halfSize),
      float3(halfSize, height, halfSize),
      float3(-halfSize, height, -halfSize),
      float3(halfSize, height, -halfSize),
   };

   float3 posW = corners[vertexID];

   float2 localIdx = float2(instanceID % gScene.waterPatchCount, instanceID / gScene.waterPatchCount)
                   - float(gScene.waterPatchCount) / 2;
   // localIdx *= 1.1;
   posW.xz += localIdx * halfSize * 2;

   output.normalW = float3(0, 1, 0);
   output.posW = posW;
   output.posH = 0;
   output.instanceID = instanceID;

   return output;
}

struct ConstantOutputType {
   float edges[4] : SV_TessFactor;
   float inside[2] : SV_InsideTessFactor;
};

struct HullOutputType {
   float3 posW : POS_W;
};

////////////////////////////////////////////////////////////////////////////////
// Patch Constant Function
////////////////////////////////////////////////////////////////////////////////
ConstantOutputType WaterPatchConstantFunction(InputPatch<VsOut, 4> patch, uint patchId : SV_PrimitiveID) {    
   ConstantOutputType output;

   float3 cameraPos = gCamera.position;
   float s = gScene.waterTessFactor * gScene.waterPatchSize;

   output.edges[0] = s / length((patch[0].posW + patch[2].posW) / 2 - cameraPos);
   output.edges[1] = s / length((patch[0].posW + patch[1].posW) / 2 - cameraPos);
   output.edges[2] = s / length((patch[1].posW + patch[3].posW) / 2 - cameraPos);
   output.edges[3] = s / length((patch[2].posW + patch[3].posW) / 2 - cameraPos);

   output.inside[0] = max4(output.edges[0], output.edges[1], output.edges[2], output.edges[3]);
   output.inside[1] = output.inside[0];

   return output;
}

////////////////////////////////////////////////////////////////////////////////
// Hull Shader
////////////////////////////////////////////////////////////////////////////////
[domain("quad")]
// [partitioning("integer")]
// [partitioning("fractional_even")]
[partitioning("fractional_odd")]
// [partitioning("pow2")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("WaterPatchConstantFunction")]
HullOutputType waterHS(InputPatch<VsOut, 4> patch, uint pointId : SV_OutputControlPointID, uint patchId : SV_PrimitiveID) {
   HullOutputType output;

   output.posW = patch[pointId].posW;

   return output;
}

struct PixelInputType {
    float3 posW : POS_W;
    float3 normalW : NORMAL_W;
    float4 posH : SV_POSITION;
};

StructuredBuffer<WaveData> gWaves;

////////////////////////////////////////////////////////////////////////////////
// Domain Shader
////////////////////////////////////////////////////////////////////////////////
#define WATER_PATCH_BC(patch, param, bc) (lerp(lerp(patch[0].param, patch[1].param, bc.x), lerp(patch[2].param, patch[3].param, bc.x), bc.y))
// #define WATER_PATCH_BC(patch, param, bc) (bc.x * patch[0].param + bc.y * patch[1].param + bc.z * patch[2].param)
[domain("quad")]
PixelInputType waterDS(ConstantOutputType input, float2 bc : SV_DomainLocation, const OutputPatch<HullOutputType, 4> patch) {
   PixelInputType output;

   float3 posW = WATER_PATCH_BC(patch, posW, bc);

   float time = gScene.animationTime;

   float3 displacement = 0;

   float3 tangent = float3(1, 0, 0);
   float3 binormal = float3(0, 0, 1);

   for (int iWave = 0; iWave < gScene.nWaves; ++iWave) {
      WaveData wave = gWaves[iWave];

      float2 d = wave.direction;
      float theta = wave.magnitude * dot(d, posW.xz) - wave.frequency * time + wave.phase;

      float2 sin_cos;
      sincos(theta, sin_cos.x, sin_cos.y);

      float3 offset = wave.amplitude;
      offset.xz *= d * wave.steepness * wave.amplitude;

      displacement += offset * float3(sin_cos.y, sin_cos.x, sin_cos.y);

      float3 derivative = wave.magnitude * offset * float3(-sin_cos.x, sin_cos.y, -sin_cos.x);
      tangent += derivative * d.x;
      binormal += derivative * d.y;
   }

   posW += displacement;

   output.posW = posW;
   output.normalW = normalize(cross(binormal, tangent));
   output.posH = mul(float4(posW, 1), gCamera.viewProjection);

   return output;
}

struct PsOut {
   float4 color : SV_Target0;
};

Texture2D<float3> gDepth;
Texture2D<float3> gRefraction;

float WaterFresnel(float dotNV) {
	float ior = 1.33f;
	float c = abs(dotNV);
	float g = sqrt(ior * ior + c * c - 1);
	float fresnel = 0.5 * pow2(g - c) / pow2(g + c) * (1 + pow2(c * (g + c) - 1) / pow2(c * (g - c) + 1));
	return saturate(fresnel);
}

PsOut waterPS(PixelInputType input) : SV_TARGET {
   float2 screenUV = input.posH.xy / gCamera.rtSize;

   float3 posW = input.posW;
   float3 normalW = normalize(input.normalW);
   if (gScene.waterPixelNormals > 0) {
      normalW = normalize(cross(ddx(input.posW), ddy(input.posW)));
   }

   float3 toCamera = gCamera.position - posW;
   float3 V = normalize(toCamera);

   float3 reflectionDirection = reflect(-V, normalW);
   float3 reflectionColor = GetSkyColor(reflectionDirection);

   float3 refractionColor = gRefraction.Sample(gSamplerLinear, screenUV);
   
   float waterDepth = length(toCamera);

   float3 scenePosW = GetWorldPositionFromDepth(screenUV, gDepth.Sample(gSamplerPoint, screenUV), gCamera.invViewProjection);
   float sceneDepth = length(gCamera.position - scenePosW);

   float underwaterLength = sceneDepth - waterDepth;

   float3 fogColor = float3(21, 95, 179) / 256;
   float fogExp = 1 - exp(-underwaterLength);
   float3 underwaterColor = lerp(refractionColor, fogColor, fogExp);

   // float fresnel = fresnelSchlick(max(dot(normalW, V), 0.0), 0.04).x; // todo:
   float fresnel = WaterFresnel(dot(normalW, V));
   float3 color = lerp(underwaterColor, reflectionColor, fresnel);

   float softZ = exp(-underwaterLength * 10);
   color = lerp(color, refractionColor, softZ);

   // color = normalW;

   PsOut output = (PsOut)0;
   output.color.rgb = color;
   output.color.a = 1;

   return output;
}
