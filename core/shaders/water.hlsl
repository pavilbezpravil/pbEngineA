#include "shared/hlslCppShared.hlsli"
#include "common.inl"
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

   float3 patchCenter = (patch[0].posW 
                       + patch[1].posW
                       + patch[2].posW
                       + patch[3].posW) / 4;
   float distance = length(patchCenter - cameraPos);

   float tessellationAmount = gScene.tessFactorEdge / distance * 10;

   output.edges[0] = tessellationAmount;
   output.edges[1] = tessellationAmount;
   output.edges[2] = tessellationAmount;
   output.edges[3] = tessellationAmount;

   output.inside[0] = gScene.tessFactorInside / distance * 10;
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

   for (int iWave = 0; iWave < gScene.nWaves; ++iWave) {
      WaveData wave = gWaves[iWave];

      float theta = wave.magnitude * dot(wave.direction, posW.xz) - wave.frequency * time + wave.phase;

      float2 sin_cos;
      sincos(theta, sin_cos.x, sin_cos.y);

      displacement.y += sin_cos.x * wave.amplitude;
      displacement.xz += sin_cos.y * wave.direction * wave.steepness * wave.amplitude;
   }

   posW += displacement;

   output.posW = posW;
   output.posH = mul(float4(posW, 1), gCamera.viewProjection);

   return output;
}

struct PsOut {
   float4 color : SV_Target0;
};

PsOut waterPS(PixelInputType input) : SV_TARGET {
   // float2 screenUV = input.posH.xy / gCamera.rtSize;

   // float3 normalW = normalize(input.normalW);
   float3 normalW = normalize(cross(ddx(input.posW), ddy(input.posW)));
   // float3 normalW = float3(0, 1, 0);
   float3 posW = input.posW;

   float3 V = normalize(gCamera.position - posW);

   float3 color = 0;

   float3 reflectionDirection = reflect(-V, normalW);

   float fresnel = fresnelSchlick(max(dot(normalW, V), 0.0), 0.04).x; // todo:

   float3 waterColor = float3(21, 95, 179) / 256;
   color = lerp(waterColor, GetSkyColor(reflectionDirection), fresnel);

   // color = normalW;

   PsOut output = (PsOut)0;
   output.color.rgb = color;
   output.color.a = 1;

   return output;
}
