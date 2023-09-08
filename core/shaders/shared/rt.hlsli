#ifndef RT_HEADER
#define RT_HEADER

struct SRTObject {
   float3 position;
   uint   id;

   float4 rotation; // quat

   float3 baseColor;
   float  metallic;

   float emissivePower;
   float roughness;
   float2 _pad;

   float3 halfSize;
   int    geomType;
};

struct BVHNode {
   float3 aabbMin;
   uint   objIdx;
   float3 aabbMax;
   uint   pad;
};

struct SRTConstants {
   uint2 rtSize;
   int rayDepth;
   int nObjects;

   int nRays;
   float random01;
   float historyWeight;
   uint importanceSampleObjIdx; // todo:

   uint _sdff3;
   uint bvhNodes;
   float2 _dymmy23;

   float4 nrdHitDistParams;
};

#endif
