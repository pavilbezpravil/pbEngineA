#include "commonResources.hlsli"
#include "common.hlsli"
#include "lighting.hlsli"
#include "pbr.hlsli"
#include "tonemaping.hlsli"

#include "NRDEncoding.hlsli"
#include "NRD.hlsli"

#ifndef ZPASS
   #define COLOR_PASS
#endif

// todo:
#define EDITOR
#ifdef EDITOR
   #include "editor.hlsli"
#endif

struct VsIn {
   float3 posL : POSITION;
   float3 normalL : NORMAL;
   uint instanceID : SV_InstanceID;
};

struct VsOut {
   float3 posW : POS_W;
   float3 motionW : MOTION_W;
   float3 normalW : NORMAL_W;
   float4 posH : SV_POSITION;
   uint instanceID : SV_InstanceID;
};

cbuffer gDrawCallCB {
   SDrawCallCB gDrawCall;
}

StructuredBuffer<SInstance> gInstances : register(t0);
StructuredBuffer<SDecal> gDecals : register(t1);

Texture2D<float> gSsao;

VsOut vs_main(VsIn input) {
   VsOut output = (VsOut)0;

   // todo:
   #ifdef OUTLINES
      SInstance instance = gDrawCall.instance;
   #else
      SInstance instance = gInstances[gDrawCall.instanceStart + input.instanceID];
   #endif

   float3 prevPosW = mul(instance.prevTransform, float4(input.posL, 1)).xyz;
   float3 posW = mul(instance.transform, float4(input.posL, 1)).xyz;
   float4 posH = mul(gCamera.viewProjection, float4(posW, 1));

   output.posW = posW;
   output.motionW = prevPosW - posW;
   output.posH = posH;
   output.instanceID = input.instanceID;

   return output;
}

#ifndef GBUFFER

struct PsOut {
   float4 color : SV_Target0;
};

// ps uses UAV writes, it removes early z test
[earlydepthstencil]
PsOut ps_main(VsOut input) : SV_TARGET {
#ifdef OUTLINES
   SMaterial material = gDrawCall.instance.material;

   PsOut output = (PsOut)0;
   output.color = float4(material.baseColor, 1);
   // output.color = float4(1, 1, 1, 1);
   return output;
#else
   uint2 pixelIdx = input.posH.xy;
   float2 screenUV = pixelIdx / gCamera.rtSize;

   float3 normalW = normalize(cross(ddx(input.posW), ddy(input.posW)));
   float3 posW = input.posW;

   float alpha = 0.75;

   SInstance instance = gInstances[gDrawCall.instanceStart + input.instanceID];
   SMaterial material = instance.material;

   Surface surface;
   surface.metalness = material.metallic;
   surface.roughness = material.roughness;

   surface.posW = posW;
   surface.normalW = normalW;

   float3 baseColor = material.baseColor;

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
      surface.metalness = lerp(surface.metalness, decal.metallic, alpha);
      surface.roughness = lerp(surface.roughness, decal.roughness, alpha);
   }

   surface.baseColor = baseColor;

   // lighting

   float3 V = normalize(gCamera.position - posW);
   float3 Lo = Shade(surface, V);

   float ssaoMask = gSsao.SampleLevel(gSamplerLinear, screenUV, 0).x;

   float3 albedo, Rf0;
   ConvertBaseColorMetalnessToAlbedoRf0(surface.baseColor, surface.metalness, albedo, Rf0 );

   // float ao = ssaoMask; // todo:
   float ao = 0; // todo:
   float3 ambient = 0.02 * albedo * ao;
   float3 color = ambient + Lo;

   color *= ssaoMask; // todo: applied on transparent too

   // color = noise(posW * 0.3);

   PsOut output = (PsOut)0;
   output.color.rgb = color;

   // float2 jitter = rand3dTo2d(posW);
   // output.color.rgb = SunShadowAttenuation(posW, jitter * 0.005);
   output.color.a = alpha;

   #ifdef ZPASS
       output.color.rgb = float4(normalW * 0.5f + 0.5f, 1);
       output.color.a = 1; // todo:
   #endif

   #if defined(COLOR_PASS) && defined(EDITOR)
      SetEntityUnderCursor(pixelIdx, instance.entityID);
   #endif

   return output;
#endif
}

#else

struct PsOut {
   float4 emissive  : SV_Target0;
   float4 baseColor : SV_Target1;
   float4 normal    : SV_Target2;
   float3 motion    : SV_Target3;
   float  viewz     : SV_Target4;
};

// ps uses UAV writes, it removes early z test
[earlydepthstencil]
PsOut ps_main(VsOut input) : SV_TARGET {
   uint2 pixelIdx = input.posH.xy;
   float2 screenUV = float2(pixelIdx) / gCamera.rtSize;

   float3 normalW = normalize(cross(ddx(input.posW), ddy(input.posW)));
   // float3 posW = input.posW;

   SInstance instance = gInstances[gDrawCall.instanceStart + input.instanceID];
   SMaterial material = instance.material;

   PsOut output;
   output.emissive = float4(material.baseColor * material.emissivePower, 1); // todo: 1?
   output.baseColor = float4(material.baseColor, material.metallic);
   output.normal = NRD_FrontEnd_PackNormalAndRoughness(normalW, material.roughness);
   output.motion = input.motionW;

   // todo: do it in vs
   output.viewz = mul(gCamera.view, float4(input.posW, 1)).z;

   #if defined(EDITOR)
      SetEntityUnderCursor(pixelIdx, instance.entityID);
   #endif

   return output;
}

#endif
