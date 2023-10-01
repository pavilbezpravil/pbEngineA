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
  // float4 posH : SV_Position;
  float3 posW : POSW;
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

  // output.posH = mul(gCamera.viewProjection, float4(input.posW, 1));
  output.posW = input.posW;
  output.color = input.color;
  output.entityID = input.entityID;

  return output;
}

#define LINE_THICKNESS 5.0f

[maxvertexcount(6)]
void MainGS(line VsOut input[2], inout TriangleStream<GsOut> output) {
  float3 p0 = input[0].posW;
  float3 p1 = input[1].posW;

  float viewZ0 = dot(GetCameraForward(), p0 - GetCameraPosition());
  float viewZ1 = dot(GetCameraForward(), p1 - GetCameraPosition());

  if (viewZ0 < 0 && viewZ1 < 0) {
    return;
  }

  float3 segment = p1 - p0;
  float3 segmentViewZ = viewZ1 - viewZ0;

  // move to positive z view space
  float additionZ = 0.01f;
  if (viewZ0 < 0) {
    p0 = p0 + segment * ((-viewZ0 + additionZ) / segmentViewZ);
  } else if (viewZ1 < 0) {
    p1 = p1 - segment * ((-viewZ1 + additionZ) / -segmentViewZ);
  }

  float4 posH0 = mul(gCamera.viewProjection, float4(p0, 1));
  float4 posH1 = mul(gCamera.viewProjection, float4(p1, 1));
  
  // pixel space
  float2 ndc0 = posH0.xy / posH0.w;
  float2 ndc1 = posH1.xy / posH1.w;
  float2 linePos0 = ndc0 * gCamera.rtSize;
  float2 linePos1 = ndc1 * gCamera.rtSize;

  float2 direction = normalize(linePos1 - linePos0);
  float2 normal = float2(-direction.y, direction.x);

  float halfThickness = LINE_THICKNESS * 0.5f;

  float2 offsetH = normal * halfThickness / (gCamera.rtSize * 0.5f);
  float2 offsetDirH = direction * halfThickness / (gCamera.rtSize * 0.5f);

  float4 pos0 = posH0;
  float4 pos1 = posH0;
  float4 pos2 = posH1;
  float4 pos3 = posH1;

  pos0.xy -= offsetDirH * posH0.w;
  pos1.xy -= offsetDirH * posH0.w;

  pos2.xy += offsetDirH * posH1.w;
  pos3.xy += offsetDirH * posH1.w;

  pos0.xy -= offsetH * posH0.w;
  pos1.xy += offsetH * posH0.w;

  pos2.xy -= offsetH * posH1.w;
  pos3.xy += offsetH * posH1.w;

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

  float halfThickness = LINE_THICKNESS / 2.f;
  float dist = SDLine(float3(input.posH.xy, 0), float3(input.linePos0, 0), float3(input.linePos1, 0));
  float alpha = saturate(1 - dist / halfThickness);
  output.color.a = smoothstep(0, 1, alpha);
  // output.color.a = alpha * alpha * alpha * alpha;
  // output.color.a = alpha;
  // output.color.a = 1;

   #if defined(EDITOR)
      SetEntityUnderCursor(pixelIdx, input.entityID);
   #endif

  return output;
}
