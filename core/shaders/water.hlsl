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

VsOut vs_main(uint instanceID : SV_InstanceID, uint vertexID : SV_VertexID) {
   VsOut output = (VsOut)0;

   float size = 10;
   float height = 2;

   float3 corners[] = {
      float3(-size, height, size),
      float3(size, height, size),
      float3(size, height, -size),
      float3(-size, height, -size),
   };

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

   float4 posH = mul(float4(posW, 1), gCamera.viewProjection);

   output.normalW = float3(0, 1, 0);
   output.posW = posW;
   output.posH = posH;
   output.instanceID = instanceID;

   return output;
}

struct PsOut {
   float4 color : SV_Target0;
};

PsOut ps_main(VsOut input) : SV_TARGET {
   float2 screenUV = input.posH.xy / gCamera.rtSize;

   float3 normalW = normalize(input.normalW);
   float3 posW = input.posW;

   float3 V = normalize(gCamera.position - posW);

   float3 color = 1;

   PsOut output = (PsOut)0;
   output.color.rgb = color;
   output.color.a = 1;

   return output;
}
