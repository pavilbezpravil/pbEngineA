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

   CVarSlider<int> cvNRays{ "render/rt/nRays", 5, 1, 128 };
   CVarSlider<int> cvRayDepth{ "render/rt/rayDepth", 3, 1, 8 };
   CVarValue<bool> cvAccumulate{ "render/rt/accumulate", true };
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
         obj.halfSize = trans.Scale() / 2.f;
         obj.baseColor = material.baseColor;
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

      if (!history || history->GetDesc().size != cameraContext.colorHDR->GetDesc().size) {
         Texture2D::Desc texDesc{
            .size = outTexture.GetDesc().size,
            .format = outTexture.GetDesc().format,
            .bindFlags = D3D11_BIND_UNORDERED_ACCESS,
            .name = "rt history",
         };

         history = Texture2D::Create(texDesc);
      }

      // cmd.ClearRenderTarget(*cameraContext.colorHDR, vec4{ 0, 0, 0, 1 });

      static mat4 cameraMatr;

      static int accumulatedFrames = 0;
      if (cvClearHistory || cameraMatr != camera.GetViewProjection()) {
         cmd.ClearUAVFloat(*history); // todo: unnecessary

         cameraMatr = camera.GetViewProjection();
         accumulatedFrames = 0;
      }

      float historyWeight = (float)accumulatedFrames / (float)(accumulatedFrames + cvNRays);
      accumulatedFrames += cvNRays;

      SRTConstants rtCB;
      rtCB.rtSize = cameraContext.colorHDR->GetDesc().size;
      rtCB.rayDepth = cvRayDepth;
      rtCB.nObjects = nObj;
      rtCB.nRays = cvNRays;
      rtCB.random01 = Random::Uniform(0.f, 1.f);
      rtCB.historyWeight = historyWeight;
      auto rtConstantsCB = cmd.AllocDynConstantBuffer(rtCB);

      {
         auto rayTracePass = GetGpuProgram(ProgramDesc::Cs("rt.cs", "rtCS"));
         rayTracePass->Activate(cmd);

         rayTracePass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);
         rayTracePass->SetSRV(cmd, "gRtObjects", *rtObjectsBuffer);

         rayTracePass->SetUAV(cmd, "gColor", *cameraContext.colorHDR);

         rayTracePass->Dispatch2D(cmd, cameraContext.colorHDR->GetDesc().size, int2{8, 8});
      }

      if (cvAccumulate) {
         cmd.pContext->Flush(); // todo:

         auto historyPass = GetGpuProgram(ProgramDesc::Cs("rt.cs", "HistoryAccCS"));

         historyPass->Activate(cmd); // todo: remove activate

         historyPass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);

         historyPass->SetUAV(cmd, "gColor", *cameraContext.colorHDR);
         historyPass->SetUAV(cmd, "gHistory", *history);

         historyPass->Dispatch2D(cmd, cameraContext.colorHDR->GetDesc().size, int2{ 8, 8 });
      }


      // cmd.CopyResource(*cameraContext.colorHDR, *history);
   }

}
