#include "pch.h"
#include "RTRenderer.h"

// todo: remove include
#include "NRD.h"
#include <NRDSettings.h>
#include <numeric>

#include "Buffer.h"
#include "CommandList.h"
#include "DbgRend.h"
#include "NRDDenoiser.h"
#include "Renderer.h" // todo:
#include "Shader.h"
#include "Texture2D.h"
#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Common.h"
#include "math/Random.h"

#include "scene/Component.h"
#include "scene/Scene.h"

#include "shared/hlslCppShared.hlsli"
#include "shared/rt.hlsli"


namespace pbe {

   // todo: n rays -> n samples
   CVarSlider<int> cvNRays{ "render/rt/nRays", 1, 1, 32 };
   // todo: depth -> bounce
   CVarSlider<int> cvRayDepth{ "render/rt/rayDepth", 2, 1, 8 };

   CVarValue<bool> cvAccumulate{ "render/rt/accumulate", false };
   CVarValue<bool> cvRTDiffuse{ "render/rt/diffuse", true };
   CVarValue<bool> cvRTSpecular{ "render/rt/specular", true }; // todo
   CVarValue<bool> cvBvhAABBRender{ "render/rt/bvh aabb render", true };
   CVarValue<uint> cvBvhAABBRenderLevel{ "render/rt/bvh aabb show level", UINT_MAX };
   CVarValue<uint> cvBvhSpliMethod{ "render/rt/bvh split method", 2 };
   CVarValue<bool> cvUsePSR{ "render/rt/use psr", false }; // todo: on my laptop it takes 50% more time

   CVarValue<bool> cvDenoise{ "render/denoise/enable", true };
   CVarValue<bool> cvNRDValidation{ "render/denoise/nrd validation", false };
   CVarTrigger cvClearHistory{ "render/denoise/clear history" };

   CVarSlider<float>  cvNRDSplitScreen{ "render/denoise/split screen", 0, 0, 1 };
   CVarValue<bool> cvNRDPerfMode{ "render/denoise/perf mode", true };
   CVarValue<bool> cvDenoiseDiffSpec{ "render/denoise/diffuse&specular", true };
   CVarValue<bool> cvDenoiseShadow{ "render/denoise/shadow", true };

   CVarValue<bool> cvFog{ "render/rt/fog enable", false };

   RTRenderer::~RTRenderer() {
      // todo: not here
      NRDTerm();
   }

   struct BVH {
      enum class SplitMethod {
         Middle,
         EqualCounts,
         SAH,
      };

      struct BuildNode {
         AABB aabb;
         uint children[2] = { UINT_MAX, UINT_MAX };
         uint objIdx = UINT_MAX; // todo: aabb idx
      };

      struct AABBInfo {
         AABB aabb;
         uint idx;
      };

      std::vector<BuildNode> nodes;
      std::vector<BVHNode> linearNodes;

      void Build(const std::span<AABB>& aabbs) {
         nodes.clear();

         uint size = (uint)aabbs.size();
         if (size == 0) {
            return;
         }

         std::vector<AABBInfo> aabbInfos(size);
         for (uint i = 0; i < size; ++i) {
            aabbInfos[i] = AABBInfo{ aabbs[i], i};
         }

         nodes.reserve(size * 3); // todo: not best size
         BuildRecursive(std::span{ aabbInfos.begin(), aabbInfos.size() }, (SplitMethod)(uint)cvBvhSpliMethod);
      }

      void Flatten() {
         linearNodes.resize(nodes.size());
         if (nodes.empty()) {
            return;
         }

         uint offset = 0;
         RecursiveFlatten(0, offset);
      }

      uint RecursiveFlatten(uint nodeIdx, uint& offset) {
         auto& node = nodes[nodeIdx];

         auto& linearNode = linearNodes[offset];
         uint currentOffset = offset++;
         linearNode.aabbMin = node.aabb.min;
         linearNode.aabbMax = node.aabb.max;
         linearNode.objIdx = node.objIdx;

         if (node.objIdx == UINT_MAX) {
            RecursiveFlatten(node.children[0], offset);
            linearNode.secondChildOffset = offset;
            RecursiveFlatten(node.children[1], offset);
         }

         return currentOffset;
      }

