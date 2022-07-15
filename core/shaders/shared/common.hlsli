struct CameraCB {
  float4x4 viewProjection;
  float4x4 transform;
  float3 color;
  int instanceStart;
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
};
