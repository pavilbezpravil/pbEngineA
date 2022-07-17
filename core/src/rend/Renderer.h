#pragma once

#include "Buffer.h"
#include "Device.h"
#include "CommandList.h"
#include "GpuTimer.h"
#include "RendRes.h"
#include "Texture2D.h"
#include "Shader.h"
#include "core/Profiler.h"
#include "math/Types.h"
#include "mesh/Mesh.h"
#include "scene/Component.h"
#include "scene/Scene.h"

#include "shared/common.hlsli"


namespace pbe {

   struct RenderCamera {
      vec3 position{};

      mat4 view;
      mat4 projection;

      vec3 Forward() const {
         return vec3{view[0][2], view[1][2] , view[2][2] };
      }

      mat4 GetViewProjection() const {
         return projection * view;
      }
   };

   struct CameraContext {
      Ref<Texture2D> color;
      Ref<Texture2D> depth;
   };

   struct RenderConfing {
      bool renderTransparency = true;
      bool transparencySorting = true;
      bool opaqueSorting = true;
      bool useZPass = false;
      bool useInstancedDraw = false;
   };

   class Renderer {
   public:
      ~Renderer() {
         rendres::Term();
      }

      RenderConfing cfg;

      Ref<GpuProgram> baseColorPass;
      Ref<GpuProgram> baseZPass;

      Mesh mesh;

      Ref<Buffer> cameraCbBuffer;
      Ref<Buffer> instanceBuffer;
      Ref<Buffer> lightBuffer;

      struct RenderObject {
         SceneTransformComponent trans;
         SimpleMaterialComponent material;
      };

      std::vector<RenderObject> opaqueObjs;
      std::vector<RenderObject> transparentObjs;

      void Init() {
         rendres::Init(); // todo:

         auto programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main", "ps_main");
         baseColorPass = GpuProgram::Create(programDesc);

         programDesc = ProgramDesc::VsPs("base.hlsl", "vs_main");
         programDesc.vs.defines.AddDefine("ZPASS");
         baseZPass = GpuProgram::Create(programDesc);

         mesh = Mesh::Create(MeshGeomCube());

         auto bufferDesc = Buffer::Desc::ConstantBuffer(sizeof(CameraCB));
         cameraCbBuffer = Buffer::Create(bufferDesc);
         cameraCbBuffer->SetDbgName("camera cb");
      }

      void UpdateInstanceBuffer(CommandList& cmd, const std::vector<RenderObject>& renderObjs) {
         if (!instanceBuffer || instanceBuffer->ElementsCount() < renderObjs.size()) {
            auto bufferDesc = Buffer::Desc::StructureBuffer((uint)renderObjs.size(), sizeof(Instance));
            instanceBuffer = Buffer::Create(bufferDesc);
            instanceBuffer->SetDbgName("instance buffer");
         }

         std::vector<Instance> instances;
         instances.reserve(renderObjs.size());
         for (auto& [trans, material] : renderObjs) {
            mat4 transform = glm::translate(mat4(1), trans.position);
            transform *= mat4{ trans.rotation };
            transform *= glm::scale(mat4(1), trans.scale);
            transform = glm::transpose(transform);

            Material m;
            m.albedo = material.albedo;
            m.roughness = material.roughness;
            m.metallic = material.metallic;

            instances.emplace_back(transform, m);
         }

         cmd.UpdateSubresource(*instanceBuffer, instances.data(), 0, instances.size() * sizeof(Instance));
      }

      void RenderDataPrepare(CommandList& cmd, Scene& scene) {
         opaqueObjs.clear();
         transparentObjs.clear();
         
         for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
            if (material.opaque) {
               opaqueObjs.emplace_back(sceneTrans, material);
            } else {
               transparentObjs.emplace_back(sceneTrans, material);
            }
         }

         if (!instanceBuffer || instanceBuffer->ElementsCount() < scene.EntitiesCount()) {
            auto bufferDesc = Buffer::Desc::StructureBuffer(scene.EntitiesCount(), sizeof(Instance));
            instanceBuffer = Buffer::Create(bufferDesc);
            instanceBuffer->SetDbgName("instance buffer");
         }

         auto nLights = (int)scene.GetEntitiesWith<LightComponent>().size();
         if (!lightBuffer || lightBuffer->ElementsCount() < nLights) {
            auto bufferDesc = Buffer::Desc::StructureBuffer(nLights, sizeof(Light));
            lightBuffer = Buffer::Create(bufferDesc);
            lightBuffer->SetDbgName("light buffer");
         }

         std::vector<Light> lights;
         lights.reserve(nLights);

         for (auto [e, trans,light] : scene.GetEntitiesWith<SceneTransformComponent, LightComponent>().each()) {
            Light l;
            l.position = trans.position;
            l.color = light.color;

            lights.emplace_back(l);
         }

         cmd.UpdateSubresource(*lightBuffer, lights.data(), 0, lights.size() * sizeof(Light));
      }

