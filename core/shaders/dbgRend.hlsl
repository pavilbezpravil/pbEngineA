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
  nointerpolation uint entityID : ENTITY_ID;  // todo: without interpolation
};

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  output.posH = mul(gCamera.viewProjection, float4(input.posW, 1));
  output.color = input.color;
  output.entityID = input.entityID;

  return output;
}

[maxvertexcount(6)]
void MainGS(line VsOut input[2], inout TriangleStream<VsOut> output) {
  float2 direction = normalize((input[1].posH.xy / input[1].posH.w - input[0].posH.xy / input[0].posH.w) * gCamera.rtSize);
  float2 normal = float2(-direction.y, direction.x);

  float thickness = 30.0f;
  float halfThickness = thickness * 0.5f;

  float2 offsetH = normal * halfThickness / (gCamera.rtSize * 0.5f);

  float4 pos0 = input[0].posH;
  float4 pos1 = input[0].posH;
  float4 pos2 = input[1].posH;
  float4 pos3 = input[1].posH;

  pos0.xy -= offsetH * input[0].posH.w;
  pos1.xy += offsetH * input[0].posH.w;

  pos2.xy -= offsetH * input[1].posH.w;
  pos3.xy += offsetH * input[1].posH.w;

  VsOut vertex;
  vertex.color = input[0].color;
  vertex.entityID = input[0].entityID;

  // first triangle
  vertex.posH = pos0;
  output.Append(vertex);

  vertex.posH = pos1;
  output.Append(vertex);

  vertex.posH = pos3;
  output.Append(vertex);

  // second triangle
  vertex.posH = pos0;
  output.Append(vertex);

  vertex.posH = pos2;
  output.Append(vertex);

  vertex.posH = pos3;
  output.Append(vertex);
}

struct PsOut {
  float4 color : SV_Target0;
};

[earlydepthstencil]
PsOut ps_main(VsOut input) : SV_TARGET {
  uint2 pixelIdx = input.posH.xy;

  PsOut output = (PsOut)0;
  output.color = input.color;
  // output.color.a = 0.5f;

   #if defined(EDITOR)
      SetEntityUnderCursor(pixelIdx, input.entityID);
   #endif

  return output;
}
