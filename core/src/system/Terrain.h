#pragma once
#include "core/Ref.h"


namespace pbe {
   class Scene;

   class Buffer;
   struct RenderContext;
   class CommandList;

   class Terrain {
   public:
      void Render(CommandList& cmd, Scene& scene, RenderContext& cameraContext);
   };

}
