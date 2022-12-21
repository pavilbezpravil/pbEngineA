#pragma once
#include "core/Ref.h"


namespace pbe {
   class Scene;

   class Buffer;
   struct CameraContext;
   class CommandList;

   class Water {
   public:
      void Render(CommandList& cmd, Scene& scene, CameraContext& cameraContext);

   private:
      Ref<Buffer> waterWaves;
   };

}
