#pragma once

#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "GpuTimer.h"
#include "RendRes.h"
#include "Texture2D.h"
#include "Shader.h"
#include "math/Types.h"
#include "mesh/Mesh.h"
#include "scene/Component.h"
#include "scene/Scene.h"


namespace pbe {

   struct RenderCamera {
      vec3 position{};

      mat4 view;
      mat4 projection;

      float zNear;
      float zFar;

      vec3 Forward() const {
         return vec3{view[0][2], view[1][2] , view[2][2] };
      }

      mat4 GetViewProjection() const {
         return projection * view;
      }
   };

   struct CameraContext {
      Ref<Texture2D> colorHDR;
      Ref<Texture2D> depth;
      Ref<Texture2D> depthCopy;
      Ref<Texture2D> normal;
      Ref<Texture2D> position;
      Ref<Texture2D> ssao;

      // todo:
      OffsetedBuffer cameraCB;
      OffsetedBuffer sceneCB;
   };

   struct RenderConfing {
      bool transparency = true;
      bool decals = true;
      bool transparencySorting = true;
      bool opaqueSorting = true;
      bool useZPass = true;
      bool ssao = false;
      bool fog = false;
      bool useInstancedDraw = true;
   };

   class Renderer {
   public:
      ~Renderer() {
         rendres::Term();
      }

      RenderConfing cfg;

      Ref<GpuProgram> baseColorPass;
      Ref<GpuProgram> baseZPass;
      Ref<GpuProgram> baseDecal;

      Ref<GpuProgram> ssaoPass;

      Mesh mesh;

      Ref<Buffer> instanceBuffer;
      Ref<Buffer> decalBuffer;
      Ref<Buffer> lightBuffer;
      Ref<Buffer> ssaoRandomDirs;

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