      void Render(DbgRend& dbgRend, uint showLevel = -1, uint nodeIdx = 0, uint level = 0) {
         if (nodeIdx == UINT_MAX || nodes.empty()) {
            return;
         }

         auto& node = nodes[nodeIdx];

         if (showLevel == -1 || showLevel == level) {
            dbgRend.DrawAABB(nullptr, node.aabb, Random::Color(level));
         }

         if (node.objIdx == UINT_MAX) {
            Render(dbgRend, showLevel, node.children[0], level + 1);
            Render(dbgRend, showLevel, node.children[1], level + 1);
         }
      }

   private:
      uint BuildRecursive(std::span<AABBInfo> aabbInfos, SplitMethod splitMethod) {
         uint count = (uint)aabbInfos.size();
         if (count == 0) {
            return UINT_MAX;
         }

         uint buildNodeIdx = (uint)nodes.size();
         nodes.push_back({});
         auto& buildNode = nodes.back();

         AABB combinedAABB = aabbInfos[0].aabb;
         for (uint i = 1; i < count; ++i) {
            combinedAABB.AddAABB(aabbInfos[i].aabb);
         }

         buildNode.aabb = combinedAABB;
         if (count == 1) {
            buildNode.objIdx = aabbInfos[0].idx;
            return buildNodeIdx;
         }

         uint mid = count / 2;
         auto axisIdx = VectorUtils::LargestAxisIdx(combinedAABB.Size());

         switch (splitMethod)
         {
         case SplitMethod::Middle: {
            float pivot = combinedAABB.Center()[axisIdx];
            auto it = std::ranges::partition(aabbInfos,
               [&](const AABBInfo& a) { return a.aabb.Center()[axisIdx] < pivot; });
            mid = (uint)it.size();
            if (mid != 0 && mid != aabbInfos.size()) break;
         }
         case SplitMethod::EqualCounts:
            mid = count / 2;
            std::ranges::nth_element(aabbInfos, aabbInfos.begin() + mid,
               [&](const AABBInfo& a, const AABBInfo& b) { return a.aabb.Center()[axisIdx] < b.aabb.Center()[axisIdx]; });
            break;
         case SplitMethod::SAH:
            if (count <= 2) {
               mid = count / 2;
               std::ranges::nth_element(aabbInfos, aabbInfos.begin() + mid,
                  [&](const AABBInfo& a, const AABBInfo& b) { return a.aabb.Center()[axisIdx] < b.aabb.Center()[axisIdx]; });
               break;
            }

            struct BucketInfo {
               int count = 0;
               AABB bounds = AABB::Empty();
            };

            constexpr uint nAxis = 3;
            constexpr uint nBuckets = 4;
            BucketInfo buckets[nBuckets * nAxis];

            float cost[(nBuckets - 1) * nAxis];

            for (uint axisIdx = 0; axisIdx < nAxis; ++axisIdx) {
               for (auto& aabbInfo : aabbInfos) {
                  auto& aabb = aabbInfo.aabb;
                  uint b = (uint)(nBuckets * combinedAABB.Offset(aabb.Center())[axisIdx]);
                  ASSERT(b < nBuckets);

                  auto& bucket = buckets[b + axisIdx * nBuckets];
                  bucket.count++;
                  bucket.bounds.AddAABB(aabb);
               }

               for (uint i = 0; i < nBuckets - 1; ++i) {
                  AABB b0, b1;

                  uint count0 = 0, count1 = 0;
                  for (uint j = 0; j <= i; ++j) {
                     auto& bucket = buckets[j + axisIdx * nBuckets];
                     b0.AddAABB(bucket.bounds);
                     count0 += bucket.count;
                  }

                  for (int j = i + 1; j < nBuckets; ++j) {
                     auto& bucket = buckets[j + axisIdx * nBuckets];
                     b1.AddAABB(bucket.bounds);
                     count1 += bucket.count;
                  }

                  cost[i + axisIdx * (nBuckets - 1)] = 1
                     + (count0 * b0.Area() + count1 * b1.Area())
                     / combinedAABB.Area();
               }
            }

            float minCost = cost[0];
            int minCostSplitBucket = 0;
            for (uint i = 1; i < (nBuckets - 1) * nAxis; ++i) {
               if (cost[i] < minCost) {
                  minCost = cost[i];
                  minCostSplitBucket = i;
               }
            }

            uint iSplitAxis = minCostSplitBucket / (nBuckets - 1);
            uint iSplitBucket = minCostSplitBucket % (nBuckets - 1);

            auto it = std::ranges::partition(
               aabbInfos,
               [&] (const AABBInfo& a) {
                  auto& aabb = a.aabb;
                  uint b = (uint)(nBuckets * combinedAABB.Offset(aabb.Center())[iSplitAxis]);
                  ASSERT(b < nBuckets);

                  return b <= iSplitBucket;
               }
            );
            mid = count - (uint)it.size();
            ASSERT(mid != 0);
            ASSERT(mid != count);

            break;
         };

         ASSERT(mid >= 1);
         buildNode.children[0] = BuildRecursive(std::span<AABBInfo>{aabbInfos.begin(), mid}, splitMethod);
         buildNode.children[1] = BuildRecursive(std::span<AABBInfo>{aabbInfos.begin() + mid, aabbInfos.end()}, splitMethod);

         return buildNodeIdx;
      }
   };

