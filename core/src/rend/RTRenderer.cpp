#include "pch.h"
#include "RTRenderer.h"
#include "Buffer.h"
#include "CommandList.h"
#include "Renderer.h" // todo:
#include "Shader.h"
#include "core/Profiler.h"
#include "math/Random.h"

#include "scene/Component.h"
#include "scene/Scene.h"

#include "shared/hlslCppShared.hlsli"
#include "shared/rt.hlsli"


namespace pbe {

   void RTRenderer::Init() {
      auto programDesc = ProgramDesc::Cs("rt.cs", "rtCS");
      rayTracePass = GpuProgram::Create(programDesc);
   }

   void RTRenderer::RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera,
                                CameraContext& cameraContext) {

      GPU_MARKER("RT Scene");
      PROFILE_GPU("RT Scene");

      std::vector<SRTObject> objs;

      for (auto [e, trans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
         SRTObject obj;
         obj.position = trans.position;
         obj.halfSize = trans.scale / 2.f;
         obj.albedo = material.albedo;
         obj.geomType = 1;

         objs.emplace_back(obj);
      }

      uint nObj = (uint)objs.size();

      if (!rtObjectsBuffer || rtObjectsBuffer->ElementsCount() < nObj) {
         auto bufferDesc = Buffer::Desc::Structured("RtObjects", nObj, sizeof(SRTObject));
         rtObjectsBuffer = Buffer::Create(bufferDesc);
      }

      cmd.UpdateSubresource(*rtObjectsBuffer, objs.data(), 0, nObj * sizeof(SRTObject));

      // cmd.ClearRenderTarget(*cameraContext.colorHDR, vec4{ 0, 0, 0, 1 });

      SCameraCB cameraCB;
      camera.FillSCameraCB(cameraCB);
      cameraCB.rtSize = cameraContext.colorHDR->GetDesc().size;

      static int iFrame = 0; // todo:
      ++iFrame;
      cameraCB.iFrame = iFrame;
      cameraContext.cameraCB = cmd.AllocDynConstantBuffer(cameraCB);

      SRTConstants rtCB;
      rtCB.rtSize = cameraContext.colorHDR->GetDesc().size;
      rtCB.rayDepth = 3;
      rtCB.nObjects = nObj;
      rtCB.nRays = 1;
      rtCB.random01 = Random::Uniform(0.f, 1.f);
      auto rtConstantsCB = cmd.AllocDynConstantBuffer(rtCB);

      rayTracePass->Activate(cmd);

      rayTracePass->SetCB<SRTConstants>(cmd, "gRTConstantsCB", *rtConstantsCB.buffer, rtConstantsCB.offset);
      rayTracePass->SetCB<SCameraCB>(cmd, "gCameraCB", *cameraContext.cameraCB.buffer, cameraContext.cameraCB.offset);
      // rayTracePass->SetCB<SSceneCB>(cmd, "gSceneCB", *cameraContext.sceneCB.buffer, cameraContext.sceneCB.offset);
      rayTracePass->SetSRV(cmd, "gRtObjects", *rtObjectsBuffer);

      rayTracePass->SetUAV(cmd, "gColor", *cameraContext.colorHDR);

      rayTracePass->Dispatch(cmd, glm::ceil(vec2{ cameraContext.colorHDR->GetDesc().size } / vec2{ 8 }));
   }

}
