#pragma once
#include "core/Ref.h"


namespace pbe {
   class Scene;

   class Buffer;
   struct CameraContext;
   class CommandList;

   class Terrain {
   public:
      void Render(CommandList& cmd, Scene& scene, CameraContext& cameraContext);
   };

}
