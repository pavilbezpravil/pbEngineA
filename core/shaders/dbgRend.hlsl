#include "commonResources.hlsli"

// todo:
#define EDITOR
#ifdef EDITOR
   #include "editor.hlsli"
#endif

struct VsIn {
  float3 posW : POSITION;
  uint entityID : UINT;
  float4 color : COLOR;
};

struct VsOut {
  float4 posH : SV_POSITION;
  float4 color : COLOR;
  uint entityID : ENTITY_ID;  // todo: without interpolation
};

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  output.posH = mul(gCamera.viewProjection, float4(input.posW, 1));
  output.color = input.color;
  output.entityID = input.entityID;

  return output;
}

struct PsOut {
  float4 color : SV_Target0;
};

PsOut ps_main(VsOut input) : SV_TARGET {
  uint2 pixelIdx = input.posH.xy;

  PsOut output = (PsOut)0;
  output.color = input.color;

   #if defined(EDITOR)
      SetEntityUnderCursor(pixelIdx, input.entityID);
   #endif

  return output;
}
