#include "pch.h"
#include "RTRenderer.h"
#include "Buffer.h"
#include "CommandList.h"
#include "Renderer.h" // todo:
#include "Shader.h"
#include "Texture2D.h"
#include "core/CVar.h"
#include "core/Profiler.h"
#include "math/Random.h"

#include "scene/Component.h"
#include "scene/Scene.h"

#include "shared/hlslCppShared.hlsli"
#include "shared/rt.hlsli"


namespace pbe {

   CVarSlider<int> cvNRays{ "render/rt/nRays", 1, 1, 128 };
   CVarSlider<int> cvRayDepth{ "render/rt/rayDepth", 3, 1, 8 };

   CVarValue<bool> cvAccumulate{ "render/rt/accumulate", false };
   CVarValue<bool> cvHistoryReprojection{ "render/rt/history reprojection", false };
   CVarValue<bool> cvImportanceSampling{ "render/rt/importance sampling", false };
   CVarValue<bool> cvDenoise{ "render/rt/denoise", true };
   CVarTrigger cvClearHistory{ "render/rt/clear history"};

   CVarSlider<float> cvReprojectionHistoryWeight{ "render/rt/reprojection/history weight", 0.5f, 0, 1 };
   CVarValue<bool> cvReprojectionShowNewPixel{ "render/rt/reprojection/show new pixel", false };
   CVarValue<bool> cvReprojectionObjID{ "render/rt/reprojection/obj id", true };
   CVarValue<bool> cvReprojectionNormal{ "render/rt/reprojection/normal", true };
   CVarValue<bool> cvReprojectionDepth{ "render/rt/reprojection/depth", true };

   void RTRenderer::Init() {

   }

   void RTRenderer::RenderScene(CommandList& cmd, const Scene& scene, const RenderCamera& camera, RenderContext& context) {
      GPU_MARKER("RT Scene");
      PROFILE_GPU("RT Scene");

      uint importanceSampleObjIdx = -1;

      std::vector<SRTObject> objs;

      for (auto [e, trans, material, geom] : scene.View<SceneTransformComponent, MaterialComponent, GeometryComponent>().each()) {
         SRTObject obj;
         obj.position = trans.Position();
         obj.id = (uint)e;

         obj.rotation = glm::make_vec4(glm::value_ptr(trans.Rotation()));

         obj.geomType = (int)geom.type;
         obj.halfSize = geom.sizeData / 2.f * trans.Scale();

         obj.baseColor = material.baseColor;
         obj.metallic = material.metallic;
         obj.roughness = material.roughness;
         obj.emissiveColor = material.emissiveColor * material.emissivePower;
         if (material.emissivePower > 0) {
            importanceSampleObjIdx = (uint)objs.size();
         }

         objs.emplace_back(obj);
      }

      uint nObj = (uint)objs.size();

      // todo: make it dynamic
      if (!rtObjectsBuffer || rtObjectsBuffer->ElementsCount() < nObj) {
         auto bufferDesc = Buffer::Desc::Structured("RtObjects", nObj, sizeof(SRTObject));
         rtObjectsBuffer = Buffer::Create(bufferDesc);
      }

      cmd.UpdateSubresource(*rtObjectsBuffer, objs.data(), 0, nObj * sizeof(SRTObject));

      auto& outTexture = *context.colorHDR;
      auto outTexSize = outTexture.GetDesc().size;

      // cmd.ClearRenderTarget(*context.colorHDR, vec4{ 0, 0, 0, 1 });

      static int accumulatedFrames = 0;

      bool resetHistory = cvClearHistory;

      float historyWeight = cvReprojectionHistoryWeight;

      if (cvAccumulate) {
         historyWeight = (float)accumulatedFrames / (float)(accumulatedFrames + cvNRays);
         accumulatedFrames += cvNRays;

         resetHistory |= camera.prevViewProjection != camera.GetViewProjection();
      } else {
         accumulatedFrames = 0;
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
      rtCB.importanceSampleObjIdx = importanceSampleObjIdx;

      rtCB.debugFlags = 0;
      rtCB.debugFlags |= cvReprojectionShowNewPixel ? DBG_FLAG_SHOW_NEW_PIXEL : 0;
      rtCB.debugFlags |= cvReprojectionObjID ? DBG_FLAG_REPR_OBJ_ID : 0;
      rtCB.debugFlags |= cvReprojectionNormal ? DBG_FLAG_REPR_NORMAL : 0;
      rtCB.debugFlags |= cvReprojectionDepth ? DBG_FLAG_REPR_DEPTH : 0;

      auto rtConstantsCB = cmd.AllocDynConstantBuffer(rtCB);

      auto setSharedResource = [&](GpuProgram& pass) {
         cmd.SetCB<SRTConstants>(pass.GetBindPoint("gRTConstantsCB"), rtConstantsCB.buffer, rtConstantsCB.offset);
         cmd.SetSRV(pass.GetBindPoint("gRtObjects"), rtObjectsBuffer);
      };

      CMD_BINDS_GUARD();

      // todo: generate by rasterizator
      if (cvDenoise) {
         GPU_MARKER("GBuffer");
         PROFILE_GPU("GBuffer");

         auto pass = GetGpuProgram(ProgramDesc::Cs("rt.hlsl", "GBufferCS"));
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         cmd.SetUAV(pass->GetBindPoint("gDepthOut"), context.depthTex);
         cmd.SetUAV(pass->GetBindPoint("gNormalOut"), context.normalTex);

         cmd.SetUAV(pass->GetBindPoint("gUnderCursorBuffer"), context.underCursorBuffer); // todo: only for editor

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });
      }