   void RTRenderer::RenderScene(CommandList& cmd, const Scene& scene, const RenderCamera& camera, RenderContext& context) {
      GPU_MARKER("RT Scene");
      PROFILE_GPU("RT Scene");

      PIX_EVENT_SYSTEM(Render, "RT Render Scene");

      std::vector<SRTObject> objs;

      std::vector<AABB> aabbs;

      for (auto [e, trans, material, geom]
         : scene.View<SceneTransformComponent, MaterialComponent, GeometryComponent>().each()) {
         auto rotation = trans.Rotation();
         auto position = trans.Position();
         auto scale = trans.Scale();

         SRTObject obj;
         obj.position = position;
         obj.id = (uint)e;

         obj.rotation = glm::make_vec4(glm::value_ptr(rotation));

         obj.geomType = (int)geom.type;
         obj.halfSize = geom.sizeData / 2.f * scale;

         obj.baseColor = material.baseColor;
         obj.metallic = material.metallic;
         obj.roughness = material.roughness;
         obj.emissivePower = material.emissivePower;

         // todo: sphere may be optimized
         // todo: not fastest way

         auto extends = scale * 0.5f;
         auto aabb = AABB::Empty();

         if (geom.type == GeomType::Box) {
            aabb.AddPoint(rotation * extends);
            aabb.AddPoint(rotation * vec3{ -extends.x, extends.y, extends.z });
            aabb.AddPoint(rotation * vec3{ extends.x, -extends.y, extends.z });
            aabb.AddPoint(rotation * vec3{ extends.x, extends.y, -extends.z });

            vec3 absMax = glm::max(abs(aabb.min), abs(aabb.max));
            aabb.min = -absMax;
            aabb.max = absMax;

            aabb.Translate(position);
         } else {
            auto sphereExtends = vec3{ extends.x };
            aabb = AABB::Extends(trans.Position(), sphereExtends);
         }

         aabbs.emplace_back(aabb);

         objs.emplace_back(obj);
      }

      // todo: dont create every frame
      BVH bvh;
      bvh.Build(aabbs);

      if (cvBvhAABBRender) {
         auto& dbgRender = *scene.dbgRend;
         // for (auto& aabb : aabbs) {
         //    dbgRender.DrawAABB(aabb);
         // }
         bvh.Render(dbgRender, cvBvhAABBRenderLevel);
      }

      bvh.Flatten();

      // todo: make it dynamic
      auto& bvhNodes = bvh.linearNodes;
      uint nBvhNodes = (uint)bvhNodes.size();
      if (!bvhNodesBuffer || bvhNodesBuffer->ElementsCount() < nBvhNodes) {
         auto bufferDesc = Buffer::Desc::Structured("BVHNodes", nBvhNodes, sizeof(BVHNode));
         bvhNodesBuffer = Buffer::Create(bufferDesc);
      }
      cmd.UpdateSubresource(*bvhNodesBuffer, bvhNodes.data(), 0, nBvhNodes * sizeof(BVHNode));

      uint nObj = (uint)objs.size();

      // todo: make it dynamic
      if (!rtObjectsBuffer || rtObjectsBuffer->ElementsCount() < nObj) {
         auto bufferDesc = Buffer::Desc::Structured("RtObjects", nObj, sizeof(SRTObject));
         rtObjectsBuffer = Buffer::Create(bufferDesc);
      }

      cmd.UpdateSubresource(*rtObjectsBuffer, objs.data(), 0, nObj * sizeof(SRTObject));

      uint nImportanceVolumes = 0;
      {
         std::vector<SRTImportanceVolume> importanceVolumes;

         for (auto [e, trans, volume]
            : scene.View<SceneTransformComponent, RTImportanceVolumeComponent>().each()) {
            SRTImportanceVolume v;
            v.position = trans.Position();
            v.radius = volume.radius; // todo: mb radius is scale?

            importanceVolumes.emplace_back(v);
         }

         nImportanceVolumes = (uint)importanceVolumes.size();

         if (!importanceVolumesBuffer || importanceVolumesBuffer->ElementsCount() < nImportanceVolumes) {
            auto bufferDesc = Buffer::Desc::Structured("ImportanceVolumes", nImportanceVolumes, sizeof(SRTImportanceVolume));
            importanceVolumesBuffer = Buffer::Create(bufferDesc);
         }

         cmd.UpdateSubresource(*importanceVolumesBuffer, importanceVolumes.data(), 0, nImportanceVolumes * sizeof(SRTImportanceVolume));
      }

      auto& outTexture = *context.colorHDR;
      auto outTexSize = outTexture.GetDesc().size;

      // cmd.ClearRenderTarget(*context.colorHDR, vec4{ 0, 0, 0, 1 });

      static int accumulatedFrames = 0;

      bool resetHistory = cvClearHistory;

      float historyWeight = 0;

      if (cvAccumulate) {
         historyWeight = (float)accumulatedFrames / (float)(accumulatedFrames + cvNRays);
         accumulatedFrames += cvNRays;

         resetHistory |= camera.GetPrevViewProjection() != camera.GetViewProjection();
      }

      if (resetHistory || !cvAccumulate) {
         // cmd.ClearUAVUint(*context.reprojectCountTexPrev);
         // cmd.ClearUAVFloat(*context.historyTex);

         accumulatedFrames = 0;
      }

      SRTConstants rtCB;
      rtCB.rtSize = outTexSize;
      rtCB.rayDepth = cvRayDepth;
      rtCB.nObjects = nObj;
      rtCB.nRays = cvNRays;
      rtCB.random01 = Random::Float(0.f, 1.f);
      rtCB.historyWeight = historyWeight;
      rtCB.nImportanceVolumes = nImportanceVolumes;

      rtCB.bvhNodes = nBvhNodes;

      nrd::HitDistanceParameters hitDistanceParametrs{};
      rtCB.nrdHitDistParams = { hitDistanceParametrs.A, hitDistanceParametrs.B,
         hitDistanceParametrs.C, hitDistanceParametrs.D };

      auto rtConstantsCB = cmd.AllocDynConstantBuffer(rtCB);

      auto setSharedResource = [&](GpuProgram& pass) {
         cmd.SetCB<SRTConstants>(pass.GetBindPoint("gRTConstantsCB"), rtConstantsCB.buffer, rtConstantsCB.offset);
         cmd.SetSRV(pass.GetBindPoint("gRtObjects"), rtObjectsBuffer);
         cmd.SetSRV(pass.GetBindPoint("gBVHNodes"), bvhNodesBuffer);
      };

      CMD_BINDS_GUARD();

      // todo: generate by rasterizator
      if (cvDenoise && false) {
         GPU_MARKER("GBuffer");
         PROFILE_GPU("GBuffer");

         auto pass = GetGpuProgram(ProgramDesc::Cs("rt.hlsl", "GBufferCS"));
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         // cmd.SetUAV(pass->GetBindPoint("gDepthOut"), context.depthTex);
         cmd.SetUAV(pass->GetBindPoint("gNormalOut"), context.normalTex);

         cmd.SetUAV(pass->GetBindPoint("gUnderCursorBuffer"), context.underCursorBuffer); // todo: only for editor

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }

      if (0) {
         GPU_MARKER("Ray Trace");
         PROFILE_GPU("Ray Trace");

         auto desc = ProgramDesc::Cs("rt.hlsl", "rtCS");
         // if (cvImportanceSampling) {
         //    desc.cs.defines.AddDefine("IMPORTANCE_SAMPLING");
         // }

         auto pass = GetGpuProgram(desc);
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         // cmd.SetUAV(pass->GetBindPoint("gColorOut"), context.rtColorNoisyTex);

         cmd.Dispatch2D(outTexSize, int2{8, 8});
      }

      if (cvRTDiffuse) {
         GPU_MARKER("RT Diffuse&Specular");
         PROFILE_GPU("RT Diffuse&Specular");

         auto desc = ProgramDesc::Cs("rt.hlsl", "RTDiffuseSpecularCS");
         if (cvUsePSR) {
            desc.cs.defines.AddDefine("USE_PSR");
         }

         auto pass = GetGpuProgram(desc);
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         pass->SetSRV(cmd, "gImportanceVolumes", importanceVolumesBuffer);

         if (cvUsePSR) {
            pass->SetUAV(cmd, "gViewZOut", context.viewz);
            pass->SetUAV(cmd, "gNormalOut", context.normalTex);
            pass->SetUAV(cmd, "gColorOut", context.colorHDR);
            pass->SetUAV(cmd, "gBaseColorOut", context.baseColorTex);
         } else {
            pass->SetSRV(cmd, "gViewZ", context.viewz);
            pass->SetSRV(cmd, "gNormal", context.normalTex);
            pass->SetSRV(cmd, "gBaseColor", context.baseColorTex);
         }

         pass->SetUAV(cmd, "gDiffuseOut", context.diffuseTex);
         pass->SetUAV(cmd, "gSpecularOut", context.specularTex);
         pass->SetUAV(cmd, "gDirectLightingOut", context.directLightingUnfilteredTex);

         pass->SetUAV(cmd, "gShadowDataOut", context.shadowDataTex);
         pass->SetUAV(cmd, "gShadowDataTranslucencyOut", context.shadowDataTranslucencyTex);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }

      Texture2D* diffuse = context.diffuseTex;
      Texture2D* specular = context.specularTex;
      Texture2D* shadow = context.shadowDataTranslucencyTex;

      if (cvDenoise) {
         GPU_MARKER("Denoise");
         PROFILE_GPU("Denoise");

         DenoiseCallDesc desc;

         desc.textureSize = outTexSize;
         desc.clearHistory = cvClearHistory;
         desc.diffuseSpecular = cvDenoiseDiffSpec;
         desc.shadow = cvDenoiseShadow;
         desc.perfMode = cvNRDPerfMode;
         desc.splitScreen = cvNRDSplitScreen;

         desc.mViewToClip = camera.projection;
         desc.mViewToClipPrev = camera.prevProjection;
         desc.mWorldToView = camera.view;
         desc.mWorldToViewPrev = camera.prevView;

         desc.IN_MV = context.motionTex;
         desc.IN_NORMAL_ROUGHNESS = context.normalTex;
         desc.IN_VIEWZ = context.viewz;
         desc.IN_DIFF_RADIANCE_HITDIST = context.diffuseTex;
         desc.IN_SPEC_RADIANCE_HITDIST = context.specularTex;
         desc.OUT_DIFF_RADIANCE_HITDIST = context.diffuseHistoryTex;
         desc.OUT_SPEC_RADIANCE_HITDIST = context.specularHistoryTex; // todo: names

         desc.IN_SHADOWDATA = context.shadowDataTex;
         desc.IN_SHADOW_TRANSLUCENCY = context.shadowDataTranslucencyTex;
         desc.OUT_SHADOW_TRANSLUCENCY = context.shadowDataTranslucencyHistoryTex;

         if (cvNRDValidation) {
            desc.validation = true;
            desc.OUT_VALIDATION = context.colorHDR;
         }

         NRDResize(outTexSize);
         NRDDenoise(cmd, desc);

         if (desc.diffuseSpecular) {
            diffuse = context.diffuseHistoryTex;
            specular = context.specularHistoryTex;
         }
         if (desc.shadow) {
            shadow = context.shadowDataTranslucencyHistoryTex;
         }
      }

      if (!cvNRDValidation) {
         GPU_MARKER("RT Combine");
         PROFILE_GPU("RT Combine");

         auto desc = ProgramDesc::Cs("rt.hlsl", "RTCombineCS");

         auto pass = GetGpuProgram(desc);
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         pass->SetSRV(cmd, "gViewZ", context.viewz);
         pass->SetSRV(cmd, "gBaseColor", context.baseColorTex);
         pass->SetSRV(cmd, "gNormal", context.normalTex);
         pass->SetSRV(cmd, "gDiffuse", diffuse);
         pass->SetSRV(cmd, "gSpecular", specular);
         pass->SetSRV(cmd, "gDirectLighting", context.directLightingUnfilteredTex);
         pass->SetSRV(cmd, "gShadowDataTranslucency", shadow);

         pass->SetUAV(cmd, "gColorOut", context.colorHDR);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }

      // todo:
      // if (cvFog)
      {
         GPU_MARKER("RT Fog");
         PROFILE_GPU("RT Fog");

         auto desc = ProgramDesc::Cs("rt.hlsl", "RTFogCS");

         auto pass = GetGpuProgram(desc);
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         pass->SetSRV(cmd, "gViewZ", context.viewz);
         pass->SetUAV(cmd, "gColorOut", context.colorHDR);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }
   }

}
