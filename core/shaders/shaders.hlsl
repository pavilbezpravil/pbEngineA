#include "shared/common.hlsli"

struct VsIn {
    float3 posL : POSITION;
    float3 normalL : NORMAL;
};

struct VsOut {
    float4 posH : SV_POSITION;
};

cbuffer gCamera {
  CameraCB camera;
}

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  float3 posW = mul(float4(input.posL, 1), camera.transform).xyz;
  float4 posH = mul(float4(posW, 1), camera.viewProjection);
  output.posH = posH;

  return output;
}

float4 ps_main(VsOut input) : SV_TARGET {
  return float4( camera.color, 1.0 );
}