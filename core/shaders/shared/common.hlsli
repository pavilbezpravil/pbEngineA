struct Material {
  float3 albedo;
  float roughness;

  float metallic;
  float3 _dymmy;
};

struct Instance {
  float4x4 transform;
  Material material;
};

struct Decal {
  float4x4 viewProjection;
};

struct Light {
  float3 position;
  int type;
  float3 direction;
  float _dymmy;
  float3 color;
  float _dymmy2;
};

struct SSceneCB {
  int nLights;
  float3 _sdfasdf;

  Light directLight;
};

struct SCameraCB {
  float4x4 projection;
  float4x4 view;
  float4x4 viewProjection;
  float4x4 invViewProjection;

  float3 position;
  int _dsfdsfdsfdsf;

  int2 rtSize;
  float2 _dymmy3;
};

struct SDrawCallCB {
  float4x4 transform;
  Material material;

  int instanceStart;
  float3 _dymmy3;
};
