#pragma once

#include "core/Common.h"
#include "math/Types.h"
#include "core/Ref.h"
#include "math/Color.h"
#include "scene/Entity.h"

namespace pbe {
   class Buffer;
   struct Transform;
   class Entity;

   struct AABB;
   struct Sphere;
   struct Frustum;
   struct RenderCamera;
   struct VertexPosColor;
   struct VertexPosUintColor;
   class GpuProgram;

   class CommandList;

   class CORE_API DbgRend {
   public:
      NON_COPYABLE(DbgRend);

      DbgRend();
      ~DbgRend();

      void DrawLine(const vec3& start, const vec3& end, const Color& color = Color_White, bool zTest = true, EntityID entityID = NullEntityID);
      void DrawSphere(const Sphere& sphere, const Color& color = Color_White, bool zTest = true);

      // trans - optional
      void DrawAABB(const Transform* trans, const AABB& aabb, const Color& color = Color_White, bool zTest = true);
      void DrawAABBOrderPoints(const vec3 points[8], const Color& color = Color_White, bool zTest = true);

      void DrawViewProjection(const mat4& invViewProjection, const Color& color = Color_White, bool zTest = true);
      void DrawFrustum(const Frustum& frustum, const vec3& pos, const vec3& forward, const Color& color = Color_White, bool zTest = true);

      void DrawLine(const Entity& entity0, const Entity& entity1, const Color& color = Color_White, bool zTest = true);

      void Clear();

      void Render(CommandList& cmd, const RenderCamera& camera);

   private:
      std::vector<VertexPosUintColor> lines;
      std::vector<VertexPosUintColor> linesNoZ;
   };

}
