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

struct SDecal {
  float4x4 viewProjection;
  float4 albedo;

  float metallic;
  float roughness;
  float2 _sdfdsf;
};

#define SLIGHT_TYPE_DIRECT (1)
#define SLIGHT_TYPE_POINT (2)

struct SLight {
  float3 position;
  int type;
  float3 direction;
  float _dymmy;
  float3 color;
  float radius;

  // todo: dont work
  float3 DirectionToLight(float3 posW) { // L
    if (type == SLIGHT_TYPE_DIRECT) {
      return -direction;
    } else if (type == SLIGHT_TYPE_POINT) {
      float3 L = normalize(position - posW);
      return L;
    }
    return float3(0, 0, 0);
  }
};

struct SSceneCB {
  int nLights;
  int nDecals;
  float2 _sdfasdf;

  SLight directLight;
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
