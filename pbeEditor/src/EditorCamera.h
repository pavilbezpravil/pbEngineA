#pragma once

#include "rend/Renderer.h"
#include "math/Types.h"

namespace pbe {

   struct EditorCamera : public RenderCamera {
      vec2 cameraAngle{};

      void Update(float dt);
      void UpdateView();
      void SetViewDirection(const vec3& direction);
   };

}
