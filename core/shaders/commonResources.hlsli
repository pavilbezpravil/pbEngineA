#ifndef COMMON_RESOURCES_HEADER
#define COMMON_RESOURCES_HEADER

#include "shared/hlslCppShared.hlsli"
#include "samplers.hlsli"

cbuffer gSceneCB : DECLARE_REGISTER(b, CB_SLOT_SCENE) {
  SSceneCB gScene;
}

cbuffer gCameraCB : DECLARE_REGISTER(b, CB_SLOT_CAMERA) {
  SCameraCB gCamera;
}

#endif
