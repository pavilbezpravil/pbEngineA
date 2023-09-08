#ifndef COMMON_RESOURCES_HEADER
#define COMMON_RESOURCES_HEADER

#include "samplers.hlsli"

cbuffer gSceneCB : DECLARE_REGISTER(b, CB_SLOT_SCENE) {
  SSceneCB gScene;
}

cbuffer gCameraCB : DECLARE_REGISTER(b, CB_SLOT_CAMERA) {
  SCameraCB gCamera;
}

cbuffer gCullCameraCB : DECLARE_REGISTER(b, CB_SLOT_CULL_CAMERA) {
  SCameraCB gCullCamera;
}

SCameraCB GetCullCamera() {
  // return gCamera;
  return gCullCamera;
}

SCameraCB GetCamera() {
  return gCamera;
}

float3 GetCameraPosition() {
  return GetCamera().position;
}

float3 GetCameraForward() {
  return GetCamera().forward;
}

#endif
