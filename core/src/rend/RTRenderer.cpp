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
   CVarValue<bool> cvAccumulate{ "render/rt/accumulate", true };
   CVarValue<bool> cvHistoryResetOnCameraMove{ "render/rt/history reset on camera move", false };
   CVarValue<bool> cvHistoryReprojection{ "render/rt/history reprojection", true };
   CVarValue<bool> cvUseHistoryWeight{ "render/rt/use history weight", true };
   CVarSlider<float> cvHistoryWeight{ "render/rt/history weight", 0.97f, 0, 1 };
   CVarTrigger cvClearHistory{ "render/rt/clear history"};

   void RTRenderer::Init() {

   }

   void RTRenderer::RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera,
                                CameraContext& cameraContext) {
      GPU_MARKER("RT Scene");
      PROFILE_GPU("RT Scene");

      std::vector<SRTObject> objs;

      for (auto [e, trans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
         SRTObject obj;
         obj.position = trans.Position();
         obj.id = (uint)e;
         obj.halfSize = trans.Scale() / 2.f;
         obj.baseColor = material.baseColor;
         obj.metallic = material.metallic;
         obj.roughness = material.roughness;
         obj.emissiveColor = material.emissiveColor * material.emissivePower;
         obj.geomType = 1;

         objs.emplace_back(obj);
      }

      uint nObj = (uint)objs.size();

      // todo: make it dynamic
      if (!rtObjectsBuffer || rtObjectsBuffer->ElementsCount() < nObj) {
         auto bufferDesc = Buffer::Desc::Structured("RtObjects", nObj, sizeof(SRTObject));
         rtObjectsBuffer = Buffer::Create(bufferDesc);
      }

      cmd.UpdateSubresource(*rtObjectsBuffer, objs.data(), 0, nObj * sizeof(SRTObject));

      auto& outTexture = *cameraContext.colorHDR;
      auto outTexSize = outTexture.GetDesc().size;

      // cmd.ClearRenderTarget(*cameraContext.colorHDR, vec4{ 0, 0, 0, 1 });

      static mat4 cameraMatr;
      static int accumulatedFrames = 0;

      bool resetOnCameraMove = cvHistoryResetOnCameraMove ? cameraMatr != camera.GetViewProjection() : false;

      if (cvClearHistory || resetOnCameraMove || !cvAccumulate) {
         cmd.ClearUAVUint(*cameraContext.reprojectCountTex2);
         cmd.ClearUAVFloat(*cameraContext.historyTex2);

         cameraMatr = camera.GetViewProjection();
         accumulatedFrames = 0;
      }

      float historyWeight = (float)accumulatedFrames / (float)(accumulatedFrames + cvNRays);
      if (cvAccumulate) {
         accumulatedFrames += cvNRays;
      }

      if (cvUseHistoryWeight) {
         accumulatedFrames = 0;
         historyWeight = cvHistoryWeight;
      }

      SRTConstants rtCB;
      rtCB.rtSize = outTexSize;
      rtCB.rayDepth = cvRayDepth;
      rtCB.nObjects = nObj;
      rtCB.nRays = cvNRays;
      rtCB.random01 = Random::Uniform(0.f, 1.f);
      rtCB.historyWeight = historyWeight;
      auto rtConstantsCB = cmd.AllocDynConstantBuffer(rtCB);

      {
         GPU_MARKER("GBuffer");
         PROFILE_GPU("GBuffer");

         auto gbufferPass = GetGpuProgram(ProgramDesc::Cs("rt.cs", "GBufferCS"));
         gbufferPass->Activate(cmd);

         gbufferPass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);
         gbufferPass->SetSRV(cmd, "gRtObjects", *rtObjectsBuffer);

         gbufferPass->SetUAV(cmd, "gDepthOut", *cameraContext.depthTex);
         gbufferPass->SetUAV(cmd, "gNormalOut", *cameraContext.normalTex);
         gbufferPass->SetUAV(cmd, "gObjIDOut", *cameraContext.objIDTex);

         gbufferPass->Dispatch2D(cmd, outTexSize, int2{ 8, 8 });

         cmd.ClearUAV_CS();
      }

      // cmd.CopyResource(*cameraContext.colorHDR, *normalTex);
      // return;

      {
         GPU_MARKER("Ray Trace");
         PROFILE_GPU("Ray Trace");

         auto rayTracePass = GetGpuProgram(ProgramDesc::Cs("rt.cs", "rtCS"));
         rayTracePass->Activate(cmd);

         rayTracePass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);
         rayTracePass->SetSRV(cmd, "gRtObjects", *rtObjectsBuffer);

         rayTracePass->SetUAV(cmd, "gColor", *cameraContext.colorHDR);

         rayTracePass->Dispatch2D(cmd, outTexSize, int2{8, 8});

         cmd.ClearUAV_CS();
      }

      if (cvAccumulate) {
         GPU_MARKER("Reproject");
         PROFILE_GPU("Reproject");

         auto desc = ProgramDesc::Cs("rt.cs", "HistoryAccCS");
         if (cvHistoryReprojection) {
            desc.cs.defines.AddDefine("ENABLE_REPROJECTION");
         }

         auto historyPass = GetGpuProgram(desc);

         historyPass->Activate(cmd); // todo: remove activate

         historyPass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);

         // todo: remove
         historyPass->SetSRV(cmd, "gRtObjects", *rtObjectsBuffer);

         historyPass->SetSRV(cmd, "gReprojectCount", *cameraContext.reprojectCountTex2);
         historyPass->SetSRV(cmd, "gDepth", *cameraContext.depthTex);
         historyPass->SetSRV(cmd, "gNormal", *cameraContext.normalTex);
         historyPass->SetSRV(cmd, "gHistory", *cameraContext.historyTex2);
         historyPass->SetSRV(cmd, "gObjIDPrev", *cameraContext.objIDTexPrev);
         historyPass->SetSRV(cmd, "gObjID", *cameraContext.objIDTex);

         historyPass->SetUAV(cmd, "gReprojectCountOut", *cameraContext.reprojectCountTex);
         historyPass->SetUAV(cmd, "gColor", *cameraContext.colorHDR);
         historyPass->SetUAV(cmd, "gHistoryOut", *cameraContext.historyTex);

         historyPass->Dispatch2D(cmd, outTexSize, int2{ 8, 8 });

         cmd.ClearUAV_CS();

         if (cvHistoryReprojection) {
            std::swap(cameraContext.historyTex, cameraContext.historyTex2);
            std::swap(cameraContext.reprojectCountTex, cameraContext.reprojectCountTex2);
            std::swap(cameraContext.objIDTex, cameraContext.objIDTexPrev);
         }
      }


      // cmd.CopyResource(*cameraContext.colorHDR, *history);
   }

}
