#include "shared/hlslCppShared.hlsli"

#define GLUE(a,b) a##b
#define DECLARE_REGISTER(prefix, regNum, spaceNum) register(GLUE(prefix,regNum))

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