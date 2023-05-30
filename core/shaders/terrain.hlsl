#include "commonResources.hlsli"
#include "common.hlsli"
#include "pbr.hlsli"
#include "noise.inl"
#include "sky.hlsli"
#include "lighting.hlsli"

// todo:
#define EDITOR
#ifdef EDITOR
  #include "editor.hlsli"
#endif

cbuffer gTerrainCB {
  STerrainCB gTerrain;
}

struct VsOut {
   float3 posW : POS_W;
   float3 normalW : NORMAL_W;
   float4 posH : SV_POSITION;
};

VsOut waterVS(uint instanceID : SV_InstanceID, uint vertexID : SV_VertexID) {
   VsOut output = (VsOut)0;

   float halfSize = gTerrain.waterPatchSize / 2;
   float height = gTerrain.center.y;

   float3 corners[] = {
      float3(-halfSize, height, halfSize),
      float3(halfSize, height, halfSize),
      float3(-halfSize, height, -halfSize),
      float3(halfSize, height, -halfSize),
   };

   float3 posW = corners[vertexID];
   // posW.xz += gTerrain.center.xz;

   float2 localIdx = float2(instanceID % gTerrain.waterPatchCount, instanceID / gTerrain.waterPatchCount)
                   - float(gTerrain.waterPatchCount) / 2;
   // localIdx *= 1.1;
   posW.xz += localIdx * halfSize * 2;

   output.normalW = float3(0, 1, 0);
   output.posW = posW;
   output.posH = 0;

   return output;
}

struct ConstantOutputType {
   float edges[4] : SV_TessFactor;
   float inside[2] : SV_InsideTessFactor;
};

struct HullOutputType {
   float3 posW : POS_W;
};

float TessFactor(float3 p) {
   float3 cameraPos = GetCullCamera().position;
   float s = gTerrain.waterTessFactor * gTerrain.waterPatchSize;

   return s / length(p - cameraPos);
}

float TessFactor(float3 p0, float3 p1) {
   return TessFactor(CentralPoint(p0, p1));
}

ConstantOutputType WaterPatchConstantFunction(InputPatch<VsOut, 4> patch, uint patchId : SV_PrimitiveID) {    
   ConstantOutputType output;

   float3 posW = (patch[0].posW + patch[1].posW + patch[2].posW + patch[3].posW) / 4;

   float sphereRadius = gTerrain.waterPatchSize; // todo:
   if (!FrustumSphereTest(GetCullCamera().frustumPlanes, posW, sphereRadius)) {
      return (ConstantOutputType)0;
   }

   output.edges[0] = TessFactor(patch[2].posW, patch[0].posW);
   output.edges[1] = TessFactor(patch[0].posW, patch[1].posW);
   output.edges[2] = TessFactor(patch[1].posW, patch[3].posW);
   output.edges[3] = TessFactor(patch[3].posW, patch[2].posW);

   output.inside[0] = output.inside[1] = max4(output.edges);

   return output;
}

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

#define WATER_PATCH_BC(patch, param, bc) (lerp(lerp(patch[0].param, patch[1].param, bc.x), lerp(patch[2].param, patch[3].param, bc.x), bc.y))
// #define WATER_PATCH_BC(patch, param, bc) (bc.x * patch[0].param + bc.y * patch[1].param + bc.z * patch[2].param)
[domain("quad")]
PixelInputType waterDS(ConstantOutputType input, float2 bc : SV_DomainLocation, const OutputPatch<HullOutputType, 4> patch) {
   PixelInputType output;

   float3 posW = WATER_PATCH_BC(patch, posW, bc);

   const float polySize = gTerrain.waterPatchSize / TessFactor(posW);

   float3 displacement = 0;

   // todo:
   float3 tangent = float3(1, 0, 0);
   float3 binormal = float3(0, 0, 1);

   posW += displacement;
   posW.y += noise(posW.xz * 0.1) * 10;

   output.posW = posW;
   output.normalW = normalize(cross(binormal, tangent));
   output.posH = mul(float4(posW, 1), gCamera.viewProjection);

   return output;
}

struct PsOut {
   float4 color : SV_Target0;
};

float3 PixelNormal(float3 posW) {
   return normalize(cross(ddx(posW), ddy(posW)));
}

[earlydepthstencil]
PsOut waterPS(PixelInputType input) : SV_TARGET {
   float2 screenUV = input.posH.xy / gCamera.rtSize;

   float3 posW = input.posW;
   float3 normalW = normalize(input.normalW);
   if (gTerrain.waterPixelNormals > 0) {
      normalW = PixelNormal(input.posW);
   }

   float3 toCamera = gCamera.position - posW;
   float3 V = normalize(toCamera);
   // float3 V = normalize(gCamera.position - posW);

   Surface surface;
   surface.metallic = 0;
   surface.roughness = 0.5;

   surface.posW = posW;
   surface.normalW = normalW;

   float3 baseColor = gTerrain.color;

   surface.albedo = baseColor * (1.0 - surface.metallic);
   surface.F0 = lerp(0.04, baseColor, surface.metallic);

   float3 Lo = Shade(surface, V);

   float3 color = Lo;

   // color = normalW;
   // color = rand2dTo1d(posW.xz);
   // color = noise(posW.xz);
   // float2 noisePos = posW.xz * 0.2;
   // float2 noise2 = float2(noise(noisePos), noise(noisePos + float2(102, 278))) * 2 - 1;
   // // noise2 = normalize(noise2);
   // float A = length(noise2);
   // color = float3(noise2, 0);

   PsOut output = (PsOut)0;
   output.color.rgb = color;
   output.color.a = 1;

   #if defined(EDITOR)
      uint2 pixelIdx = input.posH.xy;
      SetEntityUnderCursor(pixelIdx, gTerrain.entityID);
   #endif

   return output;
}
