#pragma once
#include "core/Ref.h"


namespace pbe {

   class Buffer;
   struct CameraContext;
   class CommandList;

   class Water {
   public:
      void Render(CommandList& cmd, CameraContext& cameraContext);

   private:
      Ref<Buffer> waterWaves;
   };

}
