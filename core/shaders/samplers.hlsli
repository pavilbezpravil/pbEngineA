#ifndef SAMPLERS_HEADER
#define SAMPLERS_HEADER

#include "shared/hlslCppShared.hlsli"

// todo:
SamplerState gSamplerPoint : register(s0);
SamplerState gSamplerLinear : register(s1);
SamplerComparisonState gSamplerShadow : register(s2);

#endif
