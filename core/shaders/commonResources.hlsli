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

// from NRD source
// orthoMode = { 0 - perspective, -1 - right handed ortho, 1 - left handed ortho }
float3 ReconstructViewPosition( float2 uv, float2 cameraFrustumSize, float viewZ = 1.0, float orthoMode = 0.0 ) {
    float3 p;
    p.xy = uv * cameraFrustumSize * float2(2, -2) + cameraFrustumSize * float2(-1, 1);
    p.xy *= viewZ * ( 1.0 - abs( orthoMode ) ) + orthoMode;
    p.z = viewZ;

    return p;
}

float3 ReconstructWorldPosition(float3 viewPos) {
    return mul(GetCamera().invView, float4(viewPos, 1)).xyz;
}

bool ViewZDiscard(float viewZ) {
    return viewZ > 1e4; // todo: constant
}

#endif
