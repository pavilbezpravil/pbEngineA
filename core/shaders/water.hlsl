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

   float size = 10;
   float height = 2;

   float3 corners[] = {
      float3(-size, height, size),
      float3(size, height, size),
      float3(size, height, -size),
      float3(-size, height, -size),
   };

   // float size = 0.7;
   // float height = 0.5;

   // float3 corners[] = {
   //    float3(-size, size, height),
   //    float3(size, size, height),
   //    float3(size, -size, height),
   //    float3(-size, -size, height),
   // };

   float3 posW = 0;

   if (       vertexID == 0) {
      posW = corners[0];
   } else if (vertexID == 1) {
      posW = corners[1];
   } else if (vertexID == 2) {
      posW = corners[2];
   } else if (vertexID == 3) {
      posW = corners[0];
   } else if (vertexID == 4) {
      posW = corners[2];
   } else if (vertexID == 5) {
      posW = corners[3];
   }

   // float4 posH = mul(float4(posW, 1), gCamera.viewProjection);
   float4 posH = float4(posW, 1);

   output.normalW = float3(0, 1, 0);
   output.posW = posW;
   output.posH = posH;
   output.instanceID = instanceID;

   return output;
}

struct ConstantOutputType {
   float edges[3] : SV_TessFactor;
   float inside : SV_InsideTessFactor;
};

struct HullOutputType {
   float3 position : POSITION;
};

////////////////////////////////////////////////////////////////////////////////
// Patch Constant Function
////////////////////////////////////////////////////////////////////////////////
ConstantOutputType WaterPatchConstantFunction(InputPatch<VsOut, 3> inputPatch, uint patchId : SV_PrimitiveID) {    
   ConstantOutputType output;

   float tessellationAmount = gScene.tessFactorEdge;

   output.edges[0] = tessellationAmount;
   output.edges[1] = tessellationAmount;
   output.edges[2] = tessellationAmount;

   output.inside = gScene.tessFactorInside;

   return output;
}

////////////////////////////////////////////////////////////////////////////////
// Hull Shader
////////////////////////////////////////////////////////////////////////////////
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("WaterPatchConstantFunction")]
HullOutputType waterHS(InputPatch<VsOut, 3> patch, uint pointId : SV_OutputControlPointID, uint patchId : SV_PrimitiveID) {
   HullOutputType output;

   output.position = patch[pointId].posW;

   return output;
}

struct PixelInputType {
    float3 posW : POS_W;
    float4 posH : SV_POSITION;
};

////////////////////////////////////////////////////////////////////////////////
// Domain Shader
////////////////////////////////////////////////////////////////////////////////
[domain("tri")]
PixelInputType waterDS(ConstantOutputType input, float3 uvwCoord : SV_DomainLocation, const OutputPatch<HullOutputType, 3> patch) {
   PixelInputType output;
 
   float3 posW = uvwCoord.x * patch[0].position + uvwCoord.y * patch[1].position + uvwCoord.z * patch[2].position;
   float4 posH = mul(float4(posW, 1), gCamera.viewProjection);

   output.posW = posW;
   output.posH = posH;

   return output;
}

struct PsOut {
   float4 color : SV_Target0;
};

PsOut waterPS(PixelInputType input) : SV_TARGET {
   // float2 screenUV = input.posH.xy / gCamera.rtSize;

   // float3 normalW = normalize(input.normalW);
   float3 normalW = float3(0, 1, 0);
   float3 posW = input.posW;

   float3 V = normalize(gCamera.position - posW);

   float3 color = 1;

   PsOut output = (PsOut)0;
   output.color.rgb = color;
   output.color.a = 1;

   return output;
}
