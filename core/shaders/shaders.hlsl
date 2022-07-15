#include "shared/common.hlsli"

struct VsIn {
  float3 posL : POSITION;
  float3 normalL : NORMAL;
};

struct VsOut {
  float3 posW : POS_W;
  float3 normalW : NORMAL_W;
  float4 posH : SV_POSITION;
};

cbuffer gCamera {
  CameraCB camera;
}

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  float3 posW = mul(float4(input.posL, 1), camera.transform).xyz;
  float4 posH = mul(float4(posW, 1), camera.viewProjection);

  output.posW = posW;
  output.posH = posH;

  return output;
}

float4 ps_main(VsOut input) : SV_TARGET {
  float3 a = ddx(input.posW);
  float3 b = ddy(input.posW);
  float3 normalW = normalize(cross(a, b));

  Material material;
  material.albedo = camera.color; 

  Light light = (Light)0;
  light.position = float3(0, 0, -3);

  float3 color = 0;

  float3 L = normalize(light.position - input.posW);

  float NDotL = dot(L, normalW);
  color += material.albedo * max(NDotL, 0);

  // return float4(normalW * 0.5 + 0.5, 1);
  return float4(color, 1);
}