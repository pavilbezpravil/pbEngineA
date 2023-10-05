#pragma once
#include "core/Ref.h"


namespace pbe {
   class Texture2D;

   class Buffer;
   class Scene;
   struct RenderCamera;
   class CommandList;
   class GpuProgram;
   struct RenderContext;

   class CORE_API RTRenderer {
   public:
      ~RTRenderer();

      void RenderScene(CommandList& cmd, const Scene& scene, const RenderCamera& camera, RenderContext& context);

      Ref<Buffer> rtObjectsBuffer;
      Ref<Buffer> bvhNodesBuffer;
      Ref<Buffer> importanceVolumesBuffer;
   };

}
