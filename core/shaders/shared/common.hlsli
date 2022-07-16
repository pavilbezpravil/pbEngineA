struct CameraCB {
  float4x4 viewProjection;
  float4x4 transform;
  float3 position;
  float _dymmy;
  float3 color;
  int instanceStart;
  float roughness;
  float metallic;
  float2 _dymmy3;
};

struct Instance {
  float4x4 transform;
};

struct Material {
  float3 albedo;
  float _dymmy;
};

struct Light {
  float3 position;
  int type;
  float3 direction;
  float _dymmy;
  float3 color;
  float _dymmy2;
};
