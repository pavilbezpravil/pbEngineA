#include "commonResources.hlsli"
#include "common.hlsli"
#include "lighting.hlsli"
#include "sky.hlsli"

#include "NRDEncoding.hlsli"
#include "NRD.hlsli"

StructuredBuffer<SDecal> gDecals : register(t1);

Texture2D<float4> gBaseColor;
Texture2D<float4> gNormal;
Texture2D<float> gViewZ;
RWTexture2D<float4> gColorOut;

[numthreads(8, 8, 1)]
void DeferredCS(uint2 id : SV_DispatchThreadID) {
   if (any(id >= gCamera.rtSize)) {
      return;
   }

   float viewZ = gViewZ[id];

   float2 uv = DispatchUV(id, gCamera.rtSize);
   float3 viewPos = ReconstructViewPosition( uv, GetCamera().frustumSize, viewZ );
   float3 posW = ReconstructWorldPosition(viewPos);

   float3 V = normalize(gCamera.position - posW);
   if (ViewZDiscard(viewZ)) {
      float3 skyColor = GetSkyColor(-V);
      gColorOut[id] = float4(skyColor, 1);
      return;
   }

   float4 normalRoughness = NRD_FrontEnd_UnpackNormalAndRoughness(gNormal[id]);
   float3 normal = normalRoughness.xyz;
   float  roughness = normalRoughness.w;

   float4 baseColorMetallic = gBaseColor[id];
   float3 baseColor = baseColorMetallic.xyz;
   float  metallic = baseColorMetallic.w;

   Surface surface;
   surface.metalness = metallic;
   surface.roughness = roughness;

   surface.posW = posW;
   surface.normalW = normal;

#if 0
   for (int iDecal = 0; iDecal < gScene.nDecals; ++iDecal) {
      SDecal decal = gDecals[iDecal];

      float3 posDecalSpace = mul(decal.viewProjection, float4(posW, 1)).xyz;
      if (any(posDecalSpace > float3(1, 1, 1) || posDecalSpace < float3(-1, -1, 0))) {
         continue;
      }

      float alpha = decal.baseColor.a;

      // float3 decalForward = float3(0, -1, 0);
      // alpha *= lerp(-3, 1, dot(decalForward, -surface.normalW));
      // if (alpha < 0) {
      //   continue;
      // }

      float2 decalUV = NDCToTex(posDecalSpace.xy);

      // todo: decal.baseColor
      baseColor = lerp(baseColor, decal.baseColor.rgb, alpha);
      surface.metallic = lerp(surface.metallic, decal.metallic, alpha);
      surface.roughness = lerp(surface.roughness, decal.roughness, alpha);
   }
#endif

   surface.baseColor = baseColor;

   float3 Lo = Shade(surface, V);

   float3 albedo, Rf0;
   ConvertBaseColorMetalnessToAlbedoRf0(surface.baseColor, surface.metalness, albedo, Rf0 );

   float3 ambient = 0.1 * albedo * 0; // todo: only in shadow
   float3 emissive = gColorOut[id].xyz;
   float3 color = ambient + Lo + emissive;

   gColorOut[id] = float4(color, 1);
}
