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

// #include <shared/hlslCppShared.hlsli> // todo:

#include "system/Terrain.h"
#include "system/Water.h"

struct SCameraCB;

namespace pbe {
   // class RTRenderer;

   struct CORE_API RenderCamera {
      vec3 position{};

      mat4 view;
      mat4 projection;

      mat4 prevViewProjection;

      float zNear;
      float zFar;

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

      void FillSCameraCB(SCameraCB& cameraCB) const;
   };

   struct CameraContext {
      Ref<Texture2D> colorHDR;
      Ref<Texture2D> colorLDR;
      Ref<Texture2D> depth;
      Ref<Texture2D> depthWithoutWater;
      Ref<Texture2D> linearDepth;
      Ref<Texture2D> depthCopy;
      Ref<Texture2D> waterRefraction;
      Ref<Texture2D> normal;
      Ref<Texture2D> position;
      Ref<Texture2D> ssao;

      Ref<Texture2D> shadowMap;

      // todo: simplify adding new rt
      // rt
      Ref<Texture2D> historyTex;
      Ref<Texture2D> historyTex2;
      Ref<Texture2D> depthTex;
      Ref<Texture2D> normalTex;
      Ref<Texture2D> reprojectCountTex;
      Ref<Texture2D> reprojectCountTex2;


      Ref<Buffer> underCursorBuffer; // todo:

      int2 cursorPixelIdx{-1}; // todo:
   };

   struct RenderConfing {
      bool transparency = true;
      bool decals = true;
      bool transparencySorting = true;
      bool opaqueSorting = true;
      bool useZPass = true;
      bool useShadowPass = true;
      bool ssao = false;
      bool fog = false;
      bool useInstancedDraw = true;
      bool superSampling = false;
   };

   // enum class MaterialType {
   //    Opaque,
   //    Transparent,
   // };

   // struct Material {
   //    SMaterial material; // todo:
   //    bool opaque = true; // todo:
   //    // MaterialType type = MaterialType::Opaque;
   // };
   //
   // struct DrawDesc {
   //    uint entityID = (uint)-1;
   //    mat4 transform;
   //    Material material;
   //
   //    Mesh* mesh = nullptr;
   //    AABB aabb;
   // };

   class CORE_API Renderer {
   public:
      RenderConfing cfg;

      Own<RTRenderer> rtRenderer;

      Ref<GpuProgram> baseColorPass;
      Ref<GpuProgram> baseZPass;
      Ref<GpuProgram> shadowMapPass;

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
         SimpleMaterialComponent material;
      };

      std::vector<RenderObject> opaqueObjs;
      std::vector<RenderObject> transparentObjs;
      std::vector<RenderObject> decalObjs;

      // std::vector<DrawDesc> drawDescs;

      void Init();

      void UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs);
      void RenderDataPrepare(CommandList& cmd, Scene& scene, const RenderCamera& cullCamera);

      void RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera, CameraContext& cameraContext);
      void RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs, GpuProgram& program, const CameraContext& cameraContext);

      uint GetEntityIDUnderCursor(CommandList& cmd);

   };

}
