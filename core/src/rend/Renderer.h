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

      mat4 GetViewProjection() const {
         return projection * view;
      }
   };

   class Renderer {
   public:
      ~Renderer() {
         rendres::Term();
      }

      Ref<GpuProgram> baseColorPass;
      Ref<GpuProgram> baseZPass;

      Mesh mesh;

      Ref<Buffer> cameraCbBuffer;
      Ref<Buffer> instanceBuffer;
      Ref<Buffer> lightBuffer;
      // int nLights = 4;

      bool renderAsTransparency = false;
      bool useZPass = false;
      bool useInstancedDraw = false;

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

      struct RenderObject {
         SceneTransformComponent trans;
         SimpleMaterialComponent material;
      };

      std::vector<RenderObject> opaqueObjs;
      std::vector<RenderObject> transparentObjs;

      void RenderDataPrepare(CommandList& cmd, Scene& scene) {
         // opaqueObjs.clear();
         // transparentObjs.clear();
         //
         // for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
         //    if (material.opaque) {
         //       opaqueObjs.emplace_back(sceneTrans, material);
         //    } else {
         //       transparentObjs.emplace_back(sceneTrans, material);
         //    }
         // }

         if (!instanceBuffer || instanceBuffer->ElementsCount() != scene.EntitiesCount()) {
            auto bufferDesc = Buffer::Desc::StructureBuffer(scene.EntitiesCount(), sizeof(Instance));
            instanceBuffer = Buffer::Create(bufferDesc);
            instanceBuffer->SetDbgName("instance buffer");
         }

         std::vector<Instance> instances;
         instances.reserve(scene.EntitiesCount());
         for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
            mat4 transform = glm::translate(mat4(1), sceneTrans.position);
            transform *= mat4{ sceneTrans.rotation };
            transform *= glm::scale(mat4(1), sceneTrans.scale);
            transform = glm::transpose(transform);

            Material m;
            m.albedo = material.albedo;
            m.roughness = material.roughness;
            m.metallic = material.metallic;

            instances.emplace_back(transform, m);
         }

         cmd.UpdateSubresource(*instanceBuffer, instances.data());

         auto nLights = (int)scene.GetEntitiesWith<LightComponent>().size();
         if (!lightBuffer || lightBuffer->ElementsCount() != nLights) {
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

         cmd.UpdateSubresource(*lightBuffer, lights.data());
      }

      void RenderScene(Texture2D& target, Texture2D& depth, CommandList& cmd, Scene& scene, const RenderCamera& camera) {
         if (!baseColorPass->Valid() || !baseZPass->Valid()) {
            return;
         }

         GPU_MARKER("Render Scene");
         PROFILE_GPU("Render Scene");

         RenderDataPrepare(cmd, scene);

         auto context = cmd.pContext;

         vec4 background_colour{0};
         cmd.ClearRenderTarget(target, background_colour);
         cmd.ClearDepthTarget(depth, 1);

         cmd.SetViewport({}, target.GetDesc().size);
         cmd.SetRasterizerState(rendres::rasterizerState);
         // context->OMSetBlendState(nullptr, nullptr, 0xffffffff);

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

         if (renderAsTransparency) {
            GPU_MARKER("Color Pass");
            PROFILE_GPU("Color Pass");

            cmd.SetRenderTargets(&target, &depth);
            cmd.SetDepthStencilState(rendres::depthStencilStateDisable);
            cmd.SetBlendState(rendres::blendStateTransparency);
            RenderSceneAllObjects(cmd, scene, *baseColorPass, cb);

            return;
         }

         cmd.SetBlendState(nullptr);
         if (useZPass) {
            {
               GPU_MARKER("ZPass");
               PROFILE_GPU("ZPass");

               cmd.SetRenderTargets(nullptr, &depth);
               cmd.SetDepthStencilState(rendres::depthStencilState);
               RenderSceneAllObjects(cmd, scene, *baseZPass, cb);
            }

            {
               GPU_MARKER("Color Pass");
               PROFILE_GPU("Color Pass");

               cmd.SetRenderTargets(&target, &depth);
               cmd.SetDepthStencilState(rendres::depthStencilStateEqual);
               RenderSceneAllObjects(cmd, scene, *baseColorPass, cb);
            }
         } else {
            GPU_MARKER("Color Pass (Without ZPass)");
            PROFILE_GPU("Color Pass (Without ZPass)");

            cmd.SetRenderTargets(&target, &depth);
            cmd.SetDepthStencilState(rendres::depthStencilState);
            RenderSceneAllObjects(cmd, scene, *baseColorPass, cb);
         }
      }

      void RenderSceneAllObjects(CommandList& cmd, Scene& scene, GpuProgram& program, const CameraCB& cameraCB) {
         CameraCB cb = cameraCB;

         program.Activate(cmd);
         program.SetConstantBuffer(cmd, "gCamera", *cameraCbBuffer);
         program.SetSrvBuffer(cmd, "gInstances", *instanceBuffer);
         program.SetSrvBuffer(cmd, "gLights", *lightBuffer);

         int instanceID = 0;

         for (auto [e, sceneTrans, material] : scene.GetEntitiesWith<SceneTransformComponent, SimpleMaterialComponent>().each()) {
            cb.transform = glm::translate(mat4(1), sceneTrans.position);
            cb.transform = glm::transpose(cb.transform);

            cb.color = material.albedo;
            cb.roughness = material.roughness;
            cb.metallic = material.metallic;
            cb.instanceStart = instanceID++;

            cmd.UpdateSubresource(*cameraCbBuffer, &cb);

            if (useInstancedDraw) {
               // todo: hack
               auto nGeoms = (int)scene.GetEntitiesWith<SimpleMaterialComponent>().size();
               program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount(), nGeoms);
               break;
            } else {
               // program->DrawInstanced(cmd, mesh.geom.VertexCount());
               program.DrawIndexedInstanced(cmd, mesh.geom.IndexCount());
            }
         }
      }

   };

}
