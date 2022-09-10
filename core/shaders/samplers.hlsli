#include "shared/hlslCppShared.hlsli"

// todo:
SamplerState gSamplerPoint : register(s0);
SamplerState gSamplerLinear : register(s1);
SamplerComparisonState gSamplerShadow : register(s2);

cbuffer gSceneCB : register(b10) {
  SSceneCB gScene;
}

cbuffer gCameraCB : register(b11) {
  SCameraCB gCamera;
}
