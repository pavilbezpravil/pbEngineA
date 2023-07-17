#define DBG_FLAG_SHOW_NEW_PIXEL 1
#define DBG_FLAG_REPR_OBJ_ID 2
#define DBG_FLAG_REPR_NORMAL 4
#define DBG_FLAG_REPR_DEPTH 8

struct SRTObject {
   float3 position;
   uint   id;

   float4 rotation;

   float3 baseColor;
   float  metallic;

   float3 emissiveColor;
   float  roughness;

   float3 halfSize;
   int    geomType;
};

struct SRTConstants {
   uint2 rtSize;
   int rayDepth;
   int nObjects;

   int nRays;
   float random01;
   float historyWeight;
   uint importanceSampleObjIdx; // todo:

   uint debugFlags;
   float3 _dymmy23;
};
