#include "commonResources.hlsli"

struct VsIn {
  float3 posW : POSITION;
  float4 color : COLOR;
};

struct VsOut {
  float4 color : COLOR;
  float4 posH : SV_POSITION;
};

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  output.posH = mul(gCamera.viewProjection, float4(input.posW, 1));
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
