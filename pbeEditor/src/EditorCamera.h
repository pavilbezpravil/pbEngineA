#pragma once

#include "rend/Renderer.h"
#include "math/Types.h"

namespace pbe {

   struct EditorCamera : RenderCamera {
      vec2 cameraAngle{};

      void Update(float dt);
      void UpdateView();
   };

}
