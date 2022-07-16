struct CameraCB {
  float4x4 viewProjection;
  float4x4 transform;
  float3 position;
  int nLights;
  float3 color;
  int instanceStart;
  float roughness;
  float metallic;
  float2 _dymmy3;
};

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

struct Light {
  float3 position;
  int type;
  float3 direction;
  float _dymmy;
  float3 color;
  float _dymmy2;
};
