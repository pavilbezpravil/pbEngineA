#include "shared/common.hlsli"

struct VsIn {
  float3 posW : POSITION;
  float4 color : COLOR;
};

struct VsOut {
  float4 color : COLOR;
  float4 posH : SV_POSITION;
};

cbuffer gCameraCB {
  SCameraCB gCamera;
}

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  output.posH = mul(float4(input.posW, 1), gCamera.viewProjection);
  output.color = input.color;

  return output;
}

struct PsOut {
  float4 color : SV_Target0;
};

PsOut ps_main(VsOut input) : SV_TARGET {
  PsOut output = (PsOut)0;
  output.color = input.color;
  return output;
}