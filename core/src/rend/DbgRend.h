#pragma once

#include "core/Common.h"
#include "math/Types.h"
#include "core/Ref.h"
#include "math/Color.h"

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

      void DrawLine(const vec3& start, const vec3& end, const Color& color = Color_White);
      void DrawSphere(const Sphere& sphere, const Color& color = Color_White);
      void DrawAABB(const AABB& aabb, const Color& color = Color_White);
      void DrawAABBOrderPoints(const vec3 points[8], const Color& color = Color_White);
      void DrawViewProjection(const mat4& invViewProjection, const Color& color = Color_White);
      void DrawFrustum(const Frustum& frustum, const vec3& pos, const vec3& forward, const Color& color = Color_White);

      void DrawLine(const Entity& entity0, const Entity& entity1, const Color& color = Color_White);

      void Clear();

      void Render(CommandList& cmd, const RenderCamera& camera);

   private:
      std::vector<VertexPosColor> lines;

      Ref<GpuProgram> program;
   };

}
