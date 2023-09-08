#ifndef SAMPLERS_HEADER
#define SAMPLERS_HEADER

#include "shared/hlslCppShared.hlsli"

// todo:
SamplerState gSamplerPoint : register(s0);
SamplerState gSamplerLinear : register(s1);
SamplerState gSamplerPointClamp : register(s2);
SamplerComparisonState gSamplerShadow : register(s3);

#endif
