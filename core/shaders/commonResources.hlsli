#ifndef COMMON_RESOURCES_HEADER
#define COMMON_RESOURCES_HEADER

#include "shared/hlslCppShared.hlsli"
#include "samplers.hlsli"

cbuffer gSceneCB : register(b10) {
  SSceneCB gScene;
}

cbuffer gCameraCB : register(b11) {
  SCameraCB gCamera;
}

#endif