      void RenderScene(CommandList& cmd, Scene& scene, const RenderCamera& camera, CameraContext& cameraContext) {
         if (!baseColorPass->Valid() || !baseZPass->Valid()) {
            return;
         }

         GPU_MARKER("Render Scene");
         PROFILE_GPU("Render Scene");

         RenderDataPrepare(cmd, scene);

         auto context = cmd.pContext;

         cmd.ClearRenderTarget(*cameraContext.color, vec4{0, 0, 0, 1});
         cmd.ClearDepthTarget(*cameraContext.depth, 1);

         cmd.SetViewport({}, cameraContext.color->GetDesc().size);
         cmd.SetRasterizerState(rendres::rasterizerState);

         // set mesh
         context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

         ID3D11InputLayout* inputLayout = rendres::GetInputLayout(baseColorPass->vs->blob.Get(), VertexPosNormal::inputElementDesc);
         context->IASetInputLayout(inputLayout);

         ID3D11Buffer* vBuffer = mesh.vertexBuffer->GetBuffer();
         uint offset = 0;
         context->IASetVertexBuffers(0, 1, &vBuffer, &mesh.geom.nVertexByteSize, &offset);
         context->IASetIndexBuffer(mesh.indexBuffer->GetBuffer(), DXGI_FORMAT_R16_UINT, 0);
         //

         CameraCB cb;

         cb.viewProjection = glm::transpose(camera.GetViewProjection());
         cb.position = camera.position;
         cb.nLights = (int)scene.GetEntitiesWith<LightComponent>().size();

         cmd.SetBlendState(nullptr);

         if (cfg.opaqueSorting) {
            // todo: slow. I assumed
            std::ranges::sort(opaqueObjs, [&](RenderObject& a, RenderObject& b) {
               float az = glm::dot(camera.Forward(), a.trans.position);
               float bz = glm::dot(camera.Forward(), b.trans.position);
               return az < bz;
               });
         }
         UpdateInstanceBuffer(cmd, opaqueObjs);

         if (cfg.useZPass) {
            {
               GPU_MARKER("ZPass");
               PROFILE_GPU("ZPass");

               cmd.SetRenderTargets(nullptr, cameraContext.depth);
               cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);

               RenderSceneAllObjects(cmd, opaqueObjs, *baseZPass, cb);
            }

            {
               GPU_MARKER("Color");
               PROFILE_GPU("Color");

               cmd.SetRenderTargets(cameraContext.color, cameraContext.depth);
               cmd.SetDepthStencilState(rendres::depthStencilStateEqual);
               RenderSceneAllObjects(cmd, opaqueObjs, *baseColorPass, cb);
            }
         } else {
            GPU_MARKER("Color (Without ZPass)");
            PROFILE_GPU("Color (Without ZPass)");

            cmd.SetRenderTargets(cameraContext.color, cameraContext.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadWrite);

            RenderSceneAllObjects(cmd, opaqueObjs, *baseColorPass, cb);
         }

         if (cfg.renderTransparency) {
            GPU_MARKER("Transparency");
            PROFILE_GPU("Transparency");

            cmd.SetRenderTargets(cameraContext.color, cameraContext.depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDepthReadNoWrite);
            cmd.SetBlendState(rendres::blendStateTransparency);

            if (cfg.transparencySorting) {
               // todo: slow. I assumed
               std::ranges::sort(transparentObjs, [&](RenderObject& a, RenderObject& b) {
                  float az = glm::dot(camera.Forward(), a.trans.position);
                  float bz = glm::dot(camera.Forward(), b.trans.position);
                  return az > bz;
                  });
            }

            UpdateInstanceBuffer(cmd, transparentObjs);
            RenderSceneAllObjects(cmd, transparentObjs, *baseColorPass, cb);
         }
      }

      void RenderSceneAllObjects(CommandList& cmd, const std::vector<RenderObject>& renderObjs, GpuProgram& program, const CameraCB& cameraCB) {
         CameraCB cb = cameraCB;

         program.Activate(cmd);
         program.SetConstantBuffer(cmd, "gCamera", *cameraCbBuffer);
         program.SetSrvBuffer(cmd, "gInstances", *instanceBuffer);
         program.SetSrvBuffer(cmd, "gLights", *lightBuffer);

         int instanceID = 0;

         for (const auto& [trans, material] : renderObjs) {
            cb.transform = glm::translate(mat4(1), trans.position);
            cb.transform = glm::transpose(cb.transform);

            cb.instanceStart = instanceID++;

            cb.directLight.color = vec3{1} * 0.5f;
            cb.directLight.direction = glm::normalize(vec3{-1, -1, -1});

            cmd.UpdateSubresource(*cameraCbBuffer, &cb);

            if (cfg.useInstancedDraw) {
               program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount(), (int)renderObjs.size());
               break;
            } else {
               // program->DrawInstanced(cmd, mesh.geom.VertexCount());
               program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
            }
         }
      }

   };

}
