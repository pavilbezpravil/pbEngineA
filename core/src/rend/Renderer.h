#pragma once

#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "RTRenderer.h"
#include "Texture2D.h"
#include "Shader.h"
#include "math/Types.h"
#include "mesh/Mesh.h"
#include "scene/Component.h"
#include "math/Shape.h"

#include "system/Terrain.h"
#include "system/Water.h"

struct SCameraCB;

namespace pbe {

   struct CORE_API RenderCamera {
      vec3 position{};

      mat4 view{};
      mat4 projection{};

      mat4 prevViewProjection{};

      float zNear = 0.1f;
      float zFar = 1000.f;

      vec3 Right() const {
         return vec3{ view[0][0], view[1][0] , view[2][0] };
      }

      vec3 Up() const {
         return vec3{ view[0][1], view[1][1] , view[2][1] };
      }

      vec3 Forward() const {
         return vec3{view[0][2], view[1][2] , view[2][2] };
      }

      mat4 GetViewProjection() const {
         return projection * view;
      }

      void NextFrame();

      void UpdateProj(int2 size, float fov = 90.f / 180 * PI) {
         projection = glm::perspectiveFov(fov, (float)size.x, (float)size.y, zNear, zFar);
      }

      void UpdateView(const vec3& direction, const vec3& up = vec3_Y) {
         view = glm::lookAt(position, position + direction, up);
      }

      void FillSCameraCB(SCameraCB& cameraCB) const;
   };

   struct RenderContext {
      Ref<Texture2D> colorHDR;
      Ref<Texture2D> colorLDR;
      Ref<Texture2D> depth;
      Ref<Texture2D> depthWithoutWater;
      Ref<Texture2D> linearDepth;
      Ref<Texture2D> waterRefraction;
      Ref<Texture2D> ssao;

      Ref<Texture2D> shadowMap;

      // todo: simplify adding new rt
      // rt
      Ref<Texture2D> historyTex;
      Ref<Texture2D> historyTex2;

      Ref<Texture2D> depthTex;
      Ref<Texture2D> depthTexPrev;

      Ref<Texture2D> normalTex;
      Ref<Texture2D> normalTexPrev;

      Ref<Texture2D> reprojectCountTex;
      Ref<Texture2D> reprojectCountTexPrev;

      Ref<Texture2D> objIDTex;
      Ref<Texture2D> objIDTexPrev;

      // todo:
      Ref<Buffer> underCursorBuffer;
      int2 cursorPixelIdx{-1};
   };

   CORE_API RenderContext CreateRenderContext(int2 size);

   class CORE_API Renderer {
   public:
      Own<RTRenderer> rtRenderer;

      // todo: remove
      Ref<GpuProgram> baseColorPass;

      Mesh mesh;

      Ref<Buffer> instanceBuffer;
      Ref<Buffer> decalBuffer;
      Ref<Buffer> lightBuffer;
      Ref<Buffer> ssaoRandomDirs;

      Ref<Buffer> underCursorBuffer;

      Water waterSystem;
      Terrain terrainSystem;

      struct RenderObject {
         SceneTransformComponent trans;
         MaterialComponent material;
      };

      std::vector<RenderObject> opaqueObjs;
      std::vector<RenderObject> transparentObjs;
      std::vector<RenderObject> decalObjs;

      void Init();

      void UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs);
      void RenderDataPrepare(CommandList& cmd, Scene& scene, const RenderCamera& cullCamera);

      void RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera, RenderContext& context);
      void RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs, GpuProgram& program);

      uint GetEntityIDUnderCursor(CommandList& cmd);

   };

}
