struct SRTObject {
   float3 position;
   uint   id;

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
   float _wef32;
};