      {
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

         cmd.SetUAV(pass->GetBindPoint("gColorOut"), context.rtColorNoisyTex);

         cmd.Dispatch2D(outTexSize, int2{8, 8});
      }

      if (cvAccumulate && !cvDenoise) {
         UNIMPLEMENTED();

         // todo: incorrect name
         GPU_MARKER("Reproject");
         PROFILE_GPU("Reproject");

         auto desc = ProgramDesc::Cs("rt.hlsl", "HistoryAccCS");
         if (cvHistoryReprojection) {
            desc.cs.defines.AddDefine("ENABLE_REPROJECTION");
         }

         auto pass = GetGpuProgram(desc);
         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         cmd.SetSRV(pass->GetBindPoint("gReprojectCount"), context.reprojectCountTexPrev);
         cmd.SetSRV(pass->GetBindPoint("gDepthPrev"), context.depthTexPrev);
         cmd.SetSRV(pass->GetBindPoint("gDepth"), context.depthTex);
         cmd.SetSRV(pass->GetBindPoint("gNormalPrev"), context.normalTexPrev);
         cmd.SetSRV(pass->GetBindPoint("gNormal"), context.normalTex);
         cmd.SetSRV(pass->GetBindPoint("gHistory"), context.historyTexPrev);

         cmd.SetUAV(pass->GetBindPoint("gReprojectCountOut"), context.reprojectCountTex);
         cmd.SetUAV(pass->GetBindPoint("gColor"), context.colorHDR);
         cmd.SetUAV(pass->GetBindPoint("gHistoryOut"), context.historyTex);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });

         // if (cvHistoryReprojection) {
         //    std::swap(context.historyTex, context.historyTexPrev);
         //    std::swap(context.reprojectCountTex, context.reprojectCountTexPrev);
         //    std::swap(context.normalTex, context.normalTexPrev);
         //    std::swap(context.depthTex, context.depthTexPrev);
         // }
      }

      if (cvDenoise) {
         GPU_MARKER("Denoise");
         PROFILE_GPU("Denoise");

         // cvHistoryReprojection&& cvAccumulate

         auto desc = ProgramDesc::Cs("rt.hlsl", "DenoiseCS");
         auto pass = GetGpuProgram(desc);

         cmd.SetCompute(*pass);
         setSharedResource(*pass);

         CMD_BINDS_GUARD();

         // cmd.SetSRV(pass->GetBindPoint("gReprojectCount"), context.reprojectCountTexPrev);
         cmd.SetSRV(pass->GetBindPoint("gDepth"), context.depthTex);
         cmd.SetSRV(pass->GetBindPoint("gNormal"), context.normalTex);
         cmd.SetSRV(pass->GetBindPoint("gColor"), context.rtColorNoisyTex);
         cmd.SetSRV(pass->GetBindPoint("gHistory"), context.historyTex);

         cmd.SetUAV(pass->GetBindPoint("gHistoryOut"), context.historyTexPrev); // todo: not prev
         cmd.SetUAV(pass->GetBindPoint("gColorOut"), context.colorHDR);

         cmd.Dispatch2D(outTexSize, int2{ 8, 8 });

         std::swap(context.historyTex, context.historyTexPrev);
         std::swap(context.reprojectCountTex, context.reprojectCountTexPrev);
         std::swap(context.normalTex, context.normalTexPrev);
         std::swap(context.depthTex, context.depthTexPrev);
      }
   }

}
