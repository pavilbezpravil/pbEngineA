#include "pch.h"
#include "RTRenderer.h"

// todo: remove include
#include "NRD.h"
#include <NRDSettings.h>

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
   CVarValue<bool> cvBvhAABBRender{ "render/rt/bvh aabb render", false };
   CVarValue<bool> cvUsePSR{ "render/rt/use psr", false }; // todo: on my laptop it takes 50% more time

   CVarValue<bool> cvDenoise{ "render/denoise/enable", false };
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
      struct Node {
         vec3 aabbMin;
         uint objIdx = UINT_MAX;

         vec3 aabbMax;
         uint pad;
      };

      std::vector<Node> nodes;

      std::vector<Node> buildedNodes;

      uint Nodes() const {
         return (uint)buildedNodes.size();
      }

      void Build(const std::vector<AABB>& aabbs) {
         nodes.clear();
         buildedNodes.clear();

         uint size = (uint)aabbs.size();
         if (size == 0) {
            return;
         }

         // todo: do it thought generator
         nodes.resize(size);

         for (uint i = 0; i < size; ++i) {
            nodes[i] = Node {
               .aabbMin = aabbs[i].min,
               .objIdx = (uint)i,
               .aabbMax = aabbs[i].max,
            };
         }

         // todo: not best size
         buildedNodes.reserve(size* 2);
         BuildRecursive(0, 0, size);
      }

      void Render(DbgRend& dbgRend, uint idx = 0, uint level = 0) {
         if (idx >= buildedNodes.size()) {
            return;
         }

         auto& node = buildedNodes[idx];

         dbgRend.DrawAABB(nullptr, AABB::MinMax(node.aabbMin, node.aabbMax), Random::Color(level));

         if (node.objIdx == UINT_MAX) {
            Render(dbgRend, LeftIdx(idx), level + 1);
            Render(dbgRend, RightIdx(idx), level + 1);
         }
      }

   private:
      void BuildRecursive(int buildNodeIdx, int start, int end) {
         int count = end - start;
         if (count == 0) {
            return;
         }

         AABB combinedAABB = AABB::MinMax(nodes[start].aabbMin, nodes[start].aabbMax);
         for (int i = 1; i < count; ++i) {
            auto& node = nodes[start + i];
            combinedAABB.AddAABB(AABB::MinMax(node.aabbMin, node.aabbMax));
         }

         int mid = start + (count + 1) / 2;

         if (count > 2) {
            auto axisIdx = VectorUtils::LargestAxisIdx(combinedAABB.Size());
            std::sort(nodes.begin() + start, nodes.begin() + end,
               [axisIdx](const Node& a, const Node& b) { return a.aabbMin[axisIdx] < b.aabbMin[axisIdx]; });
         } else {
            mid = start + 1;
         }

         int reqSize = buildNodeIdx + 1;
         if (reqSize > buildedNodes.size()) {
            buildedNodes.resize(reqSize);
         }

         buildedNodes[buildNodeIdx] = Node {
            .aabbMin = combinedAABB.min,
            .objIdx = count == 1 ? nodes[start].objIdx : UINT_MAX,
            .aabbMax = combinedAABB.max,
         };

         if (count > 1) {
            BuildRecursive(LeftIdx(buildNodeIdx), start, mid);
            BuildRecursive(RightIdx(buildNodeIdx), mid, end);
         }
      }

      int ParentIdx(uint idx) const {
         return idx == 0 ? 0 : (idx - 1) / 2;
      }

      int LeftIdx(uint idx) const {
         return idx * 2 + 1;
      }

      int RightIdx(uint idx) const {
         return idx * 2 + 2;
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
         bvh.Render(dbgRender);
      }

      // todo: make it dynamic
      uint bvhNodes = bvh.Nodes();
      if (!bvhNodesBuffer || bvhNodesBuffer->ElementsCount() < bvhNodes) {
         auto bufferDesc = Buffer::Desc::Structured("BVHNodes", bvhNodes, sizeof(BVHNode));
         bvhNodesBuffer = Buffer::Create(bufferDesc);
      }
      cmd.UpdateSubresource(*bvhNodesBuffer, bvh.buildedNodes.data(), 0, bvhNodes * sizeof(BVHNode));

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

      rtCB.bvhNodes = bvhNodes;

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
