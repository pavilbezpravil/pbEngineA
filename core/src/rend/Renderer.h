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
#include "scene/Scene.h"


struct SCameraCB;

namespace pbe {
   // class RTRenderer;

   struct RenderCamera {
      vec3 position{};

      mat4 view;
      mat4 projection;

      float zNear;
      float zFar;

      vec3 Up() const {
         return vec3{ view[0][1], view[1][1] , view[2][1] };
      }

      vec3 Forward() const {
         return vec3{view[0][2], view[1][2] , view[2][2] };
      }

      mat4 GetViewProjection() const {
         return projection * view;
      }

      void FillSCameraCB(SCameraCB& cameraCB) const;
   };

   struct CameraContext {
      Ref<Texture2D> colorHDR;
      Ref<Texture2D> colorLDR;
      Ref<Texture2D> depth;
      Ref<Texture2D> depthCopy;
      Ref<Texture2D> normal;
      Ref<Texture2D> position;
      Ref<Texture2D> ssao;

      Ref<Texture2D> shadowMap;
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

   class CORE_API Renderer {
   public:
      ~Renderer();

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
      Ref<Buffer> waterWaves;

      struct RenderObject {
         SceneTransformComponent trans;
         SimpleMaterialComponent material;
      };

      std::vector<RenderObject> opaqueObjs;
      std::vector<RenderObject> transparentObjs;
      std::vector<RenderObject> decalObjs;

      void Init();

      void UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs);
      void RenderDataPrepare(CommandList& cmd, Scene& scene);

      void RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera, CameraContext& cameraContext);
      void RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs, GpuProgram& program, const CameraContext& cameraContext);

   };

}
