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

struct CameraCB {
  float4x4 viewProjection;
  float4x4 projection;
  float4x4 view;
  
  float4x4 transform;

  float3 position;
  int nLights;

  int instanceStart;
  float3 _dymmy3;
  
  Material material;
  Light directLight;
};
