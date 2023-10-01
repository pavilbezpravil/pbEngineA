#include "commonResources.hlsli"
#include "common.hlsli"

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

struct GsOut {
  float4 posH : SV_POSITION;
  float2 linePos0 : LINE_POS0;
  float2 linePos1 : LINE_POS1;
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
void MainGS(line VsOut input[2], inout TriangleStream<GsOut> output) {
  // pixel space
  float2 ndc0 = input[0].posH.xy / input[0].posH.w;
  float2 ndc1 = input[1].posH.xy / input[1].posH.w;
  float2 linePos0 = ndc0 * gCamera.rtSize;
  float2 linePos1 = ndc1 * gCamera.rtSize;

  float2 direction = normalize(linePos1 - linePos0);
  float2 normal = float2(-direction.y, direction.x);

  float thickness = 30.0f;
  float halfThickness = thickness * 0.5f;

  float2 offsetH = normal * halfThickness / (gCamera.rtSize * 0.5f);
  float2 offsetDirH = direction * halfThickness / (gCamera.rtSize * 0.5f);

  float4 pos0 = input[0].posH;
  float4 pos1 = input[0].posH;
  float4 pos2 = input[1].posH;
  float4 pos3 = input[1].posH;

  pos0.xy -= offsetDirH * input[0].posH.w;
  pos1.xy -= offsetDirH * input[0].posH.w;

  pos2.xy += offsetDirH * input[1].posH.w;
  pos3.xy += offsetDirH * input[1].posH.w;

  pos0.xy -= offsetH * input[0].posH.w;
  pos1.xy += offsetH * input[0].posH.w;

  pos2.xy -= offsetH * input[1].posH.w;
  pos3.xy += offsetH * input[1].posH.w;

  GsOut vertex;
  vertex.entityID = input[0].entityID;

  vertex.linePos0 = NDCToTex(ndc0) * gCamera.rtSize;
  vertex.linePos1 = NDCToTex(ndc1) * gCamera.rtSize;

  // first triangle
  vertex.color = input[0].color;

  vertex.posH = pos0;
  output.Append(vertex);

  vertex.posH = pos1;
  output.Append(vertex);

  vertex.posH = pos3;
  output.Append(vertex);

  // second triangle
  vertex.color = input[1].color;

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

float SDCapsule( float3 p, float3 a, float3 b, float r ) {
  float3 pa = p - a, ba = b - a;
  float h = clamp( dot(pa, ba) / dot(ba, ba), 0.0, 1.0 );
  return length( pa - ba * h ) - r;
}

float SDLine( float3 p, float3 a, float3 b) {
  return SDCapsule(p, a, b, 0);
}

[earlydepthstencil]
PsOut ps_main(GsOut input) : SV_TARGET {
  uint2 pixelIdx = input.posH.xy;

  PsOut output = (PsOut)0;
  output.color = input.color;

  float halfThickness = 15.0f;
  float dist = SDLine(float3(input.posH.xy, 0), float3(input.linePos0, 0), float3(input.linePos1, 0));
  float alpha = saturate(1 - dist / halfThickness);
  // output.color.a = smoothstep(0, 1, alpha);
  output.color.a = alpha * alpha * alpha * alpha;
  // output.color.a = alpha;
  output.color.a = 1;

   #if defined(EDITOR)
      SetEntityUnderCursor(pixelIdx, input.entityID);
   #endif

  return output;
}
