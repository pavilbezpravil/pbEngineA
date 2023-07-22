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
   CVarValue<bool> cvHistoryReprojection{ "render/rt/history reprojection", true };
   CVarValue<bool> cvImportanceSampling{ "render/rt/importance sampling", false };
   CVarValue<bool> cvDenoise{ "render/rt/denoise", false };
   CVarTrigger cvClearHistory{ "render/rt/clear history"};

   CVarSlider<float> cvReprojectionHistoryWeight{ "render/rt/reprojection/history weight", 1, 0, 1 };
   CVarValue<bool> cvReprojectionShowNewPixel{ "render/rt/reprojection/show new pixel", false };
   CVarValue<bool> cvReprojectionObjID{ "render/rt/reprojection/obj id", true };
   CVarValue<bool> cvReprojectionNormal{ "render/rt/reprojection/normal", true };
   CVarValue<bool> cvReprojectionDepth{ "render/rt/reprojection/depth", true };

   void RTRenderer::Init() {

   }

   void RTRenderer::RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera, RenderContext& context) {
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
         if (cvHistoryReprojection) {
            accumulatedFrames = 0;
         } else {
            historyWeight = (float)accumulatedFrames / (float)(accumulatedFrames + cvNRays);
            accumulatedFrames += cvNRays;

            resetHistory |= camera.prevViewProjection != camera.GetViewProjection();
         }
      } else {
         accumulatedFrames = 0;
      }


      if (resetHistory || !cvAccumulate) {
         cmd.ClearUAVUint(*context.reprojectCountTexPrev);
         cmd.ClearUAVFloat(*context.historyTex2);

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

      if (cvHistoryReprojection) {
         GPU_MARKER("GBuffer");
         PROFILE_GPU("GBuffer");

         auto gbufferPass = GetGpuProgram(ProgramDesc::Cs("rt.hlsl", "GBufferCS"));
         gbufferPass->Activate(cmd);

         gbufferPass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);
         gbufferPass->SetSRV(cmd, "gRtObjects", *rtObjectsBuffer);

         gbufferPass->SetUAV(cmd, "gDepthOut", *context.depthTex);
         gbufferPass->SetUAV(cmd, "gNormalOut", *context.normalTex);
         gbufferPass->SetUAV(cmd, "gObjIDOut", *context.objIDTex);

         gbufferPass->SetUAV(cmd, "gUnderCursorBuffer", *context.underCursorBuffer); // todo: only for editor

         gbufferPass->Dispatch2D(cmd, outTexSize, int2{ 8, 8 });

         cmd.ClearUAV_CS();
      }

      // cmd.CopyResource(*context.colorHDR, *normalTex);
      // return;

      {
         GPU_MARKER("Ray Trace");
         PROFILE_GPU("Ray Trace");

         auto desc = ProgramDesc::Cs("rt.hlsl", "rtCS");
         if (cvImportanceSampling) {
            desc.cs.defines.AddDefine("IMPORTANCE_SAMPLING");
         }

         auto rayTracePass = GetGpuProgram(desc);
         rayTracePass->Activate(cmd);

         rayTracePass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);
         rayTracePass->SetSRV(cmd, "gRtObjects", *rtObjectsBuffer);

         rayTracePass->SetUAV(cmd, "gColor", *context.colorHDR);

         rayTracePass->Dispatch2D(cmd, outTexSize, int2{8, 8});

         cmd.ClearSRV_CS();
         cmd.ClearUAV_CS();
      }

      if (cvAccumulate) {
         GPU_MARKER("Reproject");
         PROFILE_GPU("Reproject");

         auto desc = ProgramDesc::Cs("rt.hlsl", "HistoryAccCS");
         if (cvHistoryReprojection) {
            desc.cs.defines.AddDefine("ENABLE_REPROJECTION");
         }

         auto historyPass = GetGpuProgram(desc);

         historyPass->Activate(cmd); // todo: remove activate

         historyPass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);

         // todo: remove
         historyPass->SetSRV(cmd, "gRtObjects", *rtObjectsBuffer);

         historyPass->SetSRV(cmd, "gReprojectCount", *context.reprojectCountTexPrev);
         historyPass->SetSRV(cmd, "gDepthPrev", *context.depthTexPrev);
         historyPass->SetSRV(cmd, "gDepth", *context.depthTex);
         historyPass->SetSRV(cmd, "gNormalPrev", *context.normalTexPrev);
         historyPass->SetSRV(cmd, "gNormal", *context.normalTex);
         historyPass->SetSRV(cmd, "gHistory", *context.historyTex2);
         historyPass->SetSRV(cmd, "gObjIDPrev", *context.objIDTexPrev);
         historyPass->SetSRV(cmd, "gObjID", *context.objIDTex);

         historyPass->SetUAV(cmd, "gReprojectCountOut", *context.reprojectCountTex);
         historyPass->SetUAV(cmd, "gColor", *context.colorHDR);
         historyPass->SetUAV(cmd, "gHistoryOut", *context.historyTex);

         historyPass->Dispatch2D(cmd, outTexSize, int2{ 8, 8 });

         cmd.ClearSRV_CS();
         cmd.ClearUAV_CS();

         if (cvHistoryReprojection) {
            std::swap(context.historyTex, context.historyTex2);
            std::swap(context.reprojectCountTex, context.reprojectCountTexPrev);
            std::swap(context.objIDTex, context.objIDTexPrev);
            std::swap(context.normalTex, context.normalTexPrev);
            std::swap(context.depthTex, context.depthTexPrev);
         }
      }

      if (cvDenoise && cvAccumulate) { // todo: cvAccumulate
         GPU_MARKER("Denoise");
         PROFILE_GPU("Denoise");

         auto desc = ProgramDesc::Cs("rt.hlsl", "DenoiseCS");
         auto denoisePass = GetGpuProgram(desc);

         denoisePass->Activate(cmd);

         denoisePass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);

         // here *prev means current frame
         denoisePass->SetSRV(cmd, "gReprojectCount", *context.reprojectCountTexPrev);
         denoisePass->SetSRV(cmd, "gDepth", *context.depthTexPrev);
         denoisePass->SetSRV(cmd, "gNormal", *context.normalTexPrev);
         denoisePass->SetSRV(cmd, "gHistory", *context.historyTex2); // read data from this
         denoisePass->SetSRV(cmd, "gObjID", *context.objIDTexPrev);

         denoisePass->SetUAV(cmd, "gHistoryOut", *context.historyTex); // read data from this
         denoisePass->SetUAV(cmd, "gColor", *context.colorHDR);

         denoisePass->Dispatch2D(cmd, outTexSize, int2{ 8, 8 });

         cmd.ClearSRV_CS();
         cmd.ClearUAV_CS();

         std::swap(context.historyTex, context.historyTex2);
      }
   }

}
