#pragma once
#include "core/Ref.h"


namespace pbe {
   class Scene;

   class Buffer;
   struct RenderContext;
   class CommandList;

   class Water {
   public:
      void Render(CommandList& cmd, Scene& scene, RenderContext& cameraContext);

   private:
      Ref<Buffer> waterWaves;
   };

}
