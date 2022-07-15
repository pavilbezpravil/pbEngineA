#include "shared/common.hlsli"

struct vs_in {
    float3 position_local : POS;
};

struct vs_out {
    float4 position_clip : SV_POSITION;
};

cbuffer gCamera {
  CameraCB camera;
}

vs_out vs_main(vs_in input) {
  vs_out output = (vs_out)0;

  float4 posL4 = float4(input.position_local, 1);

  float3 posW = mul(posL4, camera.transform).xyz;
  float4 posH = mul(float4(posW, 1), camera.viewProjection);
  output.position_clip = posH;
  // output.position_clip = float4(input.position_local, 1.0);
  return output;
}

float4 ps_main(vs_out input) : SV_TARGET {
  return float4( camera.color, 1.0 );
  // return float4( 1.0, 0.0, 1.0, 1.0 );
}