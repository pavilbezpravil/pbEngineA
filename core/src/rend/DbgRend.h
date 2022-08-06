#pragma once

#include "math/Types.h"
#include "core/Ref.h"

namespace pbe {
   struct AABB;
   struct Sphere;
   struct RenderCamera;
   struct VertexPosColor;
   class GpuProgram;

   class CommandList;

   class DbgRend {
   public:
      DbgRend();

      void DrawLine(const vec3& start, const vec3& end, const vec4& color = vec4_One);
      void DrawSphere(const Sphere& sphere, const vec4& color = vec4_One);
      void DrawAABB(const AABB& aabb, const vec4& color = vec4_One);

      void Clear();

      void Render(CommandList& cmd, const RenderCamera& camera);

   private:
      std::vector<VertexPosColor> lines;

      Ref<GpuProgram> program;
   };

}
