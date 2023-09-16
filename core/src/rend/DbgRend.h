#pragma once

#include "core/Common.h"
#include "math/Types.h"
#include "core/Ref.h"

namespace pbe {
   class Entity;

   struct AABB;
   struct Sphere;
   struct Frustum;
   struct RenderCamera;
   struct VertexPosColor;
   class GpuProgram;

   class CommandList;

   class CORE_API DbgRend {
   public:
      NON_COPYABLE(DbgRend);

      DbgRend();
      ~DbgRend();

      void DrawLine(const vec3& start, const vec3& end, const vec4& color = vec4_One);
      void DrawSphere(const Sphere& sphere, const vec4& color = vec4_One);
      void DrawAABB(const AABB& aabb, const vec4& color = vec4_One);
      void DrawAABBOrderPoints(const vec3 points[8], const vec4& color = vec4_One);
      void DrawViewProjection(const mat4& invViewProjection, const vec4& color = vec4_One);
      void DrawFrustum(const Frustum& frustum, const vec3& pos, const vec3& forward, const vec4& color = vec4_One);

      void DrawLine(const Entity& entity0, const Entity& entity1, const vec4& color = vec4_One);

      void Clear();

      void Render(CommandList& cmd, const RenderCamera& camera);

   private:
      std::vector<VertexPosColor> lines;

      Ref<GpuProgram> program;
   };

}
