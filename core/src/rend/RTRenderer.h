#pragma once
#include "core/Ref.h"


namespace pbe {

   class Buffer;
   class Scene;
   struct RenderCamera;
   class CommandList;
   class GpuProgram;
   struct CameraContext;


   class CORE_API RTRenderer {
   public:
      void Init();

      void RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera, CameraContext& cameraContext);

      Ref<GpuProgram> rayTracePass;
      Ref<Buffer> rtObjectsBuffer;

   };

}
